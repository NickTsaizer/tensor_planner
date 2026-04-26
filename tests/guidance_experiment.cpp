#include "../include/tensor_planner.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>

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

struct ExperimentContext {
  int32_t package_id = 1;
  int32_t depot_id = 2;
  int32_t goal_id = 11;
};

void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "guidance_experiment failed: " << message << '\n';
    std::exit(1);
  }
}

void add_fact(TP_State *state, int32_t predicate_id, int32_t arg0, int32_t arg1) {
  const int32_t args[2] = {arg0, arg1};
  check(tp_state_add_fact(state, predicate_id, 2, args) == TP_STATUS_OK, "add fact");
}

void add_goal(TP_State *state, int32_t package_id, int32_t location_id) {
  const int32_t args[2] = {package_id, location_id};
  check(tp_state_add_goal_fact(state, PRED_AT_PACKAGE, 2, args) == TP_STATUS_OK, "add goal");
}

void ai_ready_guidance_scorer(
  const TP_Schema_Tensors *,
  const TP_Problem_Tensors *problem,
  void *user_data,
  float *out_action_scores,
  float *,
  bool *out_has_state_value
) {
  *out_has_state_value = false;
  const ExperimentContext &context = *static_cast<const ExperimentContext *>(user_data);

  for (int32_t candidate_index = 0; candidate_index < problem->candidate_count; ++candidate_index) {
    const int32_t schema_id = problem->cand_action_schema[candidate_index];
    const int16_t *args = &problem->cand_action_arg[candidate_index * TP_MAX_PARAMS];
    float score = 0.0f;

    if (schema_id == SCHEMA_LOAD && args[1] == context.package_id && args[2] == context.depot_id) {
      score = 90.0f;
    } else if (schema_id == SCHEMA_DRIVE && args[2] == context.goal_id) {
      score = 70.0f;
    } else if (schema_id == SCHEMA_UNLOAD && args[1] == context.package_id && args[2] == context.goal_id) {
      score = 110.0f;
    } else if (schema_id == SCHEMA_DRIVE) {
      score = -5.0f;
    }

    out_action_scores[candidate_index] = score;
  }
}

TP_Domain *build_domain() {
  const TP_Limits limits {
    .max_objects = 16,
    .max_facts = 64,
    .max_goals = 4,
    .max_candidates = 64,
    .max_expansions = 500,
    .max_plan_length = 8,
  };

  TP_Domain *domain = tp_domain_create(&limits);
  check(domain != nullptr, "create domain");

  {
    TP_Predicate_Def predicate {.arity = 2, .arg_types = {TYPE_TRUCK, TYPE_LOCATION, 0, 0}};
    int32_t id = -1;
    check(tp_domain_add_predicate(domain, &predicate, &id) == TP_STATUS_OK, "add truck location predicate");
  }
  {
    TP_Predicate_Def predicate {.arity = 2, .arg_types = {TYPE_PACKAGE, TYPE_LOCATION, 0, 0}};
    int32_t id = -1;
    check(tp_domain_add_predicate(domain, &predicate, &id) == TP_STATUS_OK, "add package location predicate");
  }
  {
    TP_Predicate_Def predicate {.arity = 2, .arg_types = {TYPE_PACKAGE, TYPE_TRUCK, 0, 0}};
    int32_t id = -1;
    check(tp_domain_add_predicate(domain, &predicate, &id) == TP_STATUS_OK, "add package truck predicate");
  }
  {
    TP_Predicate_Def predicate {.arity = 2, .arg_types = {TYPE_LOCATION, TYPE_LOCATION, 0, 0}};
    int32_t id = -1;
    check(tp_domain_add_predicate(domain, &predicate, &id) == TP_STATUS_OK, "add road predicate");
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
    check(tp_domain_add_action_schema(domain, 3, types, pre, 2, eff, 2, nullptr, 0, nullptr, 0, &id) == TP_STATUS_OK, "add drive action");
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
    check(tp_domain_add_action_schema(domain, 3, types, pre, 2, eff, 2, nullptr, 0, nullptr, 0, &id) == TP_STATUS_OK, "add load action");
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
    check(tp_domain_add_action_schema(domain, 3, types, pre, 2, eff, 2, nullptr, 0, nullptr, 0, &id) == TP_STATUS_OK, "add unload action");
  }

  return domain;
}

TP_State *build_state(const TP_Domain *domain, const ExperimentContext &context) {
  const int32_t object_types[12] = {
    TYPE_TRUCK,
    TYPE_PACKAGE,
    TYPE_LOCATION,
    TYPE_LOCATION,
    TYPE_LOCATION,
    TYPE_LOCATION,
    TYPE_LOCATION,
    TYPE_LOCATION,
    TYPE_LOCATION,
    TYPE_LOCATION,
    TYPE_LOCATION,
    TYPE_LOCATION,
  };
  TP_State *state = tp_state_create(domain, 12, object_types);
  check(state != nullptr, "create state");

  add_fact(state, PRED_AT_TRUCK, 0, context.depot_id);
  add_fact(state, PRED_AT_PACKAGE, context.package_id, context.depot_id);
  for (int32_t location_id = 3; location_id < context.goal_id; ++location_id) {
    add_fact(state, PRED_ROAD, context.depot_id, location_id);
  }
  add_fact(state, PRED_ROAD, context.depot_id, context.goal_id);
  add_goal(state, context.package_id, context.goal_id);
  return state;
}

void print_result(const char *label, const TP_Solve_Result &result) {
  std::cout << label
            << ": expansions=" << result.expansions
            << " generated=" << result.generated
            << " candidate_calls=" << result.candidate_generation_calls
            << " scorer_calls=" << result.scorer_calls
            << " candidate_us=" << result.candidate_generation_time_us
            << " scorer_export_us=" << result.scorer_export_time_us
            << " scorer_us=" << result.scorer_time_us
            << " sort_us=" << result.scorer_sort_time_us
            << '\n';
}

}  // namespace

int main() {
  const ExperimentContext context {};
  TP_Domain *domain = build_domain();
  TP_State *state = build_state(domain, context);

  TP_Solver *baseline_solver = tp_solver_create(domain);
  check(baseline_solver != nullptr, "create baseline solver");
  TP_Solve_Result baseline {};
  check(tp_solver_solve(baseline_solver, state, &baseline) == TP_STATUS_OK, "solve default guided");
  check(baseline.solved, "baseline solved");
  check(baseline.plan_length == 3, "baseline plan length");

  TP_Solver *guided_solver = tp_solver_create(domain);
  check(guided_solver != nullptr, "create guided solver");
  check(tp_solver_set_custom_guidance(guided_solver, ai_ready_guidance_scorer, const_cast<ExperimentContext *>(&context)) == TP_STATUS_OK, "set custom guidance");
  TP_Solve_Result guided {};
  check(tp_solver_solve(guided_solver, state, &guided) == TP_STATUS_OK, "solve guided");
  check(guided.solved, "guided solved");
  check(guided.plan_length == baseline.plan_length, "guided plan length");

  print_result("default_goal_regression", baseline);
  print_result("custom_guided", guided);

  check(baseline.scorer_calls >= 1, "default scorer called");
  check(guided.scorer_calls >= 1, "guided scorer called");

  tp_solve_result_dispose(&guided);
  tp_solve_result_dispose(&baseline);
  tp_solver_destroy(guided_solver);
  tp_solver_destroy(baseline_solver);
  tp_state_destroy(state);
  tp_domain_destroy(domain);
  return 0;
}
