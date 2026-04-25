# Tensor Planner Wiki

Welcome to the Tensor Planner wiki. This wiki explains what the library is, how
it works internally, which bindings are available, and how to use it from C,
C++, C#, Unity, and Jai.

## What is Tensor Planner?

Tensor Planner is a compact C++20 planning library. It solves problems where you
know:

- the objects in a world,
- the facts currently true about those objects,
- the actions that can change those facts,
- and the goal facts you want to make true.

The planner searches for an ordered action sequence that transforms the initial
state into a goal state.

In game terms, instead of hard-coding this:

> walk to forest → gather fiber → walk to river → gather flint → craft axe

you describe the rules:

- characters can move between connected locations,
- resources can be gathered at locations,
- recipes produce items when ingredients are available,
- the goal is `has(diamond_pickaxe)`.

Tensor Planner finds the sequence.

## Why use it?

Use Tensor Planner when behavior is easier to describe as rules and goals than
as hand-written step-by-step scripts.

Good fits:

- NPC goal planning,
- crafting chains,
- simulation tools,
- puzzle solving,
- tactical or logistics planning,
- AI/model experiments that need tensor-shaped planning data.

Avoid it for one-off behaviors where a direct script is simpler.

## Main features

- Typed objects, predicates, action parameters, facts, and goals.
- STRIPS-like actions: preconditions, add effects, delete effects.
- Numeric functions, numeric preconditions, and numeric effects in the C layer.
- Guided search with bounded candidate grounding.
- Optional external candidate scorer callback.
- Tensor exports for schema, problem state, candidates, and action graph.
- C ABI with explicit ownership.
- C++ fluent wrapper.
- C# wrapper and Unity package.
- Jai module with generated C bindings and type-first wrapper.

## Wiki pages

| Page | What it covers |
|------|----------------|
| [Getting Started](Getting-Started) | Build, test, package, and run smoke examples. |
| [Core Concepts](Core-Concepts) | Domains, objects, predicates, actions, states, goals, and plans. |
| [How the Planner Works](How-the-Planner-Works) | Grounding, guided search, scoring, tensors, and limits. |
| [C API](C-API) | Native ABI handles, solve flow, tensors, ownership. |
| [C++ Fluent API](Cpp-Fluent-API) | Type-safe C++ wrapper with examples. |
| [C# and Unity](CSharp-and-Unity) | Managed wrapper, Unity package, samples. |
| [Jai Bindings](Jai-Bindings) | Generated C bindings and Jai typed-domain wrapper. |
| [Examples](Examples) | Movement, Tower of Hanoi, crafting progression. |
| [Memory Ownership](Memory-Ownership) | Lifetime rules for C, C++, C#, Unity, and Jai. |
| [Packaging](Packaging) | `build.sh`, output layout, Linux/Windows artifacts. |
| [FAQ and Limitations](FAQ-and-Limitations) | Common questions, constraints, and current limitations. |

## Binding overview

| Binding | Best for | Main file |
|---------|----------|-----------|
| C API | Engines, FFI, language bindings | `include/tensor_planner.h` |
| C++ fluent API | Native gameplay/simulation code | `include/tensor_planner.hpp` |
| C# / Unity | Unity and .NET code | `dev.nick.tensor-planner/Runtime/TensorPlanner.cs` |
| Jai | Jai projects and compile-time schemas | `modules/Tensor_Planner/module.jai` |

## Minimal mental model

Tensor Planner answers this question:

> Given the facts that are true now, which actions can make my goal facts true?

You provide:

1. **Domain**: the rules of the world.
2. **State**: the current world instance.
3. **Goals**: facts that must become true.
4. **Limits**: search and memory bounds.

The result is:

1. `solved` flag,
2. solve metrics,
3. ordered plan steps,
4. action arguments mapped back to object IDs or real wrapper objects.

## Repository

Main repository: <https://github.com/NickTsaizer/tensor_planner>

Key folders:

```text
include/                    C API and C++ fluent wrapper
src/                        native implementation
tests/                      native examples and smoke tests
csharp/                     .NET wrapper project and smoke test
dev.nick.tensor-planner/    Unity package
modules/Tensor_Planner/     Jai module
```

Start with [Getting Started](Getting-Started) if you want to build and run it.
