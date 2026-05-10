#include "planner_internal.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>

namespace {

constexpr float kHeuristicWeight = 1.0f;
constexpr float kActionScoreWeight = 0.08f;
constexpr float kStateValueWeight = 0.02f;
constexpr int32_t kInitialGuidedGroundingBudget = 256;
constexpr int32_t kGuidedGroundingTarget = 512;
constexpr int32_t kGuidedGroundingWidenFactor = 2;
constexpr std::size_t kHeuristicPrefilterKeep = 192;
constexpr std::size_t kPreferredKeep = 96;
constexpr std::size_t kModelKeep = 64;
constexpr std::size_t kUncertainKeep = 16;

bool signature_trace_enabled() {
  static const bool enabled = [] {
    const char *value = std::getenv("TP_TRACE_SIGNATURES");
    return value != nullptr && value[0] != '\0' && value[0] != '0';
  }();
  return enabled;
}

void log_signature_metrics(
  int32_t requests,
  int32_t rebuilds,
  int64_t time_us,
  std::size_t visited_count
) {
  if (!signature_trace_enabled()) {
    return;
  }

  std::cerr << "signature_metrics"
            << " requests=" << requests
            << " rebuilds=" << rebuilds
            << " cache_hits=" << (requests - rebuilds)
            << " time_us=" << time_us
            << " visited=" << visited_count
            << '\n';
}

float compute_priority(
  int32_t g_cost,
  int32_t heuristic_cost,
  bool has_state_value,
  float state_value,
  float action_score
) {
  float priority = static_cast<float>(g_cost) + kHeuristicWeight * static_cast<float>(heuristic_cost);
  if (has_state_value) {
    priority -= kStateValueWeight * state_value;
  }
  priority -= kActionScoreWeight * action_score;
  return priority;
}

struct SearchNode {
  TP_State state;
  int32_t parent_index = -1;
  int32_t g_cost = 0;
  int32_t h_cost = 0;
  float priority = 0.0f;
  float guidance_score = 0.0f;
  CandidateAction action {};
};

struct RankedCandidate {
  CandidateAction action;
  int32_t score = 0;
};

struct GuidedCandidate {
  CandidateAction action;
  int32_t heuristic_score = 0;
  float model_score = 0.0f;
};

struct QueueEntry {
  int32_t node_index = -1;
  float priority = 0.0f;
  int32_t g_cost = 0;
};

struct GuidedQueueCompare {
  bool operator()(const QueueEntry &lhs, const QueueEntry &rhs) const {
    if (lhs.priority != rhs.priority) {
      return lhs.priority > rhs.priority;
    }
    return lhs.g_cost > rhs.g_cost;
  }
};

template <typename Less>
std::vector<std::size_t> select_top_indices(std::size_t count, std::size_t keep_count, Less less) {
  std::vector<std::size_t> indices;
  if (count == 0 || keep_count == 0) {
    return indices;
  }

  if (keep_count >= count) {
    indices.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
      indices[index] = index;
    }
    std::sort(indices.begin(), indices.end(), less);
    return indices;
  }

  const auto worse = [&less](std::size_t lhs, std::size_t rhs) {
    return less(rhs, lhs);
  };

  indices.reserve(keep_count);
  for (std::size_t index = 0; index < count; ++index) {
    if (indices.size() < keep_count) {
      indices.push_back(index);
      std::push_heap(indices.begin(), indices.end(), worse);
      continue;
    }

    if (less(index, indices.front())) {
      std::pop_heap(indices.begin(), indices.end(), worse);
      indices.back() = index;
      std::push_heap(indices.begin(), indices.end(), worse);
    }
  }

  std::sort(indices.begin(), indices.end(), less);
  return indices;
}

void score_candidate_models(
  const TP_Domain &domain,
  const TP_State &state,
  TP_Solver *solver,
  const std::vector<CandidateAction> &candidates,
  std::vector<float> *out_action_scores,
  float *out_state_value,
  bool *out_has_state_value,
  int32_t *scorer_calls,
  int64_t *scorer_export_time_us,
  int64_t *scorer_time_us,
  int64_t *
) {
  out_action_scores->assign(candidates.size(), 0.0f);
  if (solver == nullptr || solver->scorer == nullptr || candidates.empty()) {
    *out_state_value = 0.0f;
    *out_has_state_value = false;
    return;
  }

  TP_Problem_Tensors tensors {};
  const auto export_start = std::chrono::steady_clock::now();
  if (export_problem_tensors_for_candidates_impl(domain, state, candidates, &tensors) != TP_STATUS_OK) {
    *out_state_value = 0.0f;
    *out_has_state_value = false;
    return;
  }
  const auto export_end = std::chrono::steady_clock::now();
  *scorer_export_time_us += std::chrono::duration_cast<std::chrono::microseconds>(export_end - export_start).count();

  float state_value = 0.0f;
  bool has_state_value = false;

  const auto scorer_start = std::chrono::steady_clock::now();
  solver->scorer(
    solver->has_schema_tensors ? &solver->schema_tensors : nullptr,
    &tensors,
    solver->scorer_user_data,
    out_action_scores->data(),
    &state_value,
    &has_state_value
  );
  const auto scorer_end = std::chrono::steady_clock::now();

  *scorer_time_us += std::chrono::duration_cast<std::chrono::microseconds>(scorer_end - scorer_start).count();
  ++(*scorer_calls);
  *out_state_value = state_value;
  *out_has_state_value = has_state_value;
  tp_problem_tensors_dispose(&tensors);
}

std::vector<GuidedCandidate> build_guided_candidates(
  const TP_Domain &domain,
  const TP_State &state,
  const std::vector<CandidateAction> &candidates,
  const std::vector<float> &action_scores
) {
  std::vector<GuidedCandidate> guided_candidates;
  guided_candidates.reserve(candidates.size());
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    const float model_score = index < action_scores.size() ? action_scores[index] : 0.0f;
    guided_candidates.push_back({
      candidates[index],
      score_candidate_relevance(domain, state, candidates[index]),
      model_score
    });
  }
  return guided_candidates;
}

void shortlist_guided_candidates(std::vector<GuidedCandidate> *candidates, int64_t *sort_time_us) {
  if (candidates->empty()) {
    return;
  }

  const auto sort_start = std::chrono::steady_clock::now();
  const auto preferred_less = [candidates](std::size_t lhs, std::size_t rhs) {
    const GuidedCandidate &left = (*candidates)[lhs];
    const GuidedCandidate &right = (*candidates)[rhs];
    if (left.heuristic_score != right.heuristic_score) {
      return left.heuristic_score > right.heuristic_score;
    }
    if (left.model_score != right.model_score) {
      return left.model_score > right.model_score;
    }
    if (left.action.schema_id != right.action.schema_id) {
      return left.action.schema_id < right.action.schema_id;
    }
    return left.action.args < right.action.args;
  };
  const auto model_less = [candidates](std::size_t lhs, std::size_t rhs) {
    const GuidedCandidate &left = (*candidates)[lhs];
    const GuidedCandidate &right = (*candidates)[rhs];
    if (left.model_score != right.model_score) {
      return left.model_score > right.model_score;
    }
    if (left.heuristic_score != right.heuristic_score) {
      return left.heuristic_score > right.heuristic_score;
    }
    if (left.action.schema_id != right.action.schema_id) {
      return left.action.schema_id < right.action.schema_id;
    }
    return left.action.args < right.action.args;
  };
  const auto uncertainty_less = [candidates](std::size_t lhs, std::size_t rhs) {
    const GuidedCandidate &left = (*candidates)[lhs];
    const GuidedCandidate &right = (*candidates)[rhs];
    const float left_uncertainty = std::abs(left.model_score);
    const float right_uncertainty = std::abs(right.model_score);
    if (left_uncertainty != right_uncertainty) {
      return left_uncertainty < right_uncertainty;
    }
    if (left.heuristic_score != right.heuristic_score) {
      return left.heuristic_score > right.heuristic_score;
    }
    if (left.action.schema_id != right.action.schema_id) {
      return left.action.schema_id < right.action.schema_id;
    }
    return left.action.args < right.action.args;
  };
  const auto final_less = [candidates](std::size_t lhs, std::size_t rhs) {
    const GuidedCandidate &left = (*candidates)[lhs];
    const GuidedCandidate &right = (*candidates)[rhs];
    if (left.model_score != right.model_score) {
      return left.model_score > right.model_score;
    }
    if (left.heuristic_score != right.heuristic_score) {
      return left.heuristic_score > right.heuristic_score;
    }
    if (left.action.schema_id != right.action.schema_id) {
      return left.action.schema_id < right.action.schema_id;
    }
    return left.action.args < right.action.args;
  };

  const auto preferred_order = select_top_indices(candidates->size(), kPreferredKeep, preferred_less);
  const auto model_order = select_top_indices(candidates->size(), kModelKeep, model_less);
  const auto uncertainty_order = select_top_indices(candidates->size(), kUncertainKeep, uncertainty_less);

  std::vector<uint8_t> keep(candidates->size(), 0);
  for (const std::size_t index : preferred_order) {
    keep[index] = 1;
  }
  for (const std::size_t index : model_order) {
    keep[index] = 1;
  }
  for (const std::size_t index : uncertainty_order) {
    keep[index] = 1;
  }

  std::vector<std::size_t> shortlisted_indices;
  shortlisted_indices.reserve(preferred_order.size() + model_order.size() + uncertainty_order.size());
  for (std::size_t index = 0; index < candidates->size(); ++index) {
    if (keep[index]) {
      shortlisted_indices.push_back(index);
    }
  }

  if (!shortlisted_indices.empty()) {
    std::sort(shortlisted_indices.begin(), shortlisted_indices.end(), final_less);

    std::vector<GuidedCandidate> shortlisted_candidates;
    shortlisted_candidates.reserve(shortlisted_indices.size());
    for (const std::size_t index : shortlisted_indices) {
      shortlisted_candidates.push_back((*candidates)[index]);
    }
    *candidates = std::move(shortlisted_candidates);
  }
  const auto sort_end = std::chrono::steady_clock::now();
  if (sort_time_us != nullptr) {
    *sort_time_us += std::chrono::duration_cast<std::chrono::microseconds>(sort_end - sort_start).count();
  }
}

void shortlist_heuristic_candidates(
  const TP_Domain &domain,
  const TP_State &state,
  std::vector<CandidateAction> *candidates,
  int64_t *sort_time_us
) {
  if (candidates->empty()) {
    return;
  }

  const auto sort_start = std::chrono::steady_clock::now();
  std::vector<RankedCandidate> scored;
  scored.reserve(candidates->size());
  for (const CandidateAction &candidate : *candidates) {
    scored.push_back({candidate, score_candidate_relevance(domain, state, candidate)});
  }

  const auto top_indices = select_top_indices(scored.size(), kPreferredKeep, [&scored](std::size_t lhs, std::size_t rhs) {
    if (scored[lhs].score != scored[rhs].score) {
      return scored[lhs].score > scored[rhs].score;
    }
    if (scored[lhs].action.schema_id != scored[rhs].action.schema_id) {
      return scored[lhs].action.schema_id < scored[rhs].action.schema_id;
    }
    return scored[lhs].action.args < scored[rhs].action.args;
  });

  std::vector<CandidateAction> shortlisted_candidates;
  shortlisted_candidates.reserve(top_indices.size());
  for (const std::size_t index : top_indices) {
    shortlisted_candidates.push_back(scored[index].action);
  }
  *candidates = std::move(shortlisted_candidates);

  const auto sort_end = std::chrono::steady_clock::now();
  if (sort_time_us != nullptr) {
    *sort_time_us += std::chrono::duration_cast<std::chrono::microseconds>(sort_end - sort_start).count();
  }
}

bool solver_has_model_scorer(const TP_Solver *solver) {
  return solver != nullptr && solver->scorer != nullptr;
}

void heuristic_prefilter_candidates(
  const TP_Domain &domain,
  const TP_State &state,
  std::vector<CandidateAction> *candidates
) {
  if (candidates->size() <= kHeuristicPrefilterKeep) {
    return;
  }

  std::vector<RankedCandidate> scored;
  scored.reserve(candidates->size());
  for (const CandidateAction &candidate : *candidates) {
    scored.push_back({candidate, score_candidate_relevance(domain, state, candidate)});
  }

  const auto top_indices = select_top_indices(scored.size(), kHeuristicPrefilterKeep, [&scored](std::size_t lhs, std::size_t rhs) {
    if (scored[lhs].score != scored[rhs].score) {
      return scored[lhs].score > scored[rhs].score;
    }
    if (scored[lhs].action.schema_id != scored[rhs].action.schema_id) {
      return scored[lhs].action.schema_id < scored[rhs].action.schema_id;
    }
    return scored[lhs].action.args < scored[rhs].action.args;
  });

  std::vector<CandidateAction> filtered;
  filtered.reserve(top_indices.size());
  for (const std::size_t index : top_indices) {
    filtered.push_back(scored[index].action);
  }
  *candidates = std::move(filtered);
}

std::vector<CandidateAction> generate_guided_candidates_with_widening(
  const TP_Domain &domain,
  const TP_State &state,
  int32_t max_candidates,
  const std::vector<CandidatePattern> *relevant_patterns,
  bool use_heuristic_prefilter,
  int32_t *candidate_generation_calls,
  int64_t *candidate_generation_time_us
) {
  int32_t budget = std::min(kInitialGuidedGroundingBudget, max_candidates);
  const int32_t target = use_heuristic_prefilter ? std::min(kGuidedGroundingTarget, max_candidates) : max_candidates;
  std::vector<CandidateAction> candidates;
  while (true) {
    const auto candidate_start = std::chrono::steady_clock::now();
    candidates = generate_candidate_actions(domain, state, budget, relevant_patterns);
    const auto candidate_end = std::chrono::steady_clock::now();
    *candidate_generation_time_us += std::chrono::duration_cast<std::chrono::microseconds>(
      candidate_end - candidate_start
    ).count();
    ++(*candidate_generation_calls);

    const int32_t generated_count = static_cast<int32_t>(candidates.size());
    if (use_heuristic_prefilter) {
      heuristic_prefilter_candidates(domain, state, &candidates);
    }
    if (budget >= max_candidates || generated_count >= target || generated_count < budget) {
      break;
    }

    budget = std::min(max_candidates, budget * kGuidedGroundingWidenFactor);
  }
  return candidates;
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
  int32_t signature_requests,
  int32_t signature_rebuilds,
  int64_t signature_time_us,
  std::size_t visited_count,
  TP_Solve_Result *out_result
) {
  log_signature_metrics(signature_requests, signature_rebuilds, signature_time_us, visited_count);
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
  if (out_result->plan_length > 0 && out_result->plan_actions == nullptr) {
    out_result->solved = false;
    out_result->plan_length = 0;
    out_result->remaining_goal_count = count_unsatisfied_goals(nodes[static_cast<std::size_t>(solved_index)].state);
    return TP_STATUS_LIMIT_EXCEEDED;
  }

  for (int32_t index = 0; index < out_result->plan_length; ++index) {
    copy_candidate_action(reversed_plan[static_cast<std::size_t>(index)], &out_result->plan_actions[index]);
  }

  return TP_STATUS_OK;
}

TP_Status solve_guided_search(
  TP_Solver *solver,
  const TP_Domain &domain,
  const TP_State &initial_state,
  TP_Solve_Result *out_result
) {
  const ReducedTaskPatterns reduced_task_patterns = analyze_reduced_task_patterns(domain, initial_state);
  std::priority_queue<QueueEntry, std::vector<QueueEntry>, GuidedQueueCompare> open_set;
  std::unordered_set<std::vector<int32_t>, SignatureHash> visited;
  std::vector<SearchNode> nodes;
  nodes.reserve(static_cast<std::size_t>(domain.limits.max_expansions));

  SearchNode root {};
  root.state = initial_state;
  root.h_cost = count_unsatisfied_goals(root.state);
  root.priority = compute_priority(0, root.h_cost, false, 0.0f, 0.0f);
  nodes.push_back(root);
  open_set.push({0, root.priority, 0});

  int32_t expansions = 0;
  int32_t generated = 0;
  int32_t candidate_generation_calls = 0;
  int32_t index_rebuilds = 0;
  int32_t scorer_calls = 0;
  int64_t candidate_generation_time_us = 0;
  int64_t scorer_export_time_us = 0;
  int64_t scorer_time_us = 0;
  int64_t scorer_sort_time_us = 0;
  int32_t signature_requests = 0;
  int32_t signature_rebuilds = 0;
  int64_t signature_time_us = 0;
  int32_t solved_index = -1;

  bool root_signature_rebuilt = false;
  const auto root_signature_start = std::chrono::steady_clock::now();
  visited.insert(make_state_signature(root.state, &root_signature_rebuilt));
  const auto root_signature_end = std::chrono::steady_clock::now();
  ++signature_requests;
  if (root_signature_rebuilt) {
    ++signature_rebuilds;
  }
  signature_time_us += std::chrono::duration_cast<std::chrono::microseconds>(
    root_signature_end - root_signature_start
  ).count();

  while (!open_set.empty() && expansions < domain.limits.max_expansions) {
    const QueueEntry current_entry = open_set.top();
    open_set.pop();
    const int32_t current_node_index = current_entry.node_index;
    const SearchNode &current = nodes[static_cast<std::size_t>(current_node_index)];
    const int32_t current_g_cost = current.g_cost;
    const float current_guidance_score = current.guidance_score;
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

    std::vector<CandidateAction> candidates = generate_guided_candidates_with_widening(
      domain,
      current.state,
      domain.limits.max_candidates,
      reduced_task_patterns.filtered.empty() ? nullptr : &reduced_task_patterns.filtered,
      !solver_has_model_scorer(solver),
      &candidate_generation_calls,
      &candidate_generation_time_us
    );
    float state_value = 0.0f;
    bool has_state_value = false;
    std::vector<float> action_scores;
    if (solver_has_model_scorer(solver)) {
      score_candidate_models(
        domain,
        current.state,
        solver,
        candidates,
        &action_scores,
        &state_value,
        &has_state_value,
        &scorer_calls,
        &scorer_export_time_us,
        &scorer_time_us,
        &scorer_sort_time_us
      );
      std::vector<GuidedCandidate> guided_candidates = build_guided_candidates(
        domain,
        current.state,
        candidates,
        action_scores
      );
      shortlist_guided_candidates(&guided_candidates, &scorer_sort_time_us);
      candidates.clear();
      action_scores.clear();
      candidates.reserve(guided_candidates.size());
      action_scores.reserve(guided_candidates.size());
      for (const GuidedCandidate &candidate : guided_candidates) {
        candidates.push_back(candidate.action);
        action_scores.push_back(candidate.model_score);
      }
    } else {
      shortlist_heuristic_candidates(domain, current.state, &candidates, &scorer_sort_time_us);
    }
    generated += static_cast<int32_t>(candidates.size());

    for (std::size_t candidate_index = 0; candidate_index < candidates.size(); ++candidate_index) {
      const CandidateAction &candidate = candidates[candidate_index];
      const SearchNode &parent = nodes[static_cast<std::size_t>(current_node_index)];
      TP_State next_state = apply_action(domain, parent.state, candidate);
      bool signature_rebuilt = false;
      const auto signature_start = std::chrono::steady_clock::now();
      const std::vector<int32_t> &signature = make_state_signature(next_state, &signature_rebuilt);
      const auto signature_end = std::chrono::steady_clock::now();
      ++signature_requests;
      if (signature_rebuilt) {
        ++signature_rebuilds;
      }
      signature_time_us += std::chrono::duration_cast<std::chrono::microseconds>(
        signature_end - signature_start
      ).count();
      if (visited.find(signature) != visited.end()) {
        continue;
      }
      visited.insert(signature);

      SearchNode child {};
      child.state = std::move(next_state);
      child.parent_index = current_node_index;
      child.g_cost = current_g_cost + 1;
      child.h_cost = count_unsatisfied_goals(child.state);
      const float action_score = candidate_index < action_scores.size() ? action_scores[candidate_index] : 0.0f;
      child.guidance_score = current_guidance_score + action_score;
      child.priority = compute_priority(
        child.g_cost,
        child.h_cost,
        has_state_value,
        state_value,
        child.guidance_score
      );
      child.action = candidate;

      const int32_t child_index = static_cast<int32_t>(nodes.size());
      nodes.push_back(std::move(child));
      open_set.push({child_index, nodes.back().priority, nodes.back().g_cost});

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
    signature_requests,
    signature_rebuilds,
    signature_time_us,
    visited.size(),
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
  return solve_guided_search(solver, *solver->domain, *initial_state, out_result);
}
