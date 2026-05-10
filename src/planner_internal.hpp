#ifndef TENSOR_PLANNER_INTERNAL_HPP
#define TENSOR_PLANNER_INTERNAL_HPP

#include "../include/tensor_planner.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

struct PredicateDef {
  uint8_t arity = 0;
  std::array<int32_t, TP_MAX_ARITY> arg_types {};
};

struct StateIndices;

struct FunctionDef {
  uint8_t arity = 0;
  std::array<int32_t, TP_MAX_ARITY> arg_types {};
};

struct ActionLiteral {
  int32_t predicate_id = -1;
  int8_t sign = 1;
  uint8_t arity = 0;
  std::array<int8_t, TP_MAX_ARITY> slots {};
};

struct ActionEffect {
  int32_t predicate_id = -1;
  uint8_t op = TP_EFFECT_ADD;
  uint8_t arity = 0;
  std::array<int8_t, TP_MAX_ARITY> slots {};
};

struct NumericPrecondition {
  int32_t function_id = -1;
  uint8_t cmp_op = TP_NUM_CMP_EQ;
  uint8_t arity = 0;
  std::array<int8_t, TP_MAX_ARITY> slots {};
  float rhs_value = 0.0f;
};

struct NumericEffect {
  int32_t function_id = -1;
  uint8_t op = TP_NUM_EFFECT_SET;
  uint8_t arity = 0;
  std::array<int8_t, TP_MAX_ARITY> slots {};
  float rhs_value = 0.0f;
};

struct ActionSchema {
  uint8_t arity = 0;
  std::array<int32_t, TP_MAX_PARAMS> arg_types {};
  std::vector<ActionLiteral> preconditions;
  std::vector<ActionEffect> effects;
  std::vector<NumericPrecondition> numeric_preconditions;
  std::vector<NumericEffect> numeric_effects;
};

struct Fact {
  int32_t predicate_id = -1;
  uint8_t arity = 0;
  std::array<int32_t, TP_MAX_ARITY> args {};
};

struct CandidateAction {
  int32_t schema_id = -1;
  uint8_t arity = 0;
  std::array<int32_t, TP_MAX_PARAMS> args {};
};

struct CandidatePattern {
  int32_t schema_id = -1;
  uint8_t arity = 0;
  std::array<int32_t, TP_MAX_PARAMS> args {};
};

struct ReducedTaskPatterns {
  std::vector<CandidatePattern> relevant;
  std::vector<CandidatePattern> reachable;
  std::vector<CandidatePattern> filtered;
};

struct FunctionValue {
  int32_t function_id = -1;
  uint8_t arity = 0;
  std::array<int32_t, TP_MAX_ARITY> args {};
  float value = 0.0f;
};

struct TP_Domain {
  TP_Limits limits {};
  std::vector<PredicateDef> predicates;
  std::vector<FunctionDef> functions;
  std::vector<ActionSchema> actions;
};

struct TP_State {
  const TP_Domain *domain = nullptr;
  std::vector<int32_t> object_types;
  std::vector<Fact> facts;
  std::vector<FunctionValue> function_values;
  std::vector<Fact> goals;
  mutable std::shared_ptr<StateIndices> indices_cache;
  mutable bool indices_dirty = true;
  mutable std::vector<Fact> sorted_facts_cache;
  mutable bool sorted_facts_dirty = true;
  mutable std::vector<FunctionValue> sorted_function_values_cache;
  mutable bool sorted_function_values_dirty = true;
  mutable std::vector<int32_t> signature_cache;
  mutable bool signature_dirty = true;
};

struct TP_Solver {
  const TP_Domain *domain = nullptr;
  TP_Score_Candidates_Fn scorer = nullptr;
  void *scorer_user_data = nullptr;
  TP_Schema_Tensors schema_tensors {};
  bool has_schema_tensors = false;
};

struct SignatureHash {
  std::size_t operator()(const std::vector<int32_t> &values) const noexcept {
    std::size_t seed = values.size();
    for (const int32_t value : values) {
      seed ^= static_cast<std::size_t>(value) + 0x9e3779b9 + (seed << 6U) + (seed >> 2U);
    }
    return seed;
  }
};

bool is_valid_limits(const TP_Limits &limits);
bool is_valid_predicate_def(const TP_Predicate_Def &predicate_def);
bool is_valid_function_def(const TP_Function_Def &function_def);
bool validate_action_schema(
  const TP_Domain &domain,
  uint8_t arity,
  const int32_t *arg_types,
  const TP_Action_Literal *preconditions,
  int32_t precondition_count,
  const TP_Action_Effect *effects,
  int32_t effect_count,
  const TP_Numeric_Precondition *numeric_preconditions,
  int32_t numeric_precondition_count,
  const TP_Numeric_Effect *numeric_effects,
  int32_t numeric_effect_count
);

bool validate_fact(
  const TP_Domain &domain,
  const TP_State &state,
  int32_t predicate_id,
  uint8_t arity,
  const int32_t *args
);

bool validate_function_value(
  const TP_Domain &domain,
  const TP_State &state,
  int32_t function_id,
  uint8_t arity,
  const int32_t *args
);

Fact make_fact(int32_t predicate_id, uint8_t arity, const int32_t *args);
bool fact_equals(const Fact &lhs, const Fact &rhs);
bool action_schema_uses_valid_slots(const ActionSchema &schema);
bool state_has_fact(const TP_State &state, const Fact &fact);
bool add_fact_unique(std::vector<Fact> *facts, const Fact &fact);
bool remove_fact(std::vector<Fact> *facts, const Fact &fact);
int32_t count_unsatisfied_goals(const TP_State &state);
bool upsert_function_value(std::vector<FunctionValue> *values, const FunctionValue &value);
bool try_get_function_value(const TP_State &state, const FunctionValue &query, float *out_value);
const StateIndices &get_or_build_state_indices(const TP_Domain &domain, const TP_State &state, bool *rebuilt);
void invalidate_state_indices(TP_State *state);

std::vector<CandidateAction> generate_candidate_actions(
  const TP_Domain &domain,
  const TP_State &state,
  int32_t max_candidates,
  const std::vector<CandidatePattern> *relevant_patterns = nullptr
);

std::vector<CandidatePattern> derive_relevant_action_patterns(
  const TP_Domain &domain,
  const TP_State &state
);

std::vector<CandidatePattern> derive_reachable_action_patterns(
  const TP_Domain &domain,
  const TP_State &state
);

ReducedTaskPatterns analyze_reduced_task_patterns(
  const TP_Domain &domain,
  const TP_State &state
);

int32_t score_candidate_relevance(
  const TP_Domain &domain,
  const TP_State &state,
  const CandidateAction &candidate
);

bool action_is_applicable(
  const TP_Domain &domain,
  const TP_State &state,
  const CandidateAction &action
);

TP_State apply_action(
  const TP_Domain &domain,
  const TP_State &state,
  const CandidateAction &action
);

const std::vector<int32_t> &make_state_signature(const TP_State &state, bool *rebuilt = nullptr);

TP_Status export_schema_tensors_impl(
  const TP_Domain &domain,
  TP_Schema_Tensors *out_tensors
);

TP_Status export_problem_tensors_impl(
  const TP_Domain &domain,
  const TP_State &state,
  int32_t max_candidates,
  TP_Problem_Tensors *out_tensors
);

TP_Status export_problem_tensors_for_candidates_impl(
  const TP_Domain &domain,
  const TP_State &state,
  const std::vector<CandidateAction> &candidates,
  TP_Problem_Tensors *out_tensors
);

TP_Status export_action_graph_for_candidates_impl(
  const TP_Domain &domain,
  const TP_State &state,
  const std::vector<CandidateAction> &candidates,
  TP_Action_Graph *out_graph
);

void zero_schema_tensors(TP_Schema_Tensors *tensors);
void zero_problem_tensors(TP_Problem_Tensors *tensors);
void zero_action_graph(TP_Action_Graph *graph);
void zero_solve_result(TP_Solve_Result *result);

#endif
