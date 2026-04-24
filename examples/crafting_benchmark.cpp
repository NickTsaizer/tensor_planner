#include "../include/tensor_planner.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

enum TypeId : int32_t {
  TYPE_AGENT = 0,
  TYPE_ITEM = 1,
  TYPE_LOCATION = 2,
};

enum PredicateId : int32_t {
  PRED_AT_AGENT = 0,
  PRED_HAS = 1,
  PRED_CONNECTED = 2,
  PRED_GATHER_AT = 3,
  PRED_HOME = 4,
  PRED_REQUIRES_TOOL = 5,
  PRED_REQUIRES_TRAVEL_ITEM = 6,
  PRED_FREE_DEST = 7,
  PRED_RECIPE2_A = 8,
  PRED_RECIPE2_B = 9,
  PRED_RECIPE3_A = 10,
  PRED_RECIPE3_B = 11,
  PRED_RECIPE3_C = 12,
};

enum SchemaId : int32_t {
  SCHEMA_MOVE_FREE = 0,
  SCHEMA_MOVE_GATED = 1,
  SCHEMA_GATHER_FREE = 2,
  SCHEMA_GATHER_TOOL = 3,
  SCHEMA_CRAFT_2 = 4,
  SCHEMA_CRAFT_3 = 5,
};

struct ProblemSpec {
  int32_t agent_id = 0;
  int32_t home_id = 0;
  int32_t forest_id = 0;
  int32_t cave_id = 0;
  int32_t deep_cave_id = 0;
  int32_t river_id = 0;
  int32_t ruins_id = 0;

  int32_t flint_id = 0;
  int32_t fiber_id = 0;
  int32_t flint_axe_id = 0;
  int32_t wood_id = 0;
  int32_t stick_id = 0;
  int32_t stone_id = 0;
  int32_t stone_pickaxe_id = 0;
  int32_t iron_ore_id = 0;
  int32_t iron_pickaxe_id = 0;
  int32_t diamond_id = 0;
  int32_t diamond_pickaxe_id = 0;

  std::vector<int32_t> distractor_raw_ids;
  std::vector<int32_t> distractor_tool_ids;
  std::vector<int32_t> distractor_product_ids;
};

struct RunResult {
  TP_Status status = TP_STATUS_UNSUPPORTED;
  int64_t time_us = 0;
  TP_Solve_Result solve {};
};

struct CraftingScorerContext {
  std::unordered_map<int32_t, float> item_scores;
  std::unordered_map<int32_t, float> location_scores;
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
  require_true(schema_id == expected_schema_id || expected_schema_id < 0, "unexpected schema id");
}

void add_binary_fact(TP_State *state, int32_t predicate_id, int32_t arg0, int32_t arg1) {
  const int32_t args[2] = {arg0, arg1};
  require_status(tp_state_add_fact(state, predicate_id, 2, args), "add fact");
}

void add_unary_fact(TP_State *state, int32_t predicate_id, int32_t arg0) {
  const int32_t args[1] = {arg0};
  require_status(tp_state_add_fact(state, predicate_id, 1, args), "add fact");
}

void add_goal_has(TP_State *state, int32_t item_id) {
  const int32_t args[1] = {item_id};
  require_status(tp_state_add_goal_fact(state, PRED_HAS, 1, args), "add goal fact");
}

ProblemSpec make_problem_spec(int distractor_pairs) {
  ProblemSpec spec {};
  int32_t next_id = 0;
  spec.agent_id = next_id++;
  spec.home_id = next_id++;
  spec.forest_id = next_id++;
  spec.cave_id = next_id++;
  spec.deep_cave_id = next_id++;
  spec.river_id = next_id++;
  spec.ruins_id = next_id++;

  spec.flint_id = next_id++;
  spec.fiber_id = next_id++;
  spec.flint_axe_id = next_id++;
  spec.wood_id = next_id++;
  spec.stick_id = next_id++;
  spec.stone_id = next_id++;
  spec.stone_pickaxe_id = next_id++;
  spec.iron_ore_id = next_id++;
  spec.iron_pickaxe_id = next_id++;
  spec.diamond_id = next_id++;
  spec.diamond_pickaxe_id = next_id++;

  for (int32_t index = 0; index < distractor_pairs; ++index) {
    spec.distractor_raw_ids.push_back(next_id++);
    spec.distractor_tool_ids.push_back(next_id++);
    spec.distractor_product_ids.push_back(next_id++);
  }

  return spec;
}

TP_Domain *build_domain(const ProblemSpec &spec) {
  const TP_Limits limits {
    .max_objects = 512,
    .max_facts = 4096,
    .max_goals = 64,
    .max_candidates = 8192,
    .max_expansions = 200000,
    .max_plan_length = 100,
  };

  TP_Domain *domain = tp_domain_create(&limits);
  require_true(domain != nullptr, "tp_domain_create failed");

  add_predicate(domain, 2, {TYPE_AGENT, TYPE_LOCATION});
  add_predicate(domain, 1, {TYPE_ITEM});
  add_predicate(domain, 2, {TYPE_LOCATION, TYPE_LOCATION});
  add_predicate(domain, 2, {TYPE_ITEM, TYPE_LOCATION});
  add_predicate(domain, 1, {TYPE_LOCATION});
  add_predicate(domain, 2, {TYPE_ITEM, TYPE_ITEM});
  add_predicate(domain, 2, {TYPE_LOCATION, TYPE_ITEM});
  add_predicate(domain, 1, {TYPE_LOCATION});
  add_predicate(domain, 2, {TYPE_ITEM, TYPE_ITEM});
  add_predicate(domain, 2, {TYPE_ITEM, TYPE_ITEM});
  add_predicate(domain, 2, {TYPE_ITEM, TYPE_ITEM});
  add_predicate(domain, 2, {TYPE_ITEM, TYPE_ITEM});
  add_predicate(domain, 2, {TYPE_ITEM, TYPE_ITEM});

  const int32_t move_types[3] = {TYPE_AGENT, TYPE_LOCATION, TYPE_LOCATION};
  add_action(domain, 3, move_types,
    {
      {.predicate_id = PRED_AT_AGENT, .sign = 1, .arity = 2, .slots = {0, 1, 0, 0}},
      {.predicate_id = PRED_CONNECTED, .sign = 1, .arity = 2, .slots = {1, 2, 0, 0}},
      {.predicate_id = PRED_FREE_DEST, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT_AGENT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 1, 0, 0}},
      {.predicate_id = PRED_AT_AGENT, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 2, 0, 0}},
    },
    SCHEMA_MOVE_FREE
  );

  const int32_t move_gated_types[4] = {TYPE_AGENT, TYPE_LOCATION, TYPE_LOCATION, TYPE_ITEM};
  add_action(domain, 4, move_gated_types,
    {
      {.predicate_id = PRED_AT_AGENT, .sign = 1, .arity = 2, .slots = {0, 1, 0, 0}},
      {.predicate_id = PRED_CONNECTED, .sign = 1, .arity = 2, .slots = {1, 2, 0, 0}},
      {.predicate_id = PRED_REQUIRES_TRAVEL_ITEM, .sign = 1, .arity = 2, .slots = {2, 3, 0, 0}},
      {.predicate_id = PRED_HAS, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT_AGENT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 1, 0, 0}},
      {.predicate_id = PRED_AT_AGENT, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 2, 0, 0}},
    },
    SCHEMA_MOVE_GATED
  );

  const int32_t gather_free_types[3] = {TYPE_AGENT, TYPE_ITEM, TYPE_LOCATION};
  add_action(domain, 3, gather_free_types,
    {
      {.predicate_id = PRED_AT_AGENT, .sign = 1, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_GATHER_AT, .sign = 1, .arity = 2, .slots = {1, 2, 0, 0}},
      {.predicate_id = PRED_REQUIRES_TOOL, .sign = -1, .arity = 2, .slots = {1, 1, 0, 0}},
    },
    {
      {.predicate_id = PRED_HAS, .op = TP_EFFECT_ADD, .arity = 1, .slots = {1, 0, 0, 0}},
    },
    SCHEMA_GATHER_FREE
  );

  const int32_t gather_tool_types[4] = {TYPE_AGENT, TYPE_ITEM, TYPE_LOCATION, TYPE_ITEM};
  add_action(domain, 4, gather_tool_types,
    {
      {.predicate_id = PRED_AT_AGENT, .sign = 1, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_GATHER_AT, .sign = 1, .arity = 2, .slots = {1, 2, 0, 0}},
      {.predicate_id = PRED_REQUIRES_TOOL, .sign = 1, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_HAS, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
    },
    {
      {.predicate_id = PRED_HAS, .op = TP_EFFECT_ADD, .arity = 1, .slots = {1, 0, 0, 0}},
    },
    SCHEMA_GATHER_TOOL
  );

  const int32_t craft2_types[5] = {TYPE_AGENT, TYPE_ITEM, TYPE_LOCATION, TYPE_ITEM, TYPE_ITEM};
  add_action(domain, 5, craft2_types,
    {
      {.predicate_id = PRED_AT_AGENT, .sign = 1, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_HOME, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_RECIPE2_A, .sign = 1, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_RECIPE2_B, .sign = 1, .arity = 2, .slots = {1, 4, 0, 0}},
      {.predicate_id = PRED_HAS, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_HAS, .sign = 1, .arity = 1, .slots = {4, 0, 0, 0}},
    },
    {
      {.predicate_id = PRED_HAS, .op = TP_EFFECT_DELETE, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_HAS, .op = TP_EFFECT_DELETE, .arity = 1, .slots = {4, 0, 0, 0}},
      {.predicate_id = PRED_HAS, .op = TP_EFFECT_ADD, .arity = 1, .slots = {1, 0, 0, 0}},
    },
    SCHEMA_CRAFT_2
  );

  const int32_t craft3_types[6] = {TYPE_AGENT, TYPE_ITEM, TYPE_LOCATION, TYPE_ITEM, TYPE_ITEM, TYPE_ITEM};
  add_action(domain, 6, craft3_types,
    {
      {.predicate_id = PRED_AT_AGENT, .sign = 1, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_HOME, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_RECIPE3_A, .sign = 1, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_RECIPE3_B, .sign = 1, .arity = 2, .slots = {1, 4, 0, 0}},
      {.predicate_id = PRED_RECIPE3_C, .sign = 1, .arity = 2, .slots = {1, 5, 0, 0}},
      {.predicate_id = PRED_HAS, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_HAS, .sign = 1, .arity = 1, .slots = {4, 0, 0, 0}},
      {.predicate_id = PRED_HAS, .sign = 1, .arity = 1, .slots = {5, 0, 0, 0}},
    },
    {
      {.predicate_id = PRED_HAS, .op = TP_EFFECT_DELETE, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_HAS, .op = TP_EFFECT_DELETE, .arity = 1, .slots = {4, 0, 0, 0}},
      {.predicate_id = PRED_HAS, .op = TP_EFFECT_DELETE, .arity = 1, .slots = {5, 0, 0, 0}},
      {.predicate_id = PRED_HAS, .op = TP_EFFECT_ADD, .arity = 1, .slots = {1, 0, 0, 0}},
    },
    SCHEMA_CRAFT_3
  );

  return domain;
}

TP_State *build_problem_state(const ProblemSpec &spec, TP_Domain *domain) {
  std::vector<int32_t> object_types = {
    TYPE_AGENT,
    TYPE_LOCATION, TYPE_LOCATION, TYPE_LOCATION, TYPE_LOCATION, TYPE_LOCATION, TYPE_LOCATION,
    TYPE_ITEM, TYPE_ITEM, TYPE_ITEM, TYPE_ITEM, TYPE_ITEM, TYPE_ITEM, TYPE_ITEM,
    TYPE_ITEM, TYPE_ITEM, TYPE_ITEM, TYPE_ITEM,
  };
  for (std::size_t index = 0; index < spec.distractor_raw_ids.size(); ++index) {
    object_types.push_back(TYPE_ITEM);
    object_types.push_back(TYPE_ITEM);
    object_types.push_back(TYPE_ITEM);
  }

  TP_State *state = tp_state_create(domain, static_cast<int32_t>(object_types.size()), object_types.data());
  require_true(state != nullptr, "tp_state_create failed");

  add_binary_fact(state, PRED_AT_AGENT, spec.agent_id, spec.home_id);
  add_unary_fact(state, PRED_HOME, spec.home_id);
  add_unary_fact(state, PRED_FREE_DEST, spec.home_id);
  add_unary_fact(state, PRED_FREE_DEST, spec.forest_id);
  add_unary_fact(state, PRED_FREE_DEST, spec.river_id);
  add_unary_fact(state, PRED_FREE_DEST, spec.cave_id);

  const int locations[] = {spec.home_id, spec.forest_id, spec.river_id, spec.cave_id, spec.ruins_id, spec.deep_cave_id};
  for (int i = 0; i < 5; ++i) {
    add_binary_fact(state, PRED_CONNECTED, locations[i], locations[i + 1]);
    add_binary_fact(state, PRED_CONNECTED, locations[i + 1], locations[i]);
  }
  add_binary_fact(state, PRED_CONNECTED, spec.home_id, spec.ruins_id);
  add_binary_fact(state, PRED_CONNECTED, spec.ruins_id, spec.home_id);
  add_binary_fact(state, PRED_CONNECTED, spec.forest_id, spec.deep_cave_id);
  add_binary_fact(state, PRED_CONNECTED, spec.deep_cave_id, spec.forest_id);
  add_binary_fact(state, PRED_CONNECTED, spec.cave_id, spec.deep_cave_id);
  add_binary_fact(state, PRED_CONNECTED, spec.deep_cave_id, spec.cave_id);

  add_binary_fact(state, PRED_GATHER_AT, spec.flint_id, spec.river_id);
  add_binary_fact(state, PRED_GATHER_AT, spec.fiber_id, spec.forest_id);
  add_binary_fact(state, PRED_GATHER_AT, spec.wood_id, spec.forest_id);
  add_binary_fact(state, PRED_GATHER_AT, spec.stone_id, spec.cave_id);
  add_binary_fact(state, PRED_GATHER_AT, spec.iron_ore_id, spec.cave_id);
  add_binary_fact(state, PRED_GATHER_AT, spec.diamond_id, spec.deep_cave_id);

  add_binary_fact(state, PRED_REQUIRES_TOOL, spec.wood_id, spec.flint_axe_id);
  add_binary_fact(state, PRED_REQUIRES_TOOL, spec.iron_ore_id, spec.stone_pickaxe_id);
  add_binary_fact(state, PRED_REQUIRES_TOOL, spec.diamond_id, spec.iron_pickaxe_id);
  add_binary_fact(state, PRED_REQUIRES_TRAVEL_ITEM, spec.ruins_id, spec.flint_axe_id);
  add_binary_fact(state, PRED_REQUIRES_TRAVEL_ITEM, spec.deep_cave_id, spec.iron_pickaxe_id);

  add_binary_fact(state, PRED_RECIPE2_A, spec.flint_axe_id, spec.flint_id);
  add_binary_fact(state, PRED_RECIPE2_B, spec.flint_axe_id, spec.fiber_id);
  add_binary_fact(state, PRED_RECIPE2_A, spec.stick_id, spec.wood_id);
  add_binary_fact(state, PRED_RECIPE2_B, spec.stick_id, spec.fiber_id);
  add_binary_fact(state, PRED_RECIPE2_A, spec.stone_pickaxe_id, spec.stone_id);
  add_binary_fact(state, PRED_RECIPE2_B, spec.stone_pickaxe_id, spec.stick_id);
  add_binary_fact(state, PRED_RECIPE2_A, spec.iron_pickaxe_id, spec.iron_ore_id);
  add_binary_fact(state, PRED_RECIPE2_B, spec.iron_pickaxe_id, spec.stick_id);
  add_binary_fact(state, PRED_RECIPE3_A, spec.diamond_pickaxe_id, spec.diamond_id);
  add_binary_fact(state, PRED_RECIPE3_B, spec.diamond_pickaxe_id, spec.stick_id);
  add_binary_fact(state, PRED_RECIPE3_C, spec.diamond_pickaxe_id, spec.iron_pickaxe_id);

  for (std::size_t index = 0; index < spec.distractor_raw_ids.size(); ++index) {
    const int32_t raw_id = spec.distractor_raw_ids[index];
    const int32_t tool_id = spec.distractor_tool_ids[index];
    const int32_t product_id = spec.distractor_product_ids[index];
    const int32_t location = (index % 2 == 0) ? spec.ruins_id : spec.river_id;
    add_binary_fact(state, PRED_GATHER_AT, raw_id, location);
    if (index % 2 == 1) {
      add_binary_fact(state, PRED_REQUIRES_TOOL, raw_id, tool_id);
    }
    add_binary_fact(state, PRED_RECIPE2_A, product_id, raw_id);
    add_binary_fact(state, PRED_RECIPE2_B, product_id, spec.fiber_id);
  }

  add_goal_has(state, spec.diamond_pickaxe_id);
  return state;
}

CraftingScorerContext make_context(const ProblemSpec &spec) {
  CraftingScorerContext context {};
  context.item_scores[spec.flint_id] = 3.0f;
  context.item_scores[spec.fiber_id] = 3.0f;
  context.item_scores[spec.flint_axe_id] = 10.0f;
  context.item_scores[spec.wood_id] = 6.0f;
  context.item_scores[spec.stick_id] = 8.0f;
  context.item_scores[spec.stone_id] = 8.0f;
  context.item_scores[spec.stone_pickaxe_id] = 14.0f;
  context.item_scores[spec.iron_ore_id] = 18.0f;
  context.item_scores[spec.iron_pickaxe_id] = 24.0f;
  context.item_scores[spec.diamond_id] = 36.0f;
  context.item_scores[spec.diamond_pickaxe_id] = 60.0f;
  for (int32_t item : spec.distractor_raw_ids) {
    context.item_scores[item] = 1.0f;
  }
  for (int32_t item : spec.distractor_tool_ids) {
    context.item_scores[item] = 1.5f;
  }
  for (int32_t item : spec.distractor_product_ids) {
    context.item_scores[item] = 2.0f;
  }
  context.location_scores[spec.home_id] = 16.0f;
  context.location_scores[spec.forest_id] = 8.0f;
  context.location_scores[spec.river_id] = 6.0f;
  context.location_scores[spec.cave_id] = 14.0f;
  context.location_scores[spec.deep_cave_id] = 32.0f;
  context.location_scores[spec.ruins_id] = 0.5f;
  return context;
}

void crafting_scorer(
  const TP_Schema_Tensors *,
  const TP_Problem_Tensors *problem,
  void *user_data,
  float *out_action_scores,
  float *out_state_value,
  bool *out_has_state_value
) {
  const auto *context = static_cast<const CraftingScorerContext *>(user_data);
  float state_value = 0.0f;
  for (int32_t fact_index = 0; fact_index < problem->fact_count; ++fact_index) {
    if (problem->true_pred_mask[fact_index] == 0 || problem->true_pred_id[fact_index] != PRED_HAS) {
      continue;
    }
    const int32_t item_id = problem->true_pred_arg[fact_index * TP_MAX_ARITY];
    const auto found = context->item_scores.find(item_id);
    if (found != context->item_scores.end()) {
      state_value += found->second;
    }
  }
  *out_state_value = state_value;
  *out_has_state_value = true;

  for (int32_t index = 0; index < problem->candidate_count; ++index) {
    const int16_t *args = &problem->cand_action_arg[index * TP_MAX_PARAMS];
    float score = 0.0f;
    switch (problem->cand_action_schema[index]) {
      case SCHEMA_MOVE_FREE:
      case SCHEMA_MOVE_GATED: {
        const auto found = context->location_scores.find(args[2]);
        score = found != context->location_scores.end() ? found->second : 0.25f;
        break;
      }
      case SCHEMA_GATHER_FREE:
      case SCHEMA_GATHER_TOOL: {
        const auto found = context->item_scores.find(args[1]);
        score = found != context->item_scores.end() ? found->second : 0.0f;
        break;
      }
      case SCHEMA_CRAFT_2:
      case SCHEMA_CRAFT_3: {
        const auto found = context->item_scores.find(args[1]);
        score = found != context->item_scores.end() ? found->second + 5.0f : 0.0f;
        break;
      }
      default:
        break;
    }
    out_action_scores[index] = score;
  }
}

RunResult run_mode(TP_Solver *solver, TP_State *state, TP_Solver_Mode mode, TP_Score_Candidates_Fn scorer, void *user_data) {
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

const char *mode_name(TP_Solver_Mode mode) {
  switch (mode) {
    case TP_SOLVER_MODE_PURE_GREEDY:
      return "greedy";
    case TP_SOLVER_MODE_GUIDED_MIXED:
      return "guided";
    case TP_SOLVER_MODE_SYMBOLIC_ASTAR:
      return "symbolic";
    case TP_SOLVER_MODE_OPTIMAL_DEBUG:
      return "optimal";
    default:
      return "unknown";
  }
}

RunResult run_mode_logged(
  TP_Solver *solver,
  TP_State *state,
  int distractor_pairs,
  TP_Solver_Mode mode,
  TP_Score_Candidates_Fn scorer,
  void *user_data
) {
  std::cerr << "[crafting-benchmark] start distractors=" << distractor_pairs
            << " mode=" << mode_name(mode) << std::endl;
  RunResult result = run_mode(solver, state, mode, scorer, user_data);
  std::cerr << "[crafting-benchmark] done distractors=" << distractor_pairs
            << " mode=" << mode_name(mode)
            << " status=" << result.status
            << " solved=" << (result.solve.solved ? "yes" : "no")
            << " time_us=" << result.time_us
            << " plan_length=" << result.solve.plan_length
            << " expansions=" << result.solve.expansions
            << std::endl;
  return result;
}

}  // namespace

int main(int argc, char **argv) {
  std::ofstream csv("crafting_benchmark_results.csv", std::ios::trunc);
  require_true(csv.is_open(), "failed to open crafting_benchmark_results.csv");
  const char *header = "distractors,mode,status,solved,time_us,plan_length,expansions,generated,scorer_calls\n";
  std::cout << header;
  csv << header;

  bool guided_only = false;
  std::vector<int> benchmark_distractors = {0, 8, 16};
  if (argc > 1) {
    benchmark_distractors.clear();
    for (int arg_index = 1; arg_index < argc; ++arg_index) {
      const std::string arg = argv[arg_index];
      if (arg == "--guided-only") {
        guided_only = true;
      } else {
        benchmark_distractors.push_back(std::atoi(arg.c_str()));
      }
    }
  }

  for (const int distractor_pairs : benchmark_distractors) {
    std::cerr << "[crafting-benchmark] building problem distractors=" << distractor_pairs << std::endl;
    ProblemSpec spec = make_problem_spec(distractor_pairs);
    TP_Domain *domain = build_domain(spec);
    TP_State *state = build_problem_state(spec, domain);
    TP_Solver *solver = tp_solver_create(domain);
    require_true(solver != nullptr, "tp_solver_create failed");
    CraftingScorerContext context = make_context(spec);

    auto emit = [&](const char *mode, const RunResult &result) {
      std::cout << distractor_pairs << ',' << mode << ',' << result.status << ',' << (result.solve.solved ? 1 : 0)
                << ',' << result.time_us << ',' << result.solve.plan_length << ',' << result.solve.expansions << ','
                << result.solve.generated << ',' << result.solve.scorer_calls << '\n';
      csv << distractor_pairs << ',' << mode << ',' << result.status << ',' << (result.solve.solved ? 1 : 0)
          << ',' << result.time_us << ',' << result.solve.plan_length << ',' << result.solve.expansions << ','
          << result.solve.generated << ',' << result.solve.scorer_calls << '\n';
    };

    RunResult guided = run_mode_logged(solver, state, distractor_pairs, TP_SOLVER_MODE_GUIDED_MIXED, crafting_scorer, &context);
    if (guided_only) {
      emit("guided", guided);
      tp_solve_result_dispose(&guided.solve);
      tp_solver_destroy(solver);
      tp_state_destroy(state);
      tp_domain_destroy(domain);
      continue;
    }

    RunResult greedy = run_mode_logged(solver, state, distractor_pairs, TP_SOLVER_MODE_PURE_GREEDY, nullptr, nullptr);
    RunResult symbolic = run_mode_logged(solver, state, distractor_pairs, TP_SOLVER_MODE_SYMBOLIC_ASTAR, nullptr, nullptr);
    RunResult optimal = run_mode_logged(solver, state, distractor_pairs, TP_SOLVER_MODE_OPTIMAL_DEBUG, nullptr, nullptr);

    emit("greedy", greedy);
    emit("guided", guided);
    emit("symbolic", symbolic);
    emit("optimal", optimal);

    tp_solve_result_dispose(&greedy.solve);
    tp_solve_result_dispose(&guided.solve);
    tp_solve_result_dispose(&symbolic.solve);
    tp_solve_result_dispose(&optimal.solve);
    tp_solver_destroy(solver);
    tp_state_destroy(state);
    tp_domain_destroy(domain);
  }

  std::cout << "Wrote benchmark CSV: crafting_benchmark_results.csv\n";
  return 0;
}
