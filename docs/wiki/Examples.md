# Examples

This page shows what Tensor Planner can express.

## Example 1: one-step movement

Problem:

```text
player is at home
home is connected to forest
```

Domain:

```text
move(who, from, to)
requires:
  at(who, from)
  connected(from, to)
removes:
  at(who, from)
adds:
  at(who, to)
```

Expected plan:

```text
move(player, home, forest)
```

Read:

- `tests/cpp_fluent_smoke.cpp`
- `csharp/TensorPlanner.Smoke/Program.cs`
- `dev.nick.tensor-planner/Samples~/Basic Usage/BasicUsage.cs`

## Example 2: Tower of Hanoi

Problem:

- three discs start stacked on the left peg,
- goal is the same stack on the right peg,
- larger discs cannot move onto smaller discs.

The Unity sample models both pegs and discs as `Support` objects.

Predicates:

```text
on(Support, Support)
clear(Support)
smaller(Support, Support)
```

Action:

```text
move(disc, from, to)
requires:
  on(disc, from)
  clear(disc)
  clear(to)
  smaller(disc, to)
removes:
  on(disc, from)
  clear(to)
adds:
  on(disc, to)
  clear(from)
```

The sample logs human-readable moves:

```text
move small disc off left peg onto right peg
move medium disc off large disc onto middle peg
...
```

Read:

```text
dev.nick.tensor-planner/Samples~/Tower of Hanoi/HanoiTower.cs
```

## Example 3: crafting progression

Goal:

```text
has(diamond_pickaxe)
```

World:

- locations: home, forest, river, cave, deep cave,
- resources: flint, fiber, wood, stone, iron ore, diamond,
- tools: flint axe, stone pickaxe, iron pickaxe,
- final item: diamond pickaxe.

Predicates include:

```text
at(agent, location)
has(item)
connected(location, location)
gather_at(item, location)
requires_tool(item, tool)
gather_bare_hands(item)
recipe2_a(item, ingredient)
recipe2_b(item, ingredient)
recipe3_a(item, ingredient)
recipe3_b(item, ingredient)
recipe3_c(item, ingredient)
```

Actions:

- `move`
- `gather_free`
- `gather_tool`
- `craft2`
- `craft3`

This forces the planner to discover intermediate dependencies. To make a diamond
pickaxe, it must make prerequisite tools first.

Read:

- `tests/crafting_smoke.cpp`
- `modules/Tensor_Planner/tests/crafting.jai`

## Example 4: logistics-style planning

The logistics smoke test demonstrates another classic planning shape: moving
objects through a small network with constraints.

Read:

```text
tests/logistics_smoke.cpp
```

## Pattern: model actions as rules, not scripts

Bad fit:

```text
if no axe:
  get flint
  get fiber
  craft axe
if no pickaxe:
  get stone
  get stick
  craft pickaxe
...
```

Good fit:

```text
gather_free(item) requires gather_bare_hands(item)
gather_tool(item, tool) requires has(tool)
craft2(item, a, b) requires has(a), has(b)
craft3(item, a, b, c) requires has(a), has(b), has(c)
goal has(diamond_pickaxe)
```

The planner decides which sequence satisfies the goal.
