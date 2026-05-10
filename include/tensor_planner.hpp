#ifndef TENSOR_PLANNER_HPP
#define TENSOR_PLANNER_HPP

#include "tensor_planner.h"

#include <array>
#include <cstddef>
#include <initializer_list>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tp {

class Planner;
class ActionBuilder;
class StateBuilder;

class Error : public std::runtime_error {
public:
  explicit Error(const std::string &message) : std::runtime_error(message) {}
};

struct Limits {
  int32_t max_objects = 64;
  int32_t max_facts = 256;
  int32_t max_goals = 32;
  int32_t max_candidates = 512;
  int32_t max_expansions = 4096;
  int32_t max_plan_length = 64;
};

struct Type {
  int32_t id = -1;
  std::type_index cpp_type = std::type_index(typeid(void));
  std::string name;
};

struct Action {
  int32_t id = -1;
  std::string name;
  std::vector<std::string> param_names;

  bool valid() const { return id >= 0; }
};

namespace detail {

inline void check(TP_Status status, const char *operation) {
  if (status != TP_STATUS_OK) {
    throw Error(std::string(operation) + " failed with status " + std::to_string(status));
  }
}

template <typename T>
std::string default_type_name() {
  return typeid(T).name();
}

struct ArgumentRef {
  enum class Kind {
    Parameter,
    Object,
  };

  Kind kind = Kind::Parameter;
  std::string parameter_name;
  void *object = nullptr;
  std::type_index cpp_type = std::type_index(typeid(void));
};

}  // namespace detail

struct Atom {
  int32_t predicate_id = -1;
  std::string predicate_name;
  std::vector<std::type_index> arg_types;
  std::vector<detail::ArgumentRef> args;
};

struct NumericTerm {
  int32_t function_id = -1;
  std::string function_name;
  std::vector<std::type_index> arg_types;
  std::vector<detail::ArgumentRef> args;
};

struct NumericPrecondition {
  NumericTerm term;
  TP_Num_Compare_Op op = TP_NUM_CMP_GTE;
  float value = 0.0f;
};

struct NumericEffect {
  NumericTerm term;
  TP_Num_Effect_Op op = TP_NUM_EFFECT_SET;
  float value = 0.0f;
};

class Predicate {
public:
  Predicate() = default;

  template <
    typename... Args,
    std::enable_if_t<(!std::is_convertible_v<std::decay_t<Args>, std::string_view> && ...), int> = 0
  >
  Atom operator()(Args &...args) const {
    static_assert(sizeof...(Args) > 0, "predicate atom needs at least one argument");
    std::vector<detail::ArgumentRef> refs;
    refs.reserve(sizeof...(Args));
    (refs.push_back(object_ref(args)), ...);
    validate_arity(refs.size());
    validate_object_types(refs);
    return Atom{id_, name_, arg_types_, std::move(refs)};
  }

  template <
    typename... Args,
    std::enable_if_t<(std::is_convertible_v<std::decay_t<Args>, std::string_view> && ...), int> = 0
  >
  Atom operator()(Args &&...args) const {
    static_assert(sizeof...(Args) > 0, "predicate atom needs at least one argument");
    std::vector<detail::ArgumentRef> refs;
    refs.reserve(sizeof...(Args));
    (refs.push_back(parameter_ref(std::string_view(args))), ...);
    validate_arity(refs.size());
    return Atom{id_, name_, arg_types_, std::move(refs)};
  }

  int32_t id() const { return id_; }
  const std::string &name() const { return name_; }
  const std::vector<std::type_index> &arg_types() const { return arg_types_; }

private:
  friend class Planner;

  Predicate(int32_t id, std::string name, std::vector<std::type_index> arg_types)
    : id_(id), name_(std::move(name)), arg_types_(std::move(arg_types)) {}

  static detail::ArgumentRef parameter_ref(std::string_view name) {
    return detail::ArgumentRef{
      .kind = detail::ArgumentRef::Kind::Parameter,
      .parameter_name = std::string(name),
    };
  }

  template <typename T>
  static detail::ArgumentRef object_ref(T &object) {
    return detail::ArgumentRef{
      .kind = detail::ArgumentRef::Kind::Object,
      .object = static_cast<void *>(&object),
      .cpp_type = std::type_index(typeid(T)),
    };
  }

  void validate_arity(std::size_t arity) const {
    if (arity != arg_types_.size()) {
      throw Error("wrong arity for predicate: " + name_);
    }
  }

  void validate_object_types(const std::vector<detail::ArgumentRef> &refs) const {
    for (std::size_t index = 0; index < refs.size(); ++index) {
      if (refs[index].cpp_type != arg_types_[index]) {
        throw Error("object type mismatch for predicate: " + name_);
      }
    }
  }

  int32_t id_ = -1;
  std::string name_;
  std::vector<std::type_index> arg_types_;
};

class Function {
public:
  Function() = default;

  template <
    typename... Args,
    std::enable_if_t<(!std::is_convertible_v<std::decay_t<Args>, std::string_view> && ...), int> = 0
  >
  NumericTerm operator()(Args &...args) const {
    std::vector<detail::ArgumentRef> refs;
    refs.reserve(sizeof...(Args));
    (refs.push_back(object_ref(args)), ...);
    validate_arity(refs.size());
    validate_object_types(refs);
    return NumericTerm{id_, name_, arg_types_, std::move(refs)};
  }

  template <
    typename... Args,
    std::enable_if_t<(std::is_convertible_v<std::decay_t<Args>, std::string_view> && ...), int> = 0
  >
  NumericTerm operator()(Args &&...args) const {
    std::vector<detail::ArgumentRef> refs;
    refs.reserve(sizeof...(Args));
    (refs.push_back(parameter_ref(std::string_view(args))), ...);
    validate_arity(refs.size());
    return NumericTerm{id_, name_, arg_types_, std::move(refs)};
  }

  int32_t id() const { return id_; }
  const std::string &name() const { return name_; }

private:
  friend class Planner;

  Function(int32_t id, std::string name, std::vector<std::type_index> arg_types)
    : id_(id), name_(std::move(name)), arg_types_(std::move(arg_types)) {}

  static detail::ArgumentRef parameter_ref(std::string_view name) {
    return detail::ArgumentRef{
      .kind = detail::ArgumentRef::Kind::Parameter,
      .parameter_name = std::string(name),
    };
  }

  template <typename T>
  static detail::ArgumentRef object_ref(T &object) {
    return detail::ArgumentRef{
      .kind = detail::ArgumentRef::Kind::Object,
      .object = static_cast<void *>(&object),
      .cpp_type = std::type_index(typeid(T)),
    };
  }

  void validate_arity(std::size_t arity) const {
    if (arity != arg_types_.size()) {
      throw Error("wrong arity for function: " + name_);
    }
  }

  void validate_object_types(const std::vector<detail::ArgumentRef> &refs) const {
    for (std::size_t index = 0; index < refs.size(); ++index) {
      if (refs[index].cpp_type != arg_types_[index]) {
        throw Error("object type mismatch for function: " + name_);
      }
    }
  }

  int32_t id_ = -1;
  std::string name_;
  std::vector<std::type_index> arg_types_;
};

inline NumericPrecondition compare(NumericTerm term, TP_Num_Compare_Op op, float value) {
  return NumericPrecondition{std::move(term), op, value};
}

inline NumericPrecondition operator<(NumericTerm term, float value) { return compare(std::move(term), TP_NUM_CMP_LT, value); }
inline NumericPrecondition operator<=(NumericTerm term, float value) { return compare(std::move(term), TP_NUM_CMP_LTE, value); }
inline NumericPrecondition operator==(NumericTerm term, float value) { return compare(std::move(term), TP_NUM_CMP_EQ, value); }
inline NumericPrecondition operator>=(NumericTerm term, float value) { return compare(std::move(term), TP_NUM_CMP_GTE, value); }
inline NumericPrecondition operator>(NumericTerm term, float value) { return compare(std::move(term), TP_NUM_CMP_GT, value); }

class PlanStep {
public:
  const std::string &name() const { return action_.name; }
  const Action &action() const { return action_; }
  bool is(const Action &action) const { return action_.id == action.id; }

  template <typename T>
  T *arg(const std::string &param_name) const {
    const std::size_t index = param_index(param_name);
    if (index >= args_.size()) {
      throw Error("missing plan argument: " + param_name);
    }
    if (args_[index].cpp_type != std::type_index(typeid(T))) {
      throw Error("plan argument has unexpected type: " + param_name);
    }
    return static_cast<T *>(args_[index].object);
  }

  std::string to_string() const {
    std::ostringstream stream;
    stream << action_.name << '(';
    for (std::size_t index = 0; index < args_.size(); ++index) {
      if (index > 0) {
        stream << ", ";
      }
      stream << action_.param_names[index];
    }
    stream << ')';
    return stream.str();
  }

private:
  friend class Planner;

  struct ObjectArg {
    void *object = nullptr;
    std::type_index cpp_type = std::type_index(typeid(void));
  };

  std::size_t param_index(const std::string &name) const {
    for (std::size_t index = 0; index < action_.param_names.size(); ++index) {
      if (action_.param_names[index] == name) {
        return index;
      }
    }
    throw Error("unknown action parameter: " + name);
  }

  Action action_;
  std::vector<ObjectArg> args_;
};

class SolveResult {
public:
  bool solved() const { return solved_; }
  TP_Status status() const { return status_; }
  int32_t expansions() const { return expansions_; }
  int32_t generated() const { return generated_; }
  int32_t scorer_calls() const { return scorer_calls_; }
  const std::vector<PlanStep> &steps() const { return steps_; }

private:
  friend class Planner;

  TP_Status status_ = TP_STATUS_UNSUPPORTED;
  bool solved_ = false;
  int32_t expansions_ = 0;
  int32_t generated_ = 0;
  int32_t scorer_calls_ = 0;
  std::vector<PlanStep> steps_;
};

class StateBuilder {
public:
  template <typename T>
  StateBuilder &object(T &object) {
    register_object(std::type_index(typeid(T)), static_cast<void *>(&object));
    return *this;
  }

  StateBuilder &fact(const Atom &atom) {
    facts_.push_back(atom);
    register_atom_objects(atom);
    return *this;
  }

  template <typename A, typename B>
  StateBuilder &edge(const Predicate &predicate, A &a, B &b) {
    return fact(predicate(a, b)).fact(predicate(b, a));
  }

  StateBuilder &goal(const Atom &atom) {
    goals_.push_back(atom);
    register_atom_objects(atom);
    return *this;
  }

  StateBuilder &value(const NumericTerm &term, float value) {
    values_.push_back(NumericValue{term, value});
    register_numeric_term_objects(term);
    return *this;
  }

  template <typename T>
  int32_t object_id(const T &object) const {
    const auto found = object_ids_.find(const_cast<void *>(static_cast<const void *>(&object)));
    if (found == object_ids_.end()) {
      throw Error("unknown object id lookup");
    }
    return found->second;
  }

private:
  friend class Planner;

  explicit StateBuilder(const Planner *planner) : planner_(planner) {}

  int32_t register_object(std::type_index cpp_type, void *object);
  void register_atom_objects(const Atom &atom);
  void register_numeric_term_objects(const NumericTerm &term);
  std::vector<int32_t> resolve_object_args(const Atom &atom) const;
  std::vector<int32_t> resolve_object_args(const NumericTerm &term) const;

  struct NumericValue {
    NumericTerm term;
    float value = 0.0f;
  };

  const Planner *planner_ = nullptr;
  std::unordered_map<void *, int32_t> object_ids_;
  std::vector<void *> objects_;
  std::vector<std::type_index> object_cpp_types_;
  std::vector<int32_t> object_types_;
  std::vector<Atom> facts_;
  std::vector<Atom> goals_;
  std::vector<NumericValue> values_;
};

class ActionBuilder {
public:
  ActionBuilder(Planner *planner, std::string name) : planner_(planner), name_(std::move(name)) {}

  template <typename T>
  ActionBuilder &param(const std::string &name);

  ActionBuilder &require(const Atom &atom) {
    preconditions_.push_back(make_literal(atom, 1));
    return *this;
  }

  ActionBuilder &require(const NumericPrecondition &condition) {
    numeric_preconditions_.push_back(make_numeric_precondition(condition));
    return *this;
  }

  ActionBuilder &adds(const Atom &atom) {
    effects_.push_back(make_effect(atom, TP_EFFECT_ADD));
    return *this;
  }

  ActionBuilder &removes(const Atom &atom) {
    effects_.push_back(make_effect(atom, TP_EFFECT_DELETE));
    return *this;
  }

  ActionBuilder &sets(const NumericTerm &term, float value) {
    numeric_effects_.push_back(make_numeric_effect(NumericEffect{term, TP_NUM_EFFECT_SET, value}));
    return *this;
  }

  ActionBuilder &increases(const NumericTerm &term, float value) {
    numeric_effects_.push_back(make_numeric_effect(NumericEffect{term, TP_NUM_EFFECT_ADD, value}));
    return *this;
  }

  ActionBuilder &decreases(const NumericTerm &term, float value) {
    numeric_effects_.push_back(make_numeric_effect(NumericEffect{term, TP_NUM_EFFECT_SUBTRACT, value}));
    return *this;
  }

  Action commit();

private:
  friend class Planner;

  TP_Action_Literal make_literal(const Atom &atom, int8_t sign) const;
  TP_Action_Effect make_effect(const Atom &atom, uint8_t op) const;
  TP_Numeric_Precondition make_numeric_precondition(const NumericPrecondition &condition) const;
  TP_Numeric_Effect make_numeric_effect(const NumericEffect &effect) const;
  int8_t resolve_slot(const detail::ArgumentRef &arg, const std::string &predicate_name) const;
  void validate_numeric_slots(const NumericTerm &term) const;

  Planner *planner_ = nullptr;
  std::string name_;
  std::unordered_map<std::string, int32_t> slot_ids_;
  std::unordered_map<std::string, std::type_index> slot_cpp_types_;
  std::vector<std::string> param_names_;
  std::vector<int32_t> arg_types_;
  std::vector<TP_Action_Literal> preconditions_;
  std::vector<TP_Action_Effect> effects_;
  std::vector<TP_Numeric_Precondition> numeric_preconditions_;
  std::vector<TP_Numeric_Effect> numeric_effects_;
};

class Planner {
public:
  explicit Planner(const Limits &limits = {}) {
    const TP_Limits c_limits {
      .max_objects = limits.max_objects,
      .max_facts = limits.max_facts,
      .max_goals = limits.max_goals,
      .max_candidates = limits.max_candidates,
      .max_expansions = limits.max_expansions,
      .max_plan_length = limits.max_plan_length,
    };
    domain_ = tp_domain_create(&c_limits);
    if (domain_ == nullptr) {
      throw Error("tp_domain_create failed");
    }
  }

  ~Planner() {
    if (domain_ != nullptr) {
      tp_domain_destroy(domain_);
    }
  }

  Planner(const Planner &) = delete;
  Planner &operator=(const Planner &) = delete;

  Planner(Planner &&other) noexcept { *this = std::move(other); }

  Planner &operator=(Planner &&other) noexcept {
    if (this != &other) {
      if (domain_ != nullptr) {
        tp_domain_destroy(domain_);
      }
      domain_ = other.domain_;
      other.domain_ = nullptr;
      types_ = std::move(other.types_);
      predicate_names_ = std::move(other.predicate_names_);
      action_names_ = std::move(other.action_names_);
    }
    return *this;
  }

  template <typename T>
  Type type(const std::string &name = detail::default_type_name<T>()) {
    return type_for(std::type_index(typeid(T)), name);
  }

  template <typename... Args>
  Predicate predicate(const std::string &name) {
    std::vector<Type> types;
    types.reserve(sizeof...(Args));
    (types.push_back(type<Args>()), ...);
    return predicate_from_vector(name, types, {std::type_index(typeid(Args))...});
  }

  template <typename... Args>
  Function function(const std::string &name) {
    std::vector<Type> types;
    types.reserve(sizeof...(Args));
    (types.push_back(type<Args>()), ...);
    return function_from_vector(name, types, {std::type_index(typeid(Args))...});
  }

  ActionBuilder action(const std::string &name) { return ActionBuilder(this, name); }
  StateBuilder state() const { return StateBuilder(this); }

  SolveResult solve(const StateBuilder &state) const {
    return solve_impl(state, nullptr, nullptr);
  }

  SolveResult solve(
    const StateBuilder &state,
    TP_Score_Candidates_Fn scorer,
    void *scorer_user_data = nullptr
  ) const {
    return solve_impl(state, scorer, scorer_user_data);
  }

private:
  friend class ActionBuilder;
  friend class StateBuilder;

  SolveResult solve_impl(
    const StateBuilder &state,
    TP_Score_Candidates_Fn scorer,
    void *scorer_user_data
  ) const {
    TP_State *raw_state = tp_state_create(
      domain_,
      static_cast<int32_t>(state.object_types_.size()),
      state.object_types_.data()
    );
    if (raw_state == nullptr) {
      throw Error("tp_state_create failed");
    }

    TP_Solver *solver = nullptr;
    TP_Solve_Result raw_result {};
    try {
      for (const Atom &fact : state.facts_) {
        const std::vector<int32_t> args = state.resolve_object_args(fact);
        detail::check(tp_state_add_fact(raw_state, fact.predicate_id, static_cast<uint8_t>(args.size()), args.data()), "add fact");
      }
      for (const Atom &goal : state.goals_) {
        const std::vector<int32_t> args = state.resolve_object_args(goal);
        detail::check(tp_state_add_goal_fact(raw_state, goal.predicate_id, static_cast<uint8_t>(args.size()), args.data()), "add goal");
      }
      for (const StateBuilder::NumericValue &value : state.values_) {
        const std::vector<int32_t> args = state.resolve_object_args(value.term);
        detail::check(tp_state_set_function_value(raw_state, value.term.function_id, static_cast<uint8_t>(args.size()), args.data(), value.value), "set function value");
      }

      solver = tp_solver_create(domain_);
      if (solver == nullptr) {
        throw Error("tp_solver_create failed");
      }

      if (scorer != nullptr) {
        detail::check(tp_solver_set_custom_guidance(solver, scorer, scorer_user_data), "set custom guidance");
      } else {
        detail::check(tp_solver_use_default_guidance(solver), "use default guidance");
      }

      const TP_Status status = tp_solver_solve(solver, raw_state, &raw_result);
      SolveResult result;
      result.status_ = status;
      result.solved_ = raw_result.solved;
      result.expansions_ = raw_result.expansions;
      result.generated_ = raw_result.generated;
      result.scorer_calls_ = raw_result.scorer_calls;
      if (raw_result.solved) {
        for (int32_t index = 0; index < raw_result.plan_length; ++index) {
          result.steps_.push_back(make_step(raw_result.plan_actions[index], state));
        }
      }
      tp_solve_result_dispose(&raw_result);
      tp_solver_destroy(solver);
      tp_state_destroy(raw_state);
      return result;
    } catch (...) {
      tp_solve_result_dispose(&raw_result);
      if (solver != nullptr) {
        tp_solver_destroy(solver);
      }
      tp_state_destroy(raw_state);
      throw;
    }
  }

  Type type_for(std::type_index cpp_type, const std::string &name) {
    const auto found = types_.find(cpp_type);
    if (found != types_.end()) {
      return found->second;
    }
    const int32_t id = static_cast<int32_t>(types_.size());
    Type type{id, cpp_type, name};
    types_.emplace(cpp_type, type);
    return type;
  }

  int32_t type_id_for(std::type_index cpp_type) const {
    const auto found = types_.find(cpp_type);
    if (found == types_.end()) {
      throw Error("unregistered object type");
    }
    return found->second.id;
  }

  Predicate predicate_from_vector(
    const std::string &name,
    const std::vector<Type> &types,
    std::vector<std::type_index> cpp_types
  ) {
    if (types.size() > TP_MAX_ARITY) {
      throw Error("predicate arity exceeds TP_MAX_ARITY: " + name);
    }

    TP_Predicate_Def predicate {.arity = static_cast<uint8_t>(types.size()), .arg_types = {0, 0, 0, 0}};
    for (std::size_t index = 0; index < types.size(); ++index) {
      predicate.arg_types[index] = types[index].id;
    }

    int32_t predicate_id = -1;
    detail::check(tp_domain_add_predicate(domain_, &predicate, &predicate_id), "add predicate");
    predicate_names_[predicate_id] = name;
    return Predicate(predicate_id, name, std::move(cpp_types));
  }

  Function function_from_vector(
    const std::string &name,
    const std::vector<Type> &types,
    std::vector<std::type_index> cpp_types
  ) {
    if (types.size() > TP_MAX_ARITY) {
      throw Error("function arity exceeds TP_MAX_ARITY: " + name);
    }

    TP_Function_Def function {.arity = static_cast<uint8_t>(types.size()), .arg_types = {0, 0, 0, 0}};
    for (std::size_t index = 0; index < types.size(); ++index) {
      function.arg_types[index] = types[index].id;
    }

    int32_t function_id = -1;
    detail::check(tp_domain_add_function(domain_, &function, &function_id), "add function");
    return Function(function_id, name, std::move(cpp_types));
  }

  Action commit_action(ActionBuilder &builder) {
    if (builder.arg_types_.empty()) {
      throw Error("action needs at least one parameter: " + builder.name_);
    }
    if (builder.arg_types_.size() > TP_MAX_PARAMS) {
      throw Error("action parameter count exceeds TP_MAX_PARAMS: " + builder.name_);
    }

    int32_t action_id = -1;
    detail::check(tp_domain_add_action_schema(
                    domain_,
                    static_cast<uint8_t>(builder.arg_types_.size()),
                    builder.arg_types_.data(),
                    builder.preconditions_.data(),
                    static_cast<int32_t>(builder.preconditions_.size()),
                    builder.effects_.data(),
                    static_cast<int32_t>(builder.effects_.size()),
                    builder.numeric_preconditions_.data(),
                    static_cast<int32_t>(builder.numeric_preconditions_.size()),
                    builder.numeric_effects_.data(),
                    static_cast<int32_t>(builder.numeric_effects_.size()),
                    &action_id
                  ),
                  "add action schema");

    Action action{action_id, builder.name_, builder.param_names_};
    action_names_[action_id] = action;
    return action;
  }

  PlanStep make_step(const TP_Candidate_Action &raw_action, const StateBuilder &state) const {
    PlanStep step;
    const auto action_found = action_names_.find(raw_action.schema_id);
    if (action_found == action_names_.end()) {
      step.action_ = Action{raw_action.schema_id, "action_" + std::to_string(raw_action.schema_id), {}};
    } else {
      step.action_ = action_found->second;
    }

    for (int32_t index = 0; index < raw_action.arity; ++index) {
      const int32_t object_id = raw_action.args[index];
      if (object_id < 0 || object_id >= static_cast<int32_t>(state.objects_.size())) {
        throw Error("plan references unknown object id");
      }
      step.args_.push_back({
        state.objects_[static_cast<std::size_t>(object_id)],
        state.object_cpp_types_[static_cast<std::size_t>(object_id)],
      });
    }
    return step;
  }

  TP_Domain *domain_ = nullptr;
  std::unordered_map<std::type_index, Type> types_;
  std::unordered_map<int32_t, std::string> predicate_names_;
  std::unordered_map<int32_t, Action> action_names_;
};

inline int32_t StateBuilder::register_object(std::type_index cpp_type, void *object) {
  const auto found = object_ids_.find(object);
  if (found != object_ids_.end()) {
    return found->second;
  }
  const int32_t object_id = static_cast<int32_t>(objects_.size());
  object_ids_[object] = object_id;
  objects_.push_back(object);
  object_cpp_types_.push_back(cpp_type);
  object_types_.push_back(planner_->type_id_for(cpp_type));
  return object_id;
}

inline void StateBuilder::register_atom_objects(const Atom &atom) {
  for (const detail::ArgumentRef &arg : atom.args) {
    if (arg.kind != detail::ArgumentRef::Kind::Object) {
      throw Error("state atom uses action parameter in predicate: " + atom.predicate_name);
    }
    register_object(arg.cpp_type, arg.object);
  }
}

inline void StateBuilder::register_numeric_term_objects(const NumericTerm &term) {
  for (const detail::ArgumentRef &arg : term.args) {
    if (arg.kind != detail::ArgumentRef::Kind::Object) {
      throw Error("state function term uses action parameter: " + term.function_name);
    }
    register_object(arg.cpp_type, arg.object);
  }
}

inline std::vector<int32_t> StateBuilder::resolve_object_args(const Atom &atom) const {
  std::vector<int32_t> args;
  args.reserve(atom.args.size());
  for (const detail::ArgumentRef &arg : atom.args) {
    if (arg.kind != detail::ArgumentRef::Kind::Object) {
      throw Error("state atom uses action parameter in predicate: " + atom.predicate_name);
    }
    const auto found = object_ids_.find(arg.object);
    if (found == object_ids_.end()) {
      throw Error("unknown object in predicate: " + atom.predicate_name);
    }
    args.push_back(found->second);
  }
  return args;
}

inline std::vector<int32_t> StateBuilder::resolve_object_args(const NumericTerm &term) const {
  std::vector<int32_t> args;
  args.reserve(term.args.size());
  for (const detail::ArgumentRef &arg : term.args) {
    if (arg.kind != detail::ArgumentRef::Kind::Object) {
      throw Error("state function term uses action parameter: " + term.function_name);
    }
    const auto found = object_ids_.find(arg.object);
    if (found == object_ids_.end()) {
      throw Error("unknown object in function: " + term.function_name);
    }
    args.push_back(found->second);
  }
  return args;
}

template <typename T>
ActionBuilder &ActionBuilder::param(const std::string &name) {
  if (slot_ids_.find(name) != slot_ids_.end()) {
    throw Error("duplicate action parameter: " + name);
  }
  if (arg_types_.size() >= TP_MAX_PARAMS) {
    throw Error("too many action parameters in: " + name_);
  }
  const Type type = planner_->type<T>();
  slot_ids_[name] = static_cast<int32_t>(arg_types_.size());
  slot_cpp_types_.emplace(name, std::type_index(typeid(T)));
  param_names_.push_back(name);
  arg_types_.push_back(type.id);
  return *this;
}

inline TP_Action_Literal ActionBuilder::make_literal(const Atom &atom, int8_t sign) const {
  if (atom.args.size() > TP_MAX_ARITY) {
    throw Error("precondition arity exceeds TP_MAX_ARITY: " + atom.predicate_name);
  }
  const std::size_t arg_count = atom.args.size();
  TP_Action_Literal literal {.predicate_id = atom.predicate_id, .sign = sign, .arity = 0, .slots = {-1, -1, -1, -1}};
  literal.arity = static_cast<uint8_t>(arg_count);
  for (std::size_t index = 0; index < TP_MAX_ARITY && index < arg_count; ++index) {
    literal.slots[index] = resolve_slot(atom.args[index], atom.predicate_name);
    const std::string &param_name = atom.args[index].parameter_name;
    const auto type_found = slot_cpp_types_.find(param_name);
    if (type_found == slot_cpp_types_.end() || type_found->second != atom.arg_types[index]) {
      throw Error("parameter type mismatch for predicate: " + atom.predicate_name);
    }
  }
  return literal;
}

inline TP_Action_Effect ActionBuilder::make_effect(const Atom &atom, uint8_t op) const {
  if (atom.args.size() > TP_MAX_ARITY) {
    throw Error("effect arity exceeds TP_MAX_ARITY: " + atom.predicate_name);
  }
  const std::size_t arg_count = atom.args.size();
  TP_Action_Effect effect {.predicate_id = atom.predicate_id, .op = op, .arity = 0, .slots = {-1, -1, -1, -1}};
  effect.arity = static_cast<uint8_t>(arg_count);
  for (std::size_t index = 0; index < TP_MAX_ARITY && index < arg_count; ++index) {
    effect.slots[index] = resolve_slot(atom.args[index], atom.predicate_name);
    const std::string &param_name = atom.args[index].parameter_name;
    const auto type_found = slot_cpp_types_.find(param_name);
    if (type_found == slot_cpp_types_.end() || type_found->second != atom.arg_types[index]) {
      throw Error("parameter type mismatch for predicate: " + atom.predicate_name);
    }
  }
  return effect;
}

inline TP_Numeric_Precondition ActionBuilder::make_numeric_precondition(const NumericPrecondition &condition) const {
  validate_numeric_slots(condition.term);
  const std::size_t arg_count = condition.term.args.size();
  TP_Numeric_Precondition result {};
  result.function_id = condition.term.function_id;
  result.cmp_op = static_cast<uint8_t>(condition.op);
  result.arity = static_cast<uint8_t>(arg_count);
  result.rhs_value = condition.value;
  for (int8_t &slot : result.slots) {
    slot = -1;
  }
  for (std::size_t index = 0; index < TP_MAX_ARITY && index < arg_count; ++index) {
    result.slots[index] = resolve_slot(condition.term.args[index], condition.term.function_name);
  }
  return result;
}

inline TP_Numeric_Effect ActionBuilder::make_numeric_effect(const NumericEffect &effect) const {
  validate_numeric_slots(effect.term);
  const std::size_t arg_count = effect.term.args.size();
  TP_Numeric_Effect result {};
  result.function_id = effect.term.function_id;
  result.op = static_cast<uint8_t>(effect.op);
  result.arity = static_cast<uint8_t>(arg_count);
  result.rhs_value = effect.value;
  for (int8_t &slot : result.slots) {
    slot = -1;
  }
  for (std::size_t index = 0; index < TP_MAX_ARITY && index < arg_count; ++index) {
    result.slots[index] = resolve_slot(effect.term.args[index], effect.term.function_name);
  }
  return result;
}

inline void ActionBuilder::validate_numeric_slots(const NumericTerm &term) const {
  if (term.args.size() > TP_MAX_ARITY) {
    throw Error("function arity exceeds TP_MAX_ARITY: " + term.function_name);
  }
  for (std::size_t index = 0; index < term.args.size(); ++index) {
    const std::string &param_name = term.args[index].parameter_name;
    const auto type_found = slot_cpp_types_.find(param_name);
    if (type_found == slot_cpp_types_.end() || type_found->second != term.arg_types[index]) {
      throw Error("parameter type mismatch for function: " + term.function_name);
    }
  }
}

inline int8_t ActionBuilder::resolve_slot(const detail::ArgumentRef &arg, const std::string &predicate_name) const {
  if (arg.kind != detail::ArgumentRef::Kind::Parameter) {
    throw Error("action atom uses object reference in predicate: " + predicate_name);
  }
  const auto found = slot_ids_.find(arg.parameter_name);
  if (found == slot_ids_.end()) {
    throw Error("unknown action parameter '" + arg.parameter_name + "' in " + predicate_name);
  }
  return static_cast<int8_t>(found->second);
}

inline Action ActionBuilder::commit() {
  if (planner_ == nullptr) {
    throw Error("action builder has no planner");
  }
  return planner_->commit_action(*this);
}

}  // namespace tp

#endif
