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

struct GuidanceContext {
  int32_t gather_free_id = -1;
  int32_t gather_tool_id = -1;
  int32_t craft2_id = -1;
  int32_t craft3_id = -1;
  int32_t flint_id = -1;
  int32_t fiber_id = -1;
  int32_t wood_id = -1;
  int32_t stone_id = -1;
  int32_t iron_ore_id = -1;
  int32_t diamond_id = -1;
  int32_t flint_axe_id = -1;
  int32_t stick_id = -1;
  int32_t stone_pickaxe_id = -1;
  int32_t iron_pickaxe_id = -1;
  int32_t diamond_pickaxe_id = -1;
};

void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "crafting_guidance_experiment failed: " << message << '\n';
    std::exit(1);
  }
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
    .adds(domain.has("item"))
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
    .adds(domain.has("item"))
    .commit();

  return domain;
}

void add_edge(tp::StateBuilder &state, const CraftingDomain &domain, Location &a, Location &b) {
  state.fact(domain.connected(a, b));
  state.fact(domain.connected(b, a));
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

GuidanceContext build_guidance_context(
  const CraftingDomain &domain,
  const CraftingObjects &objects,
  const tp::StateBuilder &state
) {
  const auto id_of = [&state](auto &object) {
    return state.object_id(object);
  };

  return GuidanceContext{
    .gather_free_id = domain.gather_free.id,
    .gather_tool_id = domain.gather_tool.id,
    .craft2_id = domain.craft2.id,
    .craft3_id = domain.craft3.id,
    .flint_id = id_of(objects.flint),
    .fiber_id = id_of(objects.fiber),
    .wood_id = id_of(objects.wood),
    .stone_id = id_of(objects.stone),
    .iron_ore_id = id_of(objects.iron_ore),
    .diamond_id = id_of(objects.diamond),
    .flint_axe_id = id_of(objects.flint_axe),
    .stick_id = id_of(objects.stick),
    .stone_pickaxe_id = id_of(objects.stone_pickaxe),
    .iron_pickaxe_id = id_of(objects.iron_pickaxe),
    .diamond_pickaxe_id = id_of(objects.diamond_pickaxe),
  };
}

float score_target_item(int16_t item_id, const GuidanceContext &context) {
  if (item_id == context.diamond_pickaxe_id) return 5000.0f;
  if (item_id == context.iron_pickaxe_id) return 4200.0f;
  if (item_id == context.diamond_id) return 3800.0f;
  if (item_id == context.stone_pickaxe_id) return 3200.0f;
  if (item_id == context.stone_id) return 2800.0f;
  if (item_id == context.stick_id) return 2400.0f;
  if (item_id == context.wood_id) return 2200.0f;
  if (item_id == context.flint_axe_id) return 1800.0f;
  if (item_id == context.flint_id) return 1300.0f;
  if (item_id == context.fiber_id) return 1200.0f;
  if (item_id == context.iron_ore_id) return 1000.0f;
  return -250.0f;
}

void crafting_guidance_scorer(
  const TP_Schema_Tensors *,
  const TP_Problem_Tensors *problem,
  void *user_data,
  float *out_action_scores,
  float *,
  bool *out_has_state_value
) {
  *out_has_state_value = false;
  const GuidanceContext &context = *static_cast<const GuidanceContext *>(user_data);

  for (int32_t candidate_index = 0; candidate_index < problem->candidate_count; ++candidate_index) {
    const int32_t schema_id = problem->cand_action_schema[candidate_index];
    const int16_t *args = &problem->cand_action_arg[candidate_index * TP_MAX_PARAMS];
    float score = 0.0f;
    if (schema_id == context.gather_free_id || schema_id == context.gather_tool_id) {
      score = score_target_item(args[1], context);
    } else if (schema_id == context.craft2_id || schema_id == context.craft3_id) {
      score = score_target_item(args[1], context);
    }
    out_action_scores[candidate_index] = score;
  }
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
  constexpr std::array<int32_t, 6> distraction_counts = {0, 16, 32, 64, 128, 256};
  std::cout << "crafting guidance sequential distraction sweep\n";
  std::cout << "guided_expansion_cap=" << kGuidedExpansionCap << '\n';

  for (const int32_t distraction_count : distraction_counts) {
    const RunSummary guided = solve_crafting_case(distraction_count, kGuidedExpansionCap);
    print_summary(distraction_count, "default_goal_regression", guided);
    check(guided.status == TP_STATUS_OK, "default solve status");
    check(guided.solved, "default solve solved");

    std::cout << '\n';
    std::cout << std::flush;
  }
  return 0;
}
