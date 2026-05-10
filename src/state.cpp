#include "planner_internal.hpp"

#include <cstdlib>

struct StateIndices {
  std::vector<std::vector<const Fact *>> facts_by_predicate;
  std::unordered_map<std::vector<int32_t>, float, SignatureHash> function_values;
};

namespace {

bool signature_optimization_enabled() {
  static const bool enabled = [] {
    const char *value = std::getenv("TP_DISABLE_SIGNATURE_OPTIMIZATION");
    return value == nullptr || value[0] == '\0' || value[0] == '0';
  }();
  return enabled;
}

bool allocation_failed(int32_t count, const void *pointer) {
  return count > 0 && pointer == nullptr;
}

bool is_valid_action_slot(int8_t slot, uint8_t action_arity) {
  return slot >= 0 && slot < TP_MAX_PARAMS && slot < static_cast<int8_t>(action_arity);
}

template <typename SlotOwner>
bool slots_are_valid(const SlotOwner &owner, uint8_t action_arity) {
  for (uint8_t index = 0; index < owner.arity; ++index) {
    if (!is_valid_action_slot(owner.slots[index], action_arity)) {
      return false;
    }
  }
  return true;
}

bool candidate_matches_schema(const CandidateAction &candidate, const ActionSchema &schema) {
  return candidate.schema_id >= 0 && candidate.arity == schema.arity && action_schema_uses_valid_slots(schema);
}

struct ScoredCandidateAction {
  CandidateAction action;
  int32_t score = 0;
};

constexpr int32_t kSchemaPatternFallbackMin = 8;
constexpr int32_t kSchemaPatternFallbackBudget = 32;

struct FactPattern {
  int32_t predicate_id = -1;
  uint8_t arity = 0;
  std::array<int32_t, TP_MAX_ARITY> args {};
};

struct FunctionPattern {
  int32_t function_id = -1;
  uint8_t arity = 0;
  std::array<int32_t, TP_MAX_ARITY> args {};
};

enum class ReducedTaskSubgoalKind : uint8_t {
  PositiveFact,
  NegativeFact,
  NumericCondition,
};

struct ReducedTaskSubgoal {
  ReducedTaskSubgoalKind kind = ReducedTaskSubgoalKind::PositiveFact;
  FactPattern fact {};
  FunctionPattern function {};
  uint8_t cmp_op = TP_NUM_CMP_EQ;
  float rhs_value = 0.0f;
};

struct SchemaPreconditionGroups {
  std::vector<int32_t> static_positive;
  std::vector<int32_t> dynamic_positive;
  std::vector<int32_t> dynamic_negative;
  std::vector<int32_t> dynamic_numeric;
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

std::vector<int32_t> make_fact_key(const Fact &fact) {
  std::vector<int32_t> key;
  key.reserve(static_cast<std::size_t>(fact.arity) + 2);
  key.push_back(fact.predicate_id);
  key.push_back(fact.arity);
  for (uint8_t index = 0; index < fact.arity; ++index) {
    key.push_back(fact.args[index]);
  }
  return key;
}

std::vector<int32_t> make_fact_pattern_key(const FactPattern &fact) {
  std::vector<int32_t> key;
  key.reserve(static_cast<std::size_t>(fact.arity) + 2);
  key.push_back(fact.predicate_id);
  key.push_back(fact.arity);
  for (uint8_t index = 0; index < fact.arity; ++index) {
    key.push_back(fact.args[index]);
  }
  return key;
}

std::vector<int32_t> make_function_pattern_key(const FunctionPattern &function) {
  std::vector<int32_t> key;
  key.reserve(static_cast<std::size_t>(function.arity) + 2);
  key.push_back(function.function_id);
  key.push_back(function.arity);
  for (uint8_t index = 0; index < function.arity; ++index) {
    key.push_back(function.args[index]);
  }
  return key;
}

std::vector<int32_t> make_subgoal_key(const ReducedTaskSubgoal &subgoal) {
  std::vector<int32_t> key;
  key.push_back(static_cast<int32_t>(subgoal.kind));
  if (subgoal.kind == ReducedTaskSubgoalKind::NumericCondition) {
    std::vector<int32_t> function_key = make_function_pattern_key(subgoal.function);
    key.insert(key.end(), function_key.begin(), function_key.end());
    key.push_back(static_cast<int32_t>(subgoal.cmp_op));
    int32_t encoded_value = 0;
    static_assert(sizeof(float) == sizeof(int32_t));
    std::memcpy(&encoded_value, &subgoal.rhs_value, sizeof(float));
    key.push_back(encoded_value);
    return key;
  }

  std::vector<int32_t> fact_key = make_fact_pattern_key(subgoal.fact);
  key.insert(key.end(), fact_key.begin(), fact_key.end());
  return key;
}

std::vector<int32_t> make_pattern_key(const CandidatePattern &pattern) {
  std::vector<int32_t> key;
  key.reserve(static_cast<std::size_t>(pattern.arity) + 2);
  key.push_back(pattern.schema_id);
  key.push_back(pattern.arity);
  for (uint8_t index = 0; index < pattern.arity; ++index) {
    key.push_back(pattern.args[index]);
  }
  return key;
}

std::vector<int32_t> make_pattern_signature(const CandidatePattern &pattern) {
  return make_pattern_key(pattern);
}

CandidatePattern make_unbound_pattern(int32_t schema_id, uint8_t arity) {
  CandidatePattern pattern {};
  pattern.schema_id = schema_id;
  pattern.arity = arity;
  pattern.args.fill(-1);
  return pattern;
}

SchemaPreconditionGroups partition_schema_preconditions(
  const ActionSchema &schema,
  const std::vector<bool> &mutable_predicates,
  const std::vector<bool> &mutable_functions
) {
  SchemaPreconditionGroups groups;
  groups.static_positive.reserve(schema.preconditions.size());
  groups.dynamic_positive.reserve(schema.preconditions.size());
  groups.dynamic_negative.reserve(schema.preconditions.size());
  groups.dynamic_numeric.reserve(schema.numeric_preconditions.size());
  for (int32_t precondition_index = 0; precondition_index < static_cast<int32_t>(schema.preconditions.size()); ++precondition_index) {
    const ActionLiteral &literal = schema.preconditions[static_cast<std::size_t>(precondition_index)];
    const bool is_dynamic = mutable_predicates[static_cast<std::size_t>(literal.predicate_id)];
    if (literal.sign > 0) {
      std::vector<int32_t> *target = is_dynamic ? &groups.dynamic_positive : &groups.static_positive;
      target->push_back(precondition_index);
      continue;
    }

    if (literal.sign < 0 && is_dynamic) {
      groups.dynamic_negative.push_back(precondition_index);
    }
  }

  for (int32_t precondition_index = 0;
       precondition_index < static_cast<int32_t>(schema.numeric_preconditions.size());
       ++precondition_index) {
    const NumericPrecondition &precondition =
      schema.numeric_preconditions[static_cast<std::size_t>(precondition_index)];
    if (precondition.function_id >= 0 &&
        precondition.function_id < static_cast<int32_t>(mutable_functions.size()) &&
        mutable_functions[static_cast<std::size_t>(precondition.function_id)]) {
      groups.dynamic_numeric.push_back(precondition_index);
    }
  }

  return groups;
}

bool patterns_compatible(const CandidatePattern &lhs, const CandidatePattern &rhs) {
  if (lhs.schema_id != rhs.schema_id || lhs.arity != rhs.arity) {
    return false;
  }

  for (uint8_t index = 0; index < lhs.arity; ++index) {
    if (lhs.args[index] >= 0 && rhs.args[index] >= 0 && lhs.args[index] != rhs.args[index]) {
      return false;
    }
  }

  return true;
}

CandidatePattern merge_patterns(const CandidatePattern &lhs, const CandidatePattern &rhs) {
  CandidatePattern merged = lhs;
  for (uint8_t index = 0; index < lhs.arity; ++index) {
    if (merged.args[index] < 0) {
      merged.args[index] = rhs.args[index];
    }
  }
  return merged;
}

std::vector<CandidatePattern> intersect_candidate_patterns(
  const std::vector<CandidatePattern> &relevant_patterns,
  const std::vector<CandidatePattern> &reachable_patterns
) {
  if (relevant_patterns.empty()) {
    return {};
  }
  if (reachable_patterns.empty()) {
    return relevant_patterns;
  }

  std::vector<CandidatePattern> merged_patterns;
  std::unordered_set<std::vector<int32_t>, SignatureHash> seen;
  for (const CandidatePattern &relevant : relevant_patterns) {
    bool matched = false;
    for (const CandidatePattern &reachable : reachable_patterns) {
      if (!patterns_compatible(relevant, reachable)) {
        continue;
      }
      matched = true;
      CandidatePattern merged = merge_patterns(relevant, reachable);
      if (seen.insert(make_pattern_signature(merged)).second) {
        merged_patterns.push_back(merged);
      }
    }

    if (!matched && seen.insert(make_pattern_signature(relevant)).second) {
      merged_patterns.push_back(relevant);
    }
  }

  return merged_patterns;
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
  if (!candidate_matches_schema(candidate, schema)) {
    return 0;
  }
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

Fact instantiate_fact(const ActionLiteral &literal, const CandidatePattern &pattern) {
  Fact fact {};
  fact.predicate_id = literal.predicate_id;
  fact.arity = literal.arity;
  for (uint8_t index = 0; index < literal.arity; ++index) {
    fact.args[index] = pattern.args[static_cast<std::size_t>(literal.slots[index])];
  }
  return fact;
}

FactPattern instantiate_fact_pattern(const ActionLiteral &literal, const CandidatePattern &pattern) {
  FactPattern fact {};
  fact.predicate_id = literal.predicate_id;
  fact.arity = literal.arity;
  fact.args.fill(-1);
  for (uint8_t index = 0; index < literal.arity; ++index) {
    const int8_t slot = literal.slots[index];
    if (pattern.args[static_cast<std::size_t>(slot)] >= 0) {
      fact.args[index] = pattern.args[static_cast<std::size_t>(slot)];
    }
  }
  return fact;
}

FunctionPattern instantiate_function_pattern(const NumericPrecondition &precondition, const CandidatePattern &pattern) {
  FunctionPattern function {};
  function.function_id = precondition.function_id;
  function.arity = precondition.arity;
  function.args.fill(-1);
  for (uint8_t index = 0; index < precondition.arity; ++index) {
    const int8_t slot = precondition.slots[index];
    if (pattern.args[static_cast<std::size_t>(slot)] >= 0) {
      function.args[index] = pattern.args[static_cast<std::size_t>(slot)];
    }
  }
  return function;
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
  if (!is_valid_action_slot(slot, candidate.arity)) {
    return false;
  }
  return candidate.args[static_cast<std::size_t>(slot)] >= 0;
}

bool candidate_slot_is_bound(const CandidatePattern &candidate, int8_t slot) {
  if (!is_valid_action_slot(slot, candidate.arity)) {
    return false;
  }
  return candidate.args[static_cast<std::size_t>(slot)] >= 0;
}

bool candidate_matches_pattern(const CandidateAction &action, const CandidatePattern &pattern) {
  if (action.schema_id != pattern.schema_id || action.arity != pattern.arity) {
    return false;
  }

  for (uint8_t index = 0; index < pattern.arity; ++index) {
    const int32_t expected = pattern.args[index];
    if (expected >= 0 && action.args[index] != expected) {
      return false;
    }
  }

  return true;
}

bool partial_candidate_matches_pattern(const CandidateAction &action, const CandidatePattern &pattern) {
  if (action.schema_id != pattern.schema_id || action.arity != pattern.arity) {
    return false;
  }

  for (uint8_t index = 0; index < pattern.arity; ++index) {
    const int32_t bound_arg = action.args[index];
    const int32_t expected = pattern.args[index];
    if (bound_arg >= 0 && expected >= 0 && bound_arg != expected) {
      return false;
    }
  }

  return true;
}

bool partial_candidate_matches_any_pattern(
  const CandidateAction &action,
  const std::vector<CandidatePattern> &patterns
) {
  if (patterns.empty()) {
    return true;
  }

  return std::any_of(patterns.begin(), patterns.end(), [&action](const CandidatePattern &pattern) {
    return partial_candidate_matches_pattern(action, pattern);
  });
}

bool candidate_actions_equal(const CandidateAction &lhs, const CandidateAction &rhs) {
  return lhs.schema_id == rhs.schema_id && lhs.arity == rhs.arity && lhs.args == rhs.args;
}

bool fact_signature_less(const Fact &lhs, const Fact &rhs) {
  if (lhs.predicate_id != rhs.predicate_id) {
    return lhs.predicate_id < rhs.predicate_id;
  }
  if (lhs.arity != rhs.arity) {
    return lhs.arity < rhs.arity;
  }
  return lhs.args < rhs.args;
}

bool function_value_signature_less(const FunctionValue &lhs, const FunctionValue &rhs) {
  if (lhs.function_id != rhs.function_id) {
    return lhs.function_id < rhs.function_id;
  }
  if (lhs.arity != rhs.arity) {
    return lhs.arity < rhs.arity;
  }
  return lhs.args < rhs.args;
}

TP_State copy_state_without_caches(const TP_State &state) {
  TP_State next_state {};
  next_state.domain = state.domain;
  next_state.object_types = state.object_types;
  next_state.facts = state.facts;
  next_state.function_values = state.function_values;
  next_state.goals = state.goals;
  return next_state;
}

void insert_sorted_fact(std::vector<Fact> *facts, const Fact &fact) {
  const auto position = std::lower_bound(facts->begin(), facts->end(), fact, fact_signature_less);
  facts->insert(position, fact);
}

void erase_sorted_fact(std::vector<Fact> *facts, const Fact &fact) {
  const auto position = std::lower_bound(facts->begin(), facts->end(), fact, fact_signature_less);
  if (position != facts->end() && fact_equals(*position, fact)) {
    facts->erase(position);
  }
}

void upsert_sorted_function_value(std::vector<FunctionValue> *values, const FunctionValue &value) {
  const auto position = std::lower_bound(
    values->begin(),
    values->end(),
    value,
    function_value_signature_less
  );
  if (position != values->end() &&
      position->function_id == value.function_id &&
      position->arity == value.arity &&
      position->args == value.args) {
    position->value = value.value;
    return;
  }
  values->insert(position, value);
}

const std::vector<Fact> &get_or_build_sorted_facts(const TP_State &state) {
  if (state.sorted_facts_dirty) {
    state.sorted_facts_cache = state.facts;
    std::sort(state.sorted_facts_cache.begin(), state.sorted_facts_cache.end(), fact_signature_less);
    state.sorted_facts_dirty = false;
  }
  return state.sorted_facts_cache;
}

const std::vector<FunctionValue> &get_or_build_sorted_function_values(const TP_State &state) {
  if (state.sorted_function_values_dirty) {
    state.sorted_function_values_cache = state.function_values;
    std::sort(
      state.sorted_function_values_cache.begin(),
      state.sorted_function_values_cache.end(),
      function_value_signature_less
    );
    state.sorted_function_values_dirty = false;
  }
  return state.sorted_function_values_cache;
}

void append_unique_actions(
  std::vector<CandidateAction> *destination,
  const std::vector<CandidateAction> &source,
  int32_t max_candidates
) {
  for (const CandidateAction &candidate : source) {
    if (static_cast<int32_t>(destination->size()) >= max_candidates) {
      return;
    }

    const bool exists = std::any_of(destination->begin(), destination->end(), [&candidate](const CandidateAction &existing) {
      return candidate_actions_equal(existing, candidate);
    });
    if (!exists) {
      destination->push_back(candidate);
    }
  }
}

bool fact_matches_pattern(const Fact &fact, const FactPattern &pattern) {
  if (fact.predicate_id != pattern.predicate_id || fact.arity != pattern.arity) {
    return false;
  }

  for (uint8_t index = 0; index < fact.arity; ++index) {
    if (pattern.args[index] >= 0 && fact.args[index] != pattern.args[index]) {
      return false;
    }
  }

  return true;
}

bool fact_pattern_matches_pattern(const FactPattern &fact, const FactPattern &pattern) {
  if (fact.predicate_id != pattern.predicate_id || fact.arity != pattern.arity) {
    return false;
  }

  for (uint8_t index = 0; index < fact.arity; ++index) {
    if (pattern.args[index] >= 0 && fact.args[index] != pattern.args[index]) {
      return false;
    }
  }

  return true;
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

bool bind_slots_from_fact(
  const ActionLiteral &literal,
  const Fact &fact,
  const CandidatePattern &candidate,
  CandidatePattern *out_candidate
) {
  CandidatePattern bound = candidate;
  for (uint8_t index = 0; index < literal.arity; ++index) {
    const int8_t slot = literal.slots[index];
    const int32_t fact_arg = fact.args[index];
    if (candidate_slot_is_bound(bound, slot) && bound.args[static_cast<std::size_t>(slot)] != fact_arg) {
      return false;
    }
    bound.args[static_cast<std::size_t>(slot)] = fact_arg;
  }
  *out_candidate = bound;
  return true;
}

bool bind_slots_from_fact_effect(
  const ActionEffect &effect,
  uint8_t expected_op,
  const FactPattern &fact,
  CandidatePattern *out_pattern
) {
  if (effect.predicate_id != fact.predicate_id || effect.arity != fact.arity || effect.op != expected_op) {
    return false;
  }

  for (uint8_t index = 0; index < effect.arity; ++index) {
    const int8_t slot = effect.slots[index];
    const int32_t fact_arg = fact.args[index];
    if (fact_arg < 0) {
      continue;
    }
    if (candidate_slot_is_bound(*out_pattern, slot) && out_pattern->args[static_cast<std::size_t>(slot)] != fact_arg) {
      return false;
    }
    out_pattern->args[static_cast<std::size_t>(slot)] = fact_arg;
  }

  return true;
}

bool bind_slots_from_numeric_effect(
  const NumericEffect &effect,
  const FunctionPattern &function,
  CandidatePattern *out_pattern
) {
  if (effect.function_id != function.function_id || effect.arity != function.arity) {
    return false;
  }

  for (uint8_t index = 0; index < effect.arity; ++index) {
    const int8_t slot = effect.slots[index];
    const int32_t function_arg = function.args[index];
    if (function_arg < 0) {
      continue;
    }
    if (candidate_slot_is_bound(*out_pattern, slot) && out_pattern->args[static_cast<std::size_t>(slot)] != function_arg) {
      return false;
    }
    out_pattern->args[static_cast<std::size_t>(slot)] = function_arg;
  }

  return true;
}

bool fact_is_fully_bound(const ActionLiteral &literal, const CandidatePattern &pattern) {
  for (uint8_t index = 0; index < literal.arity; ++index) {
    if (!candidate_slot_is_bound(pattern, literal.slots[index])) {
      return false;
    }
  }
  return true;
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

std::vector<bool> build_mutable_predicate_mask(const TP_Domain &domain) {
  std::vector<bool> mutable_predicates(domain.predicates.size(), false);
  for (const ActionSchema &schema : domain.actions) {
    for (const ActionEffect &effect : schema.effects) {
      if (effect.predicate_id >= 0 && effect.predicate_id < static_cast<int32_t>(mutable_predicates.size())) {
        mutable_predicates[static_cast<std::size_t>(effect.predicate_id)] = true;
      }
    }
  }
  return mutable_predicates;
}

std::vector<bool> build_mutable_function_mask(const TP_Domain &domain) {
  std::vector<bool> mutable_functions(domain.functions.size(), false);
  for (const ActionSchema &schema : domain.actions) {
    for (const NumericEffect &effect : schema.numeric_effects) {
      if (effect.function_id >= 0 && effect.function_id < static_cast<int32_t>(mutable_functions.size())) {
        mutable_functions[static_cast<std::size_t>(effect.function_id)] = true;
      }
    }
  }
  return mutable_functions;
}

void resolve_static_preconditions(
  const TP_State &state,
  const StateIndices &indices,
  const ActionSchema &schema,
  const std::vector<bool> &mutable_predicates,
  const std::vector<int32_t> &static_preconditions,
  int32_t precondition_index,
  const CandidatePattern &pattern,
  std::vector<CandidatePattern> *out_patterns
) {
  if (precondition_index >= static_cast<int32_t>(static_preconditions.size())) {
    out_patterns->push_back(pattern);
    return;
  }

  const ActionLiteral &literal = schema.preconditions[static_cast<std::size_t>(static_preconditions[static_cast<std::size_t>(precondition_index)])];
  const auto &bucket = indices.facts_by_predicate[static_cast<std::size_t>(literal.predicate_id)];
  if (fact_is_fully_bound(literal, pattern)) {
    const Fact fact = instantiate_fact(literal, pattern);
    if (state_has_fact_indexed(indices, fact)) {
      resolve_static_preconditions(
        state,
        indices,
        schema,
        mutable_predicates,
        static_preconditions,
        precondition_index + 1,
        pattern,
        out_patterns
      );
    }
    return;
  }

  for (const Fact *fact : bucket) {
    CandidatePattern bound = pattern;
    if (!bind_slots_from_fact(literal, *fact, pattern, &bound)) {
      continue;
    }

    resolve_static_preconditions(
      state,
      indices,
      schema,
      mutable_predicates,
      static_preconditions,
      precondition_index + 1,
      bound,
      out_patterns
    );
  }
}

void enqueue_subgoal(
  const ReducedTaskSubgoal &subgoal,
  std::vector<ReducedTaskSubgoal> *pending_subgoals,
  std::unordered_set<std::vector<int32_t>, SignatureHash> *seen_subgoals
) {
  if (seen_subgoals->insert(make_subgoal_key(subgoal)).second) {
    pending_subgoals->push_back(subgoal);
  }
}

void enqueue_support_subgoals(
  const TP_State &state,
  const StateIndices &indices,
  const ActionSchema &schema,
  const std::vector<bool> &mutable_predicates,
  const std::vector<bool> &mutable_functions,
  const CandidatePattern &pattern,
  std::vector<ReducedTaskSubgoal> *pending_subgoals,
  std::unordered_set<std::vector<int32_t>, SignatureHash> *seen_subgoals
) {
  for (const ActionLiteral &literal : schema.preconditions) {
    if (!mutable_predicates[static_cast<std::size_t>(literal.predicate_id)]) {
      continue;
    }

    const FactPattern fact = instantiate_fact_pattern(literal, pattern);
    if (literal.sign > 0) {
      bool satisfied = false;
      if (fact_is_fully_bound(literal, pattern)) {
        const Fact exact = instantiate_fact(literal, pattern);
        satisfied = state_has_fact_indexed(indices, exact);
      } else if (fact.predicate_id >= 0 && fact.predicate_id < static_cast<int32_t>(indices.facts_by_predicate.size())) {
        const auto &bucket = indices.facts_by_predicate[static_cast<std::size_t>(fact.predicate_id)];
        satisfied = std::any_of(bucket.begin(), bucket.end(), [&fact](const Fact *existing) {
          return fact_matches_pattern(*existing, fact);
        });
      }

      if (satisfied) {
        continue;
      }

      enqueue_subgoal(
        {.kind = ReducedTaskSubgoalKind::PositiveFact, .fact = fact},
        pending_subgoals,
        seen_subgoals
      );
      continue;
    }

    enqueue_subgoal(
      {.kind = ReducedTaskSubgoalKind::NegativeFact, .fact = fact},
      pending_subgoals,
      seen_subgoals
    );
  }

  for (const NumericPrecondition &precondition : schema.numeric_preconditions) {
    if (precondition.function_id < 0 ||
        precondition.function_id >= static_cast<int32_t>(mutable_functions.size()) ||
        !mutable_functions[static_cast<std::size_t>(precondition.function_id)]) {
      continue;
    }

    enqueue_subgoal(
      {
        .kind = ReducedTaskSubgoalKind::NumericCondition,
        .function = instantiate_function_pattern(precondition, pattern),
        .cmp_op = precondition.cmp_op,
        .rhs_value = precondition.rhs_value,
      },
      pending_subgoals,
      seen_subgoals
    );
  }
}

void add_pattern_with_supports(
  const TP_State &state,
  const StateIndices &indices,
  const ActionSchema &schema,
  const std::vector<bool> &mutable_predicates,
  const std::vector<bool> &mutable_functions,
  const CandidatePattern &pattern,
  std::unordered_set<std::vector<int32_t>, SignatureHash> *seen_patterns,
  std::vector<CandidatePattern> *patterns,
  std::vector<ReducedTaskSubgoal> *pending_subgoals,
  std::unordered_set<std::vector<int32_t>, SignatureHash> *seen_subgoals
) {
  if (seen_patterns->insert(make_pattern_key(pattern)).second) {
    patterns->push_back(pattern);
  }

  enqueue_support_subgoals(
    state,
    indices,
    schema,
    mutable_predicates,
    mutable_functions,
    pattern,
    pending_subgoals,
    seen_subgoals
  );
}

void process_positive_fact_subgoal(
  const TP_Domain &domain,
  const TP_State &state,
  const StateIndices &indices,
  const std::vector<bool> &mutable_predicates,
  const std::vector<bool> &mutable_functions,
  const FactPattern &goal,
  std::unordered_set<std::vector<int32_t>, SignatureHash> *seen_patterns,
  std::vector<CandidatePattern> *patterns,
  std::vector<ReducedTaskSubgoal> *pending_subgoals,
  std::unordered_set<std::vector<int32_t>, SignatureHash> *seen_subgoals
) {
  for (int32_t schema_id = 0; schema_id < static_cast<int32_t>(domain.actions.size()); ++schema_id) {
    const ActionSchema &schema = domain.actions[static_cast<std::size_t>(schema_id)];
    if (!action_schema_uses_valid_slots(schema)) {
      continue;
    }
    const SchemaPreconditionGroups precondition_groups =
      partition_schema_preconditions(schema, mutable_predicates, mutable_functions);

    for (const ActionEffect &effect : schema.effects) {
      CandidatePattern seed = make_unbound_pattern(schema_id, schema.arity);
      if (!bind_slots_from_fact_effect(effect, TP_EFFECT_ADD, goal, &seed)) {
        continue;
      }

      std::vector<CandidatePattern> resolved_patterns;
      resolve_static_preconditions(
        state,
        indices,
        schema,
        mutable_predicates,
        precondition_groups.static_positive,
        0,
        seed,
        &resolved_patterns
      );

      for (const CandidatePattern &pattern : resolved_patterns) {
        add_pattern_with_supports(
          state,
          indices,
          schema,
          mutable_predicates,
          mutable_functions,
          pattern,
          seen_patterns,
          patterns,
          pending_subgoals,
          seen_subgoals
        );
      }
    }
  }
}

void process_negative_fact_subgoal(
  const TP_Domain &domain,
  const TP_State &state,
  const StateIndices &indices,
  const std::vector<bool> &mutable_predicates,
  const std::vector<bool> &mutable_functions,
  const FactPattern &goal,
  std::unordered_set<std::vector<int32_t>, SignatureHash> *seen_patterns,
  std::vector<CandidatePattern> *patterns,
  std::vector<ReducedTaskSubgoal> *pending_subgoals,
  std::unordered_set<std::vector<int32_t>, SignatureHash> *seen_subgoals
) {
  for (int32_t schema_id = 0; schema_id < static_cast<int32_t>(domain.actions.size()); ++schema_id) {
    const ActionSchema &schema = domain.actions[static_cast<std::size_t>(schema_id)];
    if (!action_schema_uses_valid_slots(schema)) {
      continue;
    }
    const SchemaPreconditionGroups precondition_groups =
      partition_schema_preconditions(schema, mutable_predicates, mutable_functions);

    for (const ActionEffect &effect : schema.effects) {
      CandidatePattern seed = make_unbound_pattern(schema_id, schema.arity);
      if (!bind_slots_from_fact_effect(effect, TP_EFFECT_DELETE, goal, &seed)) {
        continue;
      }

      std::vector<CandidatePattern> resolved_patterns;
      resolve_static_preconditions(
        state,
        indices,
        schema,
        mutable_predicates,
        precondition_groups.static_positive,
        0,
        seed,
        &resolved_patterns
      );

      for (const CandidatePattern &pattern : resolved_patterns) {
        add_pattern_with_supports(
          state,
          indices,
          schema,
          mutable_predicates,
          mutable_functions,
          pattern,
          seen_patterns,
          patterns,
          pending_subgoals,
          seen_subgoals
        );
      }
    }
  }
}

void process_numeric_subgoal(
  const TP_Domain &domain,
  const TP_State &state,
  const StateIndices &indices,
  const std::vector<bool> &mutable_predicates,
  const std::vector<bool> &mutable_functions,
  const FunctionPattern &goal,
  std::unordered_set<std::vector<int32_t>, SignatureHash> *seen_patterns,
  std::vector<CandidatePattern> *patterns,
  std::vector<ReducedTaskSubgoal> *pending_subgoals,
  std::unordered_set<std::vector<int32_t>, SignatureHash> *seen_subgoals
) {
  for (int32_t schema_id = 0; schema_id < static_cast<int32_t>(domain.actions.size()); ++schema_id) {
    const ActionSchema &schema = domain.actions[static_cast<std::size_t>(schema_id)];
    if (!action_schema_uses_valid_slots(schema)) {
      continue;
    }
    const SchemaPreconditionGroups precondition_groups =
      partition_schema_preconditions(schema, mutable_predicates, mutable_functions);

    for (const NumericEffect &effect : schema.numeric_effects) {
      CandidatePattern seed = make_unbound_pattern(schema_id, schema.arity);
      if (!bind_slots_from_numeric_effect(effect, goal, &seed)) {
        continue;
      }

      std::vector<CandidatePattern> resolved_patterns;
      resolve_static_preconditions(
        state,
        indices,
        schema,
        mutable_predicates,
        precondition_groups.static_positive,
        0,
        seed,
        &resolved_patterns
      );

      for (const CandidatePattern &pattern : resolved_patterns) {
        add_pattern_with_supports(
          state,
          indices,
          schema,
          mutable_predicates,
          mutable_functions,
          pattern,
          seen_patterns,
          patterns,
          pending_subgoals,
          seen_subgoals
        );
      }
    }
  }
}

void resolve_dynamic_preconditions(
  const ActionSchema &schema,
  const std::vector<int32_t> &dynamic_preconditions,
  const std::vector<FactPattern> &reachable_facts,
  int32_t precondition_index,
  const CandidatePattern &pattern,
  std::vector<CandidatePattern> *out_patterns
) {
  if (precondition_index >= static_cast<int32_t>(dynamic_preconditions.size())) {
    out_patterns->push_back(pattern);
    return;
  }

  const ActionLiteral &literal = schema.preconditions[static_cast<std::size_t>(dynamic_preconditions[static_cast<std::size_t>(precondition_index)])];
  const FactPattern needed = instantiate_fact_pattern(literal, pattern);
  for (const FactPattern &reachable : reachable_facts) {
    if (!fact_pattern_matches_pattern(reachable, needed)) {
      continue;
    }

    CandidatePattern bound = pattern;
    bool conflict = false;
    for (uint8_t index = 0; index < literal.arity; ++index) {
      const int8_t slot = literal.slots[index];
      const int32_t fact_arg = reachable.args[index];
      if (fact_arg < 0) {
        continue;
      }
      if (candidate_slot_is_bound(bound, slot) && bound.args[static_cast<std::size_t>(slot)] != fact_arg) {
        conflict = true;
        break;
      }
      bound.args[static_cast<std::size_t>(slot)] = fact_arg;
    }

    if (!conflict) {
      resolve_dynamic_preconditions(
        schema,
        dynamic_preconditions,
        reachable_facts,
        precondition_index + 1,
        bound,
        out_patterns
      );
    }
  }
}

std::vector<CandidatePattern> filter_patterns_for_schema(
  int32_t schema_id,
  uint8_t arity,
  const std::vector<CandidatePattern> *relevant_patterns
) {
  if (relevant_patterns == nullptr || relevant_patterns->empty()) {
    return {};
  }

  std::vector<CandidatePattern> filtered;
  for (const CandidatePattern &pattern : *relevant_patterns) {
    if (pattern.schema_id == schema_id && pattern.arity == arity) {
      filtered.push_back(pattern);
    }
  }
  return filtered;
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
  if (!candidate_matches_schema(action, schema)) {
    return false;
  }
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
  const std::vector<CandidatePattern> &schema_patterns,
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
      schema_patterns,
      out_actions,
      max_candidates
    );
    candidate->args[static_cast<std::size_t>(slot_index)] = -1;
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
  const std::vector<CandidatePattern> &schema_patterns,
  std::vector<CandidateAction> *out_actions,
  int32_t max_candidates
) {
  if (static_cast<int32_t>(out_actions->size()) >= max_candidates) {
    return;
  }

  if (precondition_order_index == static_cast<int32_t>(precondition_order.size())) {
    CandidateAction completed = candidate;
    generate_assignments(
      domain,
      state,
      indices,
      schema_id,
      0,
      &completed,
      schema_patterns,
      out_actions,
      max_candidates
    );
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
      schema_patterns,
      out_actions,
      max_candidates
    );
  }
}

}  // namespace

bool action_schema_uses_valid_slots(const ActionSchema &schema) {
  if (schema.arity > TP_MAX_PARAMS) {
    return false;
  }

  return std::all_of(schema.preconditions.begin(), schema.preconditions.end(), [&schema](const ActionLiteral &literal) {
           return slots_are_valid(literal, schema.arity);
         }) &&
    std::all_of(schema.effects.begin(), schema.effects.end(), [&schema](const ActionEffect &effect) {
      return slots_are_valid(effect, schema.arity);
    }) &&
    std::all_of(schema.numeric_preconditions.begin(), schema.numeric_preconditions.end(), [&schema](const NumericPrecondition &precondition) {
      return slots_are_valid(precondition, schema.arity);
    }) &&
    std::all_of(schema.numeric_effects.begin(), schema.numeric_effects.end(), [&schema](const NumericEffect &effect) {
      return slots_are_valid(effect, schema.arity);
    });
}

int32_t score_candidate_relevance(
  const TP_Domain &domain,
  const TP_State &state,
  const CandidateAction &candidate
) {
  if (candidate.schema_id < 0 || candidate.schema_id >= static_cast<int32_t>(domain.actions.size())) {
    return 0;
  }
  return score_candidate_relevance_impl(domain, state, candidate);
}

std::vector<CandidatePattern> derive_relevant_action_patterns(
  const TP_Domain &domain,
  const TP_State &state
) {
  std::vector<CandidatePattern> patterns;
  if (state.goals.empty()) {
    return patterns;
  }

  const StateIndices &indices = get_or_build_state_indices(domain, state, nullptr);
  const std::vector<bool> mutable_predicates = build_mutable_predicate_mask(domain);
  const std::vector<bool> mutable_functions = build_mutable_function_mask(domain);
  std::vector<ReducedTaskSubgoal> pending_subgoals;
  std::unordered_set<std::vector<int32_t>, SignatureHash> seen_subgoals;
  std::unordered_set<std::vector<int32_t>, SignatureHash> seen_patterns;

  for (const Fact &goal : state.goals) {
    if (state_has_fact_indexed(indices, goal)) {
      continue;
    }
    FactPattern goal_pattern {};
    goal_pattern.predicate_id = goal.predicate_id;
    goal_pattern.arity = goal.arity;
    goal_pattern.args.fill(-1);
    for (uint8_t index = 0; index < goal.arity; ++index) {
      goal_pattern.args[index] = goal.args[index];
    }
    enqueue_subgoal(
      {.kind = ReducedTaskSubgoalKind::PositiveFact, .fact = goal_pattern},
      &pending_subgoals,
      &seen_subgoals
    );
  }

  for (std::size_t goal_index = 0; goal_index < pending_subgoals.size(); ++goal_index) {
    const ReducedTaskSubgoal &goal = pending_subgoals[goal_index];
    switch (goal.kind) {
      case ReducedTaskSubgoalKind::PositiveFact:
        process_positive_fact_subgoal(
          domain,
          state,
          indices,
          mutable_predicates,
          mutable_functions,
          goal.fact,
          &seen_patterns,
          &patterns,
          &pending_subgoals,
          &seen_subgoals
        );
        break;
      case ReducedTaskSubgoalKind::NegativeFact:
        process_negative_fact_subgoal(
          domain,
          state,
          indices,
          mutable_predicates,
          mutable_functions,
          goal.fact,
          &seen_patterns,
          &patterns,
          &pending_subgoals,
          &seen_subgoals
        );
        break;
      case ReducedTaskSubgoalKind::NumericCondition:
        process_numeric_subgoal(
          domain,
          state,
          indices,
          mutable_predicates,
          mutable_functions,
          goal.function,
          &seen_patterns,
          &patterns,
          &pending_subgoals,
          &seen_subgoals
        );
        break;
    }
  }

  return patterns;
}

std::vector<CandidatePattern> derive_reachable_action_patterns(
  const TP_Domain &domain,
  const TP_State &state
) {
  std::vector<CandidatePattern> patterns;
  const StateIndices &indices = get_or_build_state_indices(domain, state, nullptr);
  const std::vector<bool> mutable_predicates = build_mutable_predicate_mask(domain);
  const std::vector<bool> mutable_functions = build_mutable_function_mask(domain);
  std::vector<FactPattern> reachable_facts;
  std::unordered_set<std::vector<int32_t>, SignatureHash> seen_fact_patterns;
  std::unordered_set<std::vector<int32_t>, SignatureHash> seen_patterns;

  reachable_facts.reserve(state.facts.size());
  for (const Fact &fact : state.facts) {
    FactPattern pattern {};
    pattern.predicate_id = fact.predicate_id;
    pattern.arity = fact.arity;
    pattern.args.fill(-1);
    for (uint8_t index = 0; index < fact.arity; ++index) {
      pattern.args[index] = fact.args[index];
    }
    if (seen_fact_patterns.insert(make_fact_pattern_key(pattern)).second) {
      reachable_facts.push_back(pattern);
    }
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (int32_t schema_id = 0; schema_id < static_cast<int32_t>(domain.actions.size()); ++schema_id) {
      const ActionSchema &schema = domain.actions[static_cast<std::size_t>(schema_id)];
      if (!action_schema_uses_valid_slots(schema)) {
        continue;
      }
      const SchemaPreconditionGroups precondition_groups =
        partition_schema_preconditions(schema, mutable_predicates, mutable_functions);
      CandidatePattern seed = make_unbound_pattern(schema_id, schema.arity);

      std::vector<CandidatePattern> static_patterns;
      resolve_static_preconditions(
        state,
        indices,
        schema,
        mutable_predicates,
        precondition_groups.static_positive,
        0,
        seed,
        &static_patterns
      );

      for (const CandidatePattern &static_pattern : static_patterns) {
        std::vector<CandidatePattern> resolved_patterns;
        resolve_dynamic_preconditions(
          schema,
          precondition_groups.dynamic_positive,
          reachable_facts,
          0,
          static_pattern,
          &resolved_patterns
        );

        for (const CandidatePattern &pattern : resolved_patterns) {
          if (seen_patterns.insert(make_pattern_key(pattern)).second) {
            patterns.push_back(pattern);
            changed = true;
          }

          for (const ActionEffect &effect : schema.effects) {
            if (effect.op != TP_EFFECT_ADD) {
              continue;
            }
            FactPattern produced {};
            produced.predicate_id = effect.predicate_id;
            produced.arity = effect.arity;
            produced.args.fill(-1);
            for (uint8_t index = 0; index < effect.arity; ++index) {
              const int8_t slot = effect.slots[index];
              if (candidate_slot_is_bound(pattern, slot)) {
                produced.args[index] = pattern.args[static_cast<std::size_t>(slot)];
              }
            }

            if (seen_fact_patterns.insert(make_fact_pattern_key(produced)).second) {
              reachable_facts.push_back(produced);
              changed = true;
            }
          }
        }
      }
    }
  }

  return patterns;
}

ReducedTaskPatterns analyze_reduced_task_patterns(
  const TP_Domain &domain,
  const TP_State &state
) {
  ReducedTaskPatterns patterns;
  patterns.relevant = derive_relevant_action_patterns(domain, state);
  patterns.reachable = derive_reachable_action_patterns(domain, state);
  patterns.filtered = intersect_candidate_patterns(patterns.relevant, patterns.reachable);
  return patterns;
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
    state->sorted_facts_cache.clear();
    state->sorted_facts_dirty = true;
    state->sorted_function_values_cache.clear();
    state->sorted_function_values_dirty = true;
    state->signature_cache.clear();
    state->signature_dirty = true;
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

bool remove_fact(std::vector<Fact> *facts, const Fact &fact) {
  const auto removal_begin = std::remove_if(
    facts->begin(),
    facts->end(),
    [&fact](const Fact &existing) { return fact_equals(existing, fact); }
  );
  if (removal_begin == facts->end()) {
    return false;
  }

  facts->erase(removal_begin, facts->end());
  return true;
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
  if (action.schema_id < 0 || action.schema_id >= static_cast<int32_t>(domain.actions.size())) {
    return false;
  }
  const StateIndices &indices = get_or_build_state_indices(domain, state, nullptr);
  return action_is_applicable_with_indices(domain, state, indices, action);
}

TP_State apply_action(
  const TP_Domain &domain,
  const TP_State &state,
  const CandidateAction &action
) {
  TP_State next_state = copy_state_without_caches(state);
  if (action.schema_id < 0 || action.schema_id >= static_cast<int32_t>(domain.actions.size())) {
    return next_state;
  }
  const ActionSchema &schema = domain.actions[static_cast<std::size_t>(action.schema_id)];
  if (!candidate_matches_schema(action, schema)) {
    return next_state;
  }

  const bool can_reuse_sorted_facts = !state.sorted_facts_dirty;
  const bool can_reuse_sorted_function_values = !state.sorted_function_values_dirty;
  bool facts_changed = false;
  bool function_values_changed = false;
  bool copied_sorted_facts = false;
  bool copied_sorted_function_values = false;

  const auto ensure_sorted_facts_cache = [&]() {
    if (!can_reuse_sorted_facts || copied_sorted_facts) {
      return;
    }
    next_state.sorted_facts_cache = state.sorted_facts_cache;
    next_state.sorted_facts_dirty = false;
    copied_sorted_facts = true;
  };

  const auto ensure_sorted_function_values_cache = [&]() {
    if (!can_reuse_sorted_function_values || copied_sorted_function_values) {
      return;
    }
    next_state.sorted_function_values_cache = state.sorted_function_values_cache;
    next_state.sorted_function_values_dirty = false;
    copied_sorted_function_values = true;
  };

  for (const ActionEffect &effect : schema.effects) {
    const Fact fact = instantiate_fact(effect, action);
    if (effect.op == TP_EFFECT_ADD) {
      const bool added = add_fact_unique(&next_state.facts, fact);
      facts_changed = facts_changed || added;
      if (added && can_reuse_sorted_facts) {
        ensure_sorted_facts_cache();
        insert_sorted_fact(&next_state.sorted_facts_cache, fact);
      }
    } else if (effect.op == TP_EFFECT_DELETE) {
      const bool removed = remove_fact(&next_state.facts, fact);
      facts_changed = facts_changed || removed;
      if (removed && can_reuse_sorted_facts) {
        ensure_sorted_facts_cache();
        erase_sorted_fact(&next_state.sorted_facts_cache, fact);
      }
    }
  }

  for (const NumericEffect &effect : schema.numeric_effects) {
    FunctionValue value = instantiate_function_value(effect.function_id, effect.arity, effect.slots, action);
    float current_value = 0.0f;
    const bool has_existing_value = try_get_function_value(next_state, value, &current_value);
    if (has_existing_value) {
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

    const bool value_changed = !has_existing_value || current_value != value.value;
    upsert_function_value(&next_state.function_values, value);
    function_values_changed = function_values_changed || value_changed;
    if (value_changed && can_reuse_sorted_function_values) {
      ensure_sorted_function_values_cache();
      upsert_sorted_function_value(&next_state.sorted_function_values_cache, value);
    }
  }

  if (!facts_changed && !function_values_changed) {
    return next_state;
  }

  next_state.indices_cache.reset();
  next_state.indices_dirty = true;

  if (facts_changed && !copied_sorted_facts) {
    next_state.sorted_facts_cache.clear();
    next_state.sorted_facts_dirty = true;
  }

  if (function_values_changed && !copied_sorted_function_values) {
    next_state.sorted_function_values_cache.clear();
    next_state.sorted_function_values_dirty = true;
  }

  next_state.signature_cache.clear();
  next_state.signature_dirty = true;

  return next_state;
}

std::vector<CandidateAction> generate_candidate_actions(
  const TP_Domain &domain,
  const TP_State &state,
  int32_t max_candidates,
  const std::vector<CandidatePattern> *relevant_patterns
) {
  std::vector<ScoredCandidateAction> scored_actions;
  scored_actions.reserve(static_cast<std::size_t>(std::max(0, max_candidates)));
  const StateIndices &indices = get_or_build_state_indices(domain, state, nullptr);

  for (int32_t schema_id = 0; schema_id < static_cast<int32_t>(domain.actions.size()); ++schema_id) {
    std::vector<CandidateAction> schema_actions;
    schema_actions.reserve(static_cast<std::size_t>(std::max(0, max_candidates)));
    const ActionSchema &schema = domain.actions[static_cast<std::size_t>(schema_id)];
    if (!action_schema_uses_valid_slots(schema)) {
      continue;
    }
    const std::vector<CandidatePattern> schema_patterns = filter_patterns_for_schema(
      schema_id,
      schema.arity,
      relevant_patterns
    );
    if (relevant_patterns != nullptr && !relevant_patterns->empty() && schema_patterns.empty()) {
      continue;
    }

    CandidateAction candidate {};
    candidate.schema_id = schema_id;
    candidate.arity = schema.arity;
    candidate.args.fill(-1);
    const std::vector<int32_t> positive_precondition_order = get_positive_precondition_order(state, schema);

    auto generate_for_seed = [&](const CandidateAction &seed, std::vector<CandidateAction> *out_actions, int32_t action_budget) {
      if (positive_precondition_order.empty()) {
        CandidateAction mutable_seed = seed;
        generate_assignments(
          domain,
          state,
          indices,
          schema_id,
          0,
          &mutable_seed,
          schema_patterns,
          out_actions,
          action_budget
        );
      } else {
        generate_candidates_from_positive_preconditions(
          domain,
          state,
          indices,
          schema_id,
          positive_precondition_order,
          0,
          seed,
          schema_patterns,
          out_actions,
          action_budget
        );
      }
    };

    if (schema_patterns.empty()) {
      generate_for_seed(candidate, &schema_actions, max_candidates);
    } else {
      for (const CandidatePattern &pattern : schema_patterns) {
        CandidateAction seeded = candidate;
        seeded.args = pattern.args;
        generate_for_seed(seeded, &schema_actions, max_candidates);
        if (static_cast<int32_t>(schema_actions.size()) >= max_candidates) {
          break;
        }
      }

      if (static_cast<int32_t>(schema_actions.size()) < kSchemaPatternFallbackMin) {
        std::vector<CandidateAction> fallback_actions;
        fallback_actions.reserve(static_cast<std::size_t>(kSchemaPatternFallbackBudget));
        generate_for_seed(candidate, &fallback_actions, std::min(max_candidates, kSchemaPatternFallbackBudget));
        append_unique_actions(&schema_actions, fallback_actions, max_candidates);
      }
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

const std::vector<int32_t> &make_state_signature(const TP_State &state, bool *rebuilt) {
  if (rebuilt != nullptr) {
    *rebuilt = false;
  }
  if (!signature_optimization_enabled()) {
    std::vector<Fact> sorted_facts = state.facts;
    std::sort(sorted_facts.begin(), sorted_facts.end(), fact_signature_less);

    std::vector<FunctionValue> sorted_values = state.function_values;
    std::sort(sorted_values.begin(), sorted_values.end(), function_value_signature_less);

    state.signature_cache.clear();
    state.signature_cache.reserve(
      sorted_facts.size() * (TP_MAX_ARITY + 2) +
      sorted_values.size() * (TP_MAX_ARITY + 3)
    );
    for (const Fact &fact : sorted_facts) {
      state.signature_cache.push_back(fact.predicate_id);
      state.signature_cache.push_back(static_cast<int32_t>(fact.arity));
      for (uint8_t index = 0; index < fact.arity; ++index) {
        state.signature_cache.push_back(fact.args[index]);
      }
    }

    for (const FunctionValue &value : sorted_values) {
      state.signature_cache.push_back(-1000 - value.function_id);
      state.signature_cache.push_back(static_cast<int32_t>(value.arity));
      for (uint8_t index = 0; index < value.arity; ++index) {
        state.signature_cache.push_back(value.args[index]);
      }

      int32_t encoded_value = 0;
      static_assert(sizeof(float) == sizeof(int32_t));
      std::memcpy(&encoded_value, &value.value, sizeof(float));
      state.signature_cache.push_back(encoded_value);
    }

    state.signature_dirty = false;
    if (rebuilt != nullptr) {
      *rebuilt = true;
    }
    return state.signature_cache;
  }
  if (!state.signature_dirty) {
    return state.signature_cache;
  }

  const std::vector<Fact> &sorted_facts = get_or_build_sorted_facts(state);
  const std::vector<FunctionValue> &sorted_values = get_or_build_sorted_function_values(state);

  state.signature_cache.clear();
  state.signature_cache.reserve(
    sorted_facts.size() * (TP_MAX_ARITY + 2) +
    sorted_values.size() * (TP_MAX_ARITY + 3)
  );
  for (const Fact &fact : sorted_facts) {
    state.signature_cache.push_back(fact.predicate_id);
    state.signature_cache.push_back(static_cast<int32_t>(fact.arity));
    for (uint8_t index = 0; index < fact.arity; ++index) {
      state.signature_cache.push_back(fact.args[index]);
    }
  }

  for (const FunctionValue &value : sorted_values) {
    state.signature_cache.push_back(-1000 - value.function_id);
    state.signature_cache.push_back(static_cast<int32_t>(value.arity));
    for (uint8_t index = 0; index < value.arity; ++index) {
      state.signature_cache.push_back(value.args[index]);
    }

    int32_t encoded_value = 0;
    static_assert(sizeof(float) == sizeof(int32_t));
    std::memcpy(&encoded_value, &value.value, sizeof(float));
    state.signature_cache.push_back(encoded_value);
  }

  state.signature_dirty = false;
  if (rebuilt != nullptr) {
    *rebuilt = true;
  }
  return state.signature_cache;
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
