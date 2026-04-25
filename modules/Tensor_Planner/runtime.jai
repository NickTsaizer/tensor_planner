#scope_export;

Planner :: struct {
    domain: *TP_Domain;
    solver: *TP_Solver;
    limits: TP_Limits;
}

Object_Ref :: struct {
    id: s32;
}

Object_Record :: struct {
    ptr: *void;
    jai_type: Type;
    native_type: s32;
    name: string;
}

Runtime_Atom :: struct {
    predicate_id: s32;
    arity: u8;
    args: [TP_MAX_ARITY] s32;
}

Runtime_Value :: struct {
    function_id: s32;
    arity: u8;
    args: [TP_MAX_ARITY] s32;
    value: float;
}

State :: struct {
    domain: Planner_Domain;
    objects: [..] Object_Record;
    facts: [..] Runtime_Atom;
    goals: [..] Runtime_Atom;
    values: [..] Runtime_Value;
}

Solve_Result :: struct {
    raw: TP_Solve_Result;
    state: *State;
    domain: Planner_Domain;
}

Plan_Step :: struct {
    raw: TP_Candidate_Action;
    state: *State;
    domain: Planner_Domain;
}

planner_create :: (domain: Planner_Domain) -> Planner {
    result: Planner;
    result.limits = domain.limits;
    result.domain = tp_domain_create(*domain.limits);
    assert(result.domain != null);
    install_domain(*result, domain);
    result.solver = tp_solver_create(result.domain);
    assert(result.solver != null);
    return result;
}

planner_destroy :: (planner: *Planner) {
    if planner.solver {
        tp_solver_destroy(planner.solver);
        planner.solver = null;
    }
    if planner.domain {
        tp_domain_destroy(planner.domain);
        planner.domain = null;
    }
}

state_begin :: (domain: Planner_Domain) -> State {
    result: State;
    result.domain = domain;
    return result;
}

object :: (state: *State, value: *$T, name := "") -> Object_Ref {
    ptr := cast(*void) value;
    for 0..state.objects.count - 1 {
        if state.objects[it].ptr == ptr return .{ id = cast(s32) it };
    }

    native_type := type_id(state.domain, T);
    assert(native_type >= 0);

    id := cast(s32) state.objects.count;
    array_add(*state.objects, .{
        ptr = ptr,
        jai_type = T,
        native_type = native_type,
        name = name,
    });
    return .{ id = id };
}

fact :: (state: *State, predicate_name: string, args: ..Object_Ref) {
    atom := make_runtime_atom(state, predicate_name, args);
    array_add(*state.facts, atom);
}

goal :: (state: *State, predicate_name: string, args: ..Object_Ref) {
    atom := make_runtime_atom(state, predicate_name, args);
    array_add(*state.goals, atom);
}

value :: (state: *State, function_name: string, amount: float, args: ..Object_Ref) {
    numeric_value := make_runtime_value(state, function_name, amount, args);
    array_add(*state.values, numeric_value);
}

edge :: (state: *State, predicate_name: string, a: Object_Ref, b: Object_Ref) {
    fact(state, predicate_name, a, b);
    fact(state, predicate_name, b, a);
}

solve :: (planner: *Planner, state: *State) -> Solve_Result {
    object_types: [..] s32;
    for state.objects array_add(*object_types, it.native_type);

    raw_state := tp_state_create(planner.domain, cast(s32) object_types.count, object_types.data);
    assert(raw_state != null);
    defer tp_state_destroy(raw_state);

    for state.facts {
        assert_ok(tp_state_add_fact(raw_state, it.predicate_id, it.arity, it.args.data), "fact");
    }
    for state.goals {
        assert_ok(tp_state_add_goal_fact(raw_state, it.predicate_id, it.arity, it.args.data), "goal");
    }
    for state.values {
        assert_ok(tp_state_set_function_value(raw_state, it.function_id, it.arity, it.args.data, it.value), "value");
    }

    result: Solve_Result;
    result.state = state;
    result.domain = state.domain;
    assert_ok(tp_solver_solve(planner.solver, raw_state, *result.raw), "solve");
    return result;
}

solve_result_destroy :: (result: *Solve_Result) {
    tp_solve_result_dispose(*result.raw);
}

step_count :: (result: Solve_Result) -> s32 {
    return result.raw.plan_length;
}

plan_step :: (result: Solve_Result, index: s32) -> Plan_Step {
    assert(index >= 0);
    assert(index < result.raw.plan_length);
    return .{ raw = result.raw.plan_actions[index], state = result.state, domain = result.domain };
}

step_is :: (step: Plan_Step, action_name: string) -> bool {
    return step.raw.schema_id == action_id(step.domain, action_name);
}

step_arg :: (step: Plan_Step, param_name: string, $T: Type) -> *T {
    action_def := step.domain.actions[step.raw.schema_id];
    index := param_id(action_def, param_name);
    assert(index >= 0);
    assert(action_def.params[index].jai_type == T);

    object_id := step.raw.args[index];
    assert(object_id >= 0);
    assert(object_id < step.state.objects.count);
    record := step.state.objects[object_id];
    assert(record.jai_type == T);
    return cast(*T) record.ptr;
}

log_plan :: (result: Solve_Result) {
    log("Plan length: %", result.raw.plan_length);
    if result.raw.plan_length == 0 return;
    for 0..result.raw.plan_length - 1 log_step(plan_step(result, cast(s32) it), it + 1);
}

install_domain :: (planner: *Planner, domain: Planner_Domain) {
    install_predicates(planner, domain);
    install_functions(planner, domain);
    install_actions(planner, domain);
}

install_predicates :: (planner: *Planner, domain: Planner_Domain) {
    for 0..domain.predicates.count - 1 {
        pred := domain.predicates[it];
        raw: TP_Predicate_Def;
        raw.arity = pred.arity;
        if pred.arity > 0 {
            for arg_index: 0..pred.arity - 1 raw.arg_types[arg_index] = type_id(domain, pred.arg_types[arg_index]);
        }
        id: s32 = -1;
        assert_ok(tp_domain_add_predicate(planner.domain, *raw, *id), "install predicate");
        assert(id == it);
    }
}

install_functions :: (planner: *Planner, domain: Planner_Domain) {
    for 0..domain.functions.count - 1 {
        fun := domain.functions[it];
        raw: TP_Function_Def;
        raw.arity = fun.arity;
        if fun.arity > 0 {
            for arg_index: 0..fun.arity - 1 raw.arg_types[arg_index] = type_id(domain, fun.arg_types[arg_index]);
        }
        id: s32 = -1;
        assert_ok(tp_domain_add_function(planner.domain, *raw, *id), "install function");
        assert(id == it);
    }
}

install_actions :: (planner: *Planner, domain: Planner_Domain) {
    for 0..domain.actions.count - 1 {
        action_def := domain.actions[it];
        action_types: [TP_MAX_PARAMS] s32;
        if action_def.params.count > 0 {
            for param_index: 0..action_def.params.count - 1 {
                action_types[param_index] = type_id(domain, action_def.params[param_index].jai_type);
            }
        }

        preconditions := make_literals(domain, action_def, action_def.requires);
        effects := make_effects(domain, action_def);
        numeric_preconditions := make_numeric_preconditions(domain, action_def);
        numeric_effects := make_numeric_effects(domain, action_def);
        id: s32 = -1;
        assert_ok(
        tp_domain_add_action_schema(
        planner.domain,
        cast(u8) action_def.params.count,
        action_types.data,
        preconditions.data,
        cast(s32) preconditions.count,
        effects.data,
        cast(s32) effects.count,
        numeric_preconditions.data,
        cast(s32) numeric_preconditions.count,
        numeric_effects.data,
        cast(s32) numeric_effects.count,
        *id
    ),
    "install action"
);
assert(id == it);
}
}

make_runtime_value :: (state: *State, function_name: string, amount: float, refs: [] Object_Ref) -> Runtime_Value {
    fun_id := function_id(state.domain, function_name);
    assert(fun_id >= 0);
    fun := state.domain.functions[fun_id];
    assert(refs.count == fun.arity);

    result: Runtime_Value;
    result.function_id = fun_id;
    result.arity = fun.arity;
    result.value = amount;
    if refs.count > 0 {
        for 0..refs.count - 1 {
            record := state.objects[refs[it].id];
            assert(record.jai_type == fun.arg_types[it]);
            result.args[it] = refs[it].id;
        }
    }
    return result;
}

make_runtime_atom :: (state: *State, predicate_name: string, refs: [] Object_Ref) -> Runtime_Atom {
    pred_id := predicate_id(state.domain, predicate_name);
    assert(pred_id >= 0);
    pred := state.domain.predicates[pred_id];
    assert(refs.count == pred.arity);

    result: Runtime_Atom;
    result.predicate_id = pred_id;
    result.arity = pred.arity;
    if refs.count > 0 {
        for 0..refs.count - 1 {
            record := state.objects[refs[it].id];
            assert(record.jai_type == pred.arg_types[it]);
            result.args[it] = refs[it].id;
        }
    }
    return result;
}

make_literals :: (domain: Planner_Domain, action_def: Action, atoms: [] Atom) -> [..] TP_Action_Literal {
    result: [..] TP_Action_Literal;
    for atoms {
        literal: TP_Action_Literal;
        literal.predicate_id = predicate_id(domain, it.predicate_name);
        literal.sign = 1;
        literal.arity = it.arity;
        fill_slots(*literal.slots, action_def, it);
        array_add(*result, literal);
    }
    return result;
}

make_effects :: (domain: Planner_Domain, action_def: Action) -> [..] TP_Action_Effect {
    result: [..] TP_Action_Effect;
    for action_def.removes array_add(*result, make_effect(domain, action_def, it, .DELETE));
    for action_def.adds    array_add(*result, make_effect(domain, action_def, it, .ADD));
    return result;
}

make_effect :: (domain: Planner_Domain, action_def: Action, atom_def: Atom, op: TP_Effect_Op) -> TP_Action_Effect {
    result: TP_Action_Effect;
    result.predicate_id = predicate_id(domain, atom_def.predicate_name);
    result.op = cast(u8) op;
    result.arity = atom_def.arity;
    fill_slots(*result.slots, action_def, atom_def);
    return result;
}

make_numeric_preconditions :: (domain: Planner_Domain, action_def: Action) -> [..] TP_Numeric_Precondition {
    result: [..] TP_Numeric_Precondition;
    for action_def.numeric_requires {
        raw: TP_Numeric_Precondition;
        raw.function_id = function_id(domain, it.term.function_name);
        raw.cmp_op = cast(u8) it.op;
        raw.arity = it.term.arity;
        raw.rhs_value = it.value;
        fill_numeric_slots(*raw.slots, action_def, it.term);
        array_add(*result, raw);
    }
    return result;
}

make_numeric_effects :: (domain: Planner_Domain, action_def: Action) -> [..] TP_Numeric_Effect {
    result: [..] TP_Numeric_Effect;
    for action_def.numeric_effects {
        raw: TP_Numeric_Effect;
        raw.function_id = function_id(domain, it.term.function_name);
        raw.op = cast(u8) it.op;
        raw.arity = it.term.arity;
        raw.rhs_value = it.value;
        fill_numeric_slots(*raw.slots, action_def, it.term);
        array_add(*result, raw);
    }
    return result;
}

fill_numeric_slots :: (slots: *[TP_MAX_ARITY] s8, action_def: Action, term: Numeric_Term) {
    if term.arity == 0 return;
    for 0..term.arity - 1 {
        index := param_id(action_def, term.arg_names[it]);
        assert(index >= 0);
        slots.*[it] = cast(s8) index;
    }
}

fill_slots :: (slots: *[TP_MAX_ARITY] s8, action_def: Action, atom_def: Atom) {
    if atom_def.arity == 0 return;
    for 0..atom_def.arity - 1 {
        index := param_id(action_def, atom_def.arg_names[it]);
        assert(index >= 0);
        slots.*[it] = cast(s8) index;
    }
}

log_step :: (step: Plan_Step, step_number: s64) {
    action_def := step.domain.actions[step.raw.schema_id];
    builder: String_Builder;
    print_to_builder(*builder, "%: %(", step_number, action_def.name);
    if action_def.params.count > 0 {
        for 0..action_def.params.count - 1 {
            if it > 0 print_to_builder(*builder, ", ");
            object_id := step.raw.args[it];
            name := step.state.objects[object_id].name;
            if !name name = "<object>";
            print_to_builder(*builder, "%=%", action_def.params[it].name, name);
        }
    }
    print_to_builder(*builder, ")");
    log("%", builder_to_string(*builder));
}
