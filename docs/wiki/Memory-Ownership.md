# Memory Ownership

Tensor Planner crosses native and managed boundaries, so lifetime rules matter.

## C API ownership

The C API uses explicit ownership.

Destroy handles you create:

```c
tp_solver_destroy(solver);
tp_state_destroy(state);
tp_domain_destroy(domain);
```

Dispose output structs filled by API calls:

```c
tp_schema_tensors_dispose(&schema);
tp_problem_tensors_dispose(&problem);
tp_action_graph_dispose(&graph);
tp_candidate_action_list_dispose(&candidates);
tp_solve_result_dispose(&result);
```

Rule of thumb:

- If the API returns a handle, call its matching destroy function.
- If the API fills an output struct containing pointers, call its matching
  dispose function.

## Borrowed scorer data

Scorer callbacks receive borrowed pointers:

```c
const TP_Schema_Tensors *schema
const TP_Problem_Tensors *problem
```

Do not store these pointers after the callback returns.

## Solve result ownership

`TP_Solve_Result` owns `plan_actions` after a successful solve call. Release it
with:

```c
tp_solve_result_dispose(&result);
```

Do not read `result.plan_actions` after disposal.

## C++ wrapper ownership

The C++ wrapper manages native domain and solver handles through `tp::Planner`.

However, state objects are borrowed from your application:

```cpp
auto state = planner.state()
  .object(player)
  .object(home)
  .fact(at(player, home));
```

Plan steps return pointers to those same objects:

```cpp
Character *who = step.arg<Character>("who");
```

Therefore:

- keep registered objects alive while reading the result,
- keep the state/result alive while using plan-step object pointers,
- do not register temporary objects.

Avoid:

```cpp
auto state = planner.state().object(Character{"temporary"}); // bad
```

Prefer:

```cpp
Character player{"player"};
auto state = planner.state().object(player); // good
```

## C# wrapper ownership

`Planner` implements `IDisposable`:

```csharp
using (Planner planner = new Planner()) {
    // use planner
}
```

`StateBuilder` stores managed object references for objects added with
`.Object(...)`. Plan steps return those same references:

```csharp
Character who = step.Arg<Character>("who");
```

Keep these alive while reading results:

- `Planner`,
- `StateBuilder`,
- `SolveResult`,
- application objects registered in the state.

## Unity plugin lifetime

Unity loads native plugins from the package's `Runtime/Plugins` folder. Do not
delete or replace the plugin while planner instances exist.

Native plugin paths:

```text
Runtime/Plugins/x86_64/libtensor_planner.so
Runtime/Plugins/x86_64/tensor_planner.dll
```

## Jai wrapper ownership

The Jai wrapper exposes explicit destroy helpers:

```jai
planner := planner_create(Crafting);
defer planner_destroy(*planner);

result := solve(*planner, *state);
defer solve_result_destroy(*result);
```

Objects registered with `object(*state, *value, name)` are borrowed pointers to
your Jai values. Keep those values alive while using plan-step pointers returned
by `step_arg`.

## Quick checklist

- C: always destroy handles and dispose output structs.
- C++: do not register temporaries; keep objects alive.
- C#: use `using`; keep state/result references alive while reading steps.
- Unity: make sure the native plugin exists for the target platform.
- Jai: use `defer planner_destroy` and `defer solve_result_destroy`.
