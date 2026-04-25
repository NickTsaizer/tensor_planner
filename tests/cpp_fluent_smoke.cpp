#include "../include/tensor_planner.hpp"

#include <cassert>
#include <string>

namespace {

struct Character {
  std::string name;
};

struct Location {
  std::string name;
};

}  // namespace

int main() {
  Character player{"player"};
  Location home{"home"};
  Location forest{"forest"};
  Location cave{"cave"};

  tp::Planner planner(tp::Limits{
    .max_objects = 8,
    .max_facts = 16,
    .max_goals = 4,
    .max_candidates = 32,
    .max_expansions = 128,
    .max_plan_length = 8,
  });

  auto at = planner.predicate<Character, Location>("at");
  auto connected = planner.predicate<Location, Location>("connected");

  auto move = planner.action("move")
    .param<Character>("who")
    .param<Location>("from")
    .param<Location>("to")
    .require(at("who", "from"))
    .require(connected("from", "to"))
    .removes(at("who", "from"))
    .adds(at("who", "to"))
    .commit();

  auto state = planner.state()
    .object(player)
    .object(home)
    .object(forest)
    .object(cave)
    .fact(at(player, home))
    .edge(connected, forest, home)
    .edge(connected, forest, cave)
    .goal(at(player, cave));

  const tp::SolveResult result = planner.solve(state);
  assert(result.status() == TP_STATUS_OK);
  assert(result.solved());
  assert(result.steps().size() == 2);

  const tp::PlanStep &step = result.steps()[0];
  assert(step.is(move));
  assert(step.name() == "move");
  assert(step.arg<Character>("who") == &player);
  assert(step.arg<Location>("from") == &home);
  assert(step.arg<Location>("to") == &forest);

  const tp::PlanStep &reverse_step = result.steps()[1];
  assert(reverse_step.is(move));
  assert(reverse_step.arg<Character>("who") == &player);
  assert(reverse_step.arg<Location>("from") == &forest);
  assert(reverse_step.arg<Location>("to") == &cave);
  return 0;
}
