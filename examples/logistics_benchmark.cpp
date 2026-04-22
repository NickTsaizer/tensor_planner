#include "../include/tensor_planner.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
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

struct ProblemSpec {
  int32_t location_count = 0;
  int32_t package_count = 0;
  int32_t truck_id = 0;
  std::vector<int32_t> package_ids;
  std::vector<int32_t> location_ids;
  std::vector<int32_t> package_goal_locations;
};

struct GuidedScorerContext {
  std::vector<int32_t> package_ids;
  std::vector<int32_t> package_goal_locations;
};

struct RunResult {
  TP_Status status = TP_STATUS_UNSUPPORTED;
  int64_t time_us = 0;
  TP_Solve_Result solve {};
};

void require_status(TP_Status status, const char *message) {
  if (status != TP_STATUS_OK) {
    std::cerr << message << " failed with status " << status << '\n';
    std::exit(1);
  }
}

void require_true(bool value, const char *message) {
  if (!value) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

void add_binary_fact(TP_State *state, int32_t predicate_id, int32_t arg0, int32_t arg1) {
  const int32_t args[2] = {arg0, arg1};
  require_status(tp_state_add_fact(state, predicate_id, 2, args), "add fact");
}

void add_goal_package_at(TP_State *state, int32_t package_id, int32_t location_id) {
  const int32_t args[2] = {package_id, location_id};
  require_status(tp_state_add_goal_fact(state, PRED_AT_PACKAGE, 2, args), "add goal");
}

int32_t add_predicate(TP_Domain *domain, uint8_t arity, std::initializer_list<int32_t> arg_types) {
  TP_Predicate_Def predicate {.arity = arity, .arg_types = {0, 0, 0, 0}};
  int index = 0;
  for (const int32_t type_id : arg_types) {
    predicate.arg_types[index++] = type_id;
  }
  int32_t predicate_id = -1;
  require_status(tp_domain_add_predicate(domain, &predicate, &predicate_id), "add predicate");
  return predicate_id;
}

void add_action(
  TP_Domain *domain,
  uint8_t arity,
  const int32_t *arg_types,
  const std::vector<TP_Action_Literal> &preconditions,
  const std::vector<TP_Action_Effect> &effects,
  int32_t expected_schema_id
) {
  int32_t schema_id = -1;
  require_status(
    tp_domain_add_action_schema(
      domain,
      arity,
      arg_types,
      preconditions.data(),
      static_cast<int32_t>(preconditions.size()),
      effects.data(),
      static_cast<int32_t>(effects.size()),
      nullptr,
      0,
      nullptr,
      0,
      &schema_id
    ),
    "add action schema"
  );
  require_true(schema_id == expected_schema_id, "unexpected schema id");
}

void build_domain(TP_Domain *domain) {
  add_predicate(domain, 2, {TYPE_TRUCK, TYPE_LOCATION});
  add_predicate(domain, 2, {TYPE_PACKAGE, TYPE_LOCATION});
  add_predicate(domain, 2, {TYPE_PACKAGE, TYPE_TRUCK});
  add_predicate(domain, 2, {TYPE_LOCATION, TYPE_LOCATION});

  const int32_t drive_types[3] = {TYPE_TRUCK, TYPE_LOCATION, TYPE_LOCATION};
  add_action(domain, 3, drive_types,
    {
      {.predicate_id = PRED_AT_TRUCK, .sign = 1, .arity = 2, .slots = {0, 1, 0, 0}},
      {.predicate_id = PRED_ROAD, .sign = 1, .arity = 2, .slots = {1, 2, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT_TRUCK, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 1, 0, 0}},
      {.predicate_id = PRED_AT_TRUCK, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 2, 0, 0}},
    },
    SCHEMA_DRIVE
  );

  const int32_t load_types[3] = {TYPE_TRUCK, TYPE_PACKAGE, TYPE_LOCATION};
  add_action(domain, 3, load_types,
    {
      {.predicate_id = PRED_AT_TRUCK, .sign = 1, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_AT_PACKAGE, .sign = 1, .arity = 2, .slots = {1, 2, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT_PACKAGE, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {1, 2, 0, 0}},
      {.predicate_id = PRED_IN_TRUCK, .op = TP_EFFECT_ADD, .arity = 2, .slots = {1, 0, 0, 0}},
    },
    SCHEMA_LOAD
  );

  const int32_t unload_types[3] = {TYPE_TRUCK, TYPE_PACKAGE, TYPE_LOCATION};
  add_action(domain, 3, unload_types,
    {
      {.predicate_id = PRED_AT_TRUCK, .sign = 1, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_IN_TRUCK, .sign = 1, .arity = 2, .slots = {1, 0, 0, 0}},
    },
    {
      {.predicate_id = PRED_IN_TRUCK, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {1, 0, 0, 0}},
      {.predicate_id = PRED_AT_PACKAGE, .op = TP_EFFECT_ADD, .arity = 2, .slots = {1, 2, 0, 0}},
    },
    SCHEMA_UNLOAD
  );
}

ProblemSpec make_problem_spec(int32_t location_count, int32_t package_count) {
  ProblemSpec spec {};
  spec.location_count = location_count;
  spec.package_count = package_count;
  spec.truck_id = 0;
  for (int32_t index = 0; index < package_count; ++index) {
    spec.package_ids.push_back(1 + index);
  }
  for (int32_t index = 0; index < location_count; ++index) {
    spec.location_ids.push_back(1 + package_count + index);
  }
  for (int32_t index = 0; index < package_count; ++index) {
    const int32_t target = spec.location_ids[location_count - 1 - (index % std::min(location_count, 3))];
    spec.package_goal_locations.push_back(target);
  }
  return spec;
}

TP_State *build_problem_state(TP_Domain *domain, const ProblemSpec &spec) {
  std::vector<int32_t> object_types;
  object_types.push_back(TYPE_TRUCK);
  for (int32_t index = 0; index < spec.package_count; ++index) {
    object_types.push_back(TYPE_PACKAGE);
  }
  for (int32_t index = 0; index < spec.location_count; ++index) {
    object_types.push_back(TYPE_LOCATION);
  }

  TP_State *state = tp_state_create(domain, static_cast<int32_t>(object_types.size()), object_types.data());
  require_true(state != nullptr, "tp_state_create failed");

  const int32_t start_location = spec.location_ids.front();
  add_binary_fact(state, PRED_AT_TRUCK, spec.truck_id, start_location);
  for (int32_t package_index = 0; package_index < spec.package_count; ++package_index) {
    const int32_t package_location = spec.location_ids[package_index % std::max(1, spec.location_count - 1)];
    add_binary_fact(state, PRED_AT_PACKAGE, spec.package_ids[package_index], package_location);
    add_goal_package_at(state, spec.package_ids[package_index], spec.package_goal_locations[package_index]);
  }

  for (int32_t index = 0; index < spec.location_count - 1; ++index) {
    add_binary_fact(state, PRED_ROAD, spec.location_ids[index], spec.location_ids[index + 1]);
    add_binary_fact(state, PRED_ROAD, spec.location_ids[index + 1], spec.location_ids[index]);
  }

  return state;
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
  const auto *context = static_cast<const GuidedScorerContext *>(user_data);
  for (int32_t candidate_index = 0; candidate_index < problem->candidate_count; ++candidate_index) {
    const int32_t schema_id = problem->cand_action_schema[candidate_index];
    const int16_t *args = &problem->cand_action_arg[candidate_index * TP_MAX_PARAMS];
    float score = 0.0f;

    if (schema_id == SCHEMA_UNLOAD) {
      const int32_t package_id = args[1];
      const int32_t location_id = args[2];
      for (std::size_t index = 0; index < context->package_ids.size(); ++index) {
        if (context->package_ids[index] == package_id && context->package_goal_locations[index] == location_id) {
          score += 50.0f;
        }
      }
    } else if (schema_id == SCHEMA_LOAD) {
      const int32_t package_id = args[1];
      for (std::size_t index = 0; index < context->package_ids.size(); ++index) {
        if (context->package_ids[index] == package_id) {
          score += 10.0f;
        }
      }
    } else if (schema_id == SCHEMA_DRIVE) {
      score += 1.0f;
    }

    out_action_scores[candidate_index] = score;
  }
}

RunResult run_mode(
  TP_Solver *solver,
  TP_State *state,
  TP_Solver_Mode mode,
  TP_Score_Candidates_Fn scorer,
  void *user_data
) {
  require_status(tp_solver_set_mode(solver, mode), "set solver mode");
  require_status(tp_solver_set_scorer(solver, scorer, user_data), "set scorer");

  RunResult result {};
  const auto start = std::chrono::steady_clock::now();
  if (mode == TP_SOLVER_MODE_OPTIMAL_DEBUG) {
    result.status = tp_solver_solve_optimal_debug(solver, state, &result.solve);
  } else {
    result.status = tp_solver_solve(solver, state, &result.solve);
  }
  const auto end = std::chrono::steady_clock::now();
  result.time_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  return result;
}

}  // namespace

int main() {
  const TP_Limits limits {
    .max_objects = 128,
    .max_facts = 1024,
    .max_goals = 64,
    .max_candidates = 2048,
    .max_expansions = 100000,
    .max_plan_length = 256,
  };

  std::ofstream csv("logistics_benchmark_results.csv", std::ios::trunc);
  require_true(csv.is_open(), "failed to open logistics_benchmark_results.csv");
  const char *header = "locations,packages,mode,status,solved,time_us,plan_length,expansions,generated,scorer_calls\n";
  std::cout << header;
  csv << header;

  for (const int32_t location_count : {4, 6, 8}) {
    for (const int32_t package_count : {2, 3, 4}) {
      TP_Domain *domain = tp_domain_create(&limits);
      require_true(domain != nullptr, "tp_domain_create failed");
      build_domain(domain);

      const ProblemSpec spec = make_problem_spec(location_count, package_count);
      TP_State *state = build_problem_state(domain, spec);
      TP_Solver *solver = tp_solver_create(domain);
      require_true(solver != nullptr, "tp_solver_create failed");

      GuidedScorerContext context {
        .package_ids = spec.package_ids,
        .package_goal_locations = spec.package_goal_locations,
      };

      RunResult greedy = run_mode(solver, state, TP_SOLVER_MODE_PURE_GREEDY, nullptr, nullptr);
      RunResult guided = run_mode(solver, state, TP_SOLVER_MODE_GUIDED_MIXED, logistics_scorer, &context);
      RunResult symbolic = run_mode(solver, state, TP_SOLVER_MODE_SYMBOLIC_ASTAR, nullptr, nullptr);
      RunResult optimal = run_mode(solver, state, TP_SOLVER_MODE_OPTIMAL_DEBUG, nullptr, nullptr);

      auto emit_row = [&](const char *mode, const RunResult &result) {
        std::cout << location_count << ',' << package_count << ',' << mode << ',' << result.status << ','
                  << (result.solve.solved ? 1 : 0) << ',' << result.time_us << ',' << result.solve.plan_length
                  << ',' << result.solve.expansions << ',' << result.solve.generated << ','
                  << result.solve.scorer_calls << '\n';
        csv << location_count << ',' << package_count << ',' << mode << ',' << result.status << ','
            << (result.solve.solved ? 1 : 0) << ',' << result.time_us << ',' << result.solve.plan_length
            << ',' << result.solve.expansions << ',' << result.solve.generated << ','
            << result.solve.scorer_calls << '\n';
      };

      emit_row("greedy", greedy);
      emit_row("guided", guided);
      emit_row("symbolic", symbolic);
      emit_row("optimal", optimal);

      tp_solve_result_dispose(&greedy.solve);
      tp_solve_result_dispose(&guided.solve);
      tp_solve_result_dispose(&symbolic.solve);
      tp_solve_result_dispose(&optimal.solve);
      tp_solver_destroy(solver);
      tp_state_destroy(state);
      tp_domain_destroy(domain);
    }
  }

  std::cout << "Wrote benchmark CSV: logistics_benchmark_results.csv\n";
  return 0;
}
