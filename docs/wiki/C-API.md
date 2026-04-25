# C API

The C API is the stable interoperability layer. Other bindings build on top of
it, and external engines can call it directly.

Main header:

```text
include/tensor_planner.h
```

## Handles

The API uses opaque handles:

```c
typedef struct TP_Domain TP_Domain;
typedef struct TP_State TP_State;
typedef struct TP_Solver TP_Solver;
```

Create/destroy pairs:

```c
TP_Domain *tp_domain_create(const TP_Limits *limits);
void tp_domain_destroy(TP_Domain *domain);

TP_State *tp_state_create(const TP_Domain *domain, int32_t object_count, const int32_t *object_types);
void tp_state_destroy(TP_State *state);

TP_Solver *tp_solver_create(const TP_Domain *domain);
void tp_solver_destroy(TP_Solver *solver);
```

## Limits

```c
typedef struct TP_Limits {
  int32_t max_objects;
  int32_t max_facts;
  int32_t max_goals;
  int32_t max_candidates;
  int32_t max_expansions;
  int32_t max_plan_length;
} TP_Limits;
```

Limits bound memory and search. Keep them tight for small problems and increase
them for branching domains such as crafting.

## Define predicates and functions

Predicates describe boolean facts. Functions describe numeric values.

```c
TP_Status tp_domain_add_predicate(
  TP_Domain *domain,
  const TP_Predicate_Def *definition,
  int32_t *out_predicate_id
);

TP_Status tp_domain_add_function(
  TP_Domain *domain,
  const TP_Function_Def *definition,
  int32_t *out_function_id
);
```

The C layer identifies object types with integer IDs chosen by the caller.

## Define actions

Actions are schemas with:

- typed parameters,
- boolean preconditions,
- boolean effects,
- numeric preconditions,
- numeric effects.

```c
TP_Status tp_domain_add_action_schema(
  TP_Domain *domain,
  uint8_t arity,
  const int32_t *arg_types,
  int32_t precondition_count,
  const TP_Action_Literal *preconditions,
  int32_t effect_count,
  const TP_Action_Effect *effects,
  int32_t numeric_precondition_count,
  const TP_Numeric_Precondition *numeric_preconditions,
  int32_t numeric_effect_count,
  const TP_Numeric_Effect *numeric_effects,
  int32_t *out_action_id
);
```

## Build state

State creation receives object type IDs:

```c
int32_t object_types[] = { CHARACTER_TYPE, LOCATION_TYPE, LOCATION_TYPE };
TP_State *state = tp_state_create(domain, 3, object_types);
```

Add facts and goals:

```c
int32_t at_args[] = { 0, 1 }; // object 0 at object 1
tp_state_add_fact(state, at_predicate_id, 2, at_args);

int32_t goal_args[] = { 0, 2 }; // object 0 should be at object 2
tp_state_add_goal_fact(state, at_predicate_id, 2, goal_args);
```

Set numeric values:

```c
tp_state_set_function_value(state, energy_function_id, 1, actor_args, 10.0f);
```

## Solve

```c
TP_Solver *solver = tp_solver_create(domain);

TP_Solve_Result result = {0};
TP_Status status = tp_solver_solve(solver, state, &result);

if (status == TP_STATUS_OK && result.solved) {
  for (int32_t i = 0; i < result.plan_length; ++i) {
    TP_Candidate_Action step = result.plan_actions[i];
    // step.schema_id identifies the action schema.
    // step.args contains object IDs.
  }
}

tp_solve_result_dispose(&result);
tp_solver_destroy(solver);
```

## Candidate generation and tensor exports

You can ask the state to generate candidates or export tensors:

```c
TP_Candidate_Action_List candidates = {0};
tp_state_generate_candidates(state, &candidates);
tp_candidate_action_list_dispose(&candidates);

TP_Schema_Tensors schema = {0};
tp_domain_export_schema_tensors(domain, &schema);
tp_schema_tensors_dispose(&schema);

TP_Problem_Tensors problem = {0};
tp_state_export_problem_tensors(state, &problem);
tp_problem_tensors_dispose(&problem);

TP_Action_Graph graph = {0};
tp_state_export_action_graph(state, &graph);
tp_action_graph_dispose(&graph);
```

## Status values

```c
typedef enum TP_Status {
  TP_STATUS_OK = 0,
  TP_STATUS_INVALID_ARGUMENT = 1,
  TP_STATUS_LIMIT_EXCEEDED = 2,
  TP_STATUS_NOT_FOUND = 3,
  TP_STATUS_UNSUPPORTED = 4,
  TP_STATUS_NO_SOLUTION = 5
} TP_Status;
```

Always check returned status values before reading output data.

## Ownership checklist

Destroy handles:

- `tp_domain_destroy`
- `tp_state_destroy`
- `tp_solver_destroy`

Dispose output structs:

- `tp_schema_tensors_dispose`
- `tp_problem_tensors_dispose`
- `tp_action_graph_dispose`
- `tp_candidate_action_list_dispose`
- `tp_solve_result_dispose`

Do not retain borrowed pointers passed into scorer callbacks.
