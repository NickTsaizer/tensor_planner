#include "../include/tensor_planner.h"

#include <cassert>

namespace {

void prefer_last_argument(
  const TP_Schema_Tensors *,
  const TP_Problem_Tensors *problem,
  void *,
  float *out_action_scores,
  float *,
  bool *out_has_state_value
) {
  *out_has_state_value = false;
  for (int32_t index = 0; index < problem->candidate_count; ++index) {
    out_action_scores[index] = static_cast<float>(problem->cand_action_arg[index * TP_MAX_PARAMS + 2]);
  }
}

}

int main() {
  const TP_Limits limits {
    .max_objects = 8,
    .max_facts = 16,
    .max_goals = 4,
    .max_candidates = 16,
    .max_expansions = 64,
    .max_plan_length = 8,
  };

  TP_Domain *domain = tp_domain_create(&limits);
  assert(domain != nullptr);

  const TP_Predicate_Def at_predicate {
    .arity = 2,
    .arg_types = {0, 1, 0, 0},
  };

  const TP_Predicate_Def connected_predicate {
    .arity = 2,
    .arg_types = {1, 1, 0, 0},
  };
  const TP_Function_Def fuel_function {
    .arity = 1,
    .arg_types = {0, 0, 0, 0},
  };

  int32_t at_id = -1;
  int32_t connected_id = -1;
  int32_t fuel_id = -1;
  assert(tp_domain_add_predicate(domain, &at_predicate, &at_id) == TP_STATUS_OK);
  assert(tp_domain_add_predicate(domain, &connected_predicate, &connected_id) == TP_STATUS_OK);
  assert(tp_domain_add_function(domain, &fuel_function, &fuel_id) == TP_STATUS_OK);

  const int32_t action_types[3] = {0, 1, 1};
  const TP_Action_Literal preconditions[2] = {
    {.predicate_id = at_id, .sign = 1, .arity = 2, .slots = {0, 1, 0, 0}},
    {.predicate_id = connected_id, .sign = 1, .arity = 2, .slots = {1, 2, 0, 0}},
  };
  const TP_Action_Effect effects[2] = {
    {.predicate_id = at_id, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 1, 0, 0}},
    {.predicate_id = at_id, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 2, 0, 0}},
  };
  const TP_Numeric_Precondition numeric_preconditions[1] = {
    {.function_id = fuel_id, .cmp_op = TP_NUM_CMP_GTE, .arity = 1, .slots = {0, 0, 0, 0}, .rhs_value = 1.0f},
  };
  const TP_Numeric_Effect numeric_effects[1] = {
    {.function_id = fuel_id, .op = TP_NUM_EFFECT_SUBTRACT, .arity = 1, .slots = {0, 0, 0, 0}, .rhs_value = 1.0f},
  };

  int32_t move_id = -1;
  assert(tp_domain_add_action_schema(
           domain,
           3,
           action_types,
           nullptr,
           1,
           effects,
           2,
           numeric_preconditions,
           1,
           numeric_effects,
           1,
           &move_id
         ) == TP_STATUS_INVALID_ARGUMENT);
  assert(tp_domain_add_action_schema(
           domain,
           3,
           action_types,
            preconditions,
            2,
            effects,
            2,
            numeric_preconditions,
            1,
            numeric_effects,
            1,
            &move_id
          ) == TP_STATUS_OK);
  assert(move_id == 0);

  const int32_t object_types[5] = {0, 1, 1, 1, 1};
  TP_State *state = tp_state_create(domain, 5, object_types);
  assert(state != nullptr);

  const int32_t at_args[2] = {0, 1};
  const int32_t connected_args[2] = {1, 2};
  const int32_t alternative_connected_args[2] = {1, 3};
  const int32_t distractor_connected_args[2] = {3, 4};
  const int32_t goal_args[2] = {0, 2};
  const int32_t fuel_args[1] = {0};

  assert(tp_state_add_fact(state, at_id, 2, at_args) == TP_STATUS_OK);
  assert(tp_state_add_fact(state, connected_id, 2, connected_args) == TP_STATUS_OK);
  assert(tp_state_add_fact(state, connected_id, 2, alternative_connected_args) == TP_STATUS_OK);
  assert(tp_state_add_fact(state, connected_id, 2, distractor_connected_args) == TP_STATUS_OK);
  assert(tp_state_set_function_value(state, fuel_id, 1, fuel_args, 10.0f) == TP_STATUS_OK);
  assert(tp_state_add_goal_fact(state, at_id, 2, goal_args) == TP_STATUS_OK);

  TP_Schema_Tensors schema_tensors {};
  assert(tp_domain_export_schema_tensors(domain, &schema_tensors) == TP_STATUS_OK);
  assert(schema_tensors.function_count == 1);
  assert(schema_tensors.func_arity_count == 1);
  assert(schema_tensors.func_arity[0] == 1);
  assert(schema_tensors.max_num_preconditions == 1);
  assert(schema_tensors.max_num_effects == 1);
  assert(schema_tensors.num_pre_func_id[0] == fuel_id);
  assert(schema_tensors.num_eff_func_id[0] == fuel_id);
  tp_schema_tensors_dispose(&schema_tensors);

  TP_Problem_Tensors tensors {};
  assert(tp_state_export_problem_tensors(domain, state, 8, &tensors) == TP_STATUS_OK);
  assert(tensors.object_count == 5);
  assert(tensors.fact_count == 4);
  assert(tensors.function_value_count == 1);
  assert(tensors.num_func_id[0] == fuel_id);
  assert(tensors.num_func_arg[0] == 0);
  assert(tensors.num_value[0] == 10.0f);
  assert(tensors.goal_count == 1);
  assert(tensors.candidate_count == 2);
  tp_problem_tensors_dispose(&tensors);

  TP_Candidate_Action_List candidates {};
  assert(tp_state_generate_candidates(domain, state, 1, &candidates) == TP_STATUS_OK);
  assert(candidates.count == 1);
  assert(candidates.actions[0].schema_id == move_id);
  assert(candidates.actions[0].args[0] == 0);
  assert(candidates.actions[0].args[1] == 1);
  assert(candidates.actions[0].args[2] == 2);
  tp_candidate_action_list_dispose(&candidates);

  TP_Solver *solver = tp_solver_create(domain);
  assert(solver != nullptr);
  assert(tp_solver_use_tensor_baseline_scorer(solver) == TP_STATUS_OK);

  TP_Solve_Result result {};
  const TP_Status solve_status = tp_solver_solve(solver, state, &result);
  assert(solve_status == TP_STATUS_OK);
  assert(result.solved);
  assert(result.candidate_generation_calls >= 1);
  assert(result.scorer_calls >= 1);
  assert(result.scorer_time_us >= 0);
  assert(result.candidate_generation_time_us >= 0);
  assert(result.index_rebuilds >= 0);
  assert(result.plan_length == 1);
  assert(result.plan_actions[0].schema_id == move_id);
  assert(result.plan_actions[0].args[0] == 0);
  assert(result.plan_actions[0].args[2] == 2);

  TP_State *blocked_state = tp_state_create(domain, 5, object_types);
  assert(blocked_state != nullptr);
  assert(tp_state_add_fact(blocked_state, at_id, 2, at_args) == TP_STATUS_OK);
  assert(tp_state_add_fact(blocked_state, connected_id, 2, connected_args) == TP_STATUS_OK);
  assert(tp_state_add_fact(blocked_state, connected_id, 2, alternative_connected_args) == TP_STATUS_OK);
  assert(tp_state_add_fact(blocked_state, connected_id, 2, distractor_connected_args) == TP_STATUS_OK);
  assert(tp_state_set_function_value(blocked_state, fuel_id, 1, fuel_args, 0.0f) == TP_STATUS_OK);
  assert(tp_state_add_goal_fact(blocked_state, at_id, 2, goal_args) == TP_STATUS_OK);

  TP_Solve_Result blocked_result {};
  const TP_Status blocked_status = tp_solver_solve(solver, blocked_state, &blocked_result);
  assert(blocked_status == TP_STATUS_NO_SOLUTION);
  assert(!blocked_result.solved);
  tp_solve_result_dispose(&blocked_result);

  tp_solve_result_dispose(&result);
  tp_state_destroy(blocked_state);
  tp_solver_destroy(solver);
  tp_state_destroy(state);
  tp_domain_destroy(domain);
  return 0;
}
