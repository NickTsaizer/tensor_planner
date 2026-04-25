# Core Concepts

Tensor Planner is easier to use once the vocabulary is clear.

## Domain

A domain is the rulebook. It describes what kinds of things exist and what
actions are possible.

A domain contains:

- object types,
- predicates,
- numeric functions,
- action schemas,
- limits.

Example domain idea:

```text
Types: Character, Location
Predicates: at(Character, Location), connected(Location, Location)
Action: move(who: Character, from: Location, to: Location)
```

The domain does not say which character or locations exist in one specific
problem. That belongs to the state.

## Object type

Types prevent invalid action arguments.

For example:

```text
at(Character, Location)
```

means this is valid:

```text
at(player, forest)
```

but this is invalid:

```text
at(forest, player)
```

The C API uses integer type IDs. C++, C#, and Jai wrappers map language-level
types onto those IDs.

## Predicate

A predicate is a named relation that can be true or false.

Examples:

```text
at(player, home)
connected(home, forest)
has(flint)
requires_tool(wood, flint_axe)
```

Predicates define the logical state of the world.

## Numeric function

The C API also supports numeric values attached to objects.

Use this when boolean facts are not enough, for example:

```text
energy(player) >= 10
wood_count(stockpile) += 1
```

Numeric values can be used in numeric preconditions and numeric effects.

## Action schema

An action schema is a template for possible actions.

Example:

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

The action schema is not a specific action yet. It becomes grounded when the
planner substitutes actual objects, such as:

```text
move(player, home, forest)
```

## State

A state is one concrete planning problem.

It contains:

- objects,
- true facts,
- numeric values,
- goals.

Example:

```text
Objects:
  player: Character
  home: Location
  forest: Location

Facts:
  at(player, home)
  connected(home, forest)

Goal:
  at(player, forest)
```

## Goal

A goal is a fact that must become true.

Goals are conjunctive: all goals must be true for the state to count as solved.

Example:

```text
has(diamond_pickaxe)
```

In the crafting example, this single goal forces the planner to gather resources,
craft intermediate tools, travel between locations, and finally craft the target
item.

## Candidate action

A candidate action is a grounded action the planner may try.

Action schema:

```text
move(who, from, to)
```

Candidate action:

```text
move(player, home, forest)
```

Candidate generation is bounded by `TP_Limits::max_candidates`.

## Plan step

A plan step is an action selected in the final solution path.

Wrappers map plan-step arguments back to language-level objects:

- C returns object IDs in `TP_Candidate_Action::args`.
- C++ returns typed pointers with `step.arg<T>("name")`.
- C# returns typed references with `step.Arg<T>("name")`.
- Jai returns typed pointers with `step_arg(step, "name", Type)`.

## Limits

Limits bound memory and search:

```text
max_objects
max_facts
max_goals
max_candidates
max_expansions
max_plan_length
```

If a problem cannot fit within the configured limits, solve or construction calls
can fail with a status such as `TP_STATUS_LIMIT_EXCEEDED`.
