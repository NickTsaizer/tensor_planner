# Jai Bindings

Tensor Planner includes a Jai module with two layers:

1. generated C-level bindings from `include/tensor_planner.h`,
2. a type-first Jai wrapper for domains, states, solving, and typed plan-step
   arguments.

Main module:

```text
modules/Tensor_Planner/module.jai
```

## Module layout

```text
modules/Tensor_Planner/
├── module.jai            module entry point
├── generate.jai          binding generator script
├── generated/linux.jai   generated Linux C binding
├── platform.jai          platform selection
├── status.jai            status helpers
├── schema_spec.jai       compile-time typed domain API
├── runtime.jai           state, solving, result access
└── tests/crafting.jai    typed-object crafting example
```

## Imports

Examples use direct imports:

```jai
#import "Basic";
#import,dir "..";
```

## Generated C binding

`generate.jai` uses Jai's `Bindings_Generator` to generate the raw API from:

```text
include/tensor_planner.h
```

The generated Linux file links:

```jai
libtensor_planner :: #library "./libtensor_planner";
```

That means the native library must be present next to the generated binding for
compile/link use.

## Type-first wrapper

The Jai wrapper uses actual Jai `Type` values in domain definitions:

```jai
Agent :: struct { name: string; }
Item :: struct { name: string; }
Location :: struct { name: string; }

Crafting :: #run planner_domain(
    Type.[Agent, Item, Location],
    Predicate.[
        predicate("at", Agent, Location),
        predicate("has", Item),
        predicate("connected", Location, Location),
    ],
    Action.[
        action(
            "move",
            Param.[param("agent", Agent), param("from", Location), param("to", Location)],
            Atom.[atom("at", "agent", "from"), atom("connected", "from", "to")],
            Atom.[atom("at", "agent", "from")],
            Atom.[atom("at", "agent", "to")]
        ),
    ],
    default_limits()
);
```

The wrapper maps Jai types to planner type IDs at compile time.

## State and objects

Runtime states register real Jai objects:

```jai
planner := planner_create(Crafting);
defer planner_destroy(*planner);

agent := Agent.{"agent"};
home := Location.{"home"};
forest := Location.{"forest"};

state := state_begin(Crafting);
agent_ref := object(*state, *agent, agent.name);
home_ref := object(*state, *home, home.name);
forest_ref := object(*state, *forest, forest.name);

fact(*state, "at", agent_ref, home_ref);
edge(*state, "connected", home_ref, forest_ref);
goal(*state, "at", agent_ref, forest_ref);
```

`edge` is a helper that adds both directions for binary relation facts.

## Solve and read typed args

```jai
result := solve(*planner, *state);
defer solve_result_destroy(*result);

assert(result.raw.solved);

for 0..result.raw.plan_length - 1 {
    step := plan_step(result, cast(s32) it);
    if step_is(step, "move") {
        who := step_arg(step, "agent", Agent);
        to := step_arg(step, "to", Location);
    }
}
```

`step_arg(step, "name", Type)` returns a typed pointer.

## Crafting example

Path:

```text
modules/Tensor_Planner/tests/crafting.jai
```

The example models:

- movement between locations,
- free gathering,
- tool-gated gathering,
- two-input crafting,
- three-input crafting,
- final goal `has(diamond_pickaxe)`.

It validates that the final plan contains intermediate tool crafts and ends with
the expected final craft action.

## Running the example

```bash
cmake -S . -B build/Release-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release-linux --parallel
cp build/Release-linux/libtensor_planner.so modules/Tensor_Planner/generated/libtensor_planner.so
cp build/Release-linux/libtensor_planner.so modules/Tensor_Planner/libtensor_planner.so
jai modules/Tensor_Planner/tests/crafting.jai
modules/Tensor_Planner/tests/crafting
```

Expected log starts with:

```text
Tensor Planner Jai typed-object crafting example passed.
```

## Current notes

- Linux generated binding exists in the repository.
- `module.jai` has platform branches for Linux, Windows, and macOS generated
  bindings, but only generated files that exist in the repository can be loaded.
- The distribution script has Jai staging support, but Jai packaging should be
  verified when adding new generated platform files.
