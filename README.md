# Tensor Planner

Tensor Planner is a compact C++20 planning library for games, simulations, and
tooling. You describe a world as typed objects, facts, actions, and goals; the
library searches for a valid sequence of actions and returns a plan you can
execute in your own runtime.

It is designed around a small C ABI, with friendly bindings layered on top:

- **C API** for engine/runtime integrations and language bindings.
- **C++ fluent API** for strongly typed native projects.
- **C# / Unity API** for managed gameplay code and Unity packages.
- **Jai module** with generated C bindings plus a type-first Jai wrapper.

Full documentation lives in the GitHub Wiki:

- [Home](https://github.com/NickTsaizer/tensor_planner/wiki)
- [Getting Started](https://github.com/NickTsaizer/tensor_planner/wiki/Getting-Started)
- [Core Concepts](https://github.com/NickTsaizer/tensor_planner/wiki/Core-Concepts)
- [C API](https://github.com/NickTsaizer/tensor_planner/wiki/C-API)
- [C++ Fluent API](https://github.com/NickTsaizer/tensor_planner/wiki/Cpp-Fluent-API)
- [C# and Unity](https://github.com/NickTsaizer/tensor_planner/wiki/CSharp-and-Unity)
- [Jai Bindings](https://github.com/NickTsaizer/tensor_planner/wiki/Jai-Bindings)
- [Planning Patterns](https://github.com/NickTsaizer/tensor_planner/wiki/Planning-Patterns)

## Why use it?

Tensor Planner is useful when the desired outcome is clear, but the valid path
depends on current world state.

Instead of writing a separate script for every possible sequence, you define:

- what objects exist,
- what facts are true now,
- which actions can change those facts,
- and what goal facts must become true.

The planner searches the possible action space and returns a valid ordered plan.
That makes it useful for goal-directed agents, dependency-heavy systems,
simulation tooling, reachability checks, and model-assisted action ranking.

## Features

- Typed domains: predicates, numeric functions, action parameters, facts, values,
  and goals all carry type information.
- STRIPS-like action schemas with preconditions, add effects, and delete effects.
- Numeric preconditions and numeric effects for resources, costs, counters, and
  other scalar state.
- Guided search with bounded candidate grounding and deterministic behavior for
  fixed inputs.
- Optional scorer callback that receives tensor exports and ranks grounded
  candidate actions.
- Tensor exports for schema, problem state, candidates, and action graphs.
- Explicit memory ownership for native callers.
- Unity package structure with runnable scripts.
- Distribution script for C++, C#, Unity, and Jai artifacts.

## Quick start: C++ planning task

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
  auto energy = planner.function<Character>("energy");

  auto move = planner.action("move")
    .param<Character>("who")
    .param<Location>("from")
    .param<Location>("to")
    .require(at("who", "from"))
    .require(connected("from", "to"))
    .require(energy("who") >= 1.0f)
    .removes(at("who", "from"))
    .adds(at("who", "to"))
    .decreases(energy("who"), 1.0f)
    .commit();

  auto state = planner.state()
    .object(player)
    .object(home)
    .object(forest)
    .fact(at(player, home))
    .edge(connected, home, forest)
    .value(energy(player), 2.0f)
    .goal(at(player, forest));

  auto result = planner.solve(state);
  for (const tp::PlanStep &step : result.steps()) {
    if (step.is(move)) {
      Character *who = step.arg<Character>("who");
      Location *to = step.arg<Location>("to");
      // Execute: move who to destination.
    }
  }
}
```

The planner returns your real object pointers from plan steps. Objects passed to
the state are borrowed and must outlive the solve result.

## Quick start: C# / Unity

```csharp
using TensorPlanner;

sealed class Character { public Character(string name) { Name = name; } public string Name { get; } }
sealed class Location { public Location(string name) { Name = name; } public string Name { get; } }

Character player = new Character("player");
Location home = new Location("home");
Location forest = new Location("forest");

using (Planner planner = new Planner()) {
    Predicate at = planner.Predicate<Character, Location>("at");
    Predicate connected = planner.Predicate<Location, Location>("connected");
    NumericFunction energy = planner.Function<Character>("energy");

    PlannerAction move = planner.Action("move")
        .Param<Character>("who")
        .Param<Location>("from")
        .Param<Location>("to")
        .Require(at.Create("who", "from"))
        .Require(connected.Create("from", "to"))
        .Require(energy.Create("who").GreaterOrEqual(1.0f))
        .Removes(at.Create("who", "from"))
        .Adds(at.Create("who", "to"))
        .Decreases(energy.Create("who"), 1.0f)
        .Commit();

    StateBuilder state = planner.State()
        .Object(player)
        .Object(home)
        .Object(forest)
        .Fact(at.Create(player, home))
        .Edge(connected, home, forest)
        .Value(energy.Create(player), 2.0f)
        .Goal(at.Create(player, forest));

    SolveResult result = planner.Solve(state);
    foreach (PlanStep step in result.Steps) {
        if (step.Is(move)) {
            Character who = step.Arg<Character>("who");
            Location to = step.Arg<Location>("to");
        }
    }
}
```

For background solves in .NET or Unity tools, use the async wrapper:

```csharp
SolveResult result = await planner.SolveAsync(state, cancellationToken);
```

Cancellation is cooperative around the native solve call; it can cancel before
the solve starts or after it returns, but it cannot interrupt the current native
`tp_solver_solve` call mid-execution.

## Build and test

Requirements:

- CMake 3.20+
- C++20 compiler
- .NET SDK for C# wrapper validation
- Jai compiler for the Jai module and runnable snippets
- `x86_64-w64-mingw32-g++` only when cross-building Windows artifacts on Linux

Build distribution artifacts:

```bash
sh ./build.sh -release -target unity cpp sharp jai -o ./dist
```

On Windows, use the PowerShell equivalent:

```powershell
pwsh ./build.ps1 -release -os windows -target unity cpp sharp jai -o ./dist
```

Useful variants:

```bash
sh ./build.sh -target unity cpp -os linux windows -o ./dist
sh ./build.sh -debug -target cpp sharp -o ./dist -no-clean
```

```powershell
pwsh ./build.ps1 -target unity cpp -os windows -o ./dist
pwsh ./build.ps1 -debug -target cpp sharp -o ./dist -no-clean
```

Build native library and tests manually:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run the C# smoke test:

```bash
dotnet run --project csharp/TensorPlanner.Smoke/TensorPlanner.Smoke.csproj -c Release
```

## Repository layout

```text
include/                    C API and C++ fluent wrapper headers
src/                        Native planner implementation
tests/                      Native C++ smoke tests
csharp/TensorPlanner/       .NET project linking the Unity-first C# wrapper
csharp/TensorPlanner.Smoke/ C# smoke test
dev.nick.tensor-planner/    Unity package source and runnable scripts
modules/Tensor_Planner/     Jai generated binding workflow and wrapper
build.sh                    POSIX distribution build script
build.ps1                   PowerShell distribution build script for Windows
```

## Binding overview

| Binding | Best for | Entry point |
|---------|----------|-------------|
| C API | Engines, tools, other language bindings | `include/tensor_planner.h` |
| C++ fluent API | Typed C++ gameplay/simulation code | `include/tensor_planner.hpp` |
| C# / Unity | Unity gameplay code and .NET tools | `dev.nick.tensor-planner/Runtime/TensorPlanner.cs` |
| Jai | Jai modules and compile-time typed domains | `modules/Tensor_Planner/module.jai` |

## How it works, briefly

1. Define a domain: object types, predicates/functions, and action schemas.
2. Build a state: real objects, true facts, numeric values, and goals.
3. The planner grounds candidate actions from available objects and action
   schemas, filters impossible actions, then searches for a sequence that makes
   all goals true.
4. Optional scorer callbacks can rank candidates using exported tensors.
5. The result contains ordered plan steps and action arguments.

Read the full explanation in
[How the Planner Works](https://github.com/NickTsaizer/tensor_planner/wiki/How-the-Planner-Works).

## Memory ownership

Native API callers must dispose output structs and destroy handles explicitly:

- `tp_schema_tensors_dispose`
- `tp_problem_tensors_dispose`
- `tp_action_graph_dispose`
- `tp_candidate_action_list_dispose`
- `tp_solve_result_dispose`
- `tp_solver_destroy`
- `tp_state_destroy`
- `tp_domain_destroy`

The C++ and C# wrappers manage native handles, but object references in plan
steps still point back to application objects. Keep those objects and the state
alive while reading results.

## Notes and limitations

- Search is deterministic for a fixed domain, state, limits, and scorer output.
- Candidate grounding is bounded by `TP_Limits::max_candidates`.
- Action arity and predicate arity are bounded by `TP_MAX_PARAMS` and
  `TP_MAX_ARITY` in the C API.
- The public package intentionally exposes the guided planner path only.
- No license file is currently included in this repository.
