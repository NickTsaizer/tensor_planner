#import "Basic";
#import "Tensor_Planner";

TYPE_AGENT    :: 0;
TYPE_LOCATION :: 1;

build_runtime_builder :: () -> (Tensor_Planner_Builder, s32, s32, s32) {
    limits := TP_Limits.{
        max_objects     = 8,
        max_facts       = 16,
        max_goals       = 4,
        max_candidates  = 16,
        max_expansions  = 64,
        max_plan_length = 8,
    };

    object_types := .[TYPE_AGENT, TYPE_LOCATION, TYPE_LOCATION, TYPE_LOCATION, TYPE_LOCATION];
    builder := builder_create(limits, object_types);

    at_id:        s32;
    connected_id: s32;
    fuel_id:      s32;

    assert(builder_add_predicate(*builder, 2, .[TYPE_AGENT, TYPE_LOCATION], *at_id) == .OK);
    assert(builder_add_predicate(*builder, 2, .[TYPE_LOCATION, TYPE_LOCATION], *connected_id) == .OK);
    assert(builder_add_function(*builder, 1, .[TYPE_AGENT], *fuel_id) == .OK);

    action_types := .[TYPE_AGENT, TYPE_LOCATION, TYPE_LOCATION];
    preconditions := .[
        TP_Action_Literal.{predicate_id = at_id,        sign = 1, arity = 2, slots = .[0, 1, 0, 0]},
        TP_Action_Literal.{predicate_id = connected_id, sign = 1, arity = 2, slots = .[1, 2, 0, 0]},
    ];
    effects := .[
        TP_Action_Effect.{predicate_id = at_id, op = cast(u8) TP_Effect_Op.DELETE, arity = 2, slots = .[0, 1, 0, 0]},
        TP_Action_Effect.{predicate_id = at_id, op = cast(u8) TP_Effect_Op.ADD,    arity = 2, slots = .[0, 2, 0, 0]},
    ];
    numeric_preconditions := .[
        TP_Numeric_Precondition.{
            function_id = fuel_id,
            cmp_op      = cast(u8) TP_Num_Compare_Op.GTE,
            arity       = 1,
            slots       = .[0, 0, 0, 0],
            rhs_value   = 1.0,
        },
    ];
    numeric_effects := .[
        TP_Numeric_Effect.{
            function_id = fuel_id,
            op          = cast(u8) TP_Num_Effect_Op.SUBTRACT,
            arity       = 1,
            slots       = .[0, 0, 0, 0],
            rhs_value   = 1.0,
        },
    ];

    move_id: s32;
    assert(builder_add_action_schema(
        *builder,
        3,
        action_types,
        preconditions,
        effects,
        numeric_preconditions,
        numeric_effects,
        *move_id,
    ) == .OK);

    return builder, at_id, connected_id, fuel_id;
}

main :: () {
    builder, at_id, connected_id, fuel_id := build_runtime_builder();
    defer builder_destroy(*builder);

    assert(builder_add_fact(*builder, at_id, 2, .[0, 1]) == .OK);
    assert(builder_add_fact(*builder, connected_id, 2, .[1, 2]) == .OK);
    assert(builder_add_fact(*builder, connected_id, 2, .[3, 4]) == .OK);
    assert(builder_set_function_value(*builder, fuel_id, 1, .[0], 10.0) == .OK);
    assert(builder_add_goal_fact(*builder, at_id, 2, .[0, 2]) == .OK);

    candidates: TP_Candidate_Action_List;
    assert(builder_generate_candidates(*builder, 8, *candidates) == .OK);
    defer tp_candidate_action_list_dispose(*candidates);

    print("candidate_count=%\n", candidates.count);
    for 0..candidates.count-1 {
        action := candidates.actions[it];
        print("candidate[%] schema=% args=(%, %, %)\n", it, action.schema_id, action.args[0], action.args[1], action.args[2]);
    }

    result: TP_Solve_Result;
    status := builder_solve(*builder, *result);
    defer tp_solve_result_dispose(*result);

    print("solve_status=% solved=% plan_length=%\n", status, result.solved, result.plan_length);
    for 0..result.plan_length-1 {
        action := result.plan_actions[it];
        print("plan[%] schema=% args=(%, %, %)\n", it, action.schema_id, action.args[0], action.args[1], action.args[2]);
    }
}
