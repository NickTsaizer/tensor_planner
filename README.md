# Tensor Planner

Tensor Planner is a compact C-compatible C++20 planning library. It exposes a
typed planning domain, mutable problem states, tensor exports for model scoring,
and one guided planner that combines domain heuristics with optional external
candidate scores.

## Features

- C API with opaque `TP_Domain`, `TP_State`, and `TP_Solver` handles.
- C# fluent wrapper over the C ABI for .NET applications.
- Typed predicates, numeric functions, action preconditions, and effects.
- Guided search with bounded candidate grounding, heuristic prefiltering, and
  scorer-driven candidate ranking.
- Optional tensor exports for schema, state, candidates, and action graphs.
- Explicit memory ownership through `tp_*_dispose` and destroy functions.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The default build creates:

- `tensor_planner` — shared library
- `tensor_planner_solver_smoke` — core smoke test
- `tensor_planner_logistics_smoke` — guided logistics smoke test
- `tensor_planner_crafting_smoke` — guided crafting progression smoke test
- `tensor_planner_cpp_fluent_smoke` — typed C++ fluent wrapper smoke test

The C# wrapper is Unity-first. Its source lives in
`dev.nick.tensor-planner/Runtime/TensorPlanner.cs`; the NuGet-style project in
`csharp/TensorPlanner` links that same file. It can be checked with:

```bash
dotnet run --project csharp/TensorPlanner.Smoke/TensorPlanner.Smoke.csproj
```

## Quick Start

### C++ fluent wrapper

Use `tensor_planner.hpp` when you want typed game objects and readable schemas.
The wrapper keeps raw planner IDs internal and returns your object pointers from
plan steps.

```cpp
#include "tensor_planner.hpp"

struct Character { std::string name; };
struct Location { std::string name; };

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

auto result = planner.solve(state);
for (const auto &step : result.steps()) {
  if (step.is(move)) {
    Character *who = step.arg<Character>("who");
    Location *to = step.arg<Location>("to");
  }
}
```

Objects passed to `.object(...)`, `.fact(...)`, or `.goal(...)` are borrowed;
they must outlive the solve result if you read object pointers from plan steps.

### C# fluent wrapper

Use `dev.nick.tensor-planner` in Unity or `csharp/TensorPlanner` from regular
.NET applications. The C# API mirrors the C++ fluent wrapper while using C#
naming conventions and `IDisposable` for the native planner handle.

```csharp
using TensorPlanner;

sealed class Character { public Character(string name) { Name = name; } public string Name { get; } }
sealed class Location { public Location(string name) { Name = name; } public string Name { get; } }

var player = new Character("player");
var home = new Location("home");
var forest = new Location("forest");

using (Planner planner = new Planner()) {
  Predicate at = planner.Predicate<Character, Location>("at");
  Predicate connected = planner.Predicate<Location, Location>("connected");

  PlannerAction move = planner.Action("move")
    .Param<Character>("who")
    .Param<Location>("from")
    .Param<Location>("to")
    .Require(at.Call("who", "from"))
    .Require(connected.Call("from", "to"))
    .Removes(at.Call("who", "from"))
    .Adds(at.Call("who", "to"))
    .Commit();

  StateBuilder state = planner.State()
    .Object(player)
    .Object(home)
    .Object(forest)
    .Fact(at.Call(player, home))
    .Fact(connected.Call(home, forest))
    .Goal(at.Call(player, forest));

  SolveResult result = planner.Solve(state);
  foreach (PlanStep step in result.Steps) {
    if (step.Is(move)) {
      Character who = step.Arg<Character>("who");
      Location to = step.Arg<Location>("to");
    }
  }
}
```

Objects passed to `Object(...)`, `Fact(...)`, or `Goal(...)` are held by managed
reference in the state builder; keep the state/result alive while reading object
references from plan steps. The wrapper uses plain `DllImport("tensor_planner")`
for Unity compatibility. In Unity, place native plugins under
`dev.nick.tensor-planner/Runtime/Plugins` or import a generated package folder.

Unity package staging can be created with:

```bash
pwsh ./scripts/New-UnityPackage.ps1 -OutputDirectory ./dist/unity \
  -LinuxLibrary ./build/libtensor_planner.so
```

NuGet packages can include native runtime assets with:

```bash
pwsh ./scripts/New-NuGetPackage.ps1 -OutputDirectory ./dist/nuget \
  -LinuxLibrary ./build/libtensor_planner.so
```

### C API

```cpp
#include "tensor_planner.h"

TP_Limits limits {
  .max_objects = 32,
  .max_facts = 128,
  .max_goals = 16,
  .max_candidates = 512,
  .max_expansions = 4096,
  .max_plan_length = 64,
};

TP_Domain *domain = tp_domain_create(&limits);
// Add predicates, functions, and action schemas.

TP_State *state = tp_state_create(domain, object_count, object_types);
// Add initial facts, numeric values, and goals.

TP_Solver *solver = tp_solver_create(domain);
tp_solver_use_tensor_baseline_scorer(solver); // Optional.

TP_Solve_Result result {};
TP_Status status = tp_solver_solve(solver, state, &result);

if (status == TP_STATUS_OK && result.solved) {
  for (int32_t i = 0; i < result.plan_length; ++i) {
    const TP_Candidate_Action action = result.plan_actions[i];
    // Execute or inspect action.
  }
}

tp_solve_result_dispose(&result);
tp_solver_destroy(solver);
tp_state_destroy(state);
tp_domain_destroy(domain);
```

## Guided Scoring

The planner always runs the guided search path. A scorer is optional:

- Without a scorer, the planner uses the built-in domain heuristic and candidate
  prefiltering.
- `tp_solver_use_tensor_baseline_scorer` installs a small built-in demo scorer
  intended for examples with location-style goals. For production domains,
  prefer an application-provided scorer.
- `tp_solver_set_scorer` installs an application-provided callback.

Custom scorers receive schema tensors, problem tensors, and the grounded
candidate list. They write one score per candidate and may also return a state
value. Higher candidate scores make actions more preferred by the guided queue.
The tensor pointers are borrowed and valid only for the duration of the callback.
`out_action_scores` has `problem->candidate_count` entries and each entry should
be initialized by the callback. `schema` may be null if schema export failed
during solver creation, so callbacks should tolerate a null schema pointer.

```cpp
void score_candidates(
  const TP_Schema_Tensors *schema,
  const TP_Problem_Tensors *problem,
  void *user_data,
  float *out_action_scores,
  float *out_state_value,
  bool *out_has_state_value
) {
  *out_has_state_value = false;
  for (int32_t i = 0; i < problem->candidate_count; ++i) {
    out_action_scores[i] = 0.0f;
  }
}
```

## Memory Ownership

Callers own objects returned through API output structs:

- `tp_schema_tensors_dispose`
- `tp_problem_tensors_dispose`
- `tp_action_graph_dispose`
- `tp_candidate_action_list_dispose`
- `tp_solve_result_dispose`

Domains, states, and solvers are destroyed with their matching destroy function.

## Notes and Limitations

- This package intentionally exposes only the guided planner. Earlier debug and
  comparison planner variants were removed to keep the library surface focused.
- Candidate grounding is bounded by `TP_Limits::max_candidates`; increase limits
  for wider branching domains.
- Search is deterministic for a fixed domain, state, limits, and scorer output.
