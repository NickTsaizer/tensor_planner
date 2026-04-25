# C++ Fluent API

The C++ wrapper provides a typed, readable layer over the C ABI.

Main header:

```cpp
#include "tensor_planner.hpp"
```

Namespace:

```cpp
tp
```

## Why use the C++ wrapper?

Use it when you want:

- real C++ object types instead of integer type IDs,
- fluent action definitions,
- exceptions instead of manual status checks,
- plan-step arguments mapped back to typed object pointers.

## Minimal movement example

```cpp
#include "tensor_planner.hpp"

#include <string>

struct Character { std::string name; };
struct Location { std::string name; };

int main() {
  Character player{"player"};
  Location home{"home"};
  Location forest{"forest"};

  tp::Planner planner;

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
    .fact(at(player, home))
    .fact(connected(home, forest))
    .goal(at(player, forest));

  tp::SolveResult result = planner.solve(state);
  for (const tp::PlanStep &step : result.steps()) {
    if (step.is(move)) {
      Character *who = step.arg<Character>("who");
      Location *from = step.arg<Location>("from");
      Location *to = step.arg<Location>("to");
    }
  }
}
```

## Planner limits

Defaults are small and useful for examples:

```cpp
tp::Planner planner(tp::Limits{
  .max_objects = 32,
  .max_facts = 256,
  .max_goals = 8,
  .max_candidates = 512,
  .max_expansions = 20000,
  .max_plan_length = 64,
});
```

Increase `max_candidates`, `max_expansions`, and `max_plan_length` for wider
domains.

## Predicates

Create typed predicates with template arguments:

```cpp
auto at = planner.predicate<Character, Location>("at");
auto has = planner.predicate<Item>("has");
auto recipe = planner.predicate<Item, Item>("recipe2_a");
```

Use parameter names when defining action schemas:

```cpp
at("agent", "from")
```

Use real objects when defining state facts and goals:

```cpp
at(player, home)
```

The wrapper validates arity and object types.

## Actions

Action definitions are fluent:

```cpp
auto gather = planner.action("gather_free")
  .param<Agent>("agent")
  .param<Item>("item")
  .param<Location>("where")
  .require(at("agent", "where"))
  .require(gather_at("item", "where"))
  .require(gather_bare_hands("item"))
  .adds(has("item"))
  .commit();
```

Use `.removes(...)` for delete effects and `.adds(...)` for add effects.

## State builder

Register objects, then add facts and goals:

```cpp
auto state = planner.state()
  .object(agent)
  .object(home)
  .object(forest)
  .object(flint)
  .fact(at(agent, home))
  .fact(gather_at(flint, forest))
  .goal(has(flint));
```

Objects are borrowed. Keep them alive while using the solve result.

## Reading results

```cpp
tp::SolveResult result = planner.solve(state);

if (result.solved()) {
  for (const tp::PlanStep &step : result.steps()) {
    if (step.is(gather)) {
      Agent *agent = step.arg<Agent>("agent");
      Item *item = step.arg<Item>("item");
      Location *where = step.arg<Location>("where");
    }
  }
}
```

Useful result data:

- `result.solved()`
- `result.status()`
- `result.expansions()`
- `result.generated()`
- `result.steps()`

## Crafting example

The native crafting smoke test demonstrates a larger typed domain:

```text
tests/crafting_smoke.cpp
```

It models:

- movement through locations,
- free resource gathering,
- tool-gated resource gathering,
- two-ingredient recipes,
- three-ingredient recipes,
- final goal: `has(diamond_pickaxe)`.

This is the best C++ example to read after the movement smoke test.
