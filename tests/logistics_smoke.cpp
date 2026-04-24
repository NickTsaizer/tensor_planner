#include "../include/tensor_planner.h"

#include <cassert>
#include <vector>

namespace {

enum TypeId : int32_t {
  TYPE_TRUCK = 0,
  TYPE_PACKAGE = 1,
  TYPE_LOCATION = 2,
};

enum PredicateId : int32_t {
  PRED_AT_TRUCK = 0,
  PRED_AT_PACKAGE = 1,
  PRED_IN_TRUCK = 2,
  PRED_ROAD = 3,
};

enum SchemaId : int32_t {
  SCHEMA_DRIVE = 0,
  SCHEMA_LOAD = 1,
  SCHEMA_UNLOAD = 2,
};

void add_fact(TP_State *state, int32_t predicate_id, int32_t arg0, int32_t arg1) {
  const int32_t args[2] = {arg0, arg1};
  assert(tp_state_add_fact(state, predicate_id, 2, args) == TP_STATUS_OK);
}

void add_goal(TP_State *state, int32_t package_id, int32_t location_id) {
  const int32_t args[2] = {package_id, location_id};
  assert(tp_state_add_goal_fact(state, PRED_AT_PACKAGE, 2, args) == TP_STATUS_OK);
}

void logistics_scorer(
  const TP_Schema_Tensors *,
  const TP_Problem_Tensors *problem,
  void *user_data,
  float *out_action_scores,
  float *,
  bool *out_has_state_value
) {
  *out_has_state_value = false;
  const int32_t goal_location = *static_cast<int32_t *>(user_data);
  for (int32_t candidate_index = 0; candidate_index < problem->candidate_count; ++candidate_index) {
    const int32_t schema_id = problem->cand_action_schema[candidate_index];
    const int16_t *args = &problem->cand_action_arg[candidate_index * TP_MAX_PARAMS];
    float score = 0.0f;
    if (schema_id == SCHEMA_UNLOAD && args[2] == goal_location) {
      score = 50.0f;
    } else if (schema_id == SCHEMA_LOAD) {
      score = 10.0f;
    }
    out_action_scores[candidate_index] = score;
  }
}

}  // namespace

int main() {
  const TP_Limits limits {
    .max_objects = 16,
    .max_facts = 64,
    .max_goals = 8,
    .max_candidates = 64,
    .max_expansions = 2000,
    .max_plan_length = 32,
  };

  TP_Domain *domain = tp_domain_create(&limits);
  assert(domain != nullptr);

  {
    TP_Predicate_Def predicate {.arity = 2, .arg_types = {TYPE_TRUCK, TYPE_LOCATION, 0, 0}};
    int32_t id = -1;
    assert(tp_domain_add_predicate(domain, &predicate, &id) == TP_STATUS_OK);
  }
  {
    TP_Predicate_Def predicate {.arity = 2, .arg_types = {TYPE_PACKAGE, TYPE_LOCATION, 0, 0}};
    int32_t id = -1;
    assert(tp_domain_add_predicate(domain, &predicate, &id) == TP_STATUS_OK);
  }
  {
    TP_Predicate_Def predicate {.arity = 2, .arg_types = {TYPE_PACKAGE, TYPE_TRUCK, 0, 0}};
    int32_t id = -1;
    assert(tp_domain_add_predicate(domain, &predicate, &id) == TP_STATUS_OK);
  }
  {
    TP_Predicate_Def predicate {.arity = 2, .arg_types = {TYPE_LOCATION, TYPE_LOCATION, 0, 0}};
    int32_t id = -1;
    assert(tp_domain_add_predicate(domain, &predicate, &id) == TP_STATUS_OK);
  }

  {
    const int32_t types[3] = {TYPE_TRUCK, TYPE_LOCATION, TYPE_LOCATION};
    const TP_Action_Literal pre[] = {
      {.predicate_id = PRED_AT_TRUCK, .sign = 1, .arity = 2, .slots = {0, 1, 0, 0}},
      {.predicate_id = PRED_ROAD, .sign = 1, .arity = 2, .slots = {1, 2, 0, 0}},
    };
    const TP_Action_Effect eff[] = {
      {.predicate_id = PRED_AT_TRUCK, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 1, 0, 0}},
      {.predicate_id = PRED_AT_TRUCK, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 2, 0, 0}},
    };
    int32_t id = -1;
    assert(tp_domain_add_action_schema(domain, 3, types, pre, 2, eff, 2, nullptr, 0, nullptr, 0, &id) == TP_STATUS_OK);
  }
  {
    const int32_t types[3] = {TYPE_TRUCK, TYPE_PACKAGE, TYPE_LOCATION};
    const TP_Action_Literal pre[] = {
      {.predicate_id = PRED_AT_TRUCK, .sign = 1, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_AT_PACKAGE, .sign = 1, .arity = 2, .slots = {1, 2, 0, 0}},
    };
    const TP_Action_Effect eff[] = {
      {.predicate_id = PRED_AT_PACKAGE, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {1, 2, 0, 0}},
      {.predicate_id = PRED_IN_TRUCK, .op = TP_EFFECT_ADD, .arity = 2, .slots = {1, 0, 0, 0}},
    };
    int32_t id = -1;
    assert(tp_domain_add_action_schema(domain, 3, types, pre, 2, eff, 2, nullptr, 0, nullptr, 0, &id) == TP_STATUS_OK);
  }
  {
    const int32_t types[3] = {TYPE_TRUCK, TYPE_PACKAGE, TYPE_LOCATION};
    const TP_Action_Literal pre[] = {
      {.predicate_id = PRED_AT_TRUCK, .sign = 1, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_IN_TRUCK, .sign = 1, .arity = 2, .slots = {1, 0, 0, 0}},
    };
    const TP_Action_Effect eff[] = {
      {.predicate_id = PRED_IN_TRUCK, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {1, 0, 0, 0}},
      {.predicate_id = PRED_AT_PACKAGE, .op = TP_EFFECT_ADD, .arity = 2, .slots = {1, 2, 0, 0}},
    };
    int32_t id = -1;
    assert(tp_domain_add_action_schema(domain, 3, types, pre, 2, eff, 2, nullptr, 0, nullptr, 0, &id) == TP_STATUS_OK);
  }

  const int32_t object_types[5] = {TYPE_TRUCK, TYPE_PACKAGE, TYPE_LOCATION, TYPE_LOCATION, TYPE_LOCATION};
  TP_State *state = tp_state_create(domain, 5, object_types);
  assert(state != nullptr);

  add_fact(state, PRED_AT_TRUCK, 0, 2);
  add_fact(state, PRED_AT_PACKAGE, 1, 2);
  add_fact(state, PRED_ROAD, 2, 3);
  add_fact(state, PRED_ROAD, 3, 2);
  add_fact(state, PRED_ROAD, 3, 4);
  add_fact(state, PRED_ROAD, 4, 3);
  add_goal(state, 1, 4);

  TP_Solver *solver = tp_solver_create(domain);
  assert(solver != nullptr);

  int32_t goal_location = 4;
  assert(tp_solver_set_scorer(solver, logistics_scorer, &goal_location) == TP_STATUS_OK);
  TP_Solve_Result guided {};
  assert(tp_solver_solve(solver, state, &guided) == TP_STATUS_OK);
  assert(guided.solved);
  assert(guided.plan_length == 4);
  assert(guided.plan_actions[0].schema_id == SCHEMA_LOAD);
  assert(guided.plan_actions[1].schema_id == SCHEMA_DRIVE);
  assert(guided.plan_actions[2].schema_id == SCHEMA_DRIVE);
  assert(guided.plan_actions[3].schema_id == SCHEMA_UNLOAD);
  assert(guided.plan_actions[3].args[2] == goal_location);
  assert(guided.scorer_calls >= 1);

  tp_solve_result_dispose(&guided);
  tp_solver_destroy(solver);
  tp_state_destroy(state);
  tp_domain_destroy(domain);
  return 0;
}
