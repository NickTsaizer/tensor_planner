#ifndef TENSOR_PLANNER_H
#define TENSOR_PLANNER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TP_MAX_ARITY 4
#define TP_MAX_PARAMS 6

typedef struct TP_Domain TP_Domain;
typedef struct TP_State TP_State;
typedef struct TP_Solver TP_Solver;

typedef enum TP_Status {
  TP_STATUS_OK = 0,
  TP_STATUS_INVALID_ARGUMENT = 1,
  TP_STATUS_LIMIT_EXCEEDED = 2,
  TP_STATUS_NOT_FOUND = 3,
  TP_STATUS_UNSUPPORTED = 4,
  TP_STATUS_NO_SOLUTION = 5
} TP_Status;

typedef enum TP_Effect_Op {
  TP_EFFECT_ADD = 1,
  TP_EFFECT_DELETE = 2
} TP_Effect_Op;

typedef enum TP_Num_Compare_Op {
  TP_NUM_CMP_LT = 1,
  TP_NUM_CMP_LTE = 2,
  TP_NUM_CMP_EQ = 3,
  TP_NUM_CMP_GTE = 4,
  TP_NUM_CMP_GT = 5
} TP_Num_Compare_Op;

typedef enum TP_Num_Effect_Op {
  TP_NUM_EFFECT_SET = 1,
  TP_NUM_EFFECT_ADD = 2,
  TP_NUM_EFFECT_SUBTRACT = 3
} TP_Num_Effect_Op;

typedef struct TP_Limits {
  int32_t max_objects;
  int32_t max_facts;
  int32_t max_goals;
  int32_t max_candidates;
  int32_t max_expansions;
  int32_t max_plan_length;
} TP_Limits;

typedef struct TP_Predicate_Def {
  uint8_t arity;
  int32_t arg_types[TP_MAX_ARITY];
} TP_Predicate_Def;

typedef struct TP_Function_Def {
  uint8_t arity;
  int32_t arg_types[TP_MAX_ARITY];
} TP_Function_Def;

typedef struct TP_Action_Literal {
  int32_t predicate_id;
  int8_t sign;
  uint8_t arity;
  int8_t slots[TP_MAX_ARITY];
} TP_Action_Literal;

typedef struct TP_Action_Effect {
  int32_t predicate_id;
  uint8_t op;
  uint8_t arity;
  int8_t slots[TP_MAX_ARITY];
} TP_Action_Effect;

typedef struct TP_Numeric_Precondition {
  int32_t function_id;
  uint8_t cmp_op;
  uint8_t arity;
  int8_t slots[TP_MAX_ARITY];
  float rhs_value;
} TP_Numeric_Precondition;

typedef struct TP_Numeric_Effect {
  int32_t function_id;
  uint8_t op;
  uint8_t arity;
  int8_t slots[TP_MAX_ARITY];
  float rhs_value;
} TP_Numeric_Effect;

typedef struct TP_Candidate_Action {
  int32_t schema_id;
  uint8_t arity;
  int32_t args[TP_MAX_PARAMS];
} TP_Candidate_Action;

typedef struct TP_Candidate_Action_List {
  int32_t count;
  TP_Candidate_Action *actions;
} TP_Candidate_Action_List;

typedef struct TP_Schema_Tensors {
  int32_t predicate_count;
  int32_t function_count;
  int32_t action_count;
  int32_t max_preconditions;
  int32_t max_effects;
  int32_t max_num_preconditions;
  int32_t max_num_effects;
  int32_t pred_arity_count;
  int32_t pred_arg_type_count;
  int32_t func_arity_count;
  int32_t func_arg_type_count;
  int32_t action_arity_count;
  int32_t action_arg_type_count;
  int32_t pre_pred_id_count;
  int32_t pre_sign_count;
  int32_t pre_slot_count;
  int32_t eff_pred_id_count;
  int32_t eff_op_count;
  int32_t eff_slot_count;
  int32_t num_pre_func_id_count;
  int32_t num_pre_cmp_count;
  int32_t num_pre_slot_count;
  int32_t num_pre_value_count;
  int32_t num_eff_func_id_count;
  int32_t num_eff_op_count;
  int32_t num_eff_slot_count;
  int32_t num_eff_value_count;
  uint8_t *pred_arity;
  int32_t *pred_arg_type;
  uint8_t *func_arity;
  int32_t *func_arg_type;
  uint8_t *action_arity;
  int32_t *action_arg_type;
  int16_t *pre_pred_id;
  int8_t *pre_sign;
  int8_t *pre_slot;
  int16_t *eff_pred_id;
  int8_t *eff_op;
  int8_t *eff_slot;
  int16_t *num_pre_func_id;
  int8_t *num_pre_cmp;
  int8_t *num_pre_slot;
  float *num_pre_value;
  int16_t *num_eff_func_id;
  int8_t *num_eff_op;
  int8_t *num_eff_slot;
  float *num_eff_value;
} TP_Schema_Tensors;

typedef struct TP_Problem_Tensors {
  int32_t object_count;
  int32_t fact_count;
  int32_t function_value_count;
  int32_t goal_count;
  int32_t candidate_count;
  int32_t obj_type_count;
  int32_t true_pred_id_count;
  int32_t true_pred_arg_count;
  int32_t true_pred_mask_count;
  int32_t num_func_id_count;
  int32_t num_func_arg_count;
  int32_t num_value_count;
  int32_t goal_pred_id_count;
  int32_t goal_pred_arg_count;
  int32_t goal_pred_mask_count;
  int32_t cand_action_schema_count;
  int32_t cand_action_arg_count;
  int32_t cand_action_mask_count;
  int16_t *obj_type;
  int16_t *true_pred_id;
  int16_t *true_pred_arg;
  uint8_t *true_pred_mask;
  int16_t *num_func_id;
  int16_t *num_func_arg;
  float *num_value;
  int16_t *goal_pred_id;
  int16_t *goal_pred_arg;
  uint8_t *goal_pred_mask;
  int16_t *cand_action_schema;
  int16_t *cand_action_arg;
  uint8_t *cand_action_mask;
} TP_Problem_Tensors;

typedef struct TP_Action_Graph {
  int32_t object_node_count;
  int32_t fact_node_count;
  int32_t action_node_count;
  int32_t total_node_count;
  int32_t edge_count;
  int32_t node_kind_count;
  int32_t edge_src_count;
  int32_t edge_dst_count;
  int32_t edge_type_count;
  uint8_t *node_kind;
  int32_t *edge_src;
  int32_t *edge_dst;
  int32_t *edge_type;
} TP_Action_Graph;

typedef void (*TP_Score_Candidates_Fn)(
  const TP_Schema_Tensors *schema,
  const TP_Problem_Tensors *problem,
  void *user_data,
  float *out_action_scores,
  float *out_state_value,
  bool *out_has_state_value
);

typedef struct TP_Solve_Result {
  bool solved;
  int32_t expansions;
  int32_t generated;
  int32_t candidate_generation_calls;
  int32_t index_rebuilds;
  int32_t scorer_calls;
  int32_t plan_length;
  int32_t remaining_goal_count;
  int64_t candidate_generation_time_us;
  int64_t scorer_export_time_us;
  int64_t scorer_time_us;
  int64_t scorer_sort_time_us;
  TP_Candidate_Action *plan_actions;
} TP_Solve_Result;

TP_Domain *tp_domain_create(const TP_Limits *limits);
void tp_domain_destroy(TP_Domain *domain);

TP_Status tp_domain_add_predicate(
  TP_Domain *domain,
  const TP_Predicate_Def *predicate_def,
  int32_t *predicate_id_out
);

TP_Status tp_domain_add_function(
  TP_Domain *domain,
  const TP_Function_Def *function_def,
  int32_t *function_id_out
);

TP_Status tp_domain_add_action_schema(
  TP_Domain *domain,
  uint8_t arity,
  const int32_t *arg_types,
  const TP_Action_Literal *preconditions,
  int32_t precondition_count,
  const TP_Action_Effect *effects,
  int32_t effect_count,
  const TP_Numeric_Precondition *numeric_preconditions,
  int32_t numeric_precondition_count,
  const TP_Numeric_Effect *numeric_effects,
  int32_t numeric_effect_count,
  int32_t *action_id_out
);

TP_Status tp_domain_export_schema_tensors(
  const TP_Domain *domain,
  TP_Schema_Tensors *out_tensors
);

void tp_schema_tensors_dispose(TP_Schema_Tensors *tensors);

TP_State *tp_state_create(
  const TP_Domain *domain,
  int32_t object_count,
  const int32_t *object_types
);

void tp_state_destroy(TP_State *state);

TP_Status tp_state_add_fact(
  TP_State *state,
  int32_t predicate_id,
  uint8_t arity,
  const int32_t *args
);

TP_Status tp_state_add_goal_fact(
  TP_State *state,
  int32_t predicate_id,
  uint8_t arity,
  const int32_t *args
);

TP_Status tp_state_set_function_value(
  TP_State *state,
  int32_t function_id,
  uint8_t arity,
  const int32_t *args,
  float value
);

TP_Status tp_state_generate_candidates(
  const TP_Domain *domain,
  const TP_State *state,
  int32_t max_candidates,
  TP_Candidate_Action_List *out_candidates
);

void tp_candidate_action_list_dispose(TP_Candidate_Action_List *list);

TP_Status tp_state_export_problem_tensors(
  const TP_Domain *domain,
  const TP_State *state,
  int32_t max_candidates,
  TP_Problem_Tensors *out_tensors
);

TP_Status tp_state_export_action_graph(
  const TP_Domain *domain,
  const TP_State *state,
  int32_t max_candidates,
  TP_Action_Graph *out_graph
);

void tp_problem_tensors_dispose(TP_Problem_Tensors *tensors);
void tp_action_graph_dispose(TP_Action_Graph *graph);

TP_Solver *tp_solver_create(const TP_Domain *domain);
void tp_solver_destroy(TP_Solver *solver);

TP_Status tp_solver_set_scorer(
  TP_Solver *solver,
  TP_Score_Candidates_Fn scorer,
  void *user_data
);

TP_Status tp_solver_use_tensor_baseline_scorer(TP_Solver *solver);

TP_Status tp_solver_solve(
  TP_Solver *solver,
  const TP_State *initial_state,
  TP_Solve_Result *out_result
);

void tp_solve_result_dispose(TP_Solve_Result *result);

#ifdef __cplusplus
}
#endif

#endif
