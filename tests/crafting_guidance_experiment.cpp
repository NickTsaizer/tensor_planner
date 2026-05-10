#include "../src/planner_internal.hpp"

#include "../include/tensor_planner.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr int32_t kGuidedExpansionCap = 20000;

struct Agent {
  std::string name;
};

struct Item {
  std::string name;
};

struct Location {
  std::string name;
};

struct CraftingDomain {
  tp::Predicate at;
  tp::Predicate has;
  tp::Predicate connected;
  tp::Predicate gather_at;
  tp::Predicate home;
  tp::Predicate requires_tool;
  tp::Predicate gather_bare_hands;
  tp::Predicate recipe2_a;
  tp::Predicate recipe2_b;
  tp::Predicate recipe3_a;
  tp::Predicate recipe3_b;
  tp::Predicate recipe3_c;
  tp::Function count;

  tp::Action move;
  tp::Action gather_free;
  tp::Action gather_tool;
  tp::Action craft2;
  tp::Action craft3;
};

struct CraftingObjects {
  Agent agent{"agent"};
  Location home{"home"};
  Location forest{"forest"};
  Location river{"river"};
  Location cave{"cave"};
  Location deep_cave{"deep_cave"};

  Item flint{"flint"};
  Item fiber{"fiber"};
  Item wood{"wood"};
  Item stone{"stone"};
  Item iron_ore{"iron_ore"};
  Item diamond{"diamond"};
  std::vector<Item> distractions;
  Item flint_axe{"flint_axe"};
  Item stick{"stick"};
  Item stone_pickaxe{"stone_pickaxe"};
  Item iron_pickaxe{"iron_pickaxe"};
  Item diamond_pickaxe{"diamond_pickaxe"};

  explicit CraftingObjects(int32_t distraction_recipe_count) {
    distractions.reserve(static_cast<std::size_t>(distraction_recipe_count));
    for (int32_t index = 0; index < distraction_recipe_count; ++index) {
      distractions.push_back(Item{"distraction_recipe_" + std::to_string(index)});
    }
  }
};

void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "crafting_guidance_experiment failed: " << message << '\n';
    std::exit(1);
  }
}

struct NumericSlotHarness {
  TP_Domain *domain = nullptr;
  TP_State *state = nullptr;
  int32_t function_id = -1;
  int32_t action_id = -1;
  CandidateAction candidate {};
};

struct SignatureHarness {
  TP_Domain *domain = nullptr;
  int32_t predicate_id = -1;
  int32_t function_id = -1;
};

struct CandidateSelectionContext {
  int32_t goal_location_id = 2;
};

NumericSlotHarness build_numeric_slot_harness() {
  NumericSlotHarness harness;
  const TP_Limits limits {
    .max_objects = 8,
    .max_facts = 8,
    .max_goals = 4,
    .max_candidates = 64,
    .max_expansions = 32,
    .max_plan_length = 8,
  };
  harness.domain = tp_domain_create(&limits);
  check(harness.domain != nullptr, "numeric slot domain create");

  const TP_Function_Def count_function {
    .arity = 1,
    .arg_types = {0, 0, 0, 0},
  };
  check(tp_domain_add_function(harness.domain, &count_function, &harness.function_id) == TP_STATUS_OK,
        "numeric slot function add");

  const int32_t action_types[4] = {0, 0, 0, 0};
  const TP_Numeric_Precondition numeric_preconditions[1] = {
    {.function_id = harness.function_id, .cmp_op = TP_NUM_CMP_GTE, .arity = 1, .slots = {3, -1, -1, -1}, .rhs_value = 1.0f},
  };
  const TP_Numeric_Effect numeric_effects[1] = {
    {.function_id = harness.function_id, .op = TP_NUM_EFFECT_ADD, .arity = 1, .slots = {3, -1, -1, -1}, .rhs_value = 1.0f},
  };
  check(tp_domain_add_action_schema(
          harness.domain,
          4,
          action_types,
          nullptr,
          0,
          nullptr,
          0,
          numeric_preconditions,
          1,
          numeric_effects,
          1,
          &harness.action_id
        ) == TP_STATUS_OK,
        "numeric slot action add");

  const int32_t object_types[4] = {0, 0, 0, 0};
  harness.state = tp_state_create(harness.domain, 4, object_types);
  check(harness.state != nullptr, "numeric slot state create");
  const int32_t count_args[1] = {3};
  check(tp_state_set_function_value(harness.state, harness.function_id, 1, count_args, 2.0f) == TP_STATUS_OK,
        "numeric slot function value set");

  harness.candidate.schema_id = harness.action_id;
  harness.candidate.arity = 4;
  harness.candidate.args = {0, 1, 2, 3, -1, -1};
  return harness;
}

void destroy_numeric_slot_harness(NumericSlotHarness *harness) {
  if (harness->state != nullptr) {
    tp_state_destroy(harness->state);
    harness->state = nullptr;
  }
  if (harness->domain != nullptr) {
    tp_domain_destroy(harness->domain);
    harness->domain = nullptr;
  }
}

SignatureHarness build_signature_harness() {
  SignatureHarness harness;
  const TP_Limits limits {
    .max_objects = 4,
    .max_facts = 8,
    .max_goals = 2,
    .max_candidates = 8,
    .max_expansions = 8,
    .max_plan_length = 4,
  };
  harness.domain = tp_domain_create(&limits);
  check(harness.domain != nullptr, "signature domain create");

  const TP_Predicate_Def predicate {
    .arity = 1,
    .arg_types = {0, 0, 0, 0},
  };
  check(tp_domain_add_predicate(harness.domain, &predicate, &harness.predicate_id) == TP_STATUS_OK,
        "signature predicate add");

  const TP_Function_Def function_def {
    .arity = 1,
    .arg_types = {0, 0, 0, 0},
  };
  check(tp_domain_add_function(harness.domain, &function_def, &harness.function_id) == TP_STATUS_OK,
        "signature function add");
  return harness;
}

TP_State *build_signature_state(const SignatureHarness &harness, bool reverse_order, float second_value) {
  const int32_t object_types[2] = {0, 0};
  TP_State *state = tp_state_create(harness.domain, 2, object_types);
  check(state != nullptr, "signature state create");

  const int32_t object_zero[1] = {0};
  const int32_t object_one[1] = {1};

  if (reverse_order) {
    check(tp_state_add_fact(state, harness.predicate_id, 1, object_one) == TP_STATUS_OK, "signature add reversed fact one");
    check(tp_state_add_fact(state, harness.predicate_id, 1, object_zero) == TP_STATUS_OK, "signature add reversed fact zero");
    check(tp_state_set_function_value(state, harness.function_id, 1, object_one, second_value) == TP_STATUS_OK,
          "signature add reversed value one");
    check(tp_state_set_function_value(state, harness.function_id, 1, object_zero, 1.0f) == TP_STATUS_OK,
          "signature add reversed value zero");
  } else {
    check(tp_state_add_fact(state, harness.predicate_id, 1, object_zero) == TP_STATUS_OK, "signature add fact zero");
    check(tp_state_add_fact(state, harness.predicate_id, 1, object_one) == TP_STATUS_OK, "signature add fact one");
    check(tp_state_set_function_value(state, harness.function_id, 1, object_zero, 1.0f) == TP_STATUS_OK,
          "signature add value zero");
    check(tp_state_set_function_value(state, harness.function_id, 1, object_one, second_value) == TP_STATUS_OK,
          "signature add value one");
  }

  return state;
}

void destroy_signature_harness(SignatureHarness *harness) {
  if (harness->domain != nullptr) {
    tp_domain_destroy(harness->domain);
    harness->domain = nullptr;
  }
}

void hostile_candidate_selection_scorer(
  const TP_Schema_Tensors *,
  const TP_Problem_Tensors *problem,
  void *user_data,
  float *out_action_scores,
  float *,
  bool *out_has_state_value
) {
  *out_has_state_value = false;
  const CandidateSelectionContext &context = *static_cast<const CandidateSelectionContext *>(user_data);
  for (int32_t candidate_index = 0; candidate_index < problem->candidate_count; ++candidate_index) {
    const int32_t schema_id = problem->cand_action_schema[candidate_index];
    const int16_t *args = &problem->cand_action_arg[candidate_index * TP_MAX_PARAMS];
    if (schema_id == 0 && args[2] == context.goal_location_id) {
      out_action_scores[candidate_index] = -1000.0f;
    } else {
      out_action_scores[candidate_index] = 1000.0f;
    }
  }
}

TP_Domain *build_candidate_selection_domain(int32_t distraction_count) {
  const TP_Limits limits {
    .max_objects = distraction_count + 4,
    .max_facts = distraction_count + 8,
    .max_goals = 2,
    .max_candidates = distraction_count + 8,
    .max_expansions = 32,
    .max_plan_length = 2,
  };
  TP_Domain *domain = tp_domain_create(&limits);
  check(domain != nullptr, "candidate selection domain create");

  const TP_Predicate_Def at {
    .arity = 2,
    .arg_types = {0, 1, 0, 0},
  };
  int32_t predicate_id = -1;
  check(tp_domain_add_predicate(domain, &at, &predicate_id) == TP_STATUS_OK, "candidate selection at predicate add");

  const TP_Predicate_Def road {
    .arity = 2,
    .arg_types = {1, 1, 0, 0},
  };
  check(tp_domain_add_predicate(domain, &road, &predicate_id) == TP_STATUS_OK, "candidate selection road predicate add");

  const int32_t types[3] = {0, 1, 1};
  const TP_Action_Literal preconditions[2] = {
    {.predicate_id = 0, .sign = 1, .arity = 2, .slots = {0, 1, 0, 0}},
    {.predicate_id = 1, .sign = 1, .arity = 2, .slots = {1, 2, 0, 0}},
  };
  const TP_Action_Effect effects[2] = {
    {.predicate_id = 0, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 1, 0, 0}},
    {.predicate_id = 0, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 2, 0, 0}},
  };
  int32_t action_id = -1;
  check(tp_domain_add_action_schema(domain, 3, types, preconditions, 2, effects, 2, nullptr, 0, nullptr, 0, &action_id) == TP_STATUS_OK,
        "candidate selection action add");
  return domain;
}

TP_State *build_candidate_selection_state(const TP_Domain *domain, int32_t distraction_count, bool include_goal_road) {
  std::vector<int32_t> object_types(static_cast<std::size_t>(distraction_count + 3), 1);
  object_types[0] = 0;
  TP_State *state = tp_state_create(domain, static_cast<int32_t>(object_types.size()), object_types.data());
  check(state != nullptr, "candidate selection state create");

  const int32_t at_start[2] = {0, 1};
  check(tp_state_add_fact(state, 0, 2, at_start) == TP_STATUS_OK, "candidate selection add at start");

  if (include_goal_road) {
    const int32_t to_goal[2] = {1, 2};
    check(tp_state_add_fact(state, 1, 2, to_goal) == TP_STATUS_OK, "candidate selection add goal road");
  }

  for (int32_t offset = 0; offset < distraction_count; ++offset) {
    const int32_t to_decoy[2] = {1, offset + 3};
    check(tp_state_add_fact(state, 1, 2, to_decoy) == TP_STATUS_OK, "candidate selection add decoy road");
  }

  const int32_t goal_args[2] = {0, 2};
  check(tp_state_add_goal_fact(state, 0, 2, goal_args) == TP_STATUS_OK, "candidate selection add goal");
  return state;
}

void run_numeric_slot_regressions() {
  // Arrange
  NumericSlotHarness harness = build_numeric_slot_harness();

  // Act
  const ActionSchema &valid_schema = harness.domain->actions[static_cast<std::size_t>(harness.action_id)];

  // Assert
  check(action_schema_uses_valid_slots(valid_schema), "boundary numeric slot schema valid");
  check(action_is_applicable(*harness.domain, *harness.state, harness.candidate), "boundary numeric slot applicable");

  const std::vector<CandidateAction> valid_candidates = generate_candidate_actions(*harness.domain, *harness.state, 64);
  check(!valid_candidates.empty(), "boundary numeric slot candidates generated");
  for (const CandidateAction &candidate : valid_candidates) {
    check(candidate.args[3] == 3, "boundary numeric slot binds last parameter");
  }

  TP_Schema_Tensors tensors {};
  check(tp_domain_export_schema_tensors(harness.domain, &tensors) == TP_STATUS_OK, "numeric slot tensor export");
  check(tensors.num_pre_slot[0] == 3, "numeric precondition slot exported");
  check(tensors.num_pre_slot[1] == -1 && tensors.num_pre_slot[2] == -1 && tensors.num_pre_slot[3] == -1,
        "numeric precondition padding cleared");
  check(tensors.num_eff_slot[0] == 3, "numeric effect slot exported");
  check(tensors.num_eff_slot[1] == -1 && tensors.num_eff_slot[2] == -1 && tensors.num_eff_slot[3] == -1,
        "numeric effect padding cleared");
  tp_schema_tensors_dispose(&tensors);

  harness.domain->actions[static_cast<std::size_t>(harness.action_id)].numeric_preconditions[0].slots[0] = 4;
  check(!action_schema_uses_valid_slots(harness.domain->actions[static_cast<std::size_t>(harness.action_id)]),
        "invalid numeric precondition slot detected");
  check(!action_is_applicable(*harness.domain, *harness.state, harness.candidate),
        "invalid numeric precondition slot rejected");
  check(generate_candidate_actions(*harness.domain, *harness.state, 64).empty(),
        "invalid numeric precondition slot skipped during candidate generation");

  harness.domain->actions[static_cast<std::size_t>(harness.action_id)].numeric_preconditions[0].slots[0] = 3;
  harness.domain->actions[static_cast<std::size_t>(harness.action_id)].numeric_effects[0].slots[0] = 4;
  check(!action_schema_uses_valid_slots(harness.domain->actions[static_cast<std::size_t>(harness.action_id)]),
        "invalid numeric effect slot detected");
  const TP_State next_state = apply_action(*harness.domain, *harness.state, harness.candidate);
  FunctionValue query {};
  query.function_id = harness.function_id;
  query.arity = 1;
  query.args[0] = 3;
  float retained_value = 0.0f;
  check(try_get_function_value(next_state, query, &retained_value) && retained_value == 2.0f,
        "invalid numeric effect slot leaves state unchanged");

  destroy_numeric_slot_harness(&harness);
}

void run_signature_regressions() {
  // Arrange
  SignatureHarness harness = build_signature_harness();
  TP_State *ordered_state = build_signature_state(harness, false, 2.0f);
  TP_State *reversed_state = build_signature_state(harness, true, 2.0f);
  TP_State *changed_state = build_signature_state(harness, true, 3.0f);

  // Act
  bool rebuilt = false;
  const std::vector<int32_t> ordered_signature = make_state_signature(*ordered_state, &rebuilt);
  const bool ordered_rebuilt = rebuilt;
  rebuilt = false;
  const std::vector<int32_t> cached_signature = make_state_signature(*ordered_state, &rebuilt);
  const bool ordered_cached = !rebuilt;
  const std::vector<int32_t> reversed_signature = make_state_signature(*reversed_state, nullptr);
  const std::vector<int32_t> changed_signature = make_state_signature(*changed_state, nullptr);

  // Assert
  check(ordered_rebuilt, "signature cache builds on first request");
  check(ordered_cached, "signature cache reuses prior build");
  check(ordered_signature == cached_signature, "signature cache preserves content");
  check(ordered_signature == reversed_signature, "signature ignores insertion order");
  check(ordered_signature != changed_signature, "signature changes when numeric values change");

  tp_state_destroy(changed_state);
  tp_state_destroy(reversed_state);
  tp_state_destroy(ordered_state);
  destroy_signature_harness(&harness);
}

void run_candidate_selection_regressions() {
  constexpr int32_t distraction_count = 140;

  // Arrange
  CandidateSelectionContext context {};
  TP_Domain *domain = build_candidate_selection_domain(distraction_count);
  TP_State *solvable = build_candidate_selection_state(domain, distraction_count, true);
  TP_State *unsolvable = build_candidate_selection_state(domain, distraction_count, false);
  TP_Solver *solver = tp_solver_create(domain);
  check(solver != nullptr, "candidate selection solver create");
  check(tp_solver_set_custom_guidance(solver, hostile_candidate_selection_scorer, &context) == TP_STATUS_OK,
        "candidate selection set hostile guidance");

  // Act
  TP_Solve_Result solvable_result {};
  TP_Solve_Result unsolvable_result {};
  const TP_Status solvable_status = tp_solver_solve(solver, solvable, &solvable_result);
  const TP_Status unsolvable_status = tp_solver_solve(solver, unsolvable, &unsolvable_result);

  // Assert
  check(solvable_status == TP_STATUS_OK, "candidate selection solvable status");
  check(solvable_result.solved, "candidate selection keeps goal action despite hostile guidance");
  check(solvable_result.plan_length == 1, "candidate selection solves in one move");
  check(solvable_result.plan_actions[0].args[2] == context.goal_location_id,
        "candidate selection retains goal-directed move");
  check(unsolvable_status == TP_STATUS_NO_SOLUTION, "candidate selection unsolvable status");
  check(!unsolvable_result.solved, "candidate selection rejects missing goal transition");

  tp_solve_result_dispose(&unsolvable_result);
  tp_solve_result_dispose(&solvable_result);
  tp_solver_destroy(solver);
  tp_state_destroy(unsolvable);
  tp_state_destroy(solvable);
  tp_domain_destroy(domain);
}

CraftingDomain build_domain(tp::Planner &planner) {
  CraftingDomain domain {
    .at = planner.predicate<Agent, Location>("at"),
    .has = planner.predicate<Item>("has"),
    .connected = planner.predicate<Location, Location>("connected"),
    .gather_at = planner.predicate<Item, Location>("gather_at"),
    .home = planner.predicate<Location>("home"),
    .requires_tool = planner.predicate<Item, Item>("requires_tool"),
    .gather_bare_hands = planner.predicate<Item>("gather_bare_hands"),
    .recipe2_a = planner.predicate<Item, Item>("recipe2_a"),
    .recipe2_b = planner.predicate<Item, Item>("recipe2_b"),
    .recipe3_a = planner.predicate<Item, Item>("recipe3_a"),
    .recipe3_b = planner.predicate<Item, Item>("recipe3_b"),
    .recipe3_c = planner.predicate<Item, Item>("recipe3_c"),
    .count = planner.function<Item>("count"),
  };

  domain.move = planner.action("move")
    .param<Agent>("agent")
    .param<Location>("from")
    .param<Location>("to")
    .require(domain.at("agent", "from"))
    .require(domain.connected("from", "to"))
    .removes(domain.at("agent", "from"))
    .adds(domain.at("agent", "to"))
    .commit();

  domain.gather_free = planner.action("gather_free")
    .param<Agent>("agent")
    .param<Item>("item")
    .param<Location>("where")
    .require(domain.at("agent", "where"))
    .require(domain.gather_at("item", "where"))
    .require(domain.gather_bare_hands("item"))
    .adds(domain.has("item"))
    .increases(domain.count("item"), 1.0f)
    .commit();

  domain.gather_tool = planner.action("gather_tool")
    .param<Agent>("agent")
    .param<Item>("item")
    .param<Location>("where")
    .param<Item>("tool")
    .require(domain.at("agent", "where"))
    .require(domain.gather_at("item", "where"))
    .require(domain.requires_tool("item", "tool"))
    .require(domain.has("tool"))
    .adds(domain.has("item"))
    .increases(domain.count("item"), 1.0f)
    .commit();

  domain.craft2 = planner.action("craft2")
    .param<Agent>("agent")
    .param<Item>("item")
    .param<Location>("where")
    .param<Item>("first")
    .param<Item>("second")
    .require(domain.at("agent", "where"))
    .require(domain.home("where"))
    .require(domain.recipe2_a("item", "first"))
    .require(domain.recipe2_b("item", "second"))
    .require(domain.has("first"))
    .require(domain.has("second"))
    .require(domain.count("first") >= 1.0f)
    .require(domain.count("second") >= 1.0f)
    .adds(domain.has("item"))
    .decreases(domain.count("first"), 1.0f)
    .decreases(domain.count("second"), 1.0f)
    .increases(domain.count("item"), 1.0f)
    .commit();

  domain.craft3 = planner.action("craft3")
    .param<Agent>("agent")
    .param<Item>("item")
    .param<Location>("where")
    .param<Item>("first")
    .param<Item>("second")
    .param<Item>("third")
    .require(domain.at("agent", "where"))
    .require(domain.home("where"))
    .require(domain.recipe3_a("item", "first"))
    .require(domain.recipe3_b("item", "second"))
    .require(domain.recipe3_c("item", "third"))
    .require(domain.has("first"))
    .require(domain.has("second"))
    .require(domain.has("third"))
    .require(domain.count("first") >= 1.0f)
    .require(domain.count("second") >= 1.0f)
    .require(domain.count("third") >= 1.0f)
    .adds(domain.has("item"))
    .decreases(domain.count("first"), 1.0f)
    .decreases(domain.count("second"), 1.0f)
    .decreases(domain.count("third"), 1.0f)
    .increases(domain.count("item"), 1.0f)
    .commit();

  return domain;
}

void add_edge(tp::StateBuilder &state, const CraftingDomain &domain, Location &a, Location &b) {
  state.fact(domain.connected(a, b));
  state.fact(domain.connected(b, a));
}

void add_count_value(tp::StateBuilder &state, const CraftingDomain &domain, Item &item) {
  state.value(domain.count(item), 0.0f);
}

tp::StateBuilder build_state(tp::Planner &planner, const CraftingDomain &domain, CraftingObjects &objects) {
  auto state = planner.state()
    .object(objects.agent)
    .object(objects.home)
    .object(objects.forest)
    .object(objects.river)
    .object(objects.cave)
    .object(objects.deep_cave)
    .object(objects.flint)
    .object(objects.fiber)
    .object(objects.wood)
    .object(objects.stone)
    .object(objects.iron_ore)
    .object(objects.diamond);

  for (Item &distraction : objects.distractions) {
    state.object(distraction);
  }

  state
    .object(objects.flint_axe)
    .object(objects.stick)
    .object(objects.stone_pickaxe)
    .object(objects.iron_pickaxe)
    .object(objects.diamond_pickaxe)
    .fact(domain.at(objects.agent, objects.home))
    .fact(domain.home(objects.home));

  add_count_value(state, domain, objects.flint);
  add_count_value(state, domain, objects.fiber);
  add_count_value(state, domain, objects.wood);
  add_count_value(state, domain, objects.stone);
  add_count_value(state, domain, objects.iron_ore);
  add_count_value(state, domain, objects.diamond);
  for (Item &distraction : objects.distractions) {
    add_count_value(state, domain, distraction);
  }
  add_count_value(state, domain, objects.flint_axe);
  add_count_value(state, domain, objects.stick);
  add_count_value(state, domain, objects.stone_pickaxe);
  add_count_value(state, domain, objects.iron_pickaxe);
  add_count_value(state, domain, objects.diamond_pickaxe);

  add_edge(state, domain, objects.home, objects.forest);
  add_edge(state, domain, objects.forest, objects.river);
  add_edge(state, domain, objects.forest, objects.cave);
  add_edge(state, domain, objects.cave, objects.deep_cave);

  state
    .fact(domain.gather_at(objects.flint, objects.river))
    .fact(domain.gather_at(objects.fiber, objects.forest))
    .fact(domain.gather_at(objects.wood, objects.forest))
    .fact(domain.gather_at(objects.stone, objects.cave))
    .fact(domain.gather_at(objects.iron_ore, objects.cave))
    .fact(domain.gather_at(objects.diamond, objects.deep_cave))
    .fact(domain.gather_bare_hands(objects.flint))
    .fact(domain.gather_bare_hands(objects.fiber))
    .fact(domain.requires_tool(objects.wood, objects.flint_axe))
    .fact(domain.requires_tool(objects.stone, objects.flint_axe))
    .fact(domain.requires_tool(objects.iron_ore, objects.stone_pickaxe))
    .fact(domain.requires_tool(objects.diamond, objects.iron_pickaxe));

  for (Item &distraction : objects.distractions) {
    state
      .fact(domain.recipe2_a(distraction, objects.flint))
      .fact(domain.recipe2_b(distraction, objects.fiber));
  }

  state
    .fact(domain.recipe2_a(objects.flint_axe, objects.flint))
    .fact(domain.recipe2_b(objects.flint_axe, objects.fiber))
    .fact(domain.recipe2_a(objects.stick, objects.wood))
    .fact(domain.recipe2_b(objects.stick, objects.fiber))
    .fact(domain.recipe2_a(objects.stone_pickaxe, objects.stone))
    .fact(domain.recipe2_b(objects.stone_pickaxe, objects.stick))
    .fact(domain.recipe2_a(objects.iron_pickaxe, objects.iron_ore))
    .fact(domain.recipe2_b(objects.iron_pickaxe, objects.stick))
    .fact(domain.recipe3_a(objects.diamond_pickaxe, objects.diamond))
    .fact(domain.recipe3_b(objects.diamond_pickaxe, objects.stick))
    .fact(domain.recipe3_c(objects.diamond_pickaxe, objects.iron_pickaxe))
    .goal(domain.has(objects.diamond_pickaxe));

  return state;
}

void print_result(const char *label, const tp::SolveResult &result) {
  std::cout << label
            << ": expansions=" << result.expansions()
            << " generated=" << result.generated()
            << " scorer_calls=" << result.scorer_calls()
            << " plan_length=" << result.steps().size()
            << '\n';
}

struct RunSummary {
  TP_Status status = TP_STATUS_UNSUPPORTED;
  bool solved = false;
  int32_t expansions = 0;
  int32_t generated = 0;
  int32_t scorer_calls = 0;
  std::size_t plan_length = 0;
  int64_t wall_time_us = 0;
};

RunSummary solve_crafting_case(
  int32_t distraction_recipe_count,
  int32_t max_expansions
) {
  const int32_t max_candidates = std::max(512, distraction_recipe_count * 16 + 1024);
  tp::Planner planner(tp::Limits{
    .max_objects = distraction_recipe_count + 64,
    .max_facts = (distraction_recipe_count * 2) + 256,
    .max_goals = 8,
    .max_candidates = max_candidates,
    .max_expansions = max_expansions,
    .max_plan_length = 80,
  });
  CraftingObjects objects(distraction_recipe_count);
  const CraftingDomain domain = build_domain(planner);
  const tp::StateBuilder state = build_state(planner, domain, objects);

  const auto solve_start = std::chrono::steady_clock::now();
  const tp::SolveResult result = planner.solve(state);
  const auto solve_end = std::chrono::steady_clock::now();

  return RunSummary{
    .status = result.status(),
    .solved = result.solved(),
    .expansions = result.expansions(),
    .generated = result.generated(),
    .scorer_calls = result.scorer_calls(),
    .plan_length = result.steps().size(),
    .wall_time_us = std::chrono::duration_cast<std::chrono::microseconds>(solve_end - solve_start).count(),
  };
}

void print_summary(int32_t distraction_count, const char *label, const RunSummary &summary) {
  std::cout << "distractions=" << distraction_count
            << ' ' << label
            << " solved=" << (summary.solved ? "yes" : "no")
            << " status=" << summary.status
            << " expansions=" << summary.expansions
            << " generated=" << summary.generated
            << " scorer_calls=" << summary.scorer_calls
            << " plan_length=" << summary.plan_length
            << " wall_us=" << summary.wall_time_us
            << '\n';
  std::cout << std::flush;
}

}  // namespace

int main() {
  constexpr std::array<int32_t, 9> distraction_counts = {0, 16, 32, 64, 128, 256, 512, 1024, 2560};
  std::cout << "crafting guidance sequential distraction sweep\n";
  std::cout << "guided_expansion_cap=" << kGuidedExpansionCap << '\n';

  run_numeric_slot_regressions();
  run_signature_regressions();
  run_candidate_selection_regressions();

  for (const int32_t distraction_count : distraction_counts) {
    const RunSummary default_guided = solve_crafting_case(
      distraction_count,
      kGuidedExpansionCap
    );
    print_summary(distraction_count, "default_goal_regression", default_guided);
    check(default_guided.status == TP_STATUS_OK, "default solve status");
    check(default_guided.solved, "default solve solved");

    std::cout << '\n';
    std::cout << std::flush;
  }
  return 0;
}
