#import "Basic";

#scope_export

#if OS == .WINDOWS {
    tensor_planner_lib :: #library "tensor_planner.dll";
} else #if OS == .MACOS {
    tensor_planner_lib :: #library "libtensor_planner.dylib";
} else #if OS == .LINUX {
    tensor_planner_lib :: #library "libtensor_planner.so";
}

#load "builder.jai";

TP_MAX_ARITY :: 4;
TP_MAX_PARAMS :: 6;

TP_Status :: enum s32 {
    OK               :: 0;
    INVALID_ARGUMENT :: 1;
    LIMIT_EXCEEDED   :: 2;
    NOT_FOUND        :: 3;
    UNSUPPORTED      :: 4;
    NO_SOLUTION      :: 5;
}

TP_Solver_Mode :: enum s32 {
    PURE_GREEDY    :: 0;
    SYMBOLIC_ASTAR :: 1;
    GUIDED_MIXED   :: 2;
    OPTIMAL_DEBUG  :: 3;
}

TP_Effect_Op :: enum u8 {
    ADD    :: 1;
    DELETE :: 2;
}

TP_Num_Compare_Op :: enum u8 {
    LT  :: 1;
    LTE :: 2;
    EQ  :: 3;
    GTE :: 4;
    GT  :: 5;
}

TP_Num_Effect_Op :: enum u8 {
    SET      :: 1;
    ADD      :: 2;
    SUBTRACT :: 3;
}

TP_Domain :: struct {}
TP_State  :: struct {}
TP_Solver :: struct {}

TP_Limits :: struct {
    max_objects:      s32;
    max_facts:        s32;
    max_goals:        s32;
    max_candidates:   s32;
    max_expansions:   s32;
    max_plan_length:  s32;
}

TP_Predicate_Def :: struct {
    arity:     u8;
    arg_types: [TP_MAX_ARITY] s32;
}

TP_Function_Def :: struct {
    arity:     u8;
    arg_types: [TP_MAX_ARITY] s32;
}

TP_Action_Literal :: struct {
    predicate_id: s32;
    sign:         s8;
    arity:        u8;
    slots:        [TP_MAX_ARITY] s8;
}

TP_Action_Effect :: struct {
    predicate_id: s32;
    op:           u8;
    arity:        u8;
    slots:        [TP_MAX_ARITY] s8;
}

TP_Numeric_Precondition :: struct {
    function_id: s32;
    cmp_op:      u8;
    arity:       u8;
    slots:       [TP_MAX_ARITY] s8;
    rhs_value:   float;
}

TP_Numeric_Effect :: struct {
    function_id: s32;
    op:          u8;
    arity:       u8;
    slots:       [TP_MAX_ARITY] s8;
    rhs_value:   float;
}

TP_Candidate_Action :: struct {
    schema_id: s32;
    arity:     u8;
    args:      [TP_MAX_PARAMS] s32;
}

TP_Candidate_Action_List :: struct {
    count:   s32;
    actions: *TP_Candidate_Action;
}

TP_Schema_Tensors :: struct {
    predicate_count:      s32;
    function_count:       s32;
    action_count:         s32;
    max_preconditions:    s32;
    max_effects:          s32;
    max_num_preconditions:s32;
    max_num_effects:      s32;
    pred_arity_count:     s32;
    pred_arg_type_count:  s32;
    func_arity_count:     s32;
    func_arg_type_count:  s32;
    action_arity_count:   s32;
    action_arg_type_count:s32;
    pre_pred_id_count:    s32;
    pre_sign_count:       s32;
    pre_slot_count:       s32;
    eff_pred_id_count:    s32;
    eff_op_count:         s32;
    eff_slot_count:       s32;
    num_pre_func_id_count:s32;
    num_pre_cmp_count:    s32;
    num_pre_slot_count:   s32;
    num_pre_value_count:  s32;
    num_eff_func_id_count:s32;
    num_eff_op_count:     s32;
    num_eff_slot_count:   s32;
    num_eff_value_count:  s32;
    pred_arity:           *u8;
    pred_arg_type:        *s32;
    func_arity:           *u8;
    func_arg_type:        *s32;
    action_arity:         *u8;
    action_arg_type:      *s32;
    pre_pred_id:          *s16;
    pre_sign:             *s8;
    pre_slot:             *s8;
    eff_pred_id:          *s16;
    eff_op:               *s8;
    eff_slot:             *s8;
    num_pre_func_id:      *s16;
    num_pre_cmp:          *s8;
    num_pre_slot:         *s8;
    num_pre_value:        *float;
    num_eff_func_id:      *s16;
    num_eff_op:           *s8;
    num_eff_slot:         *s8;
    num_eff_value:        *float;
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

TP_Score_Candidates_Fn :: #type (*
schema: *TP_Schema_Tensors,
problem: *TP_Problem_Tensors,
user_data: *void,
out_action_scores: *float,
out_state_value: *float,
out_has_state_value: *bool
) -> void;

TP_Solve_Result :: struct {
    solved:               bool;
    expansions:           s32;
    generated:            s32;
    candidate_generation_calls: s32;
    index_rebuilds:       s32;
    scorer_calls:         s32;
    plan_length:          s32;
    remaining_goal_count: s32;
    candidate_generation_time_us: s64;
    scorer_export_time_us: s64;
    scorer_time_us:       s64;
    scorer_sort_time_us:  s64;
    plan_actions:         *TP_Candidate_Action;
}

tp_domain_create :: (limits: *TP_Limits) -> *TP_Domain #foreign tensor_planner_lib;
tp_domain_destroy :: (domain: *TP_Domain) #foreign tensor_planner_lib;

tp_domain_add_predicate :: (
domain: *TP_Domain,
predicate_def: *TP_Predicate_Def,
predicate_id_out: *s32
) -> TP_Status #foreign tensor_planner_lib;

tp_domain_add_function :: (
domain: *TP_Domain,
function_def: *TP_Function_Def,
function_id_out: *s32
) -> TP_Status #foreign tensor_planner_lib;

tp_domain_add_action_schema :: (
domain: *TP_Domain,
arity: u8,
arg_types: *s32,
preconditions: *TP_Action_Literal,
precondition_count: s32,
effects: *TP_Action_Effect,
effect_count: s32,
numeric_preconditions: *TP_Numeric_Precondition,
numeric_precondition_count: s32,
numeric_effects: *TP_Numeric_Effect,
numeric_effect_count: s32,
action_id_out: *s32
) -> TP_Status #foreign tensor_planner_lib;

tp_domain_export_schema_tensors :: (
domain: *TP_Domain,
out_tensors: *TP_Schema_Tensors
) -> TP_Status #foreign tensor_planner_lib;

tp_schema_tensors_dispose :: (tensors: *TP_Schema_Tensors) #foreign tensor_planner_lib;

tp_state_create :: (
domain: *TP_Domain,
object_count: s32,
object_types: *s32
) -> *TP_State #foreign tensor_planner_lib;

tp_state_destroy :: (state: *TP_State) #foreign tensor_planner_lib;

tp_state_add_fact :: (
state: *TP_State,
predicate_id: s32,
arity: u8,
args: *s32
) -> TP_Status #foreign tensor_planner_lib;

tp_state_add_goal_fact :: (
state: *TP_State,
predicate_id: s32,
arity: u8,
args: *s32
) -> TP_Status #foreign tensor_planner_lib;

tp_state_set_function_value :: (
state: *TP_State,
function_id: s32,
arity: u8,
args: *s32,
value: float
) -> TP_Status #foreign tensor_planner_lib;

tp_state_generate_candidates :: (
domain: *TP_Domain,
state: *TP_State,
max_candidates: s32,
out_candidates: *TP_Candidate_Action_List
) -> TP_Status #foreign tensor_planner_lib;

tp_candidate_action_list_dispose :: (
list: *TP_Candidate_Action_List
) #foreign tensor_planner_lib;

tp_state_export_problem_tensors :: (
domain: *TP_Domain,
state: *TP_State,
max_candidates: s32,
out_tensors: *TP_Problem_Tensors
) -> TP_Status #foreign tensor_planner_lib;

tp_problem_tensors_dispose :: (tensors: *TP_Problem_Tensors) #foreign tensor_planner_lib;

tp_solver_create :: (domain: *TP_Domain) -> *TP_Solver #foreign tensor_planner_lib;
tp_solver_destroy :: (solver: *TP_Solver) #foreign tensor_planner_lib;

tp_solver_set_mode :: (
solver: *TP_Solver,
mode: TP_Solver_Mode
) -> TP_Status #foreign tensor_planner_lib;

tp_solver_set_scorer :: (
solver: *TP_Solver,
scorer: TP_Score_Candidates_Fn,
user_data: *void
) -> TP_Status #foreign tensor_planner_lib;

tp_solver_use_tensor_baseline_scorer :: (
solver: *TP_Solver
) -> TP_Status #foreign tensor_planner_lib;

tp_solver_solve :: (
solver: *TP_Solver,
initial_state: *TP_State,
out_result: *TP_Solve_Result
) -> TP_Status #foreign tensor_planner_lib;

tp_solver_solve_optimal_debug :: (
solver: *TP_Solver,
initial_state: *TP_State,
out_result: *TP_Solve_Result
) -> TP_Status #foreign tensor_planner_lib;

tp_solve_result_dispose :: (result: *TP_Solve_Result) #foreign tensor_planner_lib;
