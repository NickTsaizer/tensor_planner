#include "../include/tensor_planner.hpp"

#include <cassert>
#include <string>

namespace {

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
  Item flint_axe{"flint_axe"};
  Item wood{"wood"};
  Item stick{"stick"};
  Item stone{"stone"};
  Item stone_pickaxe{"stone_pickaxe"};
  Item iron_ore{"iron_ore"};
  Item iron_pickaxe{"iron_pickaxe"};
  Item diamond{"diamond"};
  Item diamond_pickaxe{"diamond_pickaxe"};
};

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
    .object(objects.flint_axe)
    .object(objects.wood)
    .object(objects.stick)
    .object(objects.stone)
    .object(objects.stone_pickaxe)
    .object(objects.iron_ore)
    .object(objects.iron_pickaxe)
    .object(objects.diamond)
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
    .fact(domain.requires_tool(objects.diamond, objects.iron_pickaxe))
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

}  // namespace

int main() {
  tp::Planner planner(tp::Limits{
    .max_objects = 32,
    .max_facts = 256,
    .max_goals = 8,
    .max_candidates = 512,
    .max_expansions = 20000,
    .max_plan_length = 64,
  });
  CraftingObjects objects;
  const CraftingDomain domain = build_domain(planner);
  const tp::StateBuilder state = build_state(planner, domain, objects);

  const tp::SolveResult result = planner.solve(state);
  assert(result.status() == TP_STATUS_OK);
  assert(result.solved());
  assert(!result.steps().empty());

  const tp::PlanStep &last_step = result.steps().back();
  assert(last_step.is(domain.craft3));
  assert(last_step.arg<Item>("item") == &objects.diamond_pickaxe);

  bool made_flint_axe = false;
  bool made_stone_pickaxe = false;
  bool made_iron_pickaxe = false;
  for (const tp::PlanStep &step : result.steps()) {
    if (step.is(domain.craft2) && step.arg<Item>("item") == &objects.flint_axe) {
      made_flint_axe = true;
    }
    if (step.is(domain.craft2) && step.arg<Item>("item") == &objects.stone_pickaxe) {
      made_stone_pickaxe = true;
    }
    if (step.is(domain.craft2) && step.arg<Item>("item") == &objects.iron_pickaxe) {
      made_iron_pickaxe = true;
    }
  }

  assert(made_flint_axe);
  assert(made_stone_pickaxe);
  assert(made_iron_pickaxe);
  return 0;
}
