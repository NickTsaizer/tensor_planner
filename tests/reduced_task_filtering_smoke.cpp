#include "../include/tensor_planner.h"
#include "../src/planner_internal.hpp"

#include <cassert>
#include <vector>

namespace {

TP_Domain *create_domain() {
  const TP_Limits limits {
    .max_objects = 8,
    .max_facts = 16,
    .max_goals = 4,
    .max_candidates = 32,
    .max_expansions = 64,
    .max_plan_length = 8,
  };

  TP_Domain *domain = tp_domain_create(&limits);
  assert(domain != nullptr);
  return domain;
}

void solve_and_expect_plan_length(TP_Domain *domain, TP_State *state, int32_t expected_plan_length) {
  TP_Solver *solver = tp_solver_create(domain);
  assert(solver != nullptr);

  TP_Solve_Result result {};
  const TP_Status status = tp_solver_solve(solver, state, &result);
  assert(status == TP_STATUS_OK);
  assert(result.solved);
  assert(result.plan_length == expected_plan_length);

  tp_solve_result_dispose(&result);
  tp_solver_destroy(solver);
}

bool has_pattern(const std::vector<CandidatePattern> &patterns, int32_t schema_id, int32_t arg0) {
  for (const CandidatePattern &pattern : patterns) {
    if (pattern.schema_id == schema_id && pattern.arity == 1 && pattern.args[0] == arg0) {
      return true;
    }
  }
  return false;
}

bool has_candidate(const std::vector<CandidateAction> &candidates, int32_t schema_id, int32_t arg0) {
  for (const CandidateAction &candidate : candidates) {
    if (candidate.schema_id == schema_id && candidate.arity == 1 && candidate.args[0] == arg0) {
      return true;
    }
  }
  return false;
}

void negative_precondition_support_chain_stays_reachable() {
  // Arrange
  TP_Domain *domain = create_domain();

  const TP_Predicate_Def ready_def {.arity = 1, .arg_types = {0, 0, 0, 0}};
  const TP_Predicate_Def blocked_def {.arity = 1, .arg_types = {0, 0, 0, 0}};
  const TP_Predicate_Def done_def {.arity = 1, .arg_types = {0, 0, 0, 0}};
  int32_t ready_id = -1;
  int32_t blocked_id = -1;
  int32_t done_id = -1;
  assert(tp_domain_add_predicate(domain, &ready_def, &ready_id) == TP_STATUS_OK);
  assert(tp_domain_add_predicate(domain, &blocked_def, &blocked_id) == TP_STATUS_OK);
  assert(tp_domain_add_predicate(domain, &done_def, &done_id) == TP_STATUS_OK);

  const int32_t action_types[1] = {0};
  int32_t enable_ready_schema_id = -1;
  int32_t clear_blocked_schema_id = -1;
  int32_t finish_schema_id = -1;
  {
    const TP_Action_Effect effects[1] = {
      {.predicate_id = ready_id, .op = TP_EFFECT_ADD, .arity = 1, .slots = {0, 0, 0, 0}},
    };
    assert(tp_domain_add_action_schema(domain, 1, action_types, nullptr, 0, effects, 1, nullptr, 0, nullptr, 0, &enable_ready_schema_id) == TP_STATUS_OK);
  }
  {
    const TP_Action_Literal preconditions[1] = {
      {.predicate_id = blocked_id, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
    };
    const TP_Action_Effect effects[1] = {
      {.predicate_id = blocked_id, .op = TP_EFFECT_DELETE, .arity = 1, .slots = {0, 0, 0, 0}},
    };
    assert(tp_domain_add_action_schema(domain, 1, action_types, preconditions, 1, effects, 1, nullptr, 0, nullptr, 0, &clear_blocked_schema_id) == TP_STATUS_OK);
  }
  {
    const TP_Action_Literal preconditions[2] = {
      {.predicate_id = ready_id, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = blocked_id, .sign = -1, .arity = 1, .slots = {0, 0, 0, 0}},
    };
    const TP_Action_Effect effects[1] = {
      {.predicate_id = done_id, .op = TP_EFFECT_ADD, .arity = 1, .slots = {0, 0, 0, 0}},
    };
    assert(tp_domain_add_action_schema(domain, 1, action_types, preconditions, 2, effects, 1, nullptr, 0, nullptr, 0, &finish_schema_id) == TP_STATUS_OK);
  }

  const int32_t object_types[1] = {0};
  TP_State *state = tp_state_create(domain, 1, object_types);
  assert(state != nullptr);

  const int32_t obj_args[1] = {0};
  assert(tp_state_add_fact(state, blocked_id, 1, obj_args) == TP_STATUS_OK);
  assert(tp_state_add_goal_fact(state, done_id, 1, obj_args) == TP_STATUS_OK);

  // Act
  const ReducedTaskPatterns patterns = analyze_reduced_task_patterns(*domain, *state);
  const std::vector<CandidateAction> filtered_candidates =
    generate_candidate_actions(*domain, *state, domain->limits.max_candidates, &patterns.filtered);

  // Assert
  assert(has_pattern(patterns.filtered, clear_blocked_schema_id, 0));
  assert(has_pattern(patterns.filtered, finish_schema_id, 0));
  assert(!has_pattern(patterns.filtered, enable_ready_schema_id, 0));
  assert(has_candidate(filtered_candidates, clear_blocked_schema_id, 0));
  assert(!has_candidate(filtered_candidates, finish_schema_id, 0));
  solve_and_expect_plan_length(domain, state, 2);

  tp_state_destroy(state);
  tp_domain_destroy(domain);
}

void negative_precondition_support_chain_skips_support_when_not_needed() {
  // Arrange
  TP_Domain *domain = create_domain();

  const TP_Predicate_Def ready_def {.arity = 1, .arg_types = {0, 0, 0, 0}};
  const TP_Predicate_Def blocked_def {.arity = 1, .arg_types = {0, 0, 0, 0}};
  const TP_Predicate_Def done_def {.arity = 1, .arg_types = {0, 0, 0, 0}};
  int32_t ready_id = -1;
  int32_t blocked_id = -1;
  int32_t done_id = -1;
  assert(tp_domain_add_predicate(domain, &ready_def, &ready_id) == TP_STATUS_OK);
  assert(tp_domain_add_predicate(domain, &blocked_def, &blocked_id) == TP_STATUS_OK);
  assert(tp_domain_add_predicate(domain, &done_def, &done_id) == TP_STATUS_OK);

  const int32_t action_types[1] = {0};
  int32_t clear_blocked_schema_id = -1;
  int32_t finish_schema_id = -1;
  {
    const TP_Action_Literal preconditions[1] = {
      {.predicate_id = blocked_id, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
    };
    const TP_Action_Effect effects[1] = {
      {.predicate_id = blocked_id, .op = TP_EFFECT_DELETE, .arity = 1, .slots = {0, 0, 0, 0}},
    };
    assert(tp_domain_add_action_schema(domain, 1, action_types, preconditions, 1, effects, 1, nullptr, 0, nullptr, 0, &clear_blocked_schema_id) == TP_STATUS_OK);
  }
  {
    const TP_Action_Literal preconditions[2] = {
      {.predicate_id = ready_id, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = blocked_id, .sign = -1, .arity = 1, .slots = {0, 0, 0, 0}},
    };
    const TP_Action_Effect effects[1] = {
      {.predicate_id = done_id, .op = TP_EFFECT_ADD, .arity = 1, .slots = {0, 0, 0, 0}},
    };
    assert(tp_domain_add_action_schema(domain, 1, action_types, preconditions, 2, effects, 1, nullptr, 0, nullptr, 0, &finish_schema_id) == TP_STATUS_OK);
  }

  const int32_t object_types[1] = {0};
  TP_State *state = tp_state_create(domain, 1, object_types);
  assert(state != nullptr);

  const int32_t obj_args[1] = {0};
  assert(tp_state_add_fact(state, ready_id, 1, obj_args) == TP_STATUS_OK);
  assert(tp_state_add_goal_fact(state, done_id, 1, obj_args) == TP_STATUS_OK);

  // Act
  const ReducedTaskPatterns patterns = analyze_reduced_task_patterns(*domain, *state);
  const std::vector<CandidateAction> filtered_candidates =
    generate_candidate_actions(*domain, *state, domain->limits.max_candidates, &patterns.filtered);

  // Assert
  assert(!has_pattern(patterns.filtered, clear_blocked_schema_id, 0));
  assert(has_pattern(patterns.filtered, finish_schema_id, 0));
  assert(!has_candidate(filtered_candidates, clear_blocked_schema_id, 0));
  assert(has_candidate(filtered_candidates, finish_schema_id, 0));
  solve_and_expect_plan_length(domain, state, 1);

  tp_state_destroy(state);
  tp_domain_destroy(domain);
}

void numeric_precondition_support_chain_stays_reachable() {
  // Arrange
  TP_Domain *domain = create_domain();

  const TP_Predicate_Def done_def {.arity = 1, .arg_types = {0, 0, 0, 0}};
  const TP_Function_Def fuel_def {.arity = 1, .arg_types = {0, 0, 0, 0}};
  int32_t done_id = -1;
  int32_t fuel_id = -1;
  assert(tp_domain_add_predicate(domain, &done_def, &done_id) == TP_STATUS_OK);
  assert(tp_domain_add_function(domain, &fuel_def, &fuel_id) == TP_STATUS_OK);

  const int32_t action_types[1] = {0};
  int32_t refuel_schema_id = -1;
  int32_t finish_schema_id = -1;
  {
    const TP_Numeric_Effect numeric_effects[1] = {
      {.function_id = fuel_id, .op = TP_NUM_EFFECT_ADD, .arity = 1, .slots = {0, 0, 0, 0}, .rhs_value = 1.0f},
    };
    assert(tp_domain_add_action_schema(domain, 1, action_types, nullptr, 0, nullptr, 0, nullptr, 0, numeric_effects, 1, &refuel_schema_id) == TP_STATUS_OK);
  }
  {
    const TP_Numeric_Precondition numeric_preconditions[1] = {
      {.function_id = fuel_id, .cmp_op = TP_NUM_CMP_GTE, .arity = 1, .slots = {0, 0, 0, 0}, .rhs_value = 1.0f},
    };
    const TP_Action_Effect effects[1] = {
      {.predicate_id = done_id, .op = TP_EFFECT_ADD, .arity = 1, .slots = {0, 0, 0, 0}},
    };
    assert(tp_domain_add_action_schema(domain, 1, action_types, nullptr, 0, effects, 1, numeric_preconditions, 1, nullptr, 0, &finish_schema_id) == TP_STATUS_OK);
  }

  const int32_t object_types[1] = {0};
  TP_State *state = tp_state_create(domain, 1, object_types);
  assert(state != nullptr);

  const int32_t obj_args[1] = {0};
  assert(tp_state_set_function_value(state, fuel_id, 1, obj_args, 0.0f) == TP_STATUS_OK);
  assert(tp_state_add_goal_fact(state, done_id, 1, obj_args) == TP_STATUS_OK);

  // Act
  const ReducedTaskPatterns patterns = analyze_reduced_task_patterns(*domain, *state);
  const std::vector<CandidateAction> filtered_candidates =
    generate_candidate_actions(*domain, *state, domain->limits.max_candidates, &patterns.filtered);

  // Assert
  assert(has_pattern(patterns.filtered, refuel_schema_id, 0));
  assert(has_pattern(patterns.filtered, finish_schema_id, 0));
  assert(has_candidate(filtered_candidates, refuel_schema_id, 0));
  assert(!has_candidate(filtered_candidates, finish_schema_id, 0));
  solve_and_expect_plan_length(domain, state, 2);

  tp_state_destroy(state);
  tp_domain_destroy(domain);
}

void numeric_precondition_support_chain_skips_support_when_not_needed() {
  // Arrange
  TP_Domain *domain = create_domain();

  const TP_Predicate_Def done_def {.arity = 1, .arg_types = {0, 0, 0, 0}};
  const TP_Function_Def fuel_def {.arity = 1, .arg_types = {0, 0, 0, 0}};
  int32_t done_id = -1;
  int32_t fuel_id = -1;
  assert(tp_domain_add_predicate(domain, &done_def, &done_id) == TP_STATUS_OK);
  assert(tp_domain_add_function(domain, &fuel_def, &fuel_id) == TP_STATUS_OK);

  const int32_t action_types[1] = {0};
  int32_t refuel_schema_id = -1;
  int32_t finish_schema_id = -1;
  {
    const TP_Numeric_Effect numeric_effects[1] = {
      {.function_id = fuel_id, .op = TP_NUM_EFFECT_ADD, .arity = 1, .slots = {0, 0, 0, 0}, .rhs_value = 1.0f},
    };
    assert(tp_domain_add_action_schema(domain, 1, action_types, nullptr, 0, nullptr, 0, nullptr, 0, numeric_effects, 1, &refuel_schema_id) == TP_STATUS_OK);
  }
  {
    const TP_Numeric_Precondition numeric_preconditions[1] = {
      {.function_id = fuel_id, .cmp_op = TP_NUM_CMP_GTE, .arity = 1, .slots = {0, 0, 0, 0}, .rhs_value = 1.0f},
    };
    const TP_Action_Effect effects[1] = {
      {.predicate_id = done_id, .op = TP_EFFECT_ADD, .arity = 1, .slots = {0, 0, 0, 0}},
    };
    assert(tp_domain_add_action_schema(domain, 1, action_types, nullptr, 0, effects, 1, numeric_preconditions, 1, nullptr, 0, &finish_schema_id) == TP_STATUS_OK);
  }

  const int32_t object_types[1] = {0};
  TP_State *state = tp_state_create(domain, 1, object_types);
  assert(state != nullptr);

  const int32_t obj_args[1] = {0};
  assert(tp_state_set_function_value(state, fuel_id, 1, obj_args, 1.0f) == TP_STATUS_OK);
  assert(tp_state_add_goal_fact(state, done_id, 1, obj_args) == TP_STATUS_OK);

  // Act
  const ReducedTaskPatterns patterns = analyze_reduced_task_patterns(*domain, *state);
  const std::vector<CandidateAction> filtered_candidates =
    generate_candidate_actions(*domain, *state, domain->limits.max_candidates, &patterns.filtered);

  // Assert
  assert(!has_pattern(patterns.filtered, refuel_schema_id, 0));
  assert(has_pattern(patterns.filtered, finish_schema_id, 0));
  assert(!has_candidate(filtered_candidates, refuel_schema_id, 0));
  assert(has_candidate(filtered_candidates, finish_schema_id, 0));
  solve_and_expect_plan_length(domain, state, 1);

  tp_state_destroy(state);
  tp_domain_destroy(domain);
}

}  // namespace

int main() {
  negative_precondition_support_chain_stays_reachable();
  negative_precondition_support_chain_skips_support_when_not_needed();
  numeric_precondition_support_chain_stays_reachable();
  numeric_precondition_support_chain_skips_support_when_not_needed();
  return 0;
}
