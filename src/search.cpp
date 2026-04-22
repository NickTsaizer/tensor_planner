#include "planner_internal.hpp"

#include <chrono>
#include <deque>

namespace {

constexpr float kSymbolicHeuristicWeight = 1.0f;
constexpr float kActionScoreWeight = 0.05f;

bool mode_uses_scorer(TP_Solver_Mode mode) {
  return mode == TP_SOLVER_MODE_GUIDED_MIXED;
}

float compute_priority(
  TP_Solver_Mode mode,
  int32_t g_cost,
  int32_t symbolic_h,
  bool has_state_value,
  float state_value,
  float action_score
) {
  switch (mode) {
    case TP_SOLVER_MODE_PURE_GREEDY:
      return static_cast<float>(symbolic_h);
    case TP_SOLVER_MODE_SYMBOLIC_ASTAR:
      return static_cast<float>(g_cost) + kSymbolicHeuristicWeight * static_cast<float>(symbolic_h);
    case TP_SOLVER_MODE_GUIDED_MIXED: {
      float priority = static_cast<float>(g_cost) +
        kSymbolicHeuristicWeight * static_cast<float>(symbolic_h);
      (void) has_state_value;
      (void) state_value;
      priority -= kActionScoreWeight * action_score;
      return priority;
    }
    case TP_SOLVER_MODE_OPTIMAL_DEBUG:
      return static_cast<float>(g_cost);
    default:
      return static_cast<float>(symbolic_h);
  }
}

struct SearchNode {
  TP_State state;
  int32_t parent_index = -1;
  int32_t g_cost = 0;
  int32_t h_cost = 0;
  float priority = 0.0f;
  CandidateAction action {};
};

struct QueueEntry {
  int32_t node_index = -1;
  float priority = 0.0f;
  int32_t g_cost = 0;
};

struct GreedyQueueCompare {
  bool operator()(const QueueEntry &lhs, const QueueEntry &rhs) const {
    if (lhs.priority != rhs.priority) {
      return lhs.priority > rhs.priority;
    }
    return lhs.g_cost > rhs.g_cost;
  }
};

void score_and_sort_candidates(
  const TP_Domain &domain,
  const TP_State &state,
  TP_Solver *solver,
  std::vector<CandidateAction> *candidates,
  std::vector<float> *out_action_scores,
  float *out_state_value,
  bool *out_has_state_value,
  int32_t *scorer_calls,
  int64_t *scorer_export_time_us,
  int64_t *scorer_time_us,
  int64_t *scorer_sort_time_us
) {
  if (solver == nullptr || solver->scorer == nullptr || candidates->empty() || !mode_uses_scorer(solver->mode)) {
    out_action_scores->assign(candidates->size(), 0.0f);
    *out_state_value = 0.0f;
    *out_has_state_value = false;
    return;
  }

  TP_Problem_Tensors tensors {};
  const auto export_start = std::chrono::steady_clock::now();
  if (export_problem_tensors_for_candidates_impl(domain, state, *candidates, &tensors) != TP_STATUS_OK) {
    return;
  }
  const auto export_end = std::chrono::steady_clock::now();
  *scorer_export_time_us += std::chrono::duration_cast<std::chrono::microseconds>(export_end - export_start).count();

  std::vector<float> scores(candidates->size(), 0.0f);
  float state_value = 0.0f;
  bool has_state_value = false;

  const auto scorer_start = std::chrono::steady_clock::now();
  solver->scorer(
    solver->has_schema_tensors ? &solver->schema_tensors : nullptr,
    &tensors,
    solver->scorer_user_data,
    scores.data(),
    &state_value,
    &has_state_value
  );
  const auto scorer_end = std::chrono::steady_clock::now();

  *scorer_time_us += std::chrono::duration_cast<std::chrono::microseconds>(scorer_end - scorer_start).count();
  ++(*scorer_calls);
  *out_state_value = state_value;
  *out_has_state_value = has_state_value;

  std::vector<std::size_t> order(candidates->size());
  for (std::size_t index = 0; index < order.size(); ++index) {
    order[index] = index;
  }

  const auto sort_start = std::chrono::steady_clock::now();
  std::sort(order.begin(), order.end(), [&scores, candidates](std::size_t lhs, std::size_t rhs) {
    if (scores[lhs] != scores[rhs]) {
      return scores[lhs] > scores[rhs];
    }
    if ((*candidates)[lhs].schema_id != (*candidates)[rhs].schema_id) {
      return (*candidates)[lhs].schema_id < (*candidates)[rhs].schema_id;
    }
    return (*candidates)[lhs].args < (*candidates)[rhs].args;
  });

  std::vector<CandidateAction> sorted;
  sorted.reserve(candidates->size());
  for (const std::size_t index : order) {
    sorted.push_back((*candidates)[index]);
    out_action_scores->push_back(scores[index]);
  }
  const auto sort_end = std::chrono::steady_clock::now();
  *scorer_sort_time_us += std::chrono::duration_cast<std::chrono::microseconds>(sort_end - sort_start).count();
  *candidates = std::move(sorted);
  tp_problem_tensors_dispose(&tensors);
}

void copy_candidate_action(const CandidateAction &source, TP_Candidate_Action *target) {
  target->schema_id = source.schema_id;
  target->arity = source.arity;
  for (int32_t index = 0; index < TP_MAX_PARAMS; ++index) {
    target->args[index] = source.args[static_cast<std::size_t>(index)];
  }
}

TP_Status fill_solve_result(
  const std::vector<SearchNode> &nodes,
  int32_t solved_index,
  int32_t expansions,
  int32_t generated,
  int32_t candidate_generation_calls,
  int32_t index_rebuilds,
  int32_t scorer_calls,
  int64_t candidate_generation_time_us,
  int64_t scorer_export_time_us,
  int64_t scorer_time_us,
  int64_t scorer_sort_time_us,
  TP_Solve_Result *out_result
) {
  out_result->expansions = expansions;
  out_result->generated = generated;
  out_result->candidate_generation_calls = candidate_generation_calls;
  out_result->index_rebuilds = index_rebuilds;
  out_result->scorer_calls = scorer_calls;
  out_result->candidate_generation_time_us = candidate_generation_time_us;
  out_result->scorer_export_time_us = scorer_export_time_us;
  out_result->scorer_time_us = scorer_time_us;
  out_result->scorer_sort_time_us = scorer_sort_time_us;

  if (solved_index < 0) {
    out_result->solved = false;
    out_result->remaining_goal_count = count_unsatisfied_goals(nodes.front().state);
    return TP_STATUS_NO_SOLUTION;
  }

  std::vector<CandidateAction> reversed_plan;
  for (int32_t index = solved_index; index > 0; index = nodes[static_cast<std::size_t>(index)].parent_index) {
    reversed_plan.push_back(nodes[static_cast<std::size_t>(index)].action);
  }
  std::reverse(reversed_plan.begin(), reversed_plan.end());

  out_result->solved = true;
  out_result->plan_length = static_cast<int32_t>(reversed_plan.size());
  out_result->remaining_goal_count = 0;
  out_result->plan_actions = static_cast<TP_Candidate_Action *>(
    std::calloc(static_cast<std::size_t>(out_result->plan_length), sizeof(TP_Candidate_Action))
  );

  for (int32_t index = 0; index < out_result->plan_length; ++index) {
    copy_candidate_action(reversed_plan[static_cast<std::size_t>(index)], &out_result->plan_actions[index]);
  }

  return TP_STATUS_OK;
}

TP_Status solve_priority_search(
  TP_Solver *solver,
  const TP_Domain &domain,
  const TP_State &initial_state,
  TP_Solve_Result *out_result
) {
  std::priority_queue<QueueEntry, std::vector<QueueEntry>, GreedyQueueCompare> open_set;
  std::unordered_set<std::vector<int32_t>, SignatureHash> visited;
  std::vector<SearchNode> nodes;
  nodes.reserve(static_cast<std::size_t>(domain.limits.max_expansions));

  SearchNode root {};
  root.state = initial_state;
  root.h_cost = count_unsatisfied_goals(root.state);
  root.priority = compute_priority(solver->mode, 0, root.h_cost, false, 0.0f, 0.0f);
  nodes.push_back(root);
  open_set.push({0, root.priority, 0});
  visited.insert(make_state_signature(root.state));

  int32_t expansions = 0;
  int32_t generated = 0;
  int32_t candidate_generation_calls = 0;
  int32_t index_rebuilds = 0;
  int32_t scorer_calls = 0;
  int64_t candidate_generation_time_us = 0;
  int64_t scorer_export_time_us = 0;
  int64_t scorer_time_us = 0;
  int64_t scorer_sort_time_us = 0;
  int32_t solved_index = -1;

  while (!open_set.empty() && expansions < domain.limits.max_expansions) {
    const QueueEntry current_entry = open_set.top();
    open_set.pop();
    const SearchNode current = nodes[static_cast<std::size_t>(current_entry.node_index)];
    ++expansions;

    if (current.h_cost == 0) {
      solved_index = current_entry.node_index;
      break;
    }

    if (current.g_cost >= domain.limits.max_plan_length) {
      continue;
    }

    bool rebuilt = false;
    get_or_build_state_indices(domain, current.state, &rebuilt);
    if (rebuilt) {
      ++index_rebuilds;
    }

    const auto candidate_start = std::chrono::steady_clock::now();
    std::vector<CandidateAction> candidates = generate_candidate_actions(
      domain,
      current.state,
      domain.limits.max_candidates
    );
    const auto candidate_end = std::chrono::steady_clock::now();
    candidate_generation_time_us += std::chrono::duration_cast<std::chrono::microseconds>(
      candidate_end - candidate_start
    ).count();
    ++candidate_generation_calls;
    float state_value = 0.0f;
    bool has_state_value = false;
    std::vector<float> action_scores;
    score_and_sort_candidates(
      domain,
      current.state,
      solver,
      &candidates,
      &action_scores,
      &state_value,
      &has_state_value,
      &scorer_calls,
      &scorer_export_time_us,
      &scorer_time_us,
      &scorer_sort_time_us
    );
    generated += static_cast<int32_t>(candidates.size());

    for (std::size_t candidate_index = 0; candidate_index < candidates.size(); ++candidate_index) {
      const CandidateAction &candidate = candidates[candidate_index];
      TP_State next_state = apply_action(domain, current.state, candidate);
      std::vector<int32_t> signature = make_state_signature(next_state);
      if (visited.find(signature) != visited.end()) {
        continue;
      }

      SearchNode child {};
      child.state = std::move(next_state);
      child.parent_index = current_entry.node_index;
      child.g_cost = current.g_cost + 1;
      child.h_cost = count_unsatisfied_goals(child.state);
      const float action_score = candidate_index < action_scores.size() ? action_scores[candidate_index] : 0.0f;
      child.priority = compute_priority(
        solver->mode,
        child.g_cost,
        child.h_cost,
        has_state_value,
        state_value,
        action_score
      );
      child.action = candidate;

      const int32_t child_index = static_cast<int32_t>(nodes.size());
      nodes.push_back(std::move(child));
      open_set.push({child_index, nodes.back().priority, nodes.back().g_cost});
      visited.insert(std::move(signature));

      if (nodes.back().h_cost == 0) {
        solved_index = child_index;
        break;
      }
    }

    if (solved_index >= 0) {
      break;
    }
  }

  return fill_solve_result(
    nodes,
    solved_index,
    expansions,
    generated,
    candidate_generation_calls,
    index_rebuilds,
    scorer_calls,
    candidate_generation_time_us,
    scorer_export_time_us,
    scorer_time_us,
    scorer_sort_time_us,
    out_result
  );
}

TP_Status solve_optimal_debug(
  TP_Solver *solver,
  const TP_Domain &domain,
  const TP_State &initial_state,
  TP_Solve_Result *out_result
) {
  std::deque<int32_t> frontier;
  std::unordered_set<std::vector<int32_t>, SignatureHash> visited;
  std::vector<SearchNode> nodes;
  nodes.reserve(static_cast<std::size_t>(domain.limits.max_expansions));

  SearchNode root {};
  root.state = initial_state;
  root.h_cost = count_unsatisfied_goals(root.state);
  root.priority = compute_priority(TP_SOLVER_MODE_OPTIMAL_DEBUG, 0, root.h_cost, false, 0.0f, 0.0f);
  nodes.push_back(root);
  frontier.push_back(0);
  visited.insert(make_state_signature(root.state));

  int32_t expansions = 0;
  int32_t generated = 0;
  int32_t candidate_generation_calls = 0;
  int32_t index_rebuilds = 0;
  int32_t scorer_calls = 0;
  int64_t candidate_generation_time_us = 0;
  int64_t scorer_export_time_us = 0;
  int64_t scorer_time_us = 0;
  int64_t scorer_sort_time_us = 0;
  int32_t solved_index = -1;

  while (!frontier.empty() && expansions < domain.limits.max_expansions) {
    const int32_t current_index = frontier.front();
    frontier.pop_front();
    const SearchNode current = nodes[static_cast<std::size_t>(current_index)];
    ++expansions;

    if (current.h_cost == 0) {
      solved_index = current_index;
      break;
    }

    if (current.g_cost >= domain.limits.max_plan_length) {
      continue;
    }

    bool rebuilt = false;
    get_or_build_state_indices(domain, current.state, &rebuilt);
    if (rebuilt) {
      ++index_rebuilds;
    }

    const auto candidate_start = std::chrono::steady_clock::now();
    std::vector<CandidateAction> candidates = generate_candidate_actions(
      domain,
      current.state,
      domain.limits.max_candidates
    );
    const auto candidate_end = std::chrono::steady_clock::now();
    candidate_generation_time_us += std::chrono::duration_cast<std::chrono::microseconds>(
      candidate_end - candidate_start
    ).count();
    ++candidate_generation_calls;
    float state_value = 0.0f;
    bool has_state_value = false;
    std::vector<float> action_scores;
    score_and_sort_candidates(
      domain,
      current.state,
      solver,
      &candidates,
      &action_scores,
      &state_value,
      &has_state_value,
      &scorer_calls,
      &scorer_export_time_us,
      &scorer_time_us,
      &scorer_sort_time_us
    );
    generated += static_cast<int32_t>(candidates.size());

    for (std::size_t candidate_index = 0; candidate_index < candidates.size(); ++candidate_index) {
      const CandidateAction &candidate = candidates[candidate_index];
      TP_State next_state = apply_action(domain, current.state, candidate);
      std::vector<int32_t> signature = make_state_signature(next_state);
      if (visited.find(signature) != visited.end()) {
        continue;
      }

      SearchNode child {};
      child.state = std::move(next_state);
      child.parent_index = current_index;
      child.g_cost = current.g_cost + 1;
      child.h_cost = count_unsatisfied_goals(child.state);
      const float action_score = candidate_index < action_scores.size() ? action_scores[candidate_index] : 0.0f;
      child.priority = compute_priority(
        TP_SOLVER_MODE_OPTIMAL_DEBUG,
        child.g_cost,
        child.h_cost,
        has_state_value,
        state_value,
        action_score
      );
      child.action = candidate;

      const int32_t child_index = static_cast<int32_t>(nodes.size());
      nodes.push_back(std::move(child));
      frontier.push_back(child_index);
      visited.insert(std::move(signature));
    }
  }

  if (solved_index < 0) {
    for (int32_t index = 0; index < static_cast<int32_t>(nodes.size()); ++index) {
      if (nodes[static_cast<std::size_t>(index)].h_cost == 0) {
        solved_index = index;
        break;
      }
    }
  }

  return fill_solve_result(
    nodes,
    solved_index,
    expansions,
    generated,
    candidate_generation_calls,
    index_rebuilds,
    scorer_calls,
    candidate_generation_time_us,
    scorer_export_time_us,
    scorer_time_us,
    scorer_sort_time_us,
    out_result
  );
}

}  // namespace

void zero_solve_result(TP_Solve_Result *result) {
  if (result != nullptr) {
    std::memset(result, 0, sizeof(TP_Solve_Result));
  }
}

TP_Status tp_solver_solve(
  TP_Solver *solver,
  const TP_State *initial_state,
  TP_Solve_Result *out_result
) {
  if (solver == nullptr || initial_state == nullptr || out_result == nullptr) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  zero_solve_result(out_result);
  if (solver->mode == TP_SOLVER_MODE_OPTIMAL_DEBUG) {
    return solve_optimal_debug(solver, *solver->domain, *initial_state, out_result);
  }
  return solve_priority_search(solver, *solver->domain, *initial_state, out_result);
}

TP_Status tp_solver_solve_optimal_debug(
  TP_Solver *solver,
  const TP_State *initial_state,
  TP_Solve_Result *out_result
) {
  if (solver == nullptr || initial_state == nullptr || out_result == nullptr) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  zero_solve_result(out_result);
  return solve_optimal_debug(solver, *solver->domain, *initial_state, out_result);
}
