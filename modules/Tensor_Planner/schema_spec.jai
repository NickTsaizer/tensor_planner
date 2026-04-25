#scope_export;

Predicate :: struct {
    name:      string;
    arity:     u8;
    arg_types: [TP_MAX_ARITY] Type;
}

Function :: struct {
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

Numeric_Term :: struct {
    function_name: string;
    arity:         u8;
    arg_names:     [TP_MAX_ARITY] string;
}

Numeric_Precondition :: struct {
    term:  Numeric_Term;
    op:    TP_Num_Compare_Op;
    value: float;
}

Numeric_Effect :: struct {
    term:  Numeric_Term;
    op:    TP_Num_Effect_Op;
    value: float;
}

Action :: struct {
    name:     string;
    params:   [] Param;
    requires: [] Atom;
    numeric_requires: [] Numeric_Precondition;
    removes:  [] Atom;
    adds:     [] Atom;
    numeric_effects: [] Numeric_Effect;
}

Planner_Domain :: struct {
    limits:     TP_Limits;
    types:      [] Type;
    predicates: [] Predicate;
    functions:  [] Function;
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

function :: (name: string, arg_types: ..Type) -> Function {
    assert(arg_types.count <= TP_MAX_ARITY);
    result: Function;
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

numeric :: (function_name: string, arg_names: ..string) -> Numeric_Term {
    assert(arg_names.count <= TP_MAX_ARITY);
    result: Numeric_Term;
    result.function_name = function_name;
    result.arity = cast(u8) arg_names.count;
    if arg_names.count > 0 {
        for 0..arg_names.count - 1 result.arg_names[it] = arg_names[it];
    }
    return result;
}

operator < :: (term: Numeric_Term, value: float) -> Numeric_Precondition {
    return .{ term = term, op = .LT, value = value };
}

operator <= :: (term: Numeric_Term, value: float) -> Numeric_Precondition {
    return .{ term = term, op = .LTE, value = value };
}

operator == :: (term: Numeric_Term, value: float) -> Numeric_Precondition {
    return .{ term = term, op = .EQ, value = value };
}

operator >= :: (term: Numeric_Term, value: float) -> Numeric_Precondition {
    return .{ term = term, op = .GTE, value = value };
}

operator > :: (term: Numeric_Term, value: float) -> Numeric_Precondition {
    return .{ term = term, op = .GT, value = value };
}

set :: (term: Numeric_Term, value: float) -> Numeric_Effect {
    return .{ term = term, op = .SET, value = value };
}

increase :: (term: Numeric_Term, value: float) -> Numeric_Effect {
    return .{ term = term, op = .ADD, value = value };
}

decrease :: (term: Numeric_Term, value: float) -> Numeric_Effect {
    return .{ term = term, op = .SUBTRACT, value = value };
}

action :: (
name: string,
params: [] Param,
requires: [] Atom,
removes: [] Atom,
adds: [] Atom
) -> Action {
    assert(params.count <= TP_MAX_PARAMS);
    return action(name, params, requires, Numeric_Precondition.[], removes, adds, Numeric_Effect.[]);
}

action :: (
name: string,
params: [] Param,
requires: [] Atom,
numeric_requires: [] Numeric_Precondition,
removes: [] Atom,
adds: [] Atom,
numeric_effects: [] Numeric_Effect
) -> Action {
    assert(params.count <= TP_MAX_PARAMS);
    return .{
        name = name,
        params = params,
        requires = requires,
        numeric_requires = numeric_requires,
        removes = removes,
        adds = adds,
        numeric_effects = numeric_effects,
    };
}

planner_domain :: (
types: [] Type,
predicates: [] Predicate,
actions: [] Action,
limits: TP_Limits
) -> Planner_Domain {
    return planner_domain(types, predicates, Function.[], actions, limits);
}

planner_domain :: (
types: [] Type,
predicates: [] Predicate,
functions: [] Function,
actions: [] Action,
limits: TP_Limits
) -> Planner_Domain {
    result := Planner_Domain.{
        limits = limits,
        types = types,
        predicates = predicates,
        functions = functions,
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
    for domain.functions {
        fun := it;
        if fun.arity > 0 {
            for arg_index: 0..fun.arity - 1 assert(type_id(domain, fun.arg_types[arg_index]) >= 0);
        }
    }
    for domain.actions validate_action(domain, it);
}

validate_action :: (domain: Planner_Domain, action_def: Action) {
    assert(action_def.params.count <= TP_MAX_PARAMS);
    for action_def.params assert(type_id(domain, it.jai_type) >= 0);
    for action_def.requires validate_action_atom(domain, action_def, it);
    for action_def.numeric_requires validate_numeric_precondition(domain, action_def, it);
    for action_def.removes validate_action_atom(domain, action_def, it);
    for action_def.adds validate_action_atom(domain, action_def, it);
    for action_def.numeric_effects validate_numeric_effect(domain, action_def, it);
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

validate_numeric_precondition :: (domain: Planner_Domain, action_def: Action, condition: Numeric_Precondition) {
    validate_numeric_term(domain, action_def, condition.term);
}

validate_numeric_effect :: (domain: Planner_Domain, action_def: Action, effect: Numeric_Effect) {
    validate_numeric_term(domain, action_def, effect.term);
}

validate_numeric_term :: (domain: Planner_Domain, action_def: Action, term: Numeric_Term) {
    fun_index := function_id(domain, term.function_name);
    assert(fun_index >= 0);
    fun := domain.functions[fun_index];
    assert(term.arity == fun.arity);
    if term.arity > 0 {
        for arg_index: 0..term.arity - 1 {
            p_index := param_id(action_def, term.arg_names[arg_index]);
            assert(p_index >= 0);
            assert(action_def.params[p_index].jai_type == fun.arg_types[arg_index]);
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

function_id :: (domain: Planner_Domain, name: string) -> s32 {
    if domain.functions.count == 0 return -1;
    for 0..domain.functions.count - 1 if domain.functions[it].name == name return cast(s32) it;
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
