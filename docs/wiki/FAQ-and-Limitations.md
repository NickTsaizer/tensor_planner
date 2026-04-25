# FAQ and Limitations

## Is this GOAP?

Tensor Planner can be used for GOAP-like behavior, but the library is lower
level than a complete game AI framework. It provides typed planning primitives,
search, tensor exports, and bindings. You decide how plans map to game actions.

## Does it execute actions?

No. It returns a plan.

Your game/tool must execute each plan step and update its own world state.

## Are plans deterministic?

Yes, for fixed inputs:

- same domain,
- same state,
- same limits,
- same scorer output.

## What happens if there are too many possible actions?

Candidate generation is bounded by `max_candidates`. If your domain has a large
branching factor, increase limits or model actions with tighter types and
preconditions.

## What is the maximum predicate/action arity?

The C API defines:

```c
#define TP_MAX_ARITY 4
#define TP_MAX_PARAMS 6
```

So predicates/functions can have up to 4 arguments and actions can have up to 6
parameters.

## Does it support numeric values?

Yes in the C layer through numeric functions, numeric preconditions, and numeric
effects. Higher-level wrapper coverage may be narrower than the raw C API.

## Does it support custom scoring?

Yes. Use `tp_solver_set_scorer` to install a callback that receives tensor
exports and writes action scores.

There is also a built-in demo scorer:

```c
tp_solver_use_tensor_baseline_scorer(solver);
```

For production domains, prefer a scorer designed for your domain.

## Does it require machine learning?

No. Tensor exports and scoring exist for integration and experimentation, but the
planner works without an ML model.

## Why does the library expose tensor exports?

Tensor-shaped arrays are useful when you want to:

- rank candidates with a model,
- inspect planner data,
- build visualization tools,
- run experiments outside the game runtime.

## What platforms are packaged?

The package script supports:

- Linux native builds,
- Windows cross-builds from Linux with MinGW.

Unity native plugin staging supports Linux and Windows x86_64 paths.

## What bindings exist?

- C API
- C++ fluent API
- C# / Unity wrapper
- Jai generated binding plus typed wrapper

## Is there a license?

No license file is currently included in the repository. Until a license is
added, treat usage rights as unspecified.

## Current limitations

- Guided planner path only; older debug/comparison planner variants are not part
  of the public package.
- Search is bounded by configured limits.
- Predicate arity and action parameter counts are fixed by C API constants.
- High-level wrappers do not expose every low-level C feature equally.
- Jai packaging/platform coverage should be verified when adding non-Linux
  generated bindings.

## Troubleshooting

### C# cannot load `tensor_planner`

Make sure the native library is in the runtime search path or staged as a native
runtime asset. In Unity, place it under `Runtime/Plugins/x86_64/`.

### Planner returns no solution

Check:

1. Are all required initial facts present?
2. Are action preconditions too strict?
3. Are effects adding the goal facts?
4. Are object types correct?
5. Are `max_candidates`, `max_expansions`, and `max_plan_length` large enough?

### Jai example cannot link native library

Copy the native library to the module locations expected by the generated
binding and test:

```bash
cp build/Release-linux/libtensor_planner.so modules/Tensor_Planner/generated/libtensor_planner.so
cp build/Release-linux/libtensor_planner.so modules/Tensor_Planner/libtensor_planner.so
```
