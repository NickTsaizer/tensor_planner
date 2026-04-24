#include "planner_internal.hpp"

struct StateIndices {
  std::vector<std::vector<const Fact *>> facts_by_predicate;
  std::unordered_map<std::vector<int32_t>, float, SignatureHash> function_values;
};

namespace {

bool allocation_failed(int32_t count, const void *pointer) {
  return count > 0 && pointer == nullptr;
}

struct ScoredCandidateAction {
  CandidateAction action;
  int32_t score = 0;
};

Fact instantiate_fact(const ActionLiteral &literal, const CandidateAction &action);
Fact instantiate_fact(const ActionEffect &effect, const CandidateAction &action);

std::vector<int32_t> make_function_key(const FunctionValue &value) {
  std::vector<int32_t> key;
  key.reserve(static_cast<std::size_t>(value.arity) + 1);
  key.push_back(value.function_id);
  for (uint8_t index = 0; index < value.arity; ++index) {
    key.push_back(value.args[index]);
  }
  return key;
}

StateIndices build_state_indices_impl(const TP_Domain &domain, const TP_State &state) {
  StateIndices indices {};
  indices.facts_by_predicate.resize(domain.predicates.size());

  for (const Fact &fact : state.facts) {
    indices.facts_by_predicate[static_cast<std::size_t>(fact.predicate_id)].push_back(&fact);
  }

  for (const FunctionValue &value : state.function_values) {
    indices.function_values.emplace(make_function_key(value), value.value);
  }

  return indices;
}

int32_t score_candidate_relevance_impl(
  const TP_Domain &domain,
  const TP_State &state,
  const CandidateAction &candidate
) {
  const ActionSchema &schema = domain.actions[static_cast<std::size_t>(candidate.schema_id)];
  int32_t score = 0;

  for (const Fact &goal : state.goals) {
    if (state_has_fact(state, goal)) {
      continue;
    }

    for (const ActionEffect &effect : schema.effects) {
      if (effect.op != TP_EFFECT_ADD) {
        continue;
      }

      const Fact produced = instantiate_fact(effect, candidate);
      if (fact_equals(produced, goal)) {
        score += 4;
      } else if (produced.predicate_id == goal.predicate_id) {
        score += 1;
      }
    }
  }

  return score;
}

bool object_matches_type(const TP_State &state, int32_t object_id, int32_t type_id) {
  if (object_id < 0 || object_id >= static_cast<int32_t>(state.object_types.size())) {
    return false;
  }

  return state.object_types[static_cast<std::size_t>(object_id)] == type_id;
}

Fact instantiate_fact(const ActionLiteral &literal, const CandidateAction &action) {
  Fact fact {};
  fact.predicate_id = literal.predicate_id;
  fact.arity = literal.arity;
  for (uint8_t index = 0; index < literal.arity; ++index) {
    fact.args[index] = action.args[static_cast<std::size_t>(literal.slots[index])];
  }
  return fact;
}

Fact instantiate_fact(const ActionEffect &effect, const CandidateAction &action) {
  Fact fact {};
  fact.predicate_id = effect.predicate_id;
  fact.arity = effect.arity;
  for (uint8_t index = 0; index < effect.arity; ++index) {
    fact.args[index] = action.args[static_cast<std::size_t>(effect.slots[index])];
  }
  return fact;
}

FunctionValue instantiate_function_value(
  int32_t function_id,
  uint8_t arity,
  const std::array<int8_t, TP_MAX_ARITY> &slots,
  const CandidateAction &action
) {
  FunctionValue value {};
  value.function_id = function_id;
  value.arity = arity;
  for (uint8_t index = 0; index < arity; ++index) {
    value.args[index] = action.args[static_cast<std::size_t>(slots[index])];
  }
  return value;
}

bool compare_numeric_value(float lhs, uint8_t cmp_op, float rhs) {
  switch (cmp_op) {
    case TP_NUM_CMP_LT:
      return lhs < rhs;
    case TP_NUM_CMP_LTE:
      return lhs <= rhs;
    case TP_NUM_CMP_EQ:
      return lhs == rhs;
    case TP_NUM_CMP_GTE:
      return lhs >= rhs;
    case TP_NUM_CMP_GT:
      return lhs > rhs;
    default:
      return false;
  }
}

bool state_has_fact_indexed(const StateIndices &indices, const Fact &fact) {
  if (fact.predicate_id < 0 ||
      fact.predicate_id >= static_cast<int32_t>(indices.facts_by_predicate.size())) {
    return false;
  }

  const auto &bucket = indices.facts_by_predicate[static_cast<std::size_t>(fact.predicate_id)];
  return std::any_of(bucket.begin(), bucket.end(), [&fact](const Fact *existing) {
    return fact_equals(*existing, fact);
  });
}

bool candidate_slot_is_bound(const CandidateAction &candidate, int8_t slot) {
  return candidate.args[static_cast<std::size_t>(slot)] >= 0;
}

bool positive_literal_matches_fact(
  const ActionLiteral &literal,
  const Fact &fact,
  const CandidateAction &candidate
) {
  if (literal.sign <= 0 || literal.predicate_id != fact.predicate_id || literal.arity != fact.arity) {
    return false;
  }

  for (uint8_t index = 0; index < literal.arity; ++index) {
    const int8_t slot = literal.slots[index];
    const int32_t fact_arg = fact.args[index];
    if (candidate_slot_is_bound(candidate, slot) &&
        candidate.args[static_cast<std::size_t>(slot)] != fact_arg) {
      return false;
    }
  }

  return true;
}

CandidateAction bind_literal_to_fact(
  const ActionLiteral &literal,
  const Fact &fact,
  const CandidateAction &candidate
) {
  CandidateAction bound = candidate;
  for (uint8_t index = 0; index < literal.arity; ++index) {
    const int8_t slot = literal.slots[index];
    bound.args[static_cast<std::size_t>(slot)] = fact.args[index];
  }
  return bound;
}

std::vector<int32_t> get_positive_precondition_order(const TP_State &state, const ActionSchema &schema) {
  std::vector<int32_t> order;
  order.reserve(schema.preconditions.size());
  for (int32_t index = 0; index < static_cast<int32_t>(schema.preconditions.size()); ++index) {
    if (schema.preconditions[static_cast<std::size_t>(index)].sign > 0) {
      order.push_back(index);
    }
  }

  std::sort(order.begin(), order.end(), [&state, &schema](int32_t lhs, int32_t rhs) {
    const int32_t lhs_predicate = schema.preconditions[static_cast<std::size_t>(lhs)].predicate_id;
    const int32_t rhs_predicate = schema.preconditions[static_cast<std::size_t>(rhs)].predicate_id;
    const int32_t lhs_count = static_cast<int32_t>(std::count_if(
      state.facts.begin(),
      state.facts.end(),
      [lhs_predicate](const Fact &fact) { return fact.predicate_id == lhs_predicate; }
    ));
    const int32_t rhs_count = static_cast<int32_t>(std::count_if(
      state.facts.begin(),
      state.facts.end(),
      [rhs_predicate](const Fact &fact) { return fact.predicate_id == rhs_predicate; }
    ));

    if (lhs_count != rhs_count) {
      return lhs_count < rhs_count;
    }

    return lhs < rhs;
  });

  return order;
}

bool action_is_applicable_with_indices(
  const TP_Domain &domain,
  const TP_State &state,
  const StateIndices &indices,
  const CandidateAction &action
) {
  if (action.schema_id < 0 || action.schema_id >= static_cast<int32_t>(domain.actions.size())) {
    return false;
  }

  const ActionSchema &schema = domain.actions[static_cast<std::size_t>(action.schema_id)];
  for (uint8_t index = 0; index < schema.arity; ++index) {
    if (!object_matches_type(state, action.args[index], schema.arg_types[index])) {
      return false;
    }
  }

  for (const ActionLiteral &literal : schema.preconditions) {
    const Fact fact = instantiate_fact(literal, action);
    const bool is_true = state_has_fact_indexed(indices, fact);
    if ((literal.sign > 0 && !is_true) || (literal.sign < 0 && is_true)) {
      return false;
    }
  }

  for (const NumericPrecondition &precondition : schema.numeric_preconditions) {
    const FunctionValue query = instantiate_function_value(
      precondition.function_id,
      precondition.arity,
      precondition.slots,
      action
    );
    const auto found = indices.function_values.find(make_function_key(query));
    if (found == indices.function_values.end()) {
      return false;
    }

    if (!compare_numeric_value(found->second, precondition.cmp_op, precondition.rhs_value)) {
      return false;
    }
  }

  return true;
}

void generate_assignments(
  const TP_Domain &domain,
  const TP_State &state,
  const StateIndices &indices,
  int32_t schema_id,
  int32_t slot_index,
  CandidateAction *candidate,
  std::vector<CandidateAction> *out_actions,
  int32_t max_candidates
) {
  if (static_cast<int32_t>(out_actions->size()) >= max_candidates) {
    return;
  }

  const ActionSchema &schema = domain.actions[static_cast<std::size_t>(schema_id)];
  while (slot_index < schema.arity && candidate_slot_is_bound(*candidate, static_cast<int8_t>(slot_index))) {
    ++slot_index;
  }

  if (slot_index == schema.arity) {
    if (action_is_applicable_with_indices(domain, state, indices, *candidate)) {
      out_actions->push_back(*candidate);
    }
    return;
  }

  const int32_t type_id = schema.arg_types[static_cast<std::size_t>(slot_index)];
  for (int32_t object_id = 0; object_id < static_cast<int32_t>(state.object_types.size()); ++object_id) {
    if (!object_matches_type(state, object_id, type_id)) {
      continue;
    }

    candidate->args[static_cast<std::size_t>(slot_index)] = object_id;
    generate_assignments(
      domain,
      state,
      indices,
      schema_id,
      slot_index + 1,
      candidate,
      out_actions,
      max_candidates
    );
  }
}

void generate_candidates_from_positive_preconditions(
  const TP_Domain &domain,
  const TP_State &state,
  const StateIndices &indices,
  int32_t schema_id,
  const std::vector<int32_t> &precondition_order,
  int32_t precondition_order_index,
  const CandidateAction &candidate,
  std::vector<CandidateAction> *out_actions,
  int32_t max_candidates
) {
  if (static_cast<int32_t>(out_actions->size()) >= max_candidates) {
    return;
  }

  if (precondition_order_index == static_cast<int32_t>(precondition_order.size())) {
    CandidateAction completed = candidate;
    generate_assignments(domain, state, indices, schema_id, 0, &completed, out_actions, max_candidates);
    return;
  }

  const ActionSchema &schema = domain.actions[static_cast<std::size_t>(schema_id)];
  const ActionLiteral &literal =
    schema.preconditions[static_cast<std::size_t>(precondition_order[static_cast<std::size_t>(precondition_order_index)])];

  const auto &bucket = indices.facts_by_predicate[static_cast<std::size_t>(literal.predicate_id)];
  for (const Fact *fact : bucket) {
    if (!positive_literal_matches_fact(literal, *fact, candidate)) {
      continue;
    }

    const CandidateAction bound_candidate = bind_literal_to_fact(literal, *fact, candidate);
    generate_candidates_from_positive_preconditions(
      domain,
      state,
      indices,
      schema_id,
      precondition_order,
      precondition_order_index + 1,
      bound_candidate,
      out_actions,
      max_candidates
    );
  }
}

}  // namespace

int32_t score_candidate_relevance(
  const TP_Domain &domain,
  const TP_State &state,
  const CandidateAction &candidate
) {
  return score_candidate_relevance_impl(domain, state, candidate);
}

const StateIndices &get_or_build_state_indices(const TP_Domain &domain, const TP_State &state, bool *rebuilt) {
  if (rebuilt != nullptr) {
    *rebuilt = false;
  }

  if (!state.indices_cache || state.indices_dirty) {
    state.indices_cache = std::make_shared<StateIndices>(build_state_indices_impl(domain, state));
    state.indices_dirty = false;
    if (rebuilt != nullptr) {
      *rebuilt = true;
    }
  }

  return *state.indices_cache;
}

void invalidate_state_indices(TP_State *state) {
  if (state != nullptr) {
    state->indices_cache.reset();
    state->indices_dirty = true;
  }
}

bool validate_fact(
  const TP_Domain &domain,
  const TP_State &state,
  int32_t predicate_id,
  uint8_t arity,
  const int32_t *args
) {
  if (args == nullptr || predicate_id < 0 || predicate_id >= static_cast<int32_t>(domain.predicates.size())) {
    return false;
  }

  const PredicateDef &predicate = domain.predicates[static_cast<std::size_t>(predicate_id)];
  if (arity != predicate.arity) {
    return false;
  }

  for (uint8_t index = 0; index < arity; ++index) {
    if (!object_matches_type(state, args[index], predicate.arg_types[index])) {
      return false;
    }
  }

  return true;
}

bool validate_function_value(
  const TP_Domain &domain,
  const TP_State &state,
  int32_t function_id,
  uint8_t arity,
  const int32_t *args
) {
  if (args == nullptr || function_id < 0 || function_id >= static_cast<int32_t>(domain.functions.size())) {
    return false;
  }

  const FunctionDef &function = domain.functions[static_cast<std::size_t>(function_id)];
  if (arity != function.arity) {
    return false;
  }

  for (uint8_t index = 0; index < arity; ++index) {
    if (!object_matches_type(state, args[index], function.arg_types[index])) {
      return false;
    }
  }

  return true;
}

Fact make_fact(int32_t predicate_id, uint8_t arity, const int32_t *args) {
  Fact fact {};
  fact.predicate_id = predicate_id;
  fact.arity = arity;
  for (uint8_t index = 0; index < arity; ++index) {
    fact.args[index] = args[index];
  }
  return fact;
}

bool fact_equals(const Fact &lhs, const Fact &rhs) {
  if (lhs.predicate_id != rhs.predicate_id || lhs.arity != rhs.arity) {
    return false;
  }

  for (uint8_t index = 0; index < lhs.arity; ++index) {
    if (lhs.args[index] != rhs.args[index]) {
      return false;
    }
  }

  return true;
}

bool state_has_fact(const TP_State &state, const Fact &fact) {
  return std::any_of(
    state.facts.begin(),
    state.facts.end(),
    [&fact](const Fact &existing) { return fact_equals(existing, fact); }
  );
}

bool add_fact_unique(std::vector<Fact> *facts, const Fact &fact) {
  const auto duplicate = std::find_if(
    facts->begin(),
    facts->end(),
    [&fact](const Fact &existing) { return fact_equals(existing, fact); }
  );

  if (duplicate != facts->end()) {
    return false;
  }

  facts->push_back(fact);
  return true;
}

void remove_fact(std::vector<Fact> *facts, const Fact &fact) {
  facts->erase(
    std::remove_if(
      facts->begin(),
      facts->end(),
      [&fact](const Fact &existing) { return fact_equals(existing, fact); }
    ),
    facts->end()
  );
}

int32_t count_unsatisfied_goals(const TP_State &state) {
  int32_t unsatisfied = 0;
  for (const Fact &goal : state.goals) {
    if (!state_has_fact(state, goal)) {
      ++unsatisfied;
    }
  }
  return unsatisfied;
}

bool upsert_function_value(std::vector<FunctionValue> *values, const FunctionValue &value) {
  for (FunctionValue &existing : *values) {
    if (existing.function_id != value.function_id || existing.arity != value.arity) {
      continue;
    }

    bool same_args = true;
    for (uint8_t index = 0; index < value.arity; ++index) {
      if (existing.args[index] != value.args[index]) {
        same_args = false;
        break;
      }
    }

    if (same_args) {
      existing.value = value.value;
      return false;
    }
  }

  values->push_back(value);
  return true;
}

bool try_get_function_value(const TP_State &state, const FunctionValue &query, float *out_value) {
  for (const FunctionValue &existing : state.function_values) {
    if (existing.function_id != query.function_id || existing.arity != query.arity) {
      continue;
    }

    bool same_args = true;
    for (uint8_t index = 0; index < query.arity; ++index) {
      if (existing.args[index] != query.args[index]) {
        same_args = false;
        break;
      }
    }

    if (same_args) {
      *out_value = existing.value;
      return true;
    }
  }

  return false;
}

bool action_is_applicable(
  const TP_Domain &domain,
  const TP_State &state,
  const CandidateAction &action
) {
  const StateIndices &indices = get_or_build_state_indices(domain, state, nullptr);
  return action_is_applicable_with_indices(domain, state, indices, action);
}

TP_State apply_action(
  const TP_Domain &domain,
  const TP_State &state,
  const CandidateAction &action
) {
  TP_State next_state = state;
  const ActionSchema &schema = domain.actions[static_cast<std::size_t>(action.schema_id)];
  for (const ActionEffect &effect : schema.effects) {
    const Fact fact = instantiate_fact(effect, action);
    if (effect.op == TP_EFFECT_ADD) {
      add_fact_unique(&next_state.facts, fact);
    } else if (effect.op == TP_EFFECT_DELETE) {
      remove_fact(&next_state.facts, fact);
    }
  }

  for (const NumericEffect &effect : schema.numeric_effects) {
    FunctionValue value = instantiate_function_value(effect.function_id, effect.arity, effect.slots, action);
    float current_value = 0.0f;
    if (try_get_function_value(next_state, value, &current_value)) {
      value.value = current_value;
    }

    switch (effect.op) {
      case TP_NUM_EFFECT_SET:
        value.value = effect.rhs_value;
        break;
      case TP_NUM_EFFECT_ADD:
        value.value += effect.rhs_value;
        break;
      case TP_NUM_EFFECT_SUBTRACT:
        value.value -= effect.rhs_value;
        break;
      default:
        break;
    }

    upsert_function_value(&next_state.function_values, value);
  }

  invalidate_state_indices(&next_state);

  return next_state;
}

std::vector<CandidateAction> generate_candidate_actions(
  const TP_Domain &domain,
  const TP_State &state,
  int32_t max_candidates
) {
  std::vector<ScoredCandidateAction> scored_actions;
  scored_actions.reserve(static_cast<std::size_t>(std::max(0, max_candidates)));
  const StateIndices &indices = get_or_build_state_indices(domain, state, nullptr);

  for (int32_t schema_id = 0; schema_id < static_cast<int32_t>(domain.actions.size()); ++schema_id) {
    std::vector<CandidateAction> schema_actions;
    schema_actions.reserve(static_cast<std::size_t>(std::max(0, max_candidates)));
    CandidateAction candidate {};
    candidate.schema_id = schema_id;
    candidate.arity = domain.actions[static_cast<std::size_t>(schema_id)].arity;
    candidate.args.fill(-1);

    const ActionSchema &schema = domain.actions[static_cast<std::size_t>(schema_id)];
    const std::vector<int32_t> positive_precondition_order = get_positive_precondition_order(state, schema);
    if (positive_precondition_order.empty()) {
      generate_assignments(
        domain,
        state,
        indices,
        schema_id,
        0,
        &candidate,
        &schema_actions,
        max_candidates
      );
    } else {
      generate_candidates_from_positive_preconditions(
        domain,
        state,
        indices,
        schema_id,
        positive_precondition_order,
        0,
        candidate,
        &schema_actions,
        max_candidates
      );
    }

    for (const CandidateAction &generated : schema_actions) {
      scored_actions.push_back({generated, score_candidate_relevance_impl(domain, state, generated)});
    }

    if (static_cast<int32_t>(scored_actions.size()) >= max_candidates * 2) {
      break;
    }
  }

  std::sort(scored_actions.begin(), scored_actions.end(), [](const ScoredCandidateAction &lhs, const ScoredCandidateAction &rhs) {
    if (lhs.score != rhs.score) {
      return lhs.score > rhs.score;
    }

    if (lhs.action.schema_id != rhs.action.schema_id) {
      return lhs.action.schema_id < rhs.action.schema_id;
    }

    return lhs.action.args < rhs.action.args;
  });

  if (static_cast<int32_t>(scored_actions.size()) > max_candidates) {
    scored_actions.resize(static_cast<std::size_t>(max_candidates));
  }

  std::vector<CandidateAction> actions;
  actions.reserve(scored_actions.size());
  for (const ScoredCandidateAction &scored : scored_actions) {
    actions.push_back(scored.action);
  }

  return actions;
}

std::vector<int32_t> make_state_signature(const TP_State &state) {
  std::vector<Fact> sorted_facts = state.facts;
  std::sort(
    sorted_facts.begin(),
    sorted_facts.end(),
    [](const Fact &lhs, const Fact &rhs) {
      if (lhs.predicate_id != rhs.predicate_id) {
        return lhs.predicate_id < rhs.predicate_id;
      }
      if (lhs.arity != rhs.arity) {
        return lhs.arity < rhs.arity;
      }
      return lhs.args < rhs.args;
    }
  );

  std::vector<int32_t> signature;
  signature.reserve(sorted_facts.size() * (TP_MAX_ARITY + 2));
  for (const Fact &fact : sorted_facts) {
    signature.push_back(fact.predicate_id);
    signature.push_back(static_cast<int32_t>(fact.arity));
    for (uint8_t index = 0; index < fact.arity; ++index) {
      signature.push_back(fact.args[index]);
    }
  }

  std::vector<FunctionValue> sorted_values = state.function_values;
  std::sort(
    sorted_values.begin(),
    sorted_values.end(),
    [](const FunctionValue &lhs, const FunctionValue &rhs) {
      if (lhs.function_id != rhs.function_id) {
        return lhs.function_id < rhs.function_id;
      }
      if (lhs.arity != rhs.arity) {
        return lhs.arity < rhs.arity;
      }
      return lhs.args < rhs.args;
    }
  );

  for (const FunctionValue &value : sorted_values) {
    signature.push_back(-1000 - value.function_id);
    signature.push_back(static_cast<int32_t>(value.arity));
    for (uint8_t index = 0; index < value.arity; ++index) {
      signature.push_back(value.args[index]);
    }

    int32_t encoded_value = 0;
    static_assert(sizeof(float) == sizeof(int32_t));
    std::memcpy(&encoded_value, &value.value, sizeof(float));
    signature.push_back(encoded_value);
  }

  return signature;
}

TP_Status export_problem_tensors_impl(
  const TP_Domain &domain,
  const TP_State &state,
  int32_t max_candidates,
  TP_Problem_Tensors *out_tensors
) {
  if (out_tensors == nullptr || max_candidates <= 0) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  const std::vector<CandidateAction> candidates = generate_candidate_actions(domain, state, max_candidates);
  return export_problem_tensors_for_candidates_impl(domain, state, candidates, out_tensors);
}

TP_Status export_problem_tensors_for_candidates_impl(
  const TP_Domain &domain,
  const TP_State &state,
  const std::vector<CandidateAction> &candidates,
  TP_Problem_Tensors *out_tensors
) {
  if (out_tensors == nullptr) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  zero_problem_tensors(out_tensors);

  out_tensors->object_count = static_cast<int32_t>(state.object_types.size());
  out_tensors->fact_count = static_cast<int32_t>(state.facts.size());
  out_tensors->function_value_count = static_cast<int32_t>(state.function_values.size());
  out_tensors->goal_count = static_cast<int32_t>(state.goals.size());
  out_tensors->candidate_count = static_cast<int32_t>(candidates.size());
  out_tensors->obj_type_count = out_tensors->object_count;
  out_tensors->true_pred_id_count = out_tensors->fact_count;
  out_tensors->true_pred_arg_count = out_tensors->fact_count * TP_MAX_ARITY;
  out_tensors->true_pred_mask_count = out_tensors->fact_count;
  out_tensors->num_func_id_count = out_tensors->function_value_count;
  out_tensors->num_func_arg_count = out_tensors->function_value_count * TP_MAX_ARITY;
  out_tensors->num_value_count = out_tensors->function_value_count;
  out_tensors->goal_pred_id_count = out_tensors->goal_count;
  out_tensors->goal_pred_arg_count = out_tensors->goal_count * TP_MAX_ARITY;
  out_tensors->goal_pred_mask_count = out_tensors->goal_count;
  out_tensors->cand_action_schema_count = out_tensors->candidate_count;
  out_tensors->cand_action_arg_count = out_tensors->candidate_count * TP_MAX_PARAMS;
  out_tensors->cand_action_mask_count = out_tensors->candidate_count;

  out_tensors->obj_type = static_cast<int16_t *>(std::calloc(out_tensors->obj_type_count, sizeof(int16_t)));
  out_tensors->true_pred_id = static_cast<int16_t *>(std::calloc(out_tensors->true_pred_id_count, sizeof(int16_t)));
  out_tensors->true_pred_arg = static_cast<int16_t *>(std::calloc(out_tensors->true_pred_arg_count, sizeof(int16_t)));
  out_tensors->true_pred_mask = static_cast<uint8_t *>(std::calloc(out_tensors->true_pred_mask_count, sizeof(uint8_t)));
  out_tensors->num_func_id = static_cast<int16_t *>(std::calloc(out_tensors->num_func_id_count, sizeof(int16_t)));
  out_tensors->num_func_arg = static_cast<int16_t *>(std::calloc(out_tensors->num_func_arg_count, sizeof(int16_t)));
  out_tensors->num_value = static_cast<float *>(std::calloc(out_tensors->num_value_count, sizeof(float)));
  out_tensors->goal_pred_id = static_cast<int16_t *>(std::calloc(out_tensors->goal_pred_id_count, sizeof(int16_t)));
  out_tensors->goal_pred_arg = static_cast<int16_t *>(std::calloc(out_tensors->goal_pred_arg_count, sizeof(int16_t)));
  out_tensors->goal_pred_mask = static_cast<uint8_t *>(std::calloc(out_tensors->goal_pred_mask_count, sizeof(uint8_t)));
  out_tensors->cand_action_schema = static_cast<int16_t *>(std::calloc(out_tensors->cand_action_schema_count, sizeof(int16_t)));
  out_tensors->cand_action_arg = static_cast<int16_t *>(std::calloc(out_tensors->cand_action_arg_count, sizeof(int16_t)));
  out_tensors->cand_action_mask = static_cast<uint8_t *>(std::calloc(out_tensors->cand_action_mask_count, sizeof(uint8_t)));

  if (allocation_failed(out_tensors->obj_type_count, out_tensors->obj_type) ||
      allocation_failed(out_tensors->true_pred_id_count, out_tensors->true_pred_id) ||
      allocation_failed(out_tensors->true_pred_arg_count, out_tensors->true_pred_arg) ||
      allocation_failed(out_tensors->true_pred_mask_count, out_tensors->true_pred_mask) ||
      allocation_failed(out_tensors->num_func_id_count, out_tensors->num_func_id) ||
      allocation_failed(out_tensors->num_func_arg_count, out_tensors->num_func_arg) ||
      allocation_failed(out_tensors->num_value_count, out_tensors->num_value) ||
      allocation_failed(out_tensors->goal_pred_id_count, out_tensors->goal_pred_id) ||
      allocation_failed(out_tensors->goal_pred_arg_count, out_tensors->goal_pred_arg) ||
      allocation_failed(out_tensors->goal_pred_mask_count, out_tensors->goal_pred_mask) ||
      allocation_failed(out_tensors->cand_action_schema_count, out_tensors->cand_action_schema) ||
      allocation_failed(out_tensors->cand_action_arg_count, out_tensors->cand_action_arg) ||
      allocation_failed(out_tensors->cand_action_mask_count, out_tensors->cand_action_mask)) {
    tp_problem_tensors_dispose(out_tensors);
    return TP_STATUS_LIMIT_EXCEEDED;
  }

  for (int32_t object_index = 0; object_index < out_tensors->object_count; ++object_index) {
    out_tensors->obj_type[object_index] = static_cast<int16_t>(state.object_types[static_cast<std::size_t>(object_index)]);
  }

  for (int32_t fact_index = 0; fact_index < out_tensors->fact_count; ++fact_index) {
    const Fact &fact = state.facts[static_cast<std::size_t>(fact_index)];
    out_tensors->true_pred_id[fact_index] = static_cast<int16_t>(fact.predicate_id);
    out_tensors->true_pred_mask[fact_index] = 1;
    for (int32_t arg_index = 0; arg_index < TP_MAX_ARITY; ++arg_index) {
      out_tensors->true_pred_arg[fact_index * TP_MAX_ARITY + arg_index] = static_cast<int16_t>(fact.args[static_cast<std::size_t>(arg_index)]);
    }
  }

  for (int32_t value_index = 0; value_index < out_tensors->function_value_count; ++value_index) {
    const FunctionValue &value = state.function_values[static_cast<std::size_t>(value_index)];
    out_tensors->num_func_id[value_index] = static_cast<int16_t>(value.function_id);
    out_tensors->num_value[value_index] = value.value;
    for (int32_t arg_index = 0; arg_index < TP_MAX_ARITY; ++arg_index) {
      out_tensors->num_func_arg[value_index * TP_MAX_ARITY + arg_index] = static_cast<int16_t>(value.args[static_cast<std::size_t>(arg_index)]);
    }
  }

  for (int32_t goal_index = 0; goal_index < out_tensors->goal_count; ++goal_index) {
    const Fact &goal = state.goals[static_cast<std::size_t>(goal_index)];
    out_tensors->goal_pred_id[goal_index] = static_cast<int16_t>(goal.predicate_id);
    out_tensors->goal_pred_mask[goal_index] = 1;
    for (int32_t arg_index = 0; arg_index < TP_MAX_ARITY; ++arg_index) {
      out_tensors->goal_pred_arg[goal_index * TP_MAX_ARITY + arg_index] = static_cast<int16_t>(goal.args[static_cast<std::size_t>(arg_index)]);
    }
  }

  for (int32_t candidate_index = 0; candidate_index < out_tensors->candidate_count; ++candidate_index) {
    const CandidateAction &candidate = candidates[static_cast<std::size_t>(candidate_index)];
    out_tensors->cand_action_schema[candidate_index] = static_cast<int16_t>(candidate.schema_id);
    out_tensors->cand_action_mask[candidate_index] = 1;
    for (int32_t arg_index = 0; arg_index < TP_MAX_PARAMS; ++arg_index) {
      out_tensors->cand_action_arg[candidate_index * TP_MAX_PARAMS + arg_index] = static_cast<int16_t>(candidate.args[static_cast<std::size_t>(arg_index)]);
    }
  }

  return TP_STATUS_OK;
}

void zero_problem_tensors(TP_Problem_Tensors *tensors) {
  if (tensors != nullptr) {
    std::memset(tensors, 0, sizeof(TP_Problem_Tensors));
  }
}

TP_Status export_action_graph_for_candidates_impl(
  const TP_Domain &domain,
  const TP_State &state,
  const std::vector<CandidateAction> &candidates,
  TP_Action_Graph *out_graph
) {
  if (out_graph == nullptr) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  zero_action_graph(out_graph);

  const int32_t object_count = static_cast<int32_t>(state.object_types.size());
  const int32_t fact_count = static_cast<int32_t>(state.facts.size());
  const int32_t action_count = static_cast<int32_t>(candidates.size());
  const int32_t total_node_count = object_count + fact_count + action_count;

  int32_t edge_count = 0;
  for (const Fact &fact : state.facts) {
    edge_count += fact.arity * 2;
  }
  for (const CandidateAction &action : candidates) {
    edge_count += action.arity * 2;
  }

  out_graph->object_node_count = object_count;
  out_graph->fact_node_count = fact_count;
  out_graph->action_node_count = action_count;
  out_graph->total_node_count = total_node_count;
  out_graph->edge_count = edge_count;
  out_graph->node_kind_count = total_node_count;
  out_graph->edge_src_count = edge_count;
  out_graph->edge_dst_count = edge_count;
  out_graph->edge_type_count = edge_count;

  out_graph->node_kind = static_cast<uint8_t *>(std::calloc(total_node_count, sizeof(uint8_t)));
  out_graph->edge_src = static_cast<int32_t *>(std::calloc(edge_count, sizeof(int32_t)));
  out_graph->edge_dst = static_cast<int32_t *>(std::calloc(edge_count, sizeof(int32_t)));
  out_graph->edge_type = static_cast<int32_t *>(std::calloc(edge_count, sizeof(int32_t)));

  if (allocation_failed(out_graph->node_kind_count, out_graph->node_kind) ||
      allocation_failed(out_graph->edge_src_count, out_graph->edge_src) ||
      allocation_failed(out_graph->edge_dst_count, out_graph->edge_dst) ||
      allocation_failed(out_graph->edge_type_count, out_graph->edge_type)) {
    tp_action_graph_dispose(out_graph);
    return TP_STATUS_LIMIT_EXCEEDED;
  }

  for (int32_t index = 0; index < object_count; ++index) {
    out_graph->node_kind[index] = 0;
  }
  for (int32_t index = 0; index < fact_count; ++index) {
    out_graph->node_kind[object_count + index] = 1;
  }
  for (int32_t index = 0; index < action_count; ++index) {
    out_graph->node_kind[object_count + fact_count + index] = 2;
  }

  int32_t edge_index = 0;
  for (int32_t fact_index = 0; fact_index < fact_count; ++fact_index) {
    const Fact &fact = state.facts[static_cast<std::size_t>(fact_index)];
    const int32_t fact_node = object_count + fact_index;
    for (uint8_t arg_index = 0; arg_index < fact.arity; ++arg_index) {
      const int32_t object_node = fact.args[arg_index];
      out_graph->edge_src[edge_index] = fact_node;
      out_graph->edge_dst[edge_index] = object_node;
      out_graph->edge_type[edge_index] = 100 + arg_index;
      ++edge_index;

      out_graph->edge_src[edge_index] = object_node;
      out_graph->edge_dst[edge_index] = fact_node;
      out_graph->edge_type[edge_index] = 200 + arg_index;
      ++edge_index;
    }
  }

  for (int32_t action_index = 0; action_index < action_count; ++action_index) {
    const CandidateAction &action = candidates[static_cast<std::size_t>(action_index)];
    const int32_t action_node = object_count + fact_count + action_index;
    for (uint8_t arg_index = 0; arg_index < action.arity; ++arg_index) {
      const int32_t object_node = action.args[arg_index];
      out_graph->edge_src[edge_index] = action_node;
      out_graph->edge_dst[edge_index] = object_node;
      out_graph->edge_type[edge_index] = 300 + arg_index;
      ++edge_index;

      out_graph->edge_src[edge_index] = object_node;
      out_graph->edge_dst[edge_index] = action_node;
      out_graph->edge_type[edge_index] = 400 + arg_index;
      ++edge_index;
    }
  }

  return TP_STATUS_OK;
}

void zero_action_graph(TP_Action_Graph *graph) {
  if (graph != nullptr) {
    std::memset(graph, 0, sizeof(TP_Action_Graph));
  }
}
