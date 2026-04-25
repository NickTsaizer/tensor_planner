#scope_export;

Predicate :: struct {
    name:      string;
    arity:     u8;
    arg_types: [TP_MAX_ARITY] Type;
}

Param :: struct {
    name:     string;
    jai_type: Type;
}

Atom :: struct {
    predicate_name: string;
    arity:          u8;
    arg_names:      [TP_MAX_ARITY] string;
}

Action :: struct {
    name:     string;
    params:   [] Param;
    requires: [] Atom;
    removes:  [] Atom;
    adds:     [] Atom;
}

Planner_Domain :: struct {
    limits:     TP_Limits;
    types:      [] Type;
    predicates: [] Predicate;
    actions:    [] Action;
}

predicate :: (name: string, arg_types: ..Type) -> Predicate {
    assert(arg_types.count <= TP_MAX_ARITY);
    result: Predicate;
    result.name = name;
    result.arity = cast(u8) arg_types.count;
    if arg_types.count > 0 {
        for 0..arg_types.count - 1 result.arg_types[it] = arg_types[it];
    }
    return result;
}

param :: (name: string, $T: Type) -> Param {
    return .{ name = name, jai_type = T };
}

atom :: (predicate_name: string, arg_names: ..string) -> Atom {
    assert(arg_names.count <= TP_MAX_ARITY);
    result: Atom;
    result.predicate_name = predicate_name;
    result.arity = cast(u8) arg_names.count;
    if arg_names.count > 0 {
        for 0..arg_names.count - 1 result.arg_names[it] = arg_names[it];
    }
    return result;
}

action :: (
name: string,
params: [] Param,
requires: [] Atom,
removes: [] Atom,
adds: [] Atom
) -> Action {
    assert(params.count <= TP_MAX_PARAMS);
    return .{ name = name, params = params, requires = requires, removes = removes, adds = adds };
}

planner_domain :: (
types: [] Type,
predicates: [] Predicate,
actions: [] Action,
limits: TP_Limits
) -> Planner_Domain {
    result := Planner_Domain.{
        limits = limits,
        types = types,
        predicates = predicates,
        actions = actions,
    };
    validate_domain(result);
    return result;
}

validate_domain :: (domain: Planner_Domain) {
    for domain.predicates {
        pred := it;
        if pred.arity > 0 {
            for arg_index: 0..pred.arity - 1 assert(type_id(domain, pred.arg_types[arg_index]) >= 0);
        }
    }
    for domain.actions validate_action(domain, it);
}

validate_action :: (domain: Planner_Domain, action_def: Action) {
    assert(action_def.params.count <= TP_MAX_PARAMS);
    for action_def.params assert(type_id(domain, it.jai_type) >= 0);
    for action_def.requires validate_action_atom(domain, action_def, it);
    for action_def.removes validate_action_atom(domain, action_def, it);
    for action_def.adds validate_action_atom(domain, action_def, it);
}

validate_action_atom :: (domain: Planner_Domain, action_def: Action, atom_def: Atom) {
    pred_index := predicate_id(domain, atom_def.predicate_name);
    assert(pred_index >= 0);
    pred := domain.predicates[pred_index];
    assert(atom_def.arity == pred.arity);
    if atom_def.arity > 0 {
        for arg_index: 0..atom_def.arity - 1 {
            p_index := param_id(action_def, atom_def.arg_names[arg_index]);
            assert(p_index >= 0);
            assert(action_def.params[p_index].jai_type == pred.arg_types[arg_index]);
        }
    }
}

default_limits :: () -> TP_Limits {
    result: TP_Limits;
    result.max_objects = 64;
    result.max_facts = 256;
    result.max_goals = 32;
    result.max_candidates = 512;
    result.max_expansions = 4096;
    result.max_plan_length = 64;
    return result;
}

type_id :: (domain: Planner_Domain, jai_type: Type) -> s32 {
    if domain.types.count == 0 return -1;
    for 0..domain.types.count - 1 if domain.types[it] == jai_type return cast(s32) it;
    return -1;
}

predicate_id :: (domain: Planner_Domain, name: string) -> s32 {
    if domain.predicates.count == 0 return -1;
    for 0..domain.predicates.count - 1 if domain.predicates[it].name == name return cast(s32) it;
    return -1;
}

action_id :: (domain: Planner_Domain, name: string) -> s32 {
    if domain.actions.count == 0 return -1;
    for 0..domain.actions.count - 1 if domain.actions[it].name == name return cast(s32) it;
    return -1;
}

param_id :: (action_def: Action, name: string) -> s32 {
    if action_def.params.count == 0 return -1;
    for 0..action_def.params.count - 1 if action_def.params[it].name == name return cast(s32) it;
    return -1;
}
