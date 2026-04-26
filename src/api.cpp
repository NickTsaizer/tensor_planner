#include "planner_internal.hpp"

#include <queue>

namespace {

bool problem_has_fact(
  const TP_Problem_Tensors *problem,
  int16_t predicate_id,
  const int16_t *args
) {
  for (int32_t fact_index = 0; fact_index < problem->fact_count; ++fact_index) {
    if (!problem->true_pred_mask[fact_index] || problem->true_pred_id[fact_index] != predicate_id) {
      continue;
    }

    bool match = true;
    for (int32_t arg_index = 0; arg_index < TP_MAX_ARITY; ++arg_index) {
      if (problem->true_pred_arg[fact_index * TP_MAX_ARITY + arg_index] != args[arg_index]) {
        match = false;
        break;
      }
    }

    if (match) {
      return true;
    }
  }

  return false;
}

std::vector<int32_t> infer_location_objects(const TP_Problem_Tensors *problem) {
  std::vector<int32_t> locations;
  std::unordered_set<int32_t> seen;
  for (int32_t goal_index = 0; goal_index < problem->goal_count; ++goal_index) {
    if (!problem->goal_pred_mask[goal_index]) {
      continue;
    }

    const int32_t location = problem->goal_pred_arg[goal_index * TP_MAX_ARITY + 1];
    if (seen.insert(location).second) {
      locations.push_back(location);
    }
  }
  return locations;
}

std::unordered_map<int32_t, std::vector<int32_t>> build_location_graph(const TP_Problem_Tensors *problem) {
  const std::vector<int32_t> locations = infer_location_objects(problem);
  std::unordered_set<int32_t> location_set(locations.begin(), locations.end());
  std::unordered_map<int32_t, std::vector<int32_t>> graph;

  for (int32_t fact_index = 0; fact_index < problem->fact_count; ++fact_index) {
    if (!problem->true_pred_mask[fact_index]) {
      continue;
    }

    const int16_t arg0 = problem->true_pred_arg[fact_index * TP_MAX_ARITY + 0];
    const int16_t arg1 = problem->true_pred_arg[fact_index * TP_MAX_ARITY + 1];
    if (location_set.find(arg0) == location_set.end() || location_set.find(arg1) == location_set.end()) {
      continue;
    }

    graph[arg0].push_back(arg1);
  }

  return graph;
}

int32_t shortest_distance(
  const std::unordered_map<int32_t, std::vector<int32_t>> &graph,
  int32_t start,
  int32_t goal
) {
  if (start == goal) {
    return 0;
  }

  std::queue<std::pair<int32_t, int32_t>> frontier;
  std::unordered_set<int32_t> visited;
  frontier.push({start, 0});
  visited.insert(start);

  while (!frontier.empty()) {
    const auto [node, distance] = frontier.front();
    frontier.pop();
    const auto found = graph.find(node);
    if (found == graph.end()) {
      continue;
    }

    for (const int32_t next : found->second) {
      if (!visited.insert(next).second) {
        continue;
      }
      if (next == goal) {
        return distance + 1;
      }
      frontier.push({next, distance + 1});
    }
  }

  return 999999;
}

int16_t find_entity_location(
  const TP_Problem_Tensors *problem,
  int16_t predicate_id,
  int16_t entity_id,
  int16_t fallback_location
) {
  for (int32_t fact_index = 0; fact_index < problem->fact_count; ++fact_index) {
    if (!problem->true_pred_mask[fact_index] || problem->true_pred_id[fact_index] != predicate_id) {
      continue;
    }
    if (problem->true_pred_arg[fact_index * TP_MAX_ARITY + 0] == entity_id) {
      return problem->true_pred_arg[fact_index * TP_MAX_ARITY + 1];
    }
  }
  return fallback_location;
}

int32_t estimate_state_distance(
  const TP_Problem_Tensors *problem,
  const std::unordered_map<int32_t, std::vector<int32_t>> &location_graph
) {
  int32_t estimate = 0;
  for (int32_t goal_index = 0; goal_index < problem->goal_count; ++goal_index) {
    if (!problem->goal_pred_mask[goal_index]) {
      continue;
    }

    const int16_t predicate_id = problem->goal_pred_id[goal_index];
    const int16_t goal_entity = problem->goal_pred_arg[goal_index * TP_MAX_ARITY + 0];
    const int16_t goal_location = problem->goal_pred_arg[goal_index * TP_MAX_ARITY + 1];
    const int16_t *goal_args = &problem->goal_pred_arg[goal_index * TP_MAX_ARITY];
    if (problem_has_fact(problem, predicate_id, goal_args)) {
      continue;
    }

    int16_t current_location = -1;
    if (predicate_id == 5) {
      current_location = find_entity_location(problem, 5, goal_entity, -1);
    } else if (predicate_id == 6) {
      current_location = find_entity_location(problem, 6, goal_entity, -1);
    } else if (predicate_id == 7) {
      current_location = find_entity_location(problem, 7, goal_entity, -1);
    }

    if (current_location >= 0) {
      estimate += shortest_distance(location_graph, current_location, goal_location);
    } else {
      estimate += 4;
    }
  }
  return estimate;
}

int32_t estimate_candidate_distance(
  const TP_Schema_Tensors *schema,
  const TP_Problem_Tensors *problem,
  const std::unordered_map<int32_t, std::vector<int32_t>> &location_graph,
  int32_t candidate_index
) {
  int16_t simulated_locations[3] = {-1, -1, -1};
  const int32_t schema_id = problem->cand_action_schema[candidate_index];
  const int16_t *candidate_args = &problem->cand_action_arg[candidate_index * TP_MAX_PARAMS];

  auto apply_effect_location = [&](int16_t predicate_id, int16_t entity_id, int16_t location_id) {
    int slot = -1;
    if (entity_id == 0) slot = 0;
    if (entity_id == 1) slot = 1;
    if (entity_id == 2) slot = 2;
    if (slot >= 0 && (predicate_id == 5 || predicate_id == 6)) {
      simulated_locations[slot] = location_id;
    }
  };

  for (int32_t eff_index = 0; eff_index < schema->max_effects; ++eff_index) {
    const int32_t row = schema_id * schema->max_effects + eff_index;
    if (schema->eff_op[row] != TP_EFFECT_ADD) {
      continue;
    }
    int16_t effect_args[TP_MAX_ARITY] = {-1, -1, -1, -1};
    for (int32_t arg_index = 0; arg_index < TP_MAX_ARITY; ++arg_index) {
      const int8_t slot = schema->eff_slot[row * TP_MAX_ARITY + arg_index];
      if (slot >= 0) {
        effect_args[arg_index] = candidate_args[slot];
      }
    }
    apply_effect_location(schema->eff_pred_id[row], effect_args[0], effect_args[1]);
  }

  int32_t estimate = 0;
  for (int32_t goal_index = 0; goal_index < problem->goal_count; ++goal_index) {
    if (!problem->goal_pred_mask[goal_index]) {
      continue;
    }
    const int16_t predicate_id = problem->goal_pred_id[goal_index];
    const int16_t entity_id = problem->goal_pred_arg[goal_index * TP_MAX_ARITY + 0];
    const int16_t goal_location = problem->goal_pred_arg[goal_index * TP_MAX_ARITY + 1];

    int16_t current_location = -1;
    if (entity_id >= 0 && entity_id <= 2 && simulated_locations[entity_id] >= 0) {
      current_location = simulated_locations[entity_id];
    } else if (predicate_id == 5) {
      current_location = find_entity_location(problem, 5, entity_id, -1);
    } else if (predicate_id == 6) {
      current_location = find_entity_location(problem, 6, entity_id, -1);
    }

    if (current_location >= 0) {
      estimate += shortest_distance(location_graph, current_location, goal_location);
    } else {
      estimate += 4;
    }
  }

  return estimate;
}

bool entity_on_boat(
  const TP_Problem_Tensors *problem,
  int16_t predicate_id,
  int16_t entity_id,
  int16_t boat_id
) {
  for (int32_t fact_index = 0; fact_index < problem->fact_count; ++fact_index) {
    if (!problem->true_pred_mask[fact_index] || problem->true_pred_id[fact_index] != predicate_id) {
      continue;
    }
    if (problem->true_pred_arg[fact_index * TP_MAX_ARITY + 0] == entity_id &&
        problem->true_pred_arg[fact_index * TP_MAX_ARITY + 1] == boat_id) {
      return true;
    }
  }
  return false;
}

struct TensorFactKey {
  int16_t predicate_id = -1;
  uint8_t arity = 0;
  std::array<int16_t, TP_MAX_ARITY> args {-1, -1, -1, -1};
};

struct UsefulFact {
  TensorFactKey fact;
  int32_t distance = 0;
};

bool tensor_fact_equals(const TensorFactKey &lhs, const TensorFactKey &rhs) {
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

bool problem_has_tensor_fact(const TP_Problem_Tensors *problem, const TensorFactKey &fact) {
  for (int32_t fact_index = 0; fact_index < problem->fact_count; ++fact_index) {
    if (!problem->true_pred_mask[fact_index] || problem->true_pred_id[fact_index] != fact.predicate_id) {
      continue;
    }

    bool match = true;
    for (uint8_t arg_index = 0; arg_index < fact.arity; ++arg_index) {
      if (problem->true_pred_arg[fact_index * TP_MAX_ARITY + arg_index] != fact.args[arg_index]) {
        match = false;
        break;
      }
    }
    if (match) {
      return true;
    }
  }
  return false;
}

int32_t useful_fact_distance(const std::vector<UsefulFact> &facts, const TensorFactKey &fact) {
  for (const UsefulFact &useful : facts) {
    if (tensor_fact_equals(useful.fact, fact)) {
      return useful.distance;
    }
  }
  return -1;
}

bool useful_predicate_exists(const std::vector<UsefulFact> &facts, int16_t predicate_id) {
  return std::any_of(facts.begin(), facts.end(), [predicate_id](const UsefulFact &fact) {
    return fact.fact.predicate_id == predicate_id;
  });
}

bool add_useful_fact(std::vector<UsefulFact> *facts, const TensorFactKey &fact, int32_t distance) {
  for (UsefulFact &useful : *facts) {
    if (!tensor_fact_equals(useful.fact, fact)) {
      continue;
    }
    if (distance < useful.distance) {
      useful.distance = distance;
      return true;
    }
    return false;
  }

  facts->push_back({fact, distance});
  return true;
}

TensorFactKey goal_fact_at(const TP_Schema_Tensors *schema, const TP_Problem_Tensors *problem, int32_t goal_index) {
  TensorFactKey fact {};
  fact.predicate_id = problem->goal_pred_id[goal_index];
  fact.arity = schema->pred_arity[fact.predicate_id];
  for (uint8_t arg_index = 0; arg_index < fact.arity; ++arg_index) {
    fact.args[arg_index] = problem->goal_pred_arg[goal_index * TP_MAX_ARITY + arg_index];
  }
  return fact;
}

bool unify_effect_with_fact(
  const TP_Schema_Tensors *schema,
  int32_t effect_row,
  const TensorFactKey &fact,
  std::array<int16_t, TP_MAX_PARAMS> *bindings
) {
  if (schema->eff_op[effect_row] != TP_EFFECT_ADD || schema->eff_pred_id[effect_row] != fact.predicate_id) {
    return false;
  }

  for (uint8_t arg_index = 0; arg_index < fact.arity; ++arg_index) {
    const int8_t slot = schema->eff_slot[effect_row * TP_MAX_ARITY + arg_index];
    if (slot < 0 || slot >= TP_MAX_PARAMS) {
      return false;
    }
    int16_t &bound_value = (*bindings)[static_cast<std::size_t>(slot)];
    if (bound_value >= 0 && bound_value != fact.args[arg_index]) {
      return false;
    }
    bound_value = fact.args[arg_index];
  }
  return true;
}

bool precondition_matches_problem_fact(
  const TP_Schema_Tensors *schema,
  const TP_Problem_Tensors *problem,
  int32_t precondition_row,
  int32_t fact_index,
  std::array<int16_t, TP_MAX_PARAMS> *bindings
) {
  if (!problem->true_pred_mask[fact_index] || problem->true_pred_id[fact_index] != schema->pre_pred_id[precondition_row]) {
    return false;
  }

  std::array<int16_t, TP_MAX_PARAMS> next_bindings = *bindings;
  const int16_t predicate_id = schema->pre_pred_id[precondition_row];
  const uint8_t arity = schema->pred_arity[predicate_id];
  for (uint8_t arg_index = 0; arg_index < arity; ++arg_index) {
    const int8_t slot = schema->pre_slot[precondition_row * TP_MAX_ARITY + arg_index];
    if (slot < 0 || slot >= TP_MAX_PARAMS) {
      return false;
    }

    const int16_t fact_arg = problem->true_pred_arg[fact_index * TP_MAX_ARITY + arg_index];
    int16_t &bound_value = next_bindings[static_cast<std::size_t>(slot)];
    if (bound_value >= 0 && bound_value != fact_arg) {
      return false;
    }
    bound_value = fact_arg;
  }

  *bindings = next_bindings;
  return true;
}

bool precondition_has_bound_slot(
  const TP_Schema_Tensors *schema,
  int32_t precondition_row,
  const std::array<int16_t, TP_MAX_PARAMS> &bindings
) {
  const int16_t predicate_id = schema->pre_pred_id[precondition_row];
  if (predicate_id < 0) {
    return false;
  }

  const uint8_t arity = schema->pred_arity[predicate_id];
  for (uint8_t arg_index = 0; arg_index < arity; ++arg_index) {
    const int8_t slot = schema->pre_slot[precondition_row * TP_MAX_ARITY + arg_index];
    if (slot >= 0 && slot < TP_MAX_PARAMS && bindings[static_cast<std::size_t>(slot)] >= 0) {
      return true;
    }
  }
  return false;
}

bool bind_preconditions_from_current_facts(
  const TP_Schema_Tensors *schema,
  const TP_Problem_Tensors *problem,
  int32_t action_id,
  std::array<int16_t, TP_MAX_PARAMS> *bindings
) {
  bool changed = false;
  bool matched_any = false;

  for (int32_t pass = 0; pass < TP_MAX_PARAMS; ++pass) {
    changed = false;
    for (int32_t phase = 0; phase < 2; ++phase) {
      for (int32_t pre_index = 0; pre_index < schema->max_preconditions; ++pre_index) {
        const int32_t pre_row = action_id * schema->max_preconditions + pre_index;
        if (schema->pre_sign[pre_row] <= 0) {
          continue;
        }

        const bool has_bound_slot = precondition_has_bound_slot(schema, pre_row, *bindings);
        if ((phase == 0 && !has_bound_slot) || (phase == 1 && has_bound_slot)) {
          continue;
        }

        for (int32_t fact_index = 0; fact_index < problem->fact_count; ++fact_index) {
          const std::array<int16_t, TP_MAX_PARAMS> before = *bindings;
          if (!precondition_matches_problem_fact(schema, problem, pre_row, fact_index, bindings)) {
            continue;
          }
          matched_any = true;
          if (*bindings != before) {
            changed = true;
          }
          break;
        }
      }
    }
    if (!changed) {
      break;
    }
  }

  return matched_any;
}

bool make_bound_precondition_fact(
  const TP_Schema_Tensors *schema,
  int32_t precondition_row,
  const std::array<int16_t, TP_MAX_PARAMS> &bindings,
  TensorFactKey *out_fact
) {
  if (schema->pre_sign[precondition_row] <= 0) {
    return false;
  }

  out_fact->predicate_id = schema->pre_pred_id[precondition_row];
  out_fact->arity = schema->pred_arity[out_fact->predicate_id];
  for (uint8_t arg_index = 0; arg_index < out_fact->arity; ++arg_index) {
    const int8_t slot = schema->pre_slot[precondition_row * TP_MAX_ARITY + arg_index];
    if (slot < 0 || slot >= TP_MAX_PARAMS || bindings[static_cast<std::size_t>(slot)] < 0) {
      return false;
    }
    out_fact->args[arg_index] = bindings[static_cast<std::size_t>(slot)];
  }
  return true;
}

TensorFactKey instantiate_candidate_effect(
  const TP_Schema_Tensors *schema,
  const TP_Problem_Tensors *problem,
  int32_t effect_row,
  int32_t candidate_index
) {
  TensorFactKey fact {};
  fact.predicate_id = schema->eff_pred_id[effect_row];
  fact.arity = schema->pred_arity[fact.predicate_id];
  const int16_t *candidate_args = &problem->cand_action_arg[candidate_index * TP_MAX_PARAMS];
  for (uint8_t arg_index = 0; arg_index < fact.arity; ++arg_index) {
    const int8_t slot = schema->eff_slot[effect_row * TP_MAX_ARITY + arg_index];
    fact.args[arg_index] = slot >= 0 && slot < TP_MAX_PARAMS ? candidate_args[slot] : -1;
  }
  return fact;
}

std::vector<UsefulFact> build_goal_regression_facts(const TP_Schema_Tensors *schema, const TP_Problem_Tensors *problem) {
  std::vector<UsefulFact> useful_facts;
  useful_facts.reserve(static_cast<std::size_t>(problem->goal_count) * 8U);

  for (int32_t goal_index = 0; goal_index < problem->goal_count; ++goal_index) {
    if (!problem->goal_pred_mask[goal_index]) {
      continue;
    }
    const TensorFactKey goal = goal_fact_at(schema, problem, goal_index);
    if (!problem_has_tensor_fact(problem, goal)) {
      add_useful_fact(&useful_facts, goal, 0);
    }
  }

  for (int32_t distance = 0; distance < 12; ++distance) {
    const std::size_t fact_count = useful_facts.size();
    bool changed = false;
    for (std::size_t useful_index = 0; useful_index < fact_count; ++useful_index) {
      if (useful_facts[useful_index].distance != distance) {
        continue;
      }

      for (int32_t action_id = 0; action_id < schema->action_count; ++action_id) {
        for (int32_t effect_index = 0; effect_index < schema->max_effects; ++effect_index) {
          const int32_t effect_row = action_id * schema->max_effects + effect_index;
          std::array<int16_t, TP_MAX_PARAMS> bindings {-1, -1, -1, -1, -1, -1};
          if (!unify_effect_with_fact(schema, effect_row, useful_facts[useful_index].fact, &bindings)) {
            continue;
          }

          bind_preconditions_from_current_facts(schema, problem, action_id, &bindings);
          for (int32_t pre_index = 0; pre_index < schema->max_preconditions; ++pre_index) {
            const int32_t pre_row = action_id * schema->max_preconditions + pre_index;
            TensorFactKey precondition_fact {};
            if (!make_bound_precondition_fact(schema, pre_row, bindings, &precondition_fact)) {
              continue;
            }
            if (!problem_has_tensor_fact(problem, precondition_fact)) {
              changed = add_useful_fact(&useful_facts, precondition_fact, distance + 1) || changed;
            }
          }
        }
      }
    }
    if (!changed) {
      break;
    }
  }

  return useful_facts;
}

void goal_regression_scorer(
  const TP_Schema_Tensors *schema,
  const TP_Problem_Tensors *problem,
  void *,
  float *out_action_scores,
  float *out_state_value,
  bool *out_has_state_value
) {
  *out_has_state_value = false;
  if (schema == nullptr || problem == nullptr) {
    return;
  }

  const std::vector<UsefulFact> useful_facts = build_goal_regression_facts(schema, problem);
  for (int32_t candidate_index = 0; candidate_index < problem->candidate_count; ++candidate_index) {
    const int32_t schema_id = problem->cand_action_schema[candidate_index];
    float score = 0.0f;
    for (int32_t effect_index = 0; effect_index < schema->max_effects; ++effect_index) {
      const int32_t effect_row = schema_id * schema->max_effects + effect_index;
      if (schema->eff_op[effect_row] != TP_EFFECT_ADD && schema->eff_op[effect_row] != TP_EFFECT_DELETE) {
        continue;
      }

      const TensorFactKey effect_fact = instantiate_candidate_effect(schema, problem, effect_row, candidate_index);
      const int32_t distance = useful_fact_distance(useful_facts, effect_fact);
      if (distance >= 0) {
        const float value = 20000.0f / static_cast<float>((distance + 1) * (distance + 1));
        score += schema->eff_op[effect_row] == TP_EFFECT_ADD ? value : -value * 2.0f;
      } else if (schema->eff_op[effect_row] == TP_EFFECT_ADD && useful_predicate_exists(useful_facts, effect_fact.predicate_id)) {
        score -= 250.0f;
      } else if (schema->eff_op[effect_row] == TP_EFFECT_ADD) {
        score -= 10.0f;
      }
    }
    out_action_scores[candidate_index] = score;
  }

  *out_state_value = -static_cast<float>(useful_facts.size());
  *out_has_state_value = true;
}

void tensor_baseline_scorer(
  const TP_Schema_Tensors *schema,
  const TP_Problem_Tensors *problem,
  void *,
  float *out_action_scores,
  float *out_state_value,
  bool *out_has_state_value
) {
  *out_has_state_value = true;
  if (schema == nullptr || problem == nullptr) {
    *out_has_state_value = false;
    return;
  }

  const auto location_graph = build_location_graph(problem);
  *out_state_value = static_cast<float>(estimate_state_distance(problem, location_graph));

  for (int32_t candidate_index = 0; candidate_index < problem->candidate_count; ++candidate_index) {
    const int32_t schema_id = problem->cand_action_schema[candidate_index];
    const int16_t *candidate_args = &problem->cand_action_arg[candidate_index * TP_MAX_PARAMS];
    float score = 0.0f;

    for (int32_t eff_index = 0; eff_index < schema->max_effects; ++eff_index) {
      const int32_t row = schema_id * schema->max_effects + eff_index;
      const int16_t predicate_id = schema->eff_pred_id[row];
      const int8_t op = schema->eff_op[row];
      int16_t effect_args[TP_MAX_ARITY] = {-1, -1, -1, -1};
      for (int32_t arg_index = 0; arg_index < TP_MAX_ARITY; ++arg_index) {
        const int8_t slot = schema->eff_slot[row * TP_MAX_ARITY + arg_index];
        if (slot >= 0) {
          effect_args[arg_index] = candidate_args[slot];
        }
      }

      for (int32_t goal_index = 0; goal_index < problem->goal_count; ++goal_index) {
        if (!problem->goal_pred_mask[goal_index] || problem->goal_pred_id[goal_index] != predicate_id) {
          continue;
        }

        const int16_t *goal_args = &problem->goal_pred_arg[goal_index * TP_MAX_ARITY];
        const bool goal_satisfied = problem_has_fact(problem, predicate_id, goal_args);
        bool exact_match = true;
        for (int32_t arg_index = 0; arg_index < TP_MAX_ARITY; ++arg_index) {
          if (effect_args[arg_index] != goal_args[arg_index]) {
            exact_match = false;
            break;
          }
        }

        if (op == TP_EFFECT_ADD && !goal_satisfied) {
          if (exact_match) {
            score += 20.0f;
          } else if (effect_args[0] == goal_args[0] && effect_args[1] >= 0 && goal_args[1] >= 0) {
            score += 8.0f - static_cast<float>(shortest_distance(location_graph, effect_args[1], goal_args[1]));
          }
        }

        if (op == TP_EFFECT_DELETE && goal_satisfied && exact_match) {
          score -= 25.0f;
        }
      }
    }

    const int16_t farmer_id = candidate_args[0];
    const int16_t boat_id = candidate_args[1];
    const int16_t target_location = candidate_args[2];
    const int16_t boat_location = find_entity_location(problem, 7, boat_id, -1);
    const int16_t farmer_location = find_entity_location(problem, 5, farmer_id, boat_location);

    if (schema_id == 7) {
      for (int32_t goal_index = 0; goal_index < problem->goal_count; ++goal_index) {
        if (!problem->goal_pred_mask[goal_index]) {
          continue;
        }
        const int16_t goal_entity = problem->goal_pred_arg[goal_index * TP_MAX_ARITY + 0];
        const int16_t goal_location = problem->goal_pred_arg[goal_index * TP_MAX_ARITY + 1];
        const int16_t entity_location = find_entity_location(problem, 6, goal_entity, boat_location);
        const bool on_boat = entity_on_boat(problem, 9, goal_entity, boat_id);
        if (on_boat) {
          const int32_t before = shortest_distance(location_graph, boat_location, goal_location);
          const int32_t after = shortest_distance(location_graph, target_location, goal_location);
          score += static_cast<float>(before - after) * 6.0f;
        }
        if (goal_entity == farmer_id) {
          const int32_t before = shortest_distance(location_graph, farmer_location, goal_location);
          const int32_t after = shortest_distance(location_graph, target_location, goal_location);
          score += static_cast<float>(before - after) * 2.0f;
        }
      }
    }

    if (schema_id == 8) {
      const int16_t passenger_id = candidate_args[1];
      for (int32_t goal_index = 0; goal_index < problem->goal_count; ++goal_index) {
        if (!problem->goal_pred_mask[goal_index]) {
          continue;
        }
        const int16_t goal_entity = problem->goal_pred_arg[goal_index * TP_MAX_ARITY + 0];
        const int16_t goal_location = problem->goal_pred_arg[goal_index * TP_MAX_ARITY + 1];
        if (goal_entity == passenger_id) {
          const int32_t before = shortest_distance(location_graph, boat_location, goal_location);
          const int32_t after = shortest_distance(location_graph, candidate_args[3], goal_location);
          score += static_cast<float>(before - after) * 10.0f;
        }
      }
    }

    if (schema_id == 9) {
      for (int32_t goal_index = 0; goal_index < problem->goal_count; ++goal_index) {
        if (!problem->goal_pred_mask[goal_index]) {
          continue;
        }
        const int16_t goal_entity = problem->goal_pred_arg[goal_index * TP_MAX_ARITY + 0];
        const int16_t goal_location = problem->goal_pred_arg[goal_index * TP_MAX_ARITY + 1];
        if (goal_entity == farmer_id) {
          const int32_t before = shortest_distance(location_graph, boat_location, goal_location);
          const int32_t after = shortest_distance(location_graph, target_location, goal_location);
          score += static_cast<float>(before - after) * 4.0f;
        }
      }
    }

    const int32_t candidate_distance = estimate_candidate_distance(schema, problem, location_graph, candidate_index);
    score += static_cast<float>(estimate_state_distance(problem, location_graph) - candidate_distance) * 3.0f;

    out_action_scores[candidate_index] = score;
  }
}

}  // namespace

TP_Domain *tp_domain_create(const TP_Limits *limits) {
  if (limits == nullptr || !is_valid_limits(*limits)) {
    return nullptr;
  }

  auto *domain = new TP_Domain();
  domain->limits = *limits;
  return domain;
}

void tp_domain_destroy(TP_Domain *domain) {
  delete domain;
}

TP_Status tp_domain_add_predicate(
  TP_Domain *domain,
  const TP_Predicate_Def *predicate_def,
  int32_t *predicate_id_out
) {
  if (domain == nullptr || predicate_def == nullptr || predicate_id_out == nullptr) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  if (!is_valid_predicate_def(*predicate_def)) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  PredicateDef predicate {};
  predicate.arity = predicate_def->arity;
  for (uint8_t index = 0; index < predicate.arity; ++index) {
    predicate.arg_types[index] = predicate_def->arg_types[index];
  }

  domain->predicates.push_back(predicate);
  *predicate_id_out = static_cast<int32_t>(domain->predicates.size()) - 1;
  return TP_STATUS_OK;
}

TP_Status tp_domain_add_function(
  TP_Domain *domain,
  const TP_Function_Def *function_def,
  int32_t *function_id_out
) {
  if (domain == nullptr || function_def == nullptr || function_id_out == nullptr) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  if (!is_valid_function_def(*function_def)) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  FunctionDef function {};
  function.arity = function_def->arity;
  for (uint8_t index = 0; index < function.arity; ++index) {
    function.arg_types[index] = function_def->arg_types[index];
  }

  domain->functions.push_back(function);
  *function_id_out = static_cast<int32_t>(domain->functions.size()) - 1;
  return TP_STATUS_OK;
}

TP_Status tp_domain_add_action_schema(
  TP_Domain *domain,
  uint8_t arity,
  const int32_t *arg_types,
  const TP_Action_Literal *preconditions,
  int32_t precondition_count,
  const TP_Action_Effect *effects,
  int32_t effect_count,
  const TP_Numeric_Precondition *numeric_preconditions,
  int32_t numeric_precondition_count,
  const TP_Numeric_Effect *numeric_effects,
  int32_t numeric_effect_count,
  int32_t *action_id_out
) {
  if (domain == nullptr || action_id_out == nullptr) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  if (!validate_action_schema(
        *domain,
        arity,
        arg_types,
        preconditions,
        precondition_count,
        effects,
        effect_count,
        numeric_preconditions,
        numeric_precondition_count,
        numeric_effects,
        numeric_effect_count
      )) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  ActionSchema action {};
  action.arity = arity;
  for (uint8_t index = 0; index < arity; ++index) {
    action.arg_types[index] = arg_types[index];
  }

  for (int32_t index = 0; index < precondition_count; ++index) {
    ActionLiteral literal {};
    literal.predicate_id = preconditions[index].predicate_id;
    literal.sign = preconditions[index].sign;
    literal.arity = preconditions[index].arity;
    for (uint8_t slot_index = 0; slot_index < literal.arity; ++slot_index) {
      literal.slots[slot_index] = preconditions[index].slots[slot_index];
    }
    action.preconditions.push_back(literal);
  }

  for (int32_t index = 0; index < effect_count; ++index) {
    ActionEffect effect {};
    effect.predicate_id = effects[index].predicate_id;
    effect.op = effects[index].op;
    effect.arity = effects[index].arity;
    for (uint8_t slot_index = 0; slot_index < effect.arity; ++slot_index) {
      effect.slots[slot_index] = effects[index].slots[slot_index];
    }
    action.effects.push_back(effect);
  }

  for (int32_t index = 0; index < numeric_precondition_count; ++index) {
    NumericPrecondition precondition {};
    precondition.function_id = numeric_preconditions[index].function_id;
    precondition.cmp_op = numeric_preconditions[index].cmp_op;
    precondition.arity = numeric_preconditions[index].arity;
    precondition.rhs_value = numeric_preconditions[index].rhs_value;
    for (uint8_t slot_index = 0; slot_index < precondition.arity; ++slot_index) {
      precondition.slots[slot_index] = numeric_preconditions[index].slots[slot_index];
    }
    action.numeric_preconditions.push_back(precondition);
  }

  for (int32_t index = 0; index < numeric_effect_count; ++index) {
    NumericEffect effect {};
    effect.function_id = numeric_effects[index].function_id;
    effect.op = numeric_effects[index].op;
    effect.arity = numeric_effects[index].arity;
    effect.rhs_value = numeric_effects[index].rhs_value;
    for (uint8_t slot_index = 0; slot_index < effect.arity; ++slot_index) {
      effect.slots[slot_index] = numeric_effects[index].slots[slot_index];
    }
    action.numeric_effects.push_back(effect);
  }

  domain->actions.push_back(std::move(action));
  *action_id_out = static_cast<int32_t>(domain->actions.size()) - 1;
  return TP_STATUS_OK;
}

TP_Status tp_domain_export_schema_tensors(
  const TP_Domain *domain,
  TP_Schema_Tensors *out_tensors
) {
  if (domain == nullptr) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  return export_schema_tensors_impl(*domain, out_tensors);
}

void tp_schema_tensors_dispose(TP_Schema_Tensors *tensors) {
  if (tensors == nullptr) {
    return;
  }

  std::free(tensors->pred_arity);
  std::free(tensors->pred_arg_type);
  std::free(tensors->func_arity);
  std::free(tensors->func_arg_type);
  std::free(tensors->action_arity);
  std::free(tensors->action_arg_type);
  std::free(tensors->pre_pred_id);
  std::free(tensors->pre_sign);
  std::free(tensors->pre_slot);
  std::free(tensors->eff_pred_id);
  std::free(tensors->eff_op);
  std::free(tensors->eff_slot);
  std::free(tensors->num_pre_func_id);
  std::free(tensors->num_pre_cmp);
  std::free(tensors->num_pre_slot);
  std::free(tensors->num_pre_value);
  std::free(tensors->num_eff_func_id);
  std::free(tensors->num_eff_op);
  std::free(tensors->num_eff_slot);
  std::free(tensors->num_eff_value);
  zero_schema_tensors(tensors);
}

TP_State *tp_state_create(
  const TP_Domain *domain,
  int32_t object_count,
  const int32_t *object_types
) {
  if (domain == nullptr || object_types == nullptr || object_count <= 0) {
    return nullptr;
  }

  if (object_count > domain->limits.max_objects) {
    return nullptr;
  }

  auto *state = new TP_State();
  state->domain = domain;
  state->object_types.assign(object_types, object_types + object_count);
  invalidate_state_indices(state);
  return state;
}

void tp_state_destroy(TP_State *state) {
  delete state;
}

TP_Status tp_state_add_fact(
  TP_State *state,
  int32_t predicate_id,
  uint8_t arity,
  const int32_t *args
) {
  if (state == nullptr || !validate_fact(*state->domain, *state, predicate_id, arity, args)) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  if (static_cast<int32_t>(state->facts.size()) >= state->domain->limits.max_facts) {
    return TP_STATUS_LIMIT_EXCEEDED;
  }

  add_fact_unique(&state->facts, make_fact(predicate_id, arity, args));
  invalidate_state_indices(state);
  return TP_STATUS_OK;
}

TP_Status tp_state_add_goal_fact(
  TP_State *state,
  int32_t predicate_id,
  uint8_t arity,
  const int32_t *args
) {
  if (state == nullptr || !validate_fact(*state->domain, *state, predicate_id, arity, args)) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  if (static_cast<int32_t>(state->goals.size()) >= state->domain->limits.max_goals) {
    return TP_STATUS_LIMIT_EXCEEDED;
  }

  add_fact_unique(&state->goals, make_fact(predicate_id, arity, args));
  invalidate_state_indices(state);
  return TP_STATUS_OK;
}

TP_Status tp_state_set_function_value(
  TP_State *state,
  int32_t function_id,
  uint8_t arity,
  const int32_t *args,
  float value
) {
  if (state == nullptr || !validate_function_value(*state->domain, *state, function_id, arity, args)) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  FunctionValue function_value {};
  function_value.function_id = function_id;
  function_value.arity = arity;
  function_value.value = value;
  for (uint8_t index = 0; index < arity; ++index) {
    function_value.args[index] = args[index];
  }

  const bool inserted = upsert_function_value(&state->function_values, function_value);
  if (inserted && static_cast<int32_t>(state->function_values.size()) > state->domain->limits.max_facts) {
    state->function_values.pop_back();
    return TP_STATUS_LIMIT_EXCEEDED;
  }

  invalidate_state_indices(state);
  return TP_STATUS_OK;
}

TP_Status tp_state_generate_candidates(
  const TP_Domain *domain,
  const TP_State *state,
  int32_t max_candidates,
  TP_Candidate_Action_List *out_candidates
) {
  if (domain == nullptr || state == nullptr || out_candidates == nullptr || max_candidates <= 0) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  out_candidates->count = 0;
  out_candidates->actions = nullptr;

  const std::vector<CandidateAction> generated = generate_candidate_actions(*domain, *state, max_candidates);
  out_candidates->count = static_cast<int32_t>(generated.size());
  out_candidates->actions = static_cast<TP_Candidate_Action *>(
    std::calloc(static_cast<std::size_t>(out_candidates->count), sizeof(TP_Candidate_Action))
  );
  if (out_candidates->count > 0 && out_candidates->actions == nullptr) {
    out_candidates->count = 0;
    return TP_STATUS_LIMIT_EXCEEDED;
  }

  for (int32_t index = 0; index < out_candidates->count; ++index) {
    out_candidates->actions[index].schema_id = generated[static_cast<std::size_t>(index)].schema_id;
    out_candidates->actions[index].arity = generated[static_cast<std::size_t>(index)].arity;
    for (int32_t arg_index = 0; arg_index < TP_MAX_PARAMS; ++arg_index) {
      out_candidates->actions[index].args[arg_index] =
        generated[static_cast<std::size_t>(index)].args[static_cast<std::size_t>(arg_index)];
    }
  }

  return TP_STATUS_OK;
}

void tp_candidate_action_list_dispose(TP_Candidate_Action_List *list) {
  if (list == nullptr) {
    return;
  }

  std::free(list->actions);
  list->count = 0;
  list->actions = nullptr;
}

TP_Status tp_state_export_problem_tensors(
  const TP_Domain *domain,
  const TP_State *state,
  int32_t max_candidates,
  TP_Problem_Tensors *out_tensors
) {
  if (domain == nullptr || state == nullptr) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  return export_problem_tensors_impl(*domain, *state, max_candidates, out_tensors);
}

TP_Status tp_state_export_action_graph(
  const TP_Domain *domain,
  const TP_State *state,
  int32_t max_candidates,
  TP_Action_Graph *out_graph
) {
  if (domain == nullptr || state == nullptr || out_graph == nullptr || max_candidates <= 0) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  const std::vector<CandidateAction> candidates = generate_candidate_actions(*domain, *state, max_candidates);
  return export_action_graph_for_candidates_impl(*domain, *state, candidates, out_graph);
}

void tp_problem_tensors_dispose(TP_Problem_Tensors *tensors) {
  if (tensors == nullptr) {
    return;
  }

  std::free(tensors->obj_type);
  std::free(tensors->true_pred_id);
  std::free(tensors->true_pred_arg);
  std::free(tensors->true_pred_mask);
  std::free(tensors->num_func_id);
  std::free(tensors->num_func_arg);
  std::free(tensors->num_value);
  std::free(tensors->goal_pred_id);
  std::free(tensors->goal_pred_arg);
  std::free(tensors->goal_pred_mask);
  std::free(tensors->cand_action_schema);
  std::free(tensors->cand_action_arg);
  std::free(tensors->cand_action_mask);
  zero_problem_tensors(tensors);
}

void tp_action_graph_dispose(TP_Action_Graph *graph) {
  if (graph == nullptr) {
    return;
  }

  std::free(graph->node_kind);
  std::free(graph->edge_src);
  std::free(graph->edge_dst);
  std::free(graph->edge_type);
  zero_action_graph(graph);
}

TP_Solver *tp_solver_create(const TP_Domain *domain) {
  if (domain == nullptr) {
    return nullptr;
  }

  auto *solver = new TP_Solver();
  solver->domain = domain;
  if (tp_domain_export_schema_tensors(domain, &solver->schema_tensors) == TP_STATUS_OK) {
    solver->has_schema_tensors = true;
  }
  solver->scorer = goal_regression_scorer;
  solver->scorer_user_data = nullptr;
  return solver;
}

void tp_solver_destroy(TP_Solver *solver) {
  if (solver != nullptr && solver->has_schema_tensors) {
    tp_schema_tensors_dispose(&solver->schema_tensors);
  }
  delete solver;
}

TP_Status tp_solver_set_custom_guidance(
  TP_Solver *solver,
  TP_Score_Candidates_Fn scorer,
  void *user_data
) {
  if (solver == nullptr) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  solver->scorer = scorer;
  solver->scorer_user_data = user_data;
  return TP_STATUS_OK;
}

TP_Status tp_solver_use_default_guidance(TP_Solver *solver) {
  if (solver == nullptr) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  solver->scorer = goal_regression_scorer;
  solver->scorer_user_data = nullptr;
  return TP_STATUS_OK;
}

void tp_solve_result_dispose(TP_Solve_Result *result) {
  if (result == nullptr) {
    return;
  }

  std::free(result->plan_actions);
  zero_solve_result(result);
}
