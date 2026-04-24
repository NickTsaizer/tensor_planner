#scope_export

Tensor_Planner_Builder :: struct {
    domain: *TP_Domain;
    state:  *TP_State;
}

copy_s32_prefix :: (dst: *[TP_MAX_PARAMS] s32, src: [] s32) {
    assert(src.count <= TP_MAX_PARAMS);
    for 0..src.count-1 {
        dst[it] = src[it];
    }
}

copy_s32_arity_prefix :: (dst: *[TP_MAX_ARITY] s32, src: [] s32) {
    assert(src.count <= TP_MAX_ARITY);
    for 0..src.count-1 {
        dst[it] = src[it];
    }
}

make_predicate_def :: (arity: u8, arg_types: [] s32) -> TP_Predicate_Def {
    result: TP_Predicate_Def;
    result.arity = arity;
    copy_s32_arity_prefix(*result.arg_types, arg_types);
    return result;
}

make_function_def :: (arity: u8, arg_types: [] s32) -> TP_Function_Def {
    result: TP_Function_Def;
    result.arity = arity;
    copy_s32_arity_prefix(*result.arg_types, arg_types);
    return result;
}

builder_create :: (limits: TP_Limits, object_types: [] s32) -> Tensor_Planner_Builder {
    result: Tensor_Planner_Builder;
    result.domain = tp_domain_create(*limits);
    assert(result.domain != null);
    result.state = tp_state_create(result.domain, object_types.count, object_types.data);
    assert(result.state != null);
    return result;
}

builder_destroy :: (builder: *Tensor_Planner_Builder) {
    if builder.state {
        tp_state_destroy(builder.state);
    }
    if builder.domain {
        tp_domain_destroy(builder.domain);
    }
    builder.state = null;
    builder.domain = null;
}

builder_add_predicate :: (
builder: *Tensor_Planner_Builder,
arity: u8,
arg_types: [] s32,
predicate_id_out: *s32
) -> TP_Status {
    predicate_def := make_predicate_def(arity, arg_types);
    return tp_domain_add_predicate(builder.domain, *predicate_def, predicate_id_out);
}

builder_add_function :: (
builder: *Tensor_Planner_Builder,
arity: u8,
arg_types: [] s32,
function_id_out: *s32
) -> TP_Status {
    function_def := make_function_def(arity, arg_types);
    return tp_domain_add_function(builder.domain, *function_def, function_id_out);
}

builder_add_action_schema :: (
builder: *Tensor_Planner_Builder,
arity: u8,
arg_types: [] s32,
preconditions: [] TP_Action_Literal,
effects: [] TP_Action_Effect,
numeric_preconditions: [] TP_Numeric_Precondition,
numeric_effects: [] TP_Numeric_Effect,
action_id_out: *s32
) -> TP_Status {
    local_arg_types: [TP_MAX_PARAMS] s32;
    copy_s32_prefix(*local_arg_types, arg_types);
    return tp_domain_add_action_schema(
    builder.domain,
    arity,
    local_arg_types.data,
    preconditions.data,
    preconditions.count,
    effects.data,
    effects.count,
    numeric_preconditions.data,
    numeric_preconditions.count,
    numeric_effects.data,
    numeric_effects.count,
    action_id_out,
);
}

builder_add_fact :: (
builder: *Tensor_Planner_Builder,
predicate_id: s32,
arity: u8,
args: [] s32
) -> TP_Status {
    return tp_state_add_fact(builder.state, predicate_id, arity, args.data);
}

builder_add_goal_fact :: (
builder: *Tensor_Planner_Builder,
predicate_id: s32,
arity: u8,
args: [] s32
) -> TP_Status {
    return tp_state_add_goal_fact(builder.state, predicate_id, arity, args.data);
}

builder_set_function_value :: (
builder: *Tensor_Planner_Builder,
function_id: s32,
arity: u8,
args: [] s32,
value: float
) -> TP_Status {
    return tp_state_set_function_value(builder.state, function_id, arity, args.data, value);
}

builder_generate_candidates :: (
builder: *Tensor_Planner_Builder,
max_candidates: s32,
out_candidates: *TP_Candidate_Action_List
) -> TP_Status {
    return tp_state_generate_candidates(builder.domain, builder.state, max_candidates, out_candidates);
}

builder_solve :: (
builder: *Tensor_Planner_Builder,
out_result: *TP_Solve_Result
) -> TP_Status {
    solver := tp_solver_create(builder.domain);
    assert(solver != null);
    defer tp_solver_destroy(solver);
    return tp_solver_solve(solver, builder.state, out_result);
}
