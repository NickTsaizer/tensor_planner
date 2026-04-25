# How the Planner Works

This page describes the high-level solve pipeline. It is intentionally written
for library users, not as a line-by-line implementation reference.

## Solve pipeline

At solve time, Tensor Planner does this:

1. Reads the domain schema.
2. Reads the current state and goals.
3. Generates grounded candidate actions from action schemas and state objects.
4. Filters candidates that cannot apply.
5. Scores or ranks candidates.
6. Searches forward through states until all goals are true or limits are hit.
7. Returns an ordered list of actions.

## Grounding

Grounding means turning a parameterized action into concrete actions.

Schema:

```text
move(who: Character, from: Location, to: Location)
```

Objects:

```text
player: Character
home: Location
forest: Location
river: Location
```

Possible grounded actions include:

```text
move(player, home, forest)
move(player, forest, river)
move(player, river, home)
```

The type system prevents nonsensical candidates such as:

```text
move(home, player, forest)
```

## Preconditions and effects

A grounded action can apply only when its preconditions are true.

For `move(player, home, forest)`:

```text
requires:
  at(player, home)
  connected(home, forest)
```

When applied, delete effects are removed and add effects are inserted:

```text
removes:
  at(player, home)
adds:
  at(player, forest)
```

## Guided search

Tensor Planner exposes one focused planner path: guided search.

The guided planner combines:

- domain-level checks,
- heuristic prefiltering,
- optional scorer output,
- bounded expansion limits.

Without a custom scorer, it still uses the built-in logic to search for a plan.
With a scorer, user code can influence which candidate actions are preferred.

## Scorer callback

The C API scorer signature is:

```c
typedef void (*TP_Score_Candidates_Fn)(
  const TP_Schema_Tensors *schema,
  const TP_Problem_Tensors *problem,
  void *user_data,
  float *out_action_scores,
  float *out_state_value,
  bool *out_has_state_value
);
```

The callback receives borrowed tensor data and writes one score per candidate.
Higher scores make candidates more preferred by the guided queue.

Important rules:

- `problem->candidate_count` tells you how many scores to write.
- Initialize every `out_action_scores[i]`.
- `schema` can be null if schema export failed during solver setup.
- Tensor pointers are borrowed and valid only during the callback.

Minimal scorer:

```c
void score_candidates(
  const TP_Schema_Tensors *schema,
  const TP_Problem_Tensors *problem,
  void *user_data,
  float *out_action_scores,
  float *out_state_value,
  bool *out_has_state_value
) {
  (void)schema;
  (void)user_data;
  *out_has_state_value = false;
  for (int32_t i = 0; i < problem->candidate_count; ++i) {
    out_action_scores[i] = 0.0f;
  }
}
```

## Tensor exports

Tensor Planner can export planner data as flat arrays.

Main export structs:

- `TP_Schema_Tensors`: predicates, functions, actions, preconditions, effects.
- `TP_Problem_Tensors`: objects, current facts, numeric values, goals,
  candidates.
- `TP_Action_Graph`: graph-like representation of objects/facts/actions.

These exports are useful for:

- ML model scoring,
- debugging planner inputs,
- offline analysis,
- visualization tools.

## Determinism

Search is deterministic for fixed inputs:

- same domain,
- same state,
- same limits,
- same scorer output.

This makes smoke tests and examples predictable.

## Limits and failure modes

The planner is intentionally bounded. Important limits:

- `max_objects`
- `max_facts`
- `max_goals`
- `max_candidates`
- `max_expansions`
- `max_plan_length`

Common failure statuses:

- `TP_STATUS_INVALID_ARGUMENT`: an input was invalid.
- `TP_STATUS_LIMIT_EXCEEDED`: configured limits were too small.
- `TP_STATUS_NOT_FOUND`: referenced schema/object data was missing.
- `TP_STATUS_UNSUPPORTED`: requested operation is not supported.
- `TP_STATUS_NO_SOLUTION`: search completed without a valid plan.

If a valid plan exists but the planner reports no solution, increase candidate,
expansion, or plan-length limits first.
