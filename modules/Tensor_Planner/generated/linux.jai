//
// This file was auto-generated using the following command:
//
// jai modules/Tensor_Planner/generate.jai
//



TP_MAX_ARITY :: 4;
TP_MAX_PARAMS :: 6;

TP_Domain :: struct {}
TP_State :: struct {}
TP_Solver :: struct {}

TP_Status :: enum u32 {
    OK               :: 0;
    INVALID_ARGUMENT :: 1;
    LIMIT_EXCEEDED   :: 2;
    NOT_FOUND        :: 3;
    UNSUPPORTED      :: 4;
    NO_SOLUTION      :: 5;

    TP_STATUS_OK               :: OK;
    TP_STATUS_INVALID_ARGUMENT :: INVALID_ARGUMENT;
    TP_STATUS_LIMIT_EXCEEDED   :: LIMIT_EXCEEDED;
    TP_STATUS_NOT_FOUND        :: NOT_FOUND;
    TP_STATUS_UNSUPPORTED      :: UNSUPPORTED;
    TP_STATUS_NO_SOLUTION      :: NO_SOLUTION;
}

TP_Effect_Op :: enum u32 {
    ADD    :: 1;
    DELETE :: 2;

    TP_EFFECT_ADD    :: ADD;
    TP_EFFECT_DELETE :: DELETE;
}

TP_Num_Compare_Op :: enum u32 {
    LT  :: 1;
    LTE :: 2;
    EQ  :: 3;
    GTE :: 4;
    GT  :: 5;

    TP_NUM_CMP_LT  :: LT;
    TP_NUM_CMP_LTE :: LTE;
    TP_NUM_CMP_EQ  :: EQ;
    TP_NUM_CMP_GTE :: GTE;
    TP_NUM_CMP_GT  :: GT;
}

TP_Num_Effect_Op :: enum u32 {
    SET      :: 1;
    ADD      :: 2;
    SUBTRACT :: 3;

    TP_NUM_EFFECT_SET      :: SET;
    TP_NUM_EFFECT_ADD      :: ADD;
    TP_NUM_EFFECT_SUBTRACT :: SUBTRACT;
}

TP_Limits :: struct {
    max_objects:     s32;
    max_facts:       s32;
    max_goals:       s32;
    max_candidates:  s32;
    max_expansions:  s32;
    max_plan_length: s32;
}

TP_Predicate_Def :: struct {
    arity:     u8;
    arg_types: [4] s32;
}

TP_Function_Def :: struct {
    arity:     u8;
    arg_types: [4] s32;
}

TP_Action_Literal :: struct {
    predicate_id: s32;
    sign:         s8;
    arity:        u8;
    slots:        [4] s8;
}

TP_Action_Effect :: struct {
    predicate_id: s32;
    op:           u8;
    arity:        u8;
    slots:        [4] s8;
}

TP_Numeric_Precondition :: struct {
    function_id: s32;
    cmp_op:      u8;
    arity:       u8;
    slots:       [4] s8;
    rhs_value:   float;
}

TP_Numeric_Effect :: struct {
    function_id: s32;
    op:          u8;
    arity:       u8;
    slots:       [4] s8;
    rhs_value:   float;
}

TP_Candidate_Action :: struct {
    schema_id: s32;
    arity:     u8;
    args:      [6] s32;
}

TP_Candidate_Action_List :: struct {
    count:   s32;
    actions: *TP_Candidate_Action;
}

TP_Schema_Tensors :: struct {
    predicate_count:       s32;
    function_count:        s32;
    action_count:          s32;
    max_preconditions:     s32;
    max_effects:           s32;
    max_num_preconditions: s32;
    max_num_effects:       s32;
    pred_arity_count:      s32;
    pred_arg_type_count:   s32;
    func_arity_count:      s32;
    func_arg_type_count:   s32;
    action_arity_count:    s32;
    action_arg_type_count: s32;
    pre_pred_id_count:     s32;
    pre_sign_count:        s32;
    pre_slot_count:        s32;
    eff_pred_id_count:     s32;
    eff_op_count:          s32;
    eff_slot_count:        s32;
    num_pre_func_id_count: s32;
    num_pre_cmp_count:     s32;
    num_pre_slot_count:    s32;
    num_pre_value_count:   s32;
    num_eff_func_id_count: s32;
    num_eff_op_count:      s32;
    num_eff_slot_count:    s32;
    num_eff_value_count:   s32;
    pred_arity:            *u8;
    pred_arg_type:         *s32;
    func_arity:            *u8;
    func_arg_type:         *s32;
    action_arity:          *u8;
    action_arg_type:       *s32;
    pre_pred_id:           *s16;
    pre_sign:              *s8;
    pre_slot:              *s8;
    eff_pred_id:           *s16;
    eff_op:                *s8;
    eff_slot:              *s8;
    num_pre_func_id:       *s16;
    num_pre_cmp:           *s8;
    num_pre_slot:          *s8;
    num_pre_value:         *float;
    num_eff_func_id:       *s16;
    num_eff_op:            *s8;
    num_eff_slot:          *s8;
    num_eff_value:         *float;
}

TP_Problem_Tensors :: struct {
    object_count:             s32;
    fact_count:               s32;
    function_value_count:     s32;
    goal_count:               s32;
    candidate_count:          s32;
    obj_type_count:           s32;
    true_pred_id_count:       s32;
    true_pred_arg_count:      s32;
    true_pred_mask_count:     s32;
    num_func_id_count:        s32;
    num_func_arg_count:       s32;
    num_value_count:          s32;
    goal_pred_id_count:       s32;
    goal_pred_arg_count:      s32;
    goal_pred_mask_count:     s32;
    cand_action_schema_count: s32;
    cand_action_arg_count:    s32;
    cand_action_mask_count:   s32;
    obj_type:                 *s16;
    true_pred_id:             *s16;
    true_pred_arg:            *s16;
    true_pred_mask:           *u8;
    num_func_id:              *s16;
    num_func_arg:             *s16;
    num_value:                *float;
    goal_pred_id:             *s16;
    goal_pred_arg:            *s16;
    goal_pred_mask:           *u8;
    cand_action_schema:       *s16;
    cand_action_arg:          *s16;
    cand_action_mask:         *u8;
}

TP_Action_Graph :: struct {
    object_node_count: s32;
    fact_node_count:   s32;
    action_node_count: s32;
    total_node_count:  s32;
    edge_count:        s32;
    node_kind_count:   s32;
    edge_src_count:    s32;
    edge_dst_count:    s32;
    edge_type_count:   s32;
    node_kind:         *u8;
    edge_src:          *s32;
    edge_dst:          *s32;
    edge_type:         *s32;
}

TP_Score_Candidates_Fn :: #type (schema: *TP_Schema_Tensors, problem: *TP_Problem_Tensors, user_data: *void, out_action_scores: *float, out_state_value: *float, out_has_state_value: *bool) -> void #c_call;

TP_Solve_Result :: struct {
    solved:                       bool;
    expansions:                   s32;
    generated:                    s32;
    candidate_generation_calls:   s32;
    index_rebuilds:               s32;
    scorer_calls:                 s32;
    plan_length:                  s32;
    remaining_goal_count:         s32;
    candidate_generation_time_us: s64;
    scorer_export_time_us:        s64;
    scorer_time_us:               s64;
    scorer_sort_time_us:          s64;
    plan_actions:                 *TP_Candidate_Action;
}

tp_domain_create :: (limits: *TP_Limits) -> *TP_Domain #foreign libtensor_planner;
tp_domain_destroy :: (domain: *TP_Domain) -> void #foreign libtensor_planner;

tp_domain_add_predicate :: (domain: *TP_Domain, predicate_def: *TP_Predicate_Def, predicate_id_out: *s32) -> TP_Status #foreign libtensor_planner;

tp_domain_add_function :: (domain: *TP_Domain, function_def: *TP_Function_Def, function_id_out: *s32) -> TP_Status #foreign libtensor_planner;

tp_domain_add_action_schema :: (domain: *TP_Domain, arity: u8, arg_types: *s32, preconditions: *TP_Action_Literal, precondition_count: s32, effects: *TP_Action_Effect, effect_count: s32, numeric_preconditions: *TP_Numeric_Precondition, numeric_precondition_count: s32, numeric_effects: *TP_Numeric_Effect, numeric_effect_count: s32, action_id_out: *s32) -> TP_Status #foreign libtensor_planner;

tp_domain_export_schema_tensors :: (domain: *TP_Domain, out_tensors: *TP_Schema_Tensors) -> TP_Status #foreign libtensor_planner;

tp_schema_tensors_dispose :: (tensors: *TP_Schema_Tensors) -> void #foreign libtensor_planner;

tp_state_create :: (domain: *TP_Domain, object_count: s32, object_types: *s32) -> *TP_State #foreign libtensor_planner;

tp_state_destroy :: (state: *TP_State) -> void #foreign libtensor_planner;

tp_state_add_fact :: (state: *TP_State, predicate_id: s32, arity: u8, args: *s32) -> TP_Status #foreign libtensor_planner;

tp_state_add_goal_fact :: (state: *TP_State, predicate_id: s32, arity: u8, args: *s32) -> TP_Status #foreign libtensor_planner;

tp_state_set_function_value :: (state: *TP_State, function_id: s32, arity: u8, args: *s32, value: float) -> TP_Status #foreign libtensor_planner;

tp_state_generate_candidates :: (domain: *TP_Domain, state: *TP_State, max_candidates: s32, out_candidates: *TP_Candidate_Action_List) -> TP_Status #foreign libtensor_planner;

tp_candidate_action_list_dispose :: (list: *TP_Candidate_Action_List) -> void #foreign libtensor_planner;

tp_state_export_problem_tensors :: (domain: *TP_Domain, state: *TP_State, max_candidates: s32, out_tensors: *TP_Problem_Tensors) -> TP_Status #foreign libtensor_planner;

tp_state_export_action_graph :: (domain: *TP_Domain, state: *TP_State, max_candidates: s32, out_graph: *TP_Action_Graph) -> TP_Status #foreign libtensor_planner;

tp_problem_tensors_dispose :: (tensors: *TP_Problem_Tensors) -> void #foreign libtensor_planner;
tp_action_graph_dispose :: (graph: *TP_Action_Graph) -> void #foreign libtensor_planner;

tp_solver_create :: (domain: *TP_Domain) -> *TP_Solver #foreign libtensor_planner;
tp_solver_destroy :: (solver: *TP_Solver) -> void #foreign libtensor_planner;

tp_solver_set_custom_guidance :: (solver: *TP_Solver, scorer: TP_Score_Candidates_Fn, user_data: *void) -> TP_Status #foreign libtensor_planner;

tp_solver_use_default_guidance :: (solver: *TP_Solver) -> TP_Status #foreign libtensor_planner;

tp_solver_solve :: (solver: *TP_Solver, initial_state: *TP_State, out_result: *TP_Solve_Result) -> TP_Status #foreign libtensor_planner;

tp_solve_result_dispose :: (result: *TP_Solve_Result) -> void #foreign libtensor_planner;

#scope_file

#import "Basic"; // For assert

libtensor_planner :: #library "./libtensor_planner";

#run {
    {
        info := type_info(TP_Limits);
        for info.members {
            if it.name == {
                case "max_objects";
                assert(it.offset_in_bytes == 0, "TP_Limits.max_objects has unexpected offset % instead of 0", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Limits.max_objects has unexpected size % instead of 4", it.type.runtime_size);
                case "max_facts";
                assert(it.offset_in_bytes == 4, "TP_Limits.max_facts has unexpected offset % instead of 4", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Limits.max_facts has unexpected size % instead of 4", it.type.runtime_size);
                case "max_goals";
                assert(it.offset_in_bytes == 8, "TP_Limits.max_goals has unexpected offset % instead of 8", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Limits.max_goals has unexpected size % instead of 4", it.type.runtime_size);
                case "max_candidates";
                assert(it.offset_in_bytes == 12, "TP_Limits.max_candidates has unexpected offset % instead of 12", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Limits.max_candidates has unexpected size % instead of 4", it.type.runtime_size);
                case "max_expansions";
                assert(it.offset_in_bytes == 16, "TP_Limits.max_expansions has unexpected offset % instead of 16", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Limits.max_expansions has unexpected size % instead of 4", it.type.runtime_size);
                case "max_plan_length";
                assert(it.offset_in_bytes == 20, "TP_Limits.max_plan_length has unexpected offset % instead of 20", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Limits.max_plan_length has unexpected size % instead of 4", it.type.runtime_size);
            }
        }
        assert(size_of(TP_Limits) == 24, "TP_Limits has size % instead of 24", size_of(TP_Limits));
    }

    {
        info := type_info(TP_Predicate_Def);
        for info.members {
            if it.name == {
                case "arity";
                assert(it.offset_in_bytes == 0, "TP_Predicate_Def.arity has unexpected offset % instead of 0", it.offset_in_bytes);
                assert(it.type.runtime_size == 1, "TP_Predicate_Def.arity has unexpected size % instead of 1", it.type.runtime_size);
                case "arg_types";
                assert(it.offset_in_bytes == 4, "TP_Predicate_Def.arg_types has unexpected offset % instead of 4", it.offset_in_bytes);
                assert(it.type.runtime_size == 16, "TP_Predicate_Def.arg_types has unexpected size % instead of 16", it.type.runtime_size);
            }
        }
        assert(size_of(TP_Predicate_Def) == 20, "TP_Predicate_Def has size % instead of 20", size_of(TP_Predicate_Def));
    }

    {
        info := type_info(TP_Function_Def);
        for info.members {
            if it.name == {
                case "arity";
                assert(it.offset_in_bytes == 0, "TP_Function_Def.arity has unexpected offset % instead of 0", it.offset_in_bytes);
                assert(it.type.runtime_size == 1, "TP_Function_Def.arity has unexpected size % instead of 1", it.type.runtime_size);
                case "arg_types";
                assert(it.offset_in_bytes == 4, "TP_Function_Def.arg_types has unexpected offset % instead of 4", it.offset_in_bytes);
                assert(it.type.runtime_size == 16, "TP_Function_Def.arg_types has unexpected size % instead of 16", it.type.runtime_size);
            }
        }
        assert(size_of(TP_Function_Def) == 20, "TP_Function_Def has size % instead of 20", size_of(TP_Function_Def));
    }

    {
        info := type_info(TP_Action_Literal);
        for info.members {
            if it.name == {
                case "predicate_id";
                assert(it.offset_in_bytes == 0, "TP_Action_Literal.predicate_id has unexpected offset % instead of 0", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Action_Literal.predicate_id has unexpected size % instead of 4", it.type.runtime_size);
                case "sign";
                assert(it.offset_in_bytes == 4, "TP_Action_Literal.sign has unexpected offset % instead of 4", it.offset_in_bytes);
                assert(it.type.runtime_size == 1, "TP_Action_Literal.sign has unexpected size % instead of 1", it.type.runtime_size);
                case "arity";
                assert(it.offset_in_bytes == 5, "TP_Action_Literal.arity has unexpected offset % instead of 5", it.offset_in_bytes);
                assert(it.type.runtime_size == 1, "TP_Action_Literal.arity has unexpected size % instead of 1", it.type.runtime_size);
                case "slots";
                assert(it.offset_in_bytes == 6, "TP_Action_Literal.slots has unexpected offset % instead of 6", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Action_Literal.slots has unexpected size % instead of 4", it.type.runtime_size);
            }
        }
        assert(size_of(TP_Action_Literal) == 12, "TP_Action_Literal has size % instead of 12", size_of(TP_Action_Literal));
    }

    {
        info := type_info(TP_Action_Effect);
        for info.members {
            if it.name == {
                case "predicate_id";
                assert(it.offset_in_bytes == 0, "TP_Action_Effect.predicate_id has unexpected offset % instead of 0", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Action_Effect.predicate_id has unexpected size % instead of 4", it.type.runtime_size);
                case "op";
                assert(it.offset_in_bytes == 4, "TP_Action_Effect.op has unexpected offset % instead of 4", it.offset_in_bytes);
                assert(it.type.runtime_size == 1, "TP_Action_Effect.op has unexpected size % instead of 1", it.type.runtime_size);
                case "arity";
                assert(it.offset_in_bytes == 5, "TP_Action_Effect.arity has unexpected offset % instead of 5", it.offset_in_bytes);
                assert(it.type.runtime_size == 1, "TP_Action_Effect.arity has unexpected size % instead of 1", it.type.runtime_size);
                case "slots";
                assert(it.offset_in_bytes == 6, "TP_Action_Effect.slots has unexpected offset % instead of 6", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Action_Effect.slots has unexpected size % instead of 4", it.type.runtime_size);
            }
        }
        assert(size_of(TP_Action_Effect) == 12, "TP_Action_Effect has size % instead of 12", size_of(TP_Action_Effect));
    }

    {
        info := type_info(TP_Numeric_Precondition);
        for info.members {
            if it.name == {
                case "function_id";
                assert(it.offset_in_bytes == 0, "TP_Numeric_Precondition.function_id has unexpected offset % instead of 0", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Numeric_Precondition.function_id has unexpected size % instead of 4", it.type.runtime_size);
                case "cmp_op";
                assert(it.offset_in_bytes == 4, "TP_Numeric_Precondition.cmp_op has unexpected offset % instead of 4", it.offset_in_bytes);
                assert(it.type.runtime_size == 1, "TP_Numeric_Precondition.cmp_op has unexpected size % instead of 1", it.type.runtime_size);
                case "arity";
                assert(it.offset_in_bytes == 5, "TP_Numeric_Precondition.arity has unexpected offset % instead of 5", it.offset_in_bytes);
                assert(it.type.runtime_size == 1, "TP_Numeric_Precondition.arity has unexpected size % instead of 1", it.type.runtime_size);
                case "slots";
                assert(it.offset_in_bytes == 6, "TP_Numeric_Precondition.slots has unexpected offset % instead of 6", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Numeric_Precondition.slots has unexpected size % instead of 4", it.type.runtime_size);
                case "rhs_value";
                assert(it.offset_in_bytes == 12, "TP_Numeric_Precondition.rhs_value has unexpected offset % instead of 12", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Numeric_Precondition.rhs_value has unexpected size % instead of 4", it.type.runtime_size);
            }
        }
        assert(size_of(TP_Numeric_Precondition) == 16, "TP_Numeric_Precondition has size % instead of 16", size_of(TP_Numeric_Precondition));
    }

    {
        info := type_info(TP_Numeric_Effect);
        for info.members {
            if it.name == {
                case "function_id";
                assert(it.offset_in_bytes == 0, "TP_Numeric_Effect.function_id has unexpected offset % instead of 0", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Numeric_Effect.function_id has unexpected size % instead of 4", it.type.runtime_size);
                case "op";
                assert(it.offset_in_bytes == 4, "TP_Numeric_Effect.op has unexpected offset % instead of 4", it.offset_in_bytes);
                assert(it.type.runtime_size == 1, "TP_Numeric_Effect.op has unexpected size % instead of 1", it.type.runtime_size);
                case "arity";
                assert(it.offset_in_bytes == 5, "TP_Numeric_Effect.arity has unexpected offset % instead of 5", it.offset_in_bytes);
                assert(it.type.runtime_size == 1, "TP_Numeric_Effect.arity has unexpected size % instead of 1", it.type.runtime_size);
                case "slots";
                assert(it.offset_in_bytes == 6, "TP_Numeric_Effect.slots has unexpected offset % instead of 6", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Numeric_Effect.slots has unexpected size % instead of 4", it.type.runtime_size);
                case "rhs_value";
                assert(it.offset_in_bytes == 12, "TP_Numeric_Effect.rhs_value has unexpected offset % instead of 12", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Numeric_Effect.rhs_value has unexpected size % instead of 4", it.type.runtime_size);
            }
        }
        assert(size_of(TP_Numeric_Effect) == 16, "TP_Numeric_Effect has size % instead of 16", size_of(TP_Numeric_Effect));
    }

    {
        info := type_info(TP_Candidate_Action);
        for info.members {
            if it.name == {
                case "schema_id";
                assert(it.offset_in_bytes == 0, "TP_Candidate_Action.schema_id has unexpected offset % instead of 0", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Candidate_Action.schema_id has unexpected size % instead of 4", it.type.runtime_size);
                case "arity";
                assert(it.offset_in_bytes == 4, "TP_Candidate_Action.arity has unexpected offset % instead of 4", it.offset_in_bytes);
                assert(it.type.runtime_size == 1, "TP_Candidate_Action.arity has unexpected size % instead of 1", it.type.runtime_size);
                case "args";
                assert(it.offset_in_bytes == 8, "TP_Candidate_Action.args has unexpected offset % instead of 8", it.offset_in_bytes);
                assert(it.type.runtime_size == 24, "TP_Candidate_Action.args has unexpected size % instead of 24", it.type.runtime_size);
            }
        }
        assert(size_of(TP_Candidate_Action) == 32, "TP_Candidate_Action has size % instead of 32", size_of(TP_Candidate_Action));
    }

    {
        info := type_info(TP_Candidate_Action_List);
        for info.members {
            if it.name == {
                case "count";
                assert(it.offset_in_bytes == 0, "TP_Candidate_Action_List.count has unexpected offset % instead of 0", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Candidate_Action_List.count has unexpected size % instead of 4", it.type.runtime_size);
                case "actions";
                assert(it.offset_in_bytes == 8, "TP_Candidate_Action_List.actions has unexpected offset % instead of 8", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Candidate_Action_List.actions has unexpected size % instead of 8", it.type.runtime_size);
            }
        }
        assert(size_of(TP_Candidate_Action_List) == 16, "TP_Candidate_Action_List has size % instead of 16", size_of(TP_Candidate_Action_List));
    }

    {
        info := type_info(TP_Schema_Tensors);
        for info.members {
            if it.name == {
                case "predicate_count";
                assert(it.offset_in_bytes == 0, "TP_Schema_Tensors.predicate_count has unexpected offset % instead of 0", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.predicate_count has unexpected size % instead of 4", it.type.runtime_size);
                case "function_count";
                assert(it.offset_in_bytes == 4, "TP_Schema_Tensors.function_count has unexpected offset % instead of 4", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.function_count has unexpected size % instead of 4", it.type.runtime_size);
                case "action_count";
                assert(it.offset_in_bytes == 8, "TP_Schema_Tensors.action_count has unexpected offset % instead of 8", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.action_count has unexpected size % instead of 4", it.type.runtime_size);
                case "max_preconditions";
                assert(it.offset_in_bytes == 12, "TP_Schema_Tensors.max_preconditions has unexpected offset % instead of 12", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.max_preconditions has unexpected size % instead of 4", it.type.runtime_size);
                case "max_effects";
                assert(it.offset_in_bytes == 16, "TP_Schema_Tensors.max_effects has unexpected offset % instead of 16", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.max_effects has unexpected size % instead of 4", it.type.runtime_size);
                case "max_num_preconditions";
                assert(it.offset_in_bytes == 20, "TP_Schema_Tensors.max_num_preconditions has unexpected offset % instead of 20", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.max_num_preconditions has unexpected size % instead of 4", it.type.runtime_size);
                case "max_num_effects";
                assert(it.offset_in_bytes == 24, "TP_Schema_Tensors.max_num_effects has unexpected offset % instead of 24", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.max_num_effects has unexpected size % instead of 4", it.type.runtime_size);
                case "pred_arity_count";
                assert(it.offset_in_bytes == 28, "TP_Schema_Tensors.pred_arity_count has unexpected offset % instead of 28", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.pred_arity_count has unexpected size % instead of 4", it.type.runtime_size);
                case "pred_arg_type_count";
                assert(it.offset_in_bytes == 32, "TP_Schema_Tensors.pred_arg_type_count has unexpected offset % instead of 32", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.pred_arg_type_count has unexpected size % instead of 4", it.type.runtime_size);
                case "func_arity_count";
                assert(it.offset_in_bytes == 36, "TP_Schema_Tensors.func_arity_count has unexpected offset % instead of 36", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.func_arity_count has unexpected size % instead of 4", it.type.runtime_size);
                case "func_arg_type_count";
                assert(it.offset_in_bytes == 40, "TP_Schema_Tensors.func_arg_type_count has unexpected offset % instead of 40", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.func_arg_type_count has unexpected size % instead of 4", it.type.runtime_size);
                case "action_arity_count";
                assert(it.offset_in_bytes == 44, "TP_Schema_Tensors.action_arity_count has unexpected offset % instead of 44", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.action_arity_count has unexpected size % instead of 4", it.type.runtime_size);
                case "action_arg_type_count";
                assert(it.offset_in_bytes == 48, "TP_Schema_Tensors.action_arg_type_count has unexpected offset % instead of 48", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.action_arg_type_count has unexpected size % instead of 4", it.type.runtime_size);
                case "pre_pred_id_count";
                assert(it.offset_in_bytes == 52, "TP_Schema_Tensors.pre_pred_id_count has unexpected offset % instead of 52", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.pre_pred_id_count has unexpected size % instead of 4", it.type.runtime_size);
                case "pre_sign_count";
                assert(it.offset_in_bytes == 56, "TP_Schema_Tensors.pre_sign_count has unexpected offset % instead of 56", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.pre_sign_count has unexpected size % instead of 4", it.type.runtime_size);
                case "pre_slot_count";
                assert(it.offset_in_bytes == 60, "TP_Schema_Tensors.pre_slot_count has unexpected offset % instead of 60", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.pre_slot_count has unexpected size % instead of 4", it.type.runtime_size);
                case "eff_pred_id_count";
                assert(it.offset_in_bytes == 64, "TP_Schema_Tensors.eff_pred_id_count has unexpected offset % instead of 64", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.eff_pred_id_count has unexpected size % instead of 4", it.type.runtime_size);
                case "eff_op_count";
                assert(it.offset_in_bytes == 68, "TP_Schema_Tensors.eff_op_count has unexpected offset % instead of 68", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.eff_op_count has unexpected size % instead of 4", it.type.runtime_size);
                case "eff_slot_count";
                assert(it.offset_in_bytes == 72, "TP_Schema_Tensors.eff_slot_count has unexpected offset % instead of 72", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.eff_slot_count has unexpected size % instead of 4", it.type.runtime_size);
                case "num_pre_func_id_count";
                assert(it.offset_in_bytes == 76, "TP_Schema_Tensors.num_pre_func_id_count has unexpected offset % instead of 76", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.num_pre_func_id_count has unexpected size % instead of 4", it.type.runtime_size);
                case "num_pre_cmp_count";
                assert(it.offset_in_bytes == 80, "TP_Schema_Tensors.num_pre_cmp_count has unexpected offset % instead of 80", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.num_pre_cmp_count has unexpected size % instead of 4", it.type.runtime_size);
                case "num_pre_slot_count";
                assert(it.offset_in_bytes == 84, "TP_Schema_Tensors.num_pre_slot_count has unexpected offset % instead of 84", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.num_pre_slot_count has unexpected size % instead of 4", it.type.runtime_size);
                case "num_pre_value_count";
                assert(it.offset_in_bytes == 88, "TP_Schema_Tensors.num_pre_value_count has unexpected offset % instead of 88", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.num_pre_value_count has unexpected size % instead of 4", it.type.runtime_size);
                case "num_eff_func_id_count";
                assert(it.offset_in_bytes == 92, "TP_Schema_Tensors.num_eff_func_id_count has unexpected offset % instead of 92", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.num_eff_func_id_count has unexpected size % instead of 4", it.type.runtime_size);
                case "num_eff_op_count";
                assert(it.offset_in_bytes == 96, "TP_Schema_Tensors.num_eff_op_count has unexpected offset % instead of 96", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.num_eff_op_count has unexpected size % instead of 4", it.type.runtime_size);
                case "num_eff_slot_count";
                assert(it.offset_in_bytes == 100, "TP_Schema_Tensors.num_eff_slot_count has unexpected offset % instead of 100", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.num_eff_slot_count has unexpected size % instead of 4", it.type.runtime_size);
                case "num_eff_value_count";
                assert(it.offset_in_bytes == 104, "TP_Schema_Tensors.num_eff_value_count has unexpected offset % instead of 104", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Schema_Tensors.num_eff_value_count has unexpected size % instead of 4", it.type.runtime_size);
                case "pred_arity";
                assert(it.offset_in_bytes == 112, "TP_Schema_Tensors.pred_arity has unexpected offset % instead of 112", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Schema_Tensors.pred_arity has unexpected size % instead of 8", it.type.runtime_size);
                case "pred_arg_type";
                assert(it.offset_in_bytes == 120, "TP_Schema_Tensors.pred_arg_type has unexpected offset % instead of 120", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Schema_Tensors.pred_arg_type has unexpected size % instead of 8", it.type.runtime_size);
                case "func_arity";
                assert(it.offset_in_bytes == 128, "TP_Schema_Tensors.func_arity has unexpected offset % instead of 128", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Schema_Tensors.func_arity has unexpected size % instead of 8", it.type.runtime_size);
                case "func_arg_type";
                assert(it.offset_in_bytes == 136, "TP_Schema_Tensors.func_arg_type has unexpected offset % instead of 136", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Schema_Tensors.func_arg_type has unexpected size % instead of 8", it.type.runtime_size);
                case "action_arity";
                assert(it.offset_in_bytes == 144, "TP_Schema_Tensors.action_arity has unexpected offset % instead of 144", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Schema_Tensors.action_arity has unexpected size % instead of 8", it.type.runtime_size);
                case "action_arg_type";
                assert(it.offset_in_bytes == 152, "TP_Schema_Tensors.action_arg_type has unexpected offset % instead of 152", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Schema_Tensors.action_arg_type has unexpected size % instead of 8", it.type.runtime_size);
                case "pre_pred_id";
                assert(it.offset_in_bytes == 160, "TP_Schema_Tensors.pre_pred_id has unexpected offset % instead of 160", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Schema_Tensors.pre_pred_id has unexpected size % instead of 8", it.type.runtime_size);
                case "pre_sign";
                assert(it.offset_in_bytes == 168, "TP_Schema_Tensors.pre_sign has unexpected offset % instead of 168", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Schema_Tensors.pre_sign has unexpected size % instead of 8", it.type.runtime_size);
                case "pre_slot";
                assert(it.offset_in_bytes == 176, "TP_Schema_Tensors.pre_slot has unexpected offset % instead of 176", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Schema_Tensors.pre_slot has unexpected size % instead of 8", it.type.runtime_size);
                case "eff_pred_id";
                assert(it.offset_in_bytes == 184, "TP_Schema_Tensors.eff_pred_id has unexpected offset % instead of 184", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Schema_Tensors.eff_pred_id has unexpected size % instead of 8", it.type.runtime_size);
                case "eff_op";
                assert(it.offset_in_bytes == 192, "TP_Schema_Tensors.eff_op has unexpected offset % instead of 192", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Schema_Tensors.eff_op has unexpected size % instead of 8", it.type.runtime_size);
                case "eff_slot";
                assert(it.offset_in_bytes == 200, "TP_Schema_Tensors.eff_slot has unexpected offset % instead of 200", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Schema_Tensors.eff_slot has unexpected size % instead of 8", it.type.runtime_size);
                case "num_pre_func_id";
                assert(it.offset_in_bytes == 208, "TP_Schema_Tensors.num_pre_func_id has unexpected offset % instead of 208", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Schema_Tensors.num_pre_func_id has unexpected size % instead of 8", it.type.runtime_size);
                case "num_pre_cmp";
                assert(it.offset_in_bytes == 216, "TP_Schema_Tensors.num_pre_cmp has unexpected offset % instead of 216", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Schema_Tensors.num_pre_cmp has unexpected size % instead of 8", it.type.runtime_size);
                case "num_pre_slot";
                assert(it.offset_in_bytes == 224, "TP_Schema_Tensors.num_pre_slot has unexpected offset % instead of 224", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Schema_Tensors.num_pre_slot has unexpected size % instead of 8", it.type.runtime_size);
                case "num_pre_value";
                assert(it.offset_in_bytes == 232, "TP_Schema_Tensors.num_pre_value has unexpected offset % instead of 232", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Schema_Tensors.num_pre_value has unexpected size % instead of 8", it.type.runtime_size);
                case "num_eff_func_id";
                assert(it.offset_in_bytes == 240, "TP_Schema_Tensors.num_eff_func_id has unexpected offset % instead of 240", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Schema_Tensors.num_eff_func_id has unexpected size % instead of 8", it.type.runtime_size);
                case "num_eff_op";
                assert(it.offset_in_bytes == 248, "TP_Schema_Tensors.num_eff_op has unexpected offset % instead of 248", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Schema_Tensors.num_eff_op has unexpected size % instead of 8", it.type.runtime_size);
                case "num_eff_slot";
                assert(it.offset_in_bytes == 256, "TP_Schema_Tensors.num_eff_slot has unexpected offset % instead of 256", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Schema_Tensors.num_eff_slot has unexpected size % instead of 8", it.type.runtime_size);
                case "num_eff_value";
                assert(it.offset_in_bytes == 264, "TP_Schema_Tensors.num_eff_value has unexpected offset % instead of 264", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Schema_Tensors.num_eff_value has unexpected size % instead of 8", it.type.runtime_size);
            }
        }
        assert(size_of(TP_Schema_Tensors) == 272, "TP_Schema_Tensors has size % instead of 272", size_of(TP_Schema_Tensors));
    }

    {
        info := type_info(TP_Problem_Tensors);
        for info.members {
            if it.name == {
                case "object_count";
                assert(it.offset_in_bytes == 0, "TP_Problem_Tensors.object_count has unexpected offset % instead of 0", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Problem_Tensors.object_count has unexpected size % instead of 4", it.type.runtime_size);
                case "fact_count";
                assert(it.offset_in_bytes == 4, "TP_Problem_Tensors.fact_count has unexpected offset % instead of 4", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Problem_Tensors.fact_count has unexpected size % instead of 4", it.type.runtime_size);
                case "function_value_count";
                assert(it.offset_in_bytes == 8, "TP_Problem_Tensors.function_value_count has unexpected offset % instead of 8", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Problem_Tensors.function_value_count has unexpected size % instead of 4", it.type.runtime_size);
                case "goal_count";
                assert(it.offset_in_bytes == 12, "TP_Problem_Tensors.goal_count has unexpected offset % instead of 12", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Problem_Tensors.goal_count has unexpected size % instead of 4", it.type.runtime_size);
                case "candidate_count";
                assert(it.offset_in_bytes == 16, "TP_Problem_Tensors.candidate_count has unexpected offset % instead of 16", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Problem_Tensors.candidate_count has unexpected size % instead of 4", it.type.runtime_size);
                case "obj_type_count";
                assert(it.offset_in_bytes == 20, "TP_Problem_Tensors.obj_type_count has unexpected offset % instead of 20", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Problem_Tensors.obj_type_count has unexpected size % instead of 4", it.type.runtime_size);
                case "true_pred_id_count";
                assert(it.offset_in_bytes == 24, "TP_Problem_Tensors.true_pred_id_count has unexpected offset % instead of 24", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Problem_Tensors.true_pred_id_count has unexpected size % instead of 4", it.type.runtime_size);
                case "true_pred_arg_count";
                assert(it.offset_in_bytes == 28, "TP_Problem_Tensors.true_pred_arg_count has unexpected offset % instead of 28", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Problem_Tensors.true_pred_arg_count has unexpected size % instead of 4", it.type.runtime_size);
                case "true_pred_mask_count";
                assert(it.offset_in_bytes == 32, "TP_Problem_Tensors.true_pred_mask_count has unexpected offset % instead of 32", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Problem_Tensors.true_pred_mask_count has unexpected size % instead of 4", it.type.runtime_size);
                case "num_func_id_count";
                assert(it.offset_in_bytes == 36, "TP_Problem_Tensors.num_func_id_count has unexpected offset % instead of 36", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Problem_Tensors.num_func_id_count has unexpected size % instead of 4", it.type.runtime_size);
                case "num_func_arg_count";
                assert(it.offset_in_bytes == 40, "TP_Problem_Tensors.num_func_arg_count has unexpected offset % instead of 40", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Problem_Tensors.num_func_arg_count has unexpected size % instead of 4", it.type.runtime_size);
                case "num_value_count";
                assert(it.offset_in_bytes == 44, "TP_Problem_Tensors.num_value_count has unexpected offset % instead of 44", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Problem_Tensors.num_value_count has unexpected size % instead of 4", it.type.runtime_size);
                case "goal_pred_id_count";
                assert(it.offset_in_bytes == 48, "TP_Problem_Tensors.goal_pred_id_count has unexpected offset % instead of 48", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Problem_Tensors.goal_pred_id_count has unexpected size % instead of 4", it.type.runtime_size);
                case "goal_pred_arg_count";
                assert(it.offset_in_bytes == 52, "TP_Problem_Tensors.goal_pred_arg_count has unexpected offset % instead of 52", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Problem_Tensors.goal_pred_arg_count has unexpected size % instead of 4", it.type.runtime_size);
                case "goal_pred_mask_count";
                assert(it.offset_in_bytes == 56, "TP_Problem_Tensors.goal_pred_mask_count has unexpected offset % instead of 56", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Problem_Tensors.goal_pred_mask_count has unexpected size % instead of 4", it.type.runtime_size);
                case "cand_action_schema_count";
                assert(it.offset_in_bytes == 60, "TP_Problem_Tensors.cand_action_schema_count has unexpected offset % instead of 60", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Problem_Tensors.cand_action_schema_count has unexpected size % instead of 4", it.type.runtime_size);
                case "cand_action_arg_count";
                assert(it.offset_in_bytes == 64, "TP_Problem_Tensors.cand_action_arg_count has unexpected offset % instead of 64", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Problem_Tensors.cand_action_arg_count has unexpected size % instead of 4", it.type.runtime_size);
                case "cand_action_mask_count";
                assert(it.offset_in_bytes == 68, "TP_Problem_Tensors.cand_action_mask_count has unexpected offset % instead of 68", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Problem_Tensors.cand_action_mask_count has unexpected size % instead of 4", it.type.runtime_size);
                case "obj_type";
                assert(it.offset_in_bytes == 72, "TP_Problem_Tensors.obj_type has unexpected offset % instead of 72", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Problem_Tensors.obj_type has unexpected size % instead of 8", it.type.runtime_size);
                case "true_pred_id";
                assert(it.offset_in_bytes == 80, "TP_Problem_Tensors.true_pred_id has unexpected offset % instead of 80", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Problem_Tensors.true_pred_id has unexpected size % instead of 8", it.type.runtime_size);
                case "true_pred_arg";
                assert(it.offset_in_bytes == 88, "TP_Problem_Tensors.true_pred_arg has unexpected offset % instead of 88", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Problem_Tensors.true_pred_arg has unexpected size % instead of 8", it.type.runtime_size);
                case "true_pred_mask";
                assert(it.offset_in_bytes == 96, "TP_Problem_Tensors.true_pred_mask has unexpected offset % instead of 96", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Problem_Tensors.true_pred_mask has unexpected size % instead of 8", it.type.runtime_size);
                case "num_func_id";
                assert(it.offset_in_bytes == 104, "TP_Problem_Tensors.num_func_id has unexpected offset % instead of 104", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Problem_Tensors.num_func_id has unexpected size % instead of 8", it.type.runtime_size);
                case "num_func_arg";
                assert(it.offset_in_bytes == 112, "TP_Problem_Tensors.num_func_arg has unexpected offset % instead of 112", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Problem_Tensors.num_func_arg has unexpected size % instead of 8", it.type.runtime_size);
                case "num_value";
                assert(it.offset_in_bytes == 120, "TP_Problem_Tensors.num_value has unexpected offset % instead of 120", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Problem_Tensors.num_value has unexpected size % instead of 8", it.type.runtime_size);
                case "goal_pred_id";
                assert(it.offset_in_bytes == 128, "TP_Problem_Tensors.goal_pred_id has unexpected offset % instead of 128", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Problem_Tensors.goal_pred_id has unexpected size % instead of 8", it.type.runtime_size);
                case "goal_pred_arg";
                assert(it.offset_in_bytes == 136, "TP_Problem_Tensors.goal_pred_arg has unexpected offset % instead of 136", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Problem_Tensors.goal_pred_arg has unexpected size % instead of 8", it.type.runtime_size);
                case "goal_pred_mask";
                assert(it.offset_in_bytes == 144, "TP_Problem_Tensors.goal_pred_mask has unexpected offset % instead of 144", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Problem_Tensors.goal_pred_mask has unexpected size % instead of 8", it.type.runtime_size);
                case "cand_action_schema";
                assert(it.offset_in_bytes == 152, "TP_Problem_Tensors.cand_action_schema has unexpected offset % instead of 152", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Problem_Tensors.cand_action_schema has unexpected size % instead of 8", it.type.runtime_size);
                case "cand_action_arg";
                assert(it.offset_in_bytes == 160, "TP_Problem_Tensors.cand_action_arg has unexpected offset % instead of 160", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Problem_Tensors.cand_action_arg has unexpected size % instead of 8", it.type.runtime_size);
                case "cand_action_mask";
                assert(it.offset_in_bytes == 168, "TP_Problem_Tensors.cand_action_mask has unexpected offset % instead of 168", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Problem_Tensors.cand_action_mask has unexpected size % instead of 8", it.type.runtime_size);
            }
        }
        assert(size_of(TP_Problem_Tensors) == 176, "TP_Problem_Tensors has size % instead of 176", size_of(TP_Problem_Tensors));
    }

    {
        info := type_info(TP_Action_Graph);
        for info.members {
            if it.name == {
                case "object_node_count";
                assert(it.offset_in_bytes == 0, "TP_Action_Graph.object_node_count has unexpected offset % instead of 0", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Action_Graph.object_node_count has unexpected size % instead of 4", it.type.runtime_size);
                case "fact_node_count";
                assert(it.offset_in_bytes == 4, "TP_Action_Graph.fact_node_count has unexpected offset % instead of 4", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Action_Graph.fact_node_count has unexpected size % instead of 4", it.type.runtime_size);
                case "action_node_count";
                assert(it.offset_in_bytes == 8, "TP_Action_Graph.action_node_count has unexpected offset % instead of 8", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Action_Graph.action_node_count has unexpected size % instead of 4", it.type.runtime_size);
                case "total_node_count";
                assert(it.offset_in_bytes == 12, "TP_Action_Graph.total_node_count has unexpected offset % instead of 12", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Action_Graph.total_node_count has unexpected size % instead of 4", it.type.runtime_size);
                case "edge_count";
                assert(it.offset_in_bytes == 16, "TP_Action_Graph.edge_count has unexpected offset % instead of 16", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Action_Graph.edge_count has unexpected size % instead of 4", it.type.runtime_size);
                case "node_kind_count";
                assert(it.offset_in_bytes == 20, "TP_Action_Graph.node_kind_count has unexpected offset % instead of 20", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Action_Graph.node_kind_count has unexpected size % instead of 4", it.type.runtime_size);
                case "edge_src_count";
                assert(it.offset_in_bytes == 24, "TP_Action_Graph.edge_src_count has unexpected offset % instead of 24", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Action_Graph.edge_src_count has unexpected size % instead of 4", it.type.runtime_size);
                case "edge_dst_count";
                assert(it.offset_in_bytes == 28, "TP_Action_Graph.edge_dst_count has unexpected offset % instead of 28", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Action_Graph.edge_dst_count has unexpected size % instead of 4", it.type.runtime_size);
                case "edge_type_count";
                assert(it.offset_in_bytes == 32, "TP_Action_Graph.edge_type_count has unexpected offset % instead of 32", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Action_Graph.edge_type_count has unexpected size % instead of 4", it.type.runtime_size);
                case "node_kind";
                assert(it.offset_in_bytes == 40, "TP_Action_Graph.node_kind has unexpected offset % instead of 40", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Action_Graph.node_kind has unexpected size % instead of 8", it.type.runtime_size);
                case "edge_src";
                assert(it.offset_in_bytes == 48, "TP_Action_Graph.edge_src has unexpected offset % instead of 48", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Action_Graph.edge_src has unexpected size % instead of 8", it.type.runtime_size);
                case "edge_dst";
                assert(it.offset_in_bytes == 56, "TP_Action_Graph.edge_dst has unexpected offset % instead of 56", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Action_Graph.edge_dst has unexpected size % instead of 8", it.type.runtime_size);
                case "edge_type";
                assert(it.offset_in_bytes == 64, "TP_Action_Graph.edge_type has unexpected offset % instead of 64", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Action_Graph.edge_type has unexpected size % instead of 8", it.type.runtime_size);
            }
        }
        assert(size_of(TP_Action_Graph) == 72, "TP_Action_Graph has size % instead of 72", size_of(TP_Action_Graph));
    }

    {
        info := type_info(TP_Solve_Result);
        for info.members {
            if it.name == {
                case "solved";
                assert(it.offset_in_bytes == 0, "TP_Solve_Result.solved has unexpected offset % instead of 0", it.offset_in_bytes);
                assert(it.type.runtime_size == 1, "TP_Solve_Result.solved has unexpected size % instead of 1", it.type.runtime_size);
                case "expansions";
                assert(it.offset_in_bytes == 4, "TP_Solve_Result.expansions has unexpected offset % instead of 4", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Solve_Result.expansions has unexpected size % instead of 4", it.type.runtime_size);
                case "generated";
                assert(it.offset_in_bytes == 8, "TP_Solve_Result.generated has unexpected offset % instead of 8", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Solve_Result.generated has unexpected size % instead of 4", it.type.runtime_size);
                case "candidate_generation_calls";
                assert(it.offset_in_bytes == 12, "TP_Solve_Result.candidate_generation_calls has unexpected offset % instead of 12", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Solve_Result.candidate_generation_calls has unexpected size % instead of 4", it.type.runtime_size);
                case "index_rebuilds";
                assert(it.offset_in_bytes == 16, "TP_Solve_Result.index_rebuilds has unexpected offset % instead of 16", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Solve_Result.index_rebuilds has unexpected size % instead of 4", it.type.runtime_size);
                case "scorer_calls";
                assert(it.offset_in_bytes == 20, "TP_Solve_Result.scorer_calls has unexpected offset % instead of 20", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Solve_Result.scorer_calls has unexpected size % instead of 4", it.type.runtime_size);
                case "plan_length";
                assert(it.offset_in_bytes == 24, "TP_Solve_Result.plan_length has unexpected offset % instead of 24", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Solve_Result.plan_length has unexpected size % instead of 4", it.type.runtime_size);
                case "remaining_goal_count";
                assert(it.offset_in_bytes == 28, "TP_Solve_Result.remaining_goal_count has unexpected offset % instead of 28", it.offset_in_bytes);
                assert(it.type.runtime_size == 4, "TP_Solve_Result.remaining_goal_count has unexpected size % instead of 4", it.type.runtime_size);
                case "candidate_generation_time_us";
                assert(it.offset_in_bytes == 32, "TP_Solve_Result.candidate_generation_time_us has unexpected offset % instead of 32", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Solve_Result.candidate_generation_time_us has unexpected size % instead of 8", it.type.runtime_size);
                case "scorer_export_time_us";
                assert(it.offset_in_bytes == 40, "TP_Solve_Result.scorer_export_time_us has unexpected offset % instead of 40", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Solve_Result.scorer_export_time_us has unexpected size % instead of 8", it.type.runtime_size);
                case "scorer_time_us";
                assert(it.offset_in_bytes == 48, "TP_Solve_Result.scorer_time_us has unexpected offset % instead of 48", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Solve_Result.scorer_time_us has unexpected size % instead of 8", it.type.runtime_size);
                case "scorer_sort_time_us";
                assert(it.offset_in_bytes == 56, "TP_Solve_Result.scorer_sort_time_us has unexpected offset % instead of 56", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Solve_Result.scorer_sort_time_us has unexpected size % instead of 8", it.type.runtime_size);
                case "plan_actions";
                assert(it.offset_in_bytes == 64, "TP_Solve_Result.plan_actions has unexpected offset % instead of 64", it.offset_in_bytes);
                assert(it.type.runtime_size == 8, "TP_Solve_Result.plan_actions has unexpected size % instead of 8", it.type.runtime_size);
            }
        }
        assert(size_of(TP_Solve_Result) == 72, "TP_Solve_Result has size % instead of 72", size_of(TP_Solve_Result));
    }
}
