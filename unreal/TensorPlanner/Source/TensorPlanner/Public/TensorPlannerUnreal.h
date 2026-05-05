#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"

#include "tensor_planner.h"

#include <stdexcept>
#include <type_traits>

namespace tpue {

class Planner;
class ActionBuilder;
class StateBuilder;

class Error : public std::runtime_error {
public:
    explicit Error(const FString& InMessage)
        : std::runtime_error(TCHAR_TO_UTF8(*InMessage)), Message(InMessage) {
    }

    const FString& message() const { return Message; }

private:
    FString Message;
};

struct Limits {
    int32 MaxObjects = 64;
    int32 MaxFacts = 256;
    int32 MaxGoals = 32;
    int32 MaxCandidates = 512;
    int32 MaxExpansions = 4096;
    int32 MaxPlanLength = 64;
};

struct Type {
    int32 Id = -1;
    const UClass* UnrealClass = nullptr;
    FString Name;
};

struct ObjectRef {
    TSoftObjectPtr<UObject> Object;
    const UClass* UnrealClass = nullptr;

    bool IsNull() const {
        return Object.IsNull();
    }

    bool IsValid() const {
        return Object.IsValid();
    }

    bool IsPending() const {
        return Object.IsPending();
    }

    FSoftObjectPath Path() const {
        return Object.ToSoftObjectPath();
    }

    FString PathString() const {
        return Path().ToString();
    }

    UObject* GetIfLoaded() const {
        return Object.Get();
    }

    UObject* LoadSynchronous() const {
        return Object.LoadSynchronous();
    }

    template <typename TObject>
    TSoftObjectPtr<TObject> As() const {
        static_assert(TIsDerivedFrom<TObject, UObject>::Value, "TObject must derive from UObject");
        return TSoftObjectPtr<TObject>(Path());
    }

    template <typename TObject>
    static ObjectRef From(const TSoftObjectPtr<TObject>& InObject) {
        static_assert(TIsDerivedFrom<TObject, UObject>::Value, "TObject must derive from UObject");
        ObjectRef Ref;
        Ref.Object = TSoftObjectPtr<UObject>(InObject.ToSoftObjectPath());
        Ref.UnrealClass = TObject::StaticClass();
        return Ref;
    }

    template <typename TObject>
    static ObjectRef From(TObject* InObject) {
        static_assert(TIsDerivedFrom<TObject, UObject>::Value, "TObject must derive from UObject");
        ObjectRef Ref;
        Ref.Object = TSoftObjectPtr<UObject>(InObject);
        Ref.UnrealClass = TObject::StaticClass();
        return Ref;
    }
};

struct Action {
    int32 Id = -1;
    FString Name;
    TArray<FString> ParamNames;

    bool Valid() const {
        return Id >= 0;
    }

    bool valid() const {
        return Valid();
    }
};

namespace detail {

inline FString StatusText(TP_Status Status) {
    return FString::FromInt(static_cast<int32>(Status));
}

inline void Check(TP_Status Status, const TCHAR* Operation) {
    if (Status != TP_STATUS_OK) {
        throw Error(FString::Printf(TEXT("%s failed with status %s"), Operation, *StatusText(Status)));
    }
}

template <typename T>
struct is_string_like : std::false_type {};

template <> struct is_string_like<FString> : std::true_type {};
template <> struct is_string_like<FName> : std::true_type {};
template <> struct is_string_like<FStringView> : std::true_type {};
template <> struct is_string_like<const TCHAR*> : std::true_type {};
template <> struct is_string_like<TCHAR*> : std::true_type {};

template <typename T>
constexpr bool is_string_like_v = is_string_like<std::decay_t<T>>::value;

inline FString ToString(const FString& Value) { return Value; }
inline FString ToString(const FName& Value) { return Value.ToString(); }
inline FString ToString(FStringView Value) { return FString(Value); }
inline FString ToString(const TCHAR* Value) { return FString(Value); }
inline FString ToString(TCHAR* Value) { return FString(Value); }

struct ArgumentRef {
    enum class Kind {
        Parameter,
        Object,
    };

    Kind RefKind = Kind::Parameter;
    FString ParameterName;
    ObjectRef Reference;
    const UClass* DeclaredClass = nullptr;
};

inline ArgumentRef ParameterRef(const FString& Name) {
    ArgumentRef Ref;
    Ref.RefKind = ArgumentRef::Kind::Parameter;
    Ref.ParameterName = Name;
    return Ref;
}

inline ArgumentRef ObjectArgument(const ObjectRef& Reference) {
    if (Reference.UnrealClass == nullptr) {
        throw Error(TEXT("object reference is missing a declared Unreal class"));
    }

    ArgumentRef Ref;
    Ref.RefKind = ArgumentRef::Kind::Object;
    Ref.Reference = Reference;
    Ref.DeclaredClass = Reference.UnrealClass;
    return Ref;
}

template <typename TObject>
ArgumentRef ObjectRefFromSoft(const TSoftObjectPtr<TObject>& Object) {
    static_assert(TIsDerivedFrom<TObject, UObject>::Value, "TObject must derive from UObject");
    return ObjectArgument(ObjectRef::From(Object));
}

template <typename TObject>
ArgumentRef ObjectRefFromPointer(TObject* Object) {
    static_assert(TIsDerivedFrom<TObject, UObject>::Value, "TObject must derive from UObject");
    return ObjectArgument(ObjectRef::From(Object));
}

}  // namespace detail

struct Atom {
    int32 PredicateId = -1;
    FString PredicateName;
    TArray<const UClass*> ArgTypes;
    TArray<detail::ArgumentRef> Args;
};

struct NumericTerm {
    int32 FunctionId = -1;
    FString FunctionName;
    TArray<const UClass*> ArgTypes;
    TArray<detail::ArgumentRef> Args;
};

struct NumericPrecondition {
    NumericTerm Term;
    TP_Num_Compare_Op Op = TP_NUM_CMP_GTE;
    float Value = 0.0f;
};

struct NumericEffect {
    NumericTerm Term;
    TP_Num_Effect_Op Op = TP_NUM_EFFECT_SET;
    float Value = 0.0f;
};

class Predicate {
public:
    Predicate() = default;

    template <typename... Args, std::enable_if_t<(!detail::is_string_like_v<Args> && ...), int> = 0>
    Atom operator()(Args&&... InArgs) const {
        TArray<detail::ArgumentRef> Refs;
        Refs.Reserve(sizeof...(Args));
        (Refs.Add(MakeObjectArgument(Forward<Args>(InArgs))), ...);
        ValidateArity(Refs.Num());
        ValidateObjectTypes(Refs);
        Atom Result;
        Result.PredicateId = IdValue;
        Result.PredicateName = NameValue;
        Result.ArgTypes = ArgTypesValue;
        Result.Args = MoveTemp(Refs);
        return Result;
    }

    template <typename... Args, std::enable_if_t<(detail::is_string_like_v<Args> && ...), int> = 0>
    Atom operator()(Args&&... InArgs) const {
        TArray<detail::ArgumentRef> Refs;
        Refs.Reserve(sizeof...(Args));
        (Refs.Add(detail::ParameterRef(detail::ToString(Forward<Args>(InArgs)))), ...);
        ValidateArity(Refs.Num());
        Atom Result;
        Result.PredicateId = IdValue;
        Result.PredicateName = NameValue;
        Result.ArgTypes = ArgTypesValue;
        Result.Args = MoveTemp(Refs);
        return Result;
    }

    int32 Id() const { return IdValue; }
    const FString& Name() const { return NameValue; }
    const TArray<const UClass*>& ArgTypes() const { return ArgTypesValue; }
    int32 id() const { return Id(); }
    const FString& name() const { return Name(); }
    const TArray<const UClass*>& arg_types() const { return ArgTypes(); }

private:
    friend class Planner;

    Predicate(int32 InId, FString InName, TArray<const UClass*> InArgTypes)
        : IdValue(InId), NameValue(MoveTemp(InName)), ArgTypesValue(MoveTemp(InArgTypes)) {
    }

    template <typename TObject>
    static detail::ArgumentRef MakeObjectArgument(const TSoftObjectPtr<TObject>& Object) {
        return detail::ObjectRefFromSoft(Object);
    }

    template <typename TObject>
    static detail::ArgumentRef MakeObjectArgument(TObject* Object) {
        return detail::ObjectRefFromPointer(Object);
    }

    static detail::ArgumentRef MakeObjectArgument(const ObjectRef& Reference) {
        return detail::ObjectArgument(Reference);
    }

    void ValidateArity(int32 Arity) const {
        if (Arity != ArgTypesValue.Num()) {
            throw Error(FString::Printf(TEXT("wrong arity for predicate: %s"), *NameValue));
        }
    }

    void ValidateObjectTypes(const TArray<detail::ArgumentRef>& Refs) const {
        for (int32 Index = 0; Index < Refs.Num(); ++Index) {
            if (Refs[Index].DeclaredClass != ArgTypesValue[Index]) {
                throw Error(FString::Printf(TEXT("object type mismatch for predicate: %s"), *NameValue));
            }
        }
    }

    int32 IdValue = -1;
    FString NameValue;
    TArray<const UClass*> ArgTypesValue;
};

class Function {
public:
    Function() = default;

    int32 Id() const { return IdValue; }
    const FString& Name() const { return NameValue; }
    int32 id() const { return Id(); }
    const FString& name() const { return Name(); }

    template <typename... Args, std::enable_if_t<(!detail::is_string_like_v<Args> && ...), int> = 0>
    NumericTerm operator()(Args&&... InArgs) const {
        TArray<detail::ArgumentRef> Refs;
        Refs.Reserve(sizeof...(Args));
        (Refs.Add(MakeObjectArgument(Forward<Args>(InArgs))), ...);
        ValidateArity(Refs.Num());
        ValidateObjectTypes(Refs);
        NumericTerm Result;
        Result.FunctionId = IdValue;
        Result.FunctionName = NameValue;
        Result.ArgTypes = ArgTypesValue;
        Result.Args = MoveTemp(Refs);
        return Result;
    }

    template <typename... Args, std::enable_if_t<(detail::is_string_like_v<Args> && ...), int> = 0>
    NumericTerm operator()(Args&&... InArgs) const {
        TArray<detail::ArgumentRef> Refs;
        Refs.Reserve(sizeof...(Args));
        (Refs.Add(detail::ParameterRef(detail::ToString(Forward<Args>(InArgs)))), ...);
        ValidateArity(Refs.Num());
        NumericTerm Result;
        Result.FunctionId = IdValue;
        Result.FunctionName = NameValue;
        Result.ArgTypes = ArgTypesValue;
        Result.Args = MoveTemp(Refs);
        return Result;
    }

private:
    friend class Planner;

    Function(int32 InId, FString InName, TArray<const UClass*> InArgTypes)
        : IdValue(InId), NameValue(MoveTemp(InName)), ArgTypesValue(MoveTemp(InArgTypes)) {
    }

    template <typename TObject>
    static detail::ArgumentRef MakeObjectArgument(const TSoftObjectPtr<TObject>& Object) {
        return detail::ObjectRefFromSoft(Object);
    }

    template <typename TObject>
    static detail::ArgumentRef MakeObjectArgument(TObject* Object) {
        return detail::ObjectRefFromPointer(Object);
    }

    static detail::ArgumentRef MakeObjectArgument(const ObjectRef& Reference) {
        return detail::ObjectArgument(Reference);
    }

    void ValidateArity(int32 Arity) const {
        if (Arity != ArgTypesValue.Num()) {
            throw Error(FString::Printf(TEXT("wrong arity for function: %s"), *NameValue));
        }
    }

    void ValidateObjectTypes(const TArray<detail::ArgumentRef>& Refs) const {
        for (int32 Index = 0; Index < Refs.Num(); ++Index) {
            if (Refs[Index].DeclaredClass != ArgTypesValue[Index]) {
                throw Error(FString::Printf(TEXT("object type mismatch for function: %s"), *NameValue));
            }
        }
    }

    int32 IdValue = -1;
    FString NameValue;
    TArray<const UClass*> ArgTypesValue;
};

inline NumericPrecondition Compare(NumericTerm Term, TP_Num_Compare_Op Op, float Value) {
    NumericPrecondition Result;
    Result.Term = MoveTemp(Term);
    Result.Op = Op;
    Result.Value = Value;
    return Result;
}

inline NumericPrecondition operator<(NumericTerm Term, float Value) { return Compare(MoveTemp(Term), TP_NUM_CMP_LT, Value); }
inline NumericPrecondition operator<=(NumericTerm Term, float Value) { return Compare(MoveTemp(Term), TP_NUM_CMP_LTE, Value); }
inline NumericPrecondition operator==(NumericTerm Term, float Value) { return Compare(MoveTemp(Term), TP_NUM_CMP_EQ, Value); }
inline NumericPrecondition operator>=(NumericTerm Term, float Value) { return Compare(MoveTemp(Term), TP_NUM_CMP_GTE, Value); }
inline NumericPrecondition operator>(NumericTerm Term, float Value) { return Compare(MoveTemp(Term), TP_NUM_CMP_GT, Value); }

class PlanStep {
public:
    const FString& Name() const { return ActionValue.Name; }
    const Action& GetAction() const { return ActionValue; }
    bool Is(const Action& InAction) const { return ActionValue.Id == InAction.Id; }
    const FString& name() const { return Name(); }
    const Action& action() const { return GetAction(); }
    bool is(const Action& InAction) const { return Is(InAction); }

    template <typename TObject>
    TSoftObjectPtr<TObject> Arg(const FString& ParamName) const {
        static_assert(TIsDerivedFrom<TObject, UObject>::Value, "TObject must derive from UObject");
        const int32 Index = ParamIndex(ParamName);
        if (!ArgsValue.IsValidIndex(Index)) {
            throw Error(FString::Printf(TEXT("missing plan argument: %s"), *ParamName));
        }
        if (ArgsValue[Index].UnrealClass != TObject::StaticClass()) {
            throw Error(FString::Printf(TEXT("plan argument has unexpected type: %s"), *ParamName));
        }
        return ArgsValue[Index].As<TObject>();
    }

    const ObjectRef& ArgObject(const FString& ParamName) const {
        const int32 Index = ParamIndex(ParamName);
        if (!ArgsValue.IsValidIndex(Index)) {
            throw Error(FString::Printf(TEXT("missing plan argument: %s"), *ParamName));
        }
        return ArgsValue[Index];
    }

    template <typename TObject>
    TSoftObjectPtr<TObject> arg(const FString& ParamName) const {
        return Arg<TObject>(ParamName);
    }

    const ObjectRef& arg_object(const FString& ParamName) const {
        return ArgObject(ParamName);
    }

    FString ToString() const {
        FString Result = ActionValue.Name + TEXT("(");
        for (int32 Index = 0; Index < ArgsValue.Num(); ++Index) {
            if (Index > 0) {
                Result += TEXT(", ");
            }
            Result += ActionValue.ParamNames[Index];
        }
        Result += TEXT(")");
        return Result;
    }

    FString to_string() const {
        return ToString();
    }

private:
    friend class Planner;

    int32 ParamIndex(const FString& Name) const {
        for (int32 Index = 0; Index < ActionValue.ParamNames.Num(); ++Index) {
            if (ActionValue.ParamNames[Index] == Name) {
                return Index;
            }
        }
        throw Error(FString::Printf(TEXT("unknown action parameter: %s"), *Name));
    }

    Action ActionValue;
    TArray<ObjectRef> ArgsValue;
};

class SolveResult {
public:
    bool Solved() const { return bSolved; }
    TP_Status Status() const { return StatusValue; }
    int32 Expansions() const { return ExpansionsValue; }
    int32 Generated() const { return GeneratedValue; }
    int32 ScorerCalls() const { return ScorerCallsValue; }
    const TArray<PlanStep>& Steps() const { return StepsValue; }
    bool solved() const { return Solved(); }
    TP_Status status() const { return Status(); }
    int32 expansions() const { return Expansions(); }
    int32 generated() const { return Generated(); }
    int32 scorer_calls() const { return ScorerCalls(); }
    const TArray<PlanStep>& steps() const { return Steps(); }

private:
    friend class Planner;

    TP_Status StatusValue = TP_STATUS_UNSUPPORTED;
    bool bSolved = false;
    int32 ExpansionsValue = 0;
    int32 GeneratedValue = 0;
    int32 ScorerCallsValue = 0;
    TArray<PlanStep> StepsValue;
};

class StateBuilder {
public:
    template <typename TObject>
    StateBuilder& Object(const TSoftObjectPtr<TObject>& InObject) {
        RegisterObject(ObjectRef::From(InObject));
        return *this;
    }

    template <typename TObject>
    StateBuilder& Object(TObject* InObject) {
        RegisterObject(ObjectRef::From(InObject));
        return *this;
    }

    StateBuilder& Object(const ObjectRef& InObject) {
        RegisterObject(InObject);
        return *this;
    }

    template <typename TObject>
    StateBuilder& object(const TSoftObjectPtr<TObject>& InObject) {
        return Object(InObject);
    }

    template <typename TObject>
    StateBuilder& object(TObject* InObject) {
        return Object(InObject);
    }

    StateBuilder& object(const ObjectRef& InObject) {
        return Object(InObject);
    }

    StateBuilder& Fact(const Atom& InAtom) {
        FactsValue.Add(InAtom);
        RegisterAtomObjects(InAtom);
        return *this;
    }

    StateBuilder& fact(const Atom& InAtom) {
        return Fact(InAtom);
    }

    template <typename A, typename B>
    StateBuilder& Edge(const Predicate& InPredicate, A&& First, B&& Second) {
        return Fact(InPredicate(Forward<A>(First), Forward<B>(Second)))
            .Fact(InPredicate(Forward<B>(Second), Forward<A>(First)));
    }

    template <typename A, typename B>
    StateBuilder& edge(const Predicate& InPredicate, A&& First, B&& Second) {
        return Edge(InPredicate, Forward<A>(First), Forward<B>(Second));
    }

    StateBuilder& Goal(const Atom& InAtom) {
        GoalsValue.Add(InAtom);
        RegisterAtomObjects(InAtom);
        return *this;
    }

    StateBuilder& goal(const Atom& InAtom) {
        return Goal(InAtom);
    }

    StateBuilder& Value(const NumericTerm& InTerm, float InValue) {
        NumericValue Entry;
        Entry.Term = InTerm;
        Entry.Value = InValue;
        ValuesValue.Add(Entry);
        RegisterNumericTermObjects(InTerm);
        return *this;
    }

    StateBuilder& value(const NumericTerm& InTerm, float InValue) {
        return Value(InTerm, InValue);
    }

private:
    friend class Planner;

    struct NumericValue {
        NumericTerm Term;
        float Value = 0.0f;
    };

    explicit StateBuilder(Planner* InPlanner)
        : PlannerValue(InPlanner) {
    }

    int32 RegisterObject(const ObjectRef& InObject);
    void RegisterAtomObjects(const Atom& InAtom);
    void RegisterNumericTermObjects(const NumericTerm& InTerm);
    TArray<int32> ResolveObjectArgs(const Atom& InAtom) const;
    TArray<int32> ResolveObjectArgs(const NumericTerm& InTerm) const;

    Planner* PlannerValue = nullptr;
    TMap<FString, int32> ObjectIdsValue;
    TArray<ObjectRef> ObjectsValue;
    TArray<const UClass*> ObjectClassesValue;
    TArray<int32> ObjectTypesValue;
    TArray<Atom> FactsValue;
    TArray<Atom> GoalsValue;
    TArray<NumericValue> ValuesValue;
};

class ActionBuilder {
public:
    ActionBuilder(Planner* InPlanner, FString InName)
        : PlannerValue(InPlanner), NameValue(MoveTemp(InName)) {
    }

    template <typename TObject>
    ActionBuilder& Param(const FString& Name);

    template <typename TObject>
    ActionBuilder& param(const FString& Name) {
        return Param<TObject>(Name);
    }

    ActionBuilder& Require(const Atom& InAtom) {
        PreconditionsValue.Add(MakeLiteral(InAtom, 1));
        return *this;
    }

    ActionBuilder& require(const Atom& InAtom) {
        return Require(InAtom);
    }

    ActionBuilder& Require(const NumericPrecondition& InCondition) {
        NumericPreconditionsValue.Add(MakeNumericPrecondition(InCondition));
        return *this;
    }

    ActionBuilder& require(const NumericPrecondition& InCondition) {
        return Require(InCondition);
    }

    ActionBuilder& Adds(const Atom& InAtom) {
        EffectsValue.Add(MakeEffect(InAtom, TP_EFFECT_ADD));
        return *this;
    }

    ActionBuilder& adds(const Atom& InAtom) {
        return Adds(InAtom);
    }

    ActionBuilder& Removes(const Atom& InAtom) {
        EffectsValue.Add(MakeEffect(InAtom, TP_EFFECT_DELETE));
        return *this;
    }

    ActionBuilder& removes(const Atom& InAtom) {
        return Removes(InAtom);
    }

    ActionBuilder& Sets(const NumericTerm& InTerm, float InValue) {
        NumericEffect Effect;
        Effect.Term = InTerm;
        Effect.Op = TP_NUM_EFFECT_SET;
        Effect.Value = InValue;
        NumericEffectsValue.Add(MakeNumericEffect(Effect));
        return *this;
    }

    ActionBuilder& sets(const NumericTerm& InTerm, float InValue) {
        return Sets(InTerm, InValue);
    }

    ActionBuilder& Increases(const NumericTerm& InTerm, float InValue) {
        NumericEffect Effect;
        Effect.Term = InTerm;
        Effect.Op = TP_NUM_EFFECT_ADD;
        Effect.Value = InValue;
        NumericEffectsValue.Add(MakeNumericEffect(Effect));
        return *this;
    }

    ActionBuilder& increases(const NumericTerm& InTerm, float InValue) {
        return Increases(InTerm, InValue);
    }

    ActionBuilder& Decreases(const NumericTerm& InTerm, float InValue) {
        NumericEffect Effect;
        Effect.Term = InTerm;
        Effect.Op = TP_NUM_EFFECT_SUBTRACT;
        Effect.Value = InValue;
        NumericEffectsValue.Add(MakeNumericEffect(Effect));
        return *this;
    }

    ActionBuilder& decreases(const NumericTerm& InTerm, float InValue) {
        return Decreases(InTerm, InValue);
    }

    Action Commit();

    Action commit() {
        return Commit();
    }

private:
    friend class Planner;

    TP_Action_Literal MakeLiteral(const Atom& InAtom, int8 Sign) const;
    TP_Action_Effect MakeEffect(const Atom& InAtom, uint8 Op) const;
    TP_Numeric_Precondition MakeNumericPrecondition(const NumericPrecondition& InCondition) const;
    TP_Numeric_Effect MakeNumericEffect(const NumericEffect& InEffect) const;
    int8 ResolveSlot(const detail::ArgumentRef& InArg, const FString& InOwnerName) const;
    void ValidateNumericSlots(const NumericTerm& InTerm) const;

    Planner* PlannerValue = nullptr;
    FString NameValue;
    TMap<FString, int32> SlotIdsValue;
    TMap<FString, const UClass*> SlotClassesValue;
    TArray<FString> ParamNamesValue;
    TArray<int32> ArgTypesValue;
    TArray<TP_Action_Literal> PreconditionsValue;
    TArray<TP_Action_Effect> EffectsValue;
    TArray<TP_Numeric_Precondition> NumericPreconditionsValue;
    TArray<TP_Numeric_Effect> NumericEffectsValue;
};

class Planner {
public:
    explicit Planner(const Limits& InLimits = Limits()) {
        TP_Limits NativeLimits{};
        NativeLimits.max_objects = InLimits.MaxObjects;
        NativeLimits.max_facts = InLimits.MaxFacts;
        NativeLimits.max_goals = InLimits.MaxGoals;
        NativeLimits.max_candidates = InLimits.MaxCandidates;
        NativeLimits.max_expansions = InLimits.MaxExpansions;
        NativeLimits.max_plan_length = InLimits.MaxPlanLength;

        DomainValue = tp_domain_create(&NativeLimits);
        if (DomainValue == nullptr) {
            throw Error(TEXT("tp_domain_create failed"));
        }
    }

    ~Planner() {
        if (DomainValue != nullptr) {
            tp_domain_destroy(DomainValue);
        }
    }

    Planner(const Planner&) = delete;
    Planner& operator=(const Planner&) = delete;

    Planner(Planner&& Other) noexcept {
        *this = MoveTemp(Other);
    }

    Planner& operator=(Planner&& Other) noexcept {
        if (this != &Other) {
            if (DomainValue != nullptr) {
                tp_domain_destroy(DomainValue);
            }
            DomainValue = Other.DomainValue;
            Other.DomainValue = nullptr;
            TypesValue = MoveTemp(Other.TypesValue);
            PredicateNamesValue = MoveTemp(Other.PredicateNamesValue);
            ActionNamesValue = MoveTemp(Other.ActionNamesValue);
        }
        return *this;
    }

    template <typename TObject>
    Type TypeOf(const FString& Name = TObject::StaticClass()->GetName()) {
        static_assert(TIsDerivedFrom<TObject, UObject>::Value, "TObject must derive from UObject");
        return TypeFor(TObject::StaticClass(), Name);
    }

    template <typename TObject>
    Type type(const FString& Name = TObject::StaticClass()->GetName()) {
        return TypeOf<TObject>(Name);
    }

    template <typename... TObjects>
    Predicate PredicateOf(const FString& Name) {
        TArray<Type> Types;
        TArray<const UClass*> Classes;
        Types.Reserve(sizeof...(TObjects));
        Classes.Reserve(sizeof...(TObjects));
        (Types.Add(TypeOf<TObjects>()), ...);
        (Classes.Add(TObjects::StaticClass()), ...);
        return PredicateFromArray(Name, Types, MoveTemp(Classes));
    }

    template <typename... TObjects>
    Predicate predicate(const FString& Name) {
        return PredicateOf<TObjects...>(Name);
    }

    template <typename... TObjects>
    Function FunctionOf(const FString& Name) {
        TArray<Type> Types;
        TArray<const UClass*> Classes;
        Types.Reserve(sizeof...(TObjects));
        Classes.Reserve(sizeof...(TObjects));
        (Types.Add(TypeOf<TObjects>()), ...);
        (Classes.Add(TObjects::StaticClass()), ...);
        return FunctionFromArray(Name, Types, MoveTemp(Classes));
    }

    template <typename... TObjects>
    Function function(const FString& Name) {
        return FunctionOf<TObjects...>(Name);
    }

    ActionBuilder ActionOf(const FString& Name) { return ActionBuilder(this, Name); }
    StateBuilder State() { return StateBuilder(this); }

    ActionBuilder action(const FString& Name) { return ActionOf(Name); }
    StateBuilder state() { return State(); }

    SolveResult Solve(const StateBuilder& StateValue) const {
        return SolveImpl(StateValue, nullptr, nullptr);
    }

    SolveResult solve(const StateBuilder& StateValue) const {
        return Solve(StateValue);
    }

    SolveResult Solve(const StateBuilder& StateValue, TP_Score_Candidates_Fn Scorer, void* ScorerUserData = nullptr) const {
        return SolveImpl(StateValue, Scorer, ScorerUserData);
    }

    SolveResult solve(const StateBuilder& StateValue, TP_Score_Candidates_Fn Scorer, void* ScorerUserData = nullptr) const {
        return Solve(StateValue, Scorer, ScorerUserData);
    }

private:
    friend class ActionBuilder;
    friend class StateBuilder;

    SolveResult SolveImpl(const StateBuilder& StateValue, TP_Score_Candidates_Fn Scorer, void* ScorerUserData) const {
        TP_State* RawState = tp_state_create(DomainValue, StateValue.ObjectTypesValue.Num(), StateValue.ObjectTypesValue.GetData());
        if (RawState == nullptr) {
            throw Error(TEXT("tp_state_create failed"));
        }

        TP_Solver* Solver = nullptr;
        TP_Solve_Result RawResult{};

        try {
            for (const Atom& Fact : StateValue.FactsValue) {
                const TArray<int32> Args = StateValue.ResolveObjectArgs(Fact);
                detail::Check(tp_state_add_fact(RawState, Fact.PredicateId, static_cast<uint8>(Args.Num()), Args.GetData()), TEXT("add fact"));
            }

            for (const Atom& Goal : StateValue.GoalsValue) {
                const TArray<int32> Args = StateValue.ResolveObjectArgs(Goal);
                detail::Check(tp_state_add_goal_fact(RawState, Goal.PredicateId, static_cast<uint8>(Args.Num()), Args.GetData()), TEXT("add goal"));
            }

            for (const StateBuilder::NumericValue& Value : StateValue.ValuesValue) {
                const TArray<int32> Args = StateValue.ResolveObjectArgs(Value.Term);
                detail::Check(tp_state_set_function_value(RawState, Value.Term.FunctionId, static_cast<uint8>(Args.Num()), Args.GetData(), Value.Value), TEXT("set function value"));
            }

            Solver = tp_solver_create(DomainValue);
            if (Solver == nullptr) {
                throw Error(TEXT("tp_solver_create failed"));
            }

            if (Scorer != nullptr) {
                detail::Check(tp_solver_set_custom_guidance(Solver, Scorer, ScorerUserData), TEXT("set custom guidance"));
            } else {
                detail::Check(tp_solver_use_default_guidance(Solver), TEXT("use default guidance"));
            }

            const TP_Status Status = tp_solver_solve(Solver, RawState, &RawResult);
            SolveResult Result;
            Result.StatusValue = Status;
            Result.bSolved = RawResult.solved;
            Result.ExpansionsValue = RawResult.expansions;
            Result.GeneratedValue = RawResult.generated;
            Result.ScorerCallsValue = RawResult.scorer_calls;

            if (RawResult.solved) {
                for (int32 Index = 0; Index < RawResult.plan_length; ++Index) {
                    Result.StepsValue.Add(MakeStep(RawResult.plan_actions[Index], StateValue));
                }
            }

            tp_solve_result_dispose(&RawResult);
            tp_solver_destroy(Solver);
            tp_state_destroy(RawState);
            return Result;
        } catch (...) {
            tp_solve_result_dispose(&RawResult);
            if (Solver != nullptr) {
                tp_solver_destroy(Solver);
            }
            tp_state_destroy(RawState);
            throw;
        }
    }

    Type TypeFor(const UClass* UnrealClass, const FString& Name) {
        const Type* Existing = TypesValue.Find(UnrealClass);
        if (Existing != nullptr) {
            return *Existing;
        }

        Type NewType;
        NewType.Id = TypesValue.Num();
        NewType.UnrealClass = UnrealClass;
        NewType.Name = Name;
        TypesValue.Add(UnrealClass, NewType);
        return NewType;
    }

    int32 TypeIdFor(const UClass* UnrealClass) const {
        const Type* Found = TypesValue.Find(UnrealClass);
        if (Found == nullptr) {
            throw Error(TEXT("unregistered Unreal object type"));
        }
        return Found->Id;
    }

    Predicate PredicateFromArray(const FString& Name, const TArray<Type>& Types, TArray<const UClass*> Classes) {
        if (Types.Num() > TP_MAX_ARITY) {
            throw Error(FString::Printf(TEXT("predicate arity exceeds TP_MAX_ARITY: %s"), *Name));
        }

        TP_Predicate_Def NativePredicate{};
        NativePredicate.arity = static_cast<uint8>(Types.Num());
        for (int32 Index = 0; Index < Types.Num(); ++Index) {
            NativePredicate.arg_types[Index] = Types[Index].Id;
        }

        int32 PredicateId = -1;
        detail::Check(tp_domain_add_predicate(DomainValue, &NativePredicate, &PredicateId), TEXT("add predicate"));
        PredicateNamesValue.Add(PredicateId, Name);
        return Predicate(PredicateId, Name, MoveTemp(Classes));
    }

    Function FunctionFromArray(const FString& Name, const TArray<Type>& Types, TArray<const UClass*> Classes) {
        if (Types.Num() > TP_MAX_ARITY) {
            throw Error(FString::Printf(TEXT("function arity exceeds TP_MAX_ARITY: %s"), *Name));
        }

        TP_Function_Def NativeFunction{};
        NativeFunction.arity = static_cast<uint8>(Types.Num());
        for (int32 Index = 0; Index < Types.Num(); ++Index) {
            NativeFunction.arg_types[Index] = Types[Index].Id;
        }

        int32 FunctionId = -1;
        detail::Check(tp_domain_add_function(DomainValue, &NativeFunction, &FunctionId), TEXT("add function"));
        return Function(FunctionId, Name, MoveTemp(Classes));
    }

    Action CommitAction(ActionBuilder& Builder) {
        if (Builder.ArgTypesValue.IsEmpty()) {
            throw Error(FString::Printf(TEXT("action needs at least one parameter: %s"), *Builder.NameValue));
        }
        if (Builder.ArgTypesValue.Num() > TP_MAX_PARAMS) {
            throw Error(FString::Printf(TEXT("action parameter count exceeds TP_MAX_PARAMS: %s"), *Builder.NameValue));
        }

        int32 ActionId = -1;
        detail::Check(
            tp_domain_add_action_schema(
                DomainValue,
                static_cast<uint8>(Builder.ArgTypesValue.Num()),
                Builder.ArgTypesValue.GetData(),
                Builder.PreconditionsValue.GetData(),
                Builder.PreconditionsValue.Num(),
                Builder.EffectsValue.GetData(),
                Builder.EffectsValue.Num(),
                Builder.NumericPreconditionsValue.GetData(),
                Builder.NumericPreconditionsValue.Num(),
                Builder.NumericEffectsValue.GetData(),
                Builder.NumericEffectsValue.Num(),
                &ActionId),
            TEXT("add action schema"));

        Action NewAction;
        NewAction.Id = ActionId;
        NewAction.Name = Builder.NameValue;
        NewAction.ParamNames = Builder.ParamNamesValue;
        ActionNamesValue.Add(ActionId, NewAction);
        return NewAction;
    }

    PlanStep MakeStep(const TP_Candidate_Action& RawAction, const StateBuilder& StateValue) const {
        PlanStep Step;
        if (const Action* FoundAction = ActionNamesValue.Find(RawAction.schema_id)) {
            Step.ActionValue = *FoundAction;
        } else {
            Step.ActionValue.Id = RawAction.schema_id;
            Step.ActionValue.Name = FString::Printf(TEXT("action_%d"), RawAction.schema_id);
        }

        for (int32 Index = 0; Index < RawAction.arity; ++Index) {
            const int32 ObjectId = RawAction.args[Index];
            if (!StateValue.ObjectsValue.IsValidIndex(ObjectId)) {
                throw Error(TEXT("plan references unknown object id"));
            }
            Step.ArgsValue.Add(StateValue.ObjectsValue[ObjectId]);
        }

        return Step;
    }

    TP_Domain* DomainValue = nullptr;
    TMap<const UClass*, Type> TypesValue;
    TMap<int32, FString> PredicateNamesValue;
    TMap<int32, Action> ActionNamesValue;
};

inline int32 StateBuilder::RegisterObject(const ObjectRef& InObject) {
    if (InObject.UnrealClass == nullptr) {
        throw Error(TEXT("state object is missing a declared Unreal class"));
    }

    const FString PathKey = InObject.PathString();
    if (PathKey.IsEmpty()) {
        throw Error(TEXT("state object has an empty soft object path"));
    }

    if (const int32* ExistingId = ObjectIdsValue.Find(PathKey)) {
        return *ExistingId;
    }

    const int32 ObjectId = ObjectsValue.Num();
    ObjectIdsValue.Add(PathKey, ObjectId);
    ObjectsValue.Add(InObject);
    ObjectClassesValue.Add(InObject.UnrealClass);
    ObjectTypesValue.Add(PlannerValue->TypeFor(InObject.UnrealClass, InObject.UnrealClass->GetName()).Id);
    return ObjectId;
}

inline void StateBuilder::RegisterAtomObjects(const Atom& InAtom) {
    for (const detail::ArgumentRef& Arg : InAtom.Args) {
        if (Arg.RefKind != detail::ArgumentRef::Kind::Object) {
            throw Error(FString::Printf(TEXT("state atom uses action parameter in predicate: %s"), *InAtom.PredicateName));
        }
        RegisterObject(Arg.Reference);
    }
}

inline void StateBuilder::RegisterNumericTermObjects(const NumericTerm& InTerm) {
    for (const detail::ArgumentRef& Arg : InTerm.Args) {
        if (Arg.RefKind != detail::ArgumentRef::Kind::Object) {
            throw Error(FString::Printf(TEXT("state function term uses action parameter: %s"), *InTerm.FunctionName));
        }
        RegisterObject(Arg.Reference);
    }
}

inline TArray<int32> StateBuilder::ResolveObjectArgs(const Atom& InAtom) const {
    TArray<int32> Args;
    Args.Reserve(InAtom.Args.Num());
    for (const detail::ArgumentRef& Arg : InAtom.Args) {
        if (Arg.RefKind != detail::ArgumentRef::Kind::Object) {
            throw Error(FString::Printf(TEXT("state atom uses action parameter in predicate: %s"), *InAtom.PredicateName));
        }
        const int32* FoundId = ObjectIdsValue.Find(Arg.Reference.PathString());
        if (FoundId == nullptr) {
            throw Error(FString::Printf(TEXT("unknown object in predicate: %s"), *InAtom.PredicateName));
        }
        Args.Add(*FoundId);
    }
    return Args;
}

inline TArray<int32> StateBuilder::ResolveObjectArgs(const NumericTerm& InTerm) const {
    TArray<int32> Args;
    Args.Reserve(InTerm.Args.Num());
    for (const detail::ArgumentRef& Arg : InTerm.Args) {
        if (Arg.RefKind != detail::ArgumentRef::Kind::Object) {
            throw Error(FString::Printf(TEXT("state function term uses action parameter: %s"), *InTerm.FunctionName));
        }
        const int32* FoundId = ObjectIdsValue.Find(Arg.Reference.PathString());
        if (FoundId == nullptr) {
            throw Error(FString::Printf(TEXT("unknown object in function: %s"), *InTerm.FunctionName));
        }
        Args.Add(*FoundId);
    }
    return Args;
}

template <typename TObject>
inline ActionBuilder& ActionBuilder::Param(const FString& Name) {
    static_assert(TIsDerivedFrom<TObject, UObject>::Value, "TObject must derive from UObject");
    if (SlotIdsValue.Contains(Name)) {
        throw Error(FString::Printf(TEXT("duplicate action parameter: %s"), *Name));
    }
    if (ArgTypesValue.Num() >= TP_MAX_PARAMS) {
        throw Error(FString::Printf(TEXT("too many action parameters in: %s"), *NameValue));
    }

    const Type RegisteredType = PlannerValue->TypeOf<TObject>();
    SlotIdsValue.Add(Name, ArgTypesValue.Num());
    SlotClassesValue.Add(Name, TObject::StaticClass());
    ParamNamesValue.Add(Name);
    ArgTypesValue.Add(RegisteredType.Id);
    return *this;
}

inline TP_Action_Literal ActionBuilder::MakeLiteral(const Atom& InAtom, int8 Sign) const {
    if (InAtom.Args.Num() > TP_MAX_ARITY) {
        throw Error(FString::Printf(TEXT("precondition arity exceeds TP_MAX_ARITY: %s"), *InAtom.PredicateName));
    }

    TP_Action_Literal Literal{};
    Literal.predicate_id = InAtom.PredicateId;
    Literal.sign = Sign;
    Literal.arity = static_cast<uint8>(InAtom.Args.Num());

    for (int32 Index = 0; Index < InAtom.Args.Num(); ++Index) {
        Literal.slots[Index] = ResolveSlot(InAtom.Args[Index], InAtom.PredicateName);
        const UClass* const* SlotClass = SlotClassesValue.Find(InAtom.Args[Index].ParameterName);
        if (SlotClass == nullptr || *SlotClass != InAtom.ArgTypes[Index]) {
            throw Error(FString::Printf(TEXT("parameter type mismatch for predicate: %s"), *InAtom.PredicateName));
        }
    }

    return Literal;
}

inline TP_Action_Effect ActionBuilder::MakeEffect(const Atom& InAtom, uint8 Op) const {
    if (InAtom.Args.Num() > TP_MAX_ARITY) {
        throw Error(FString::Printf(TEXT("effect arity exceeds TP_MAX_ARITY: %s"), *InAtom.PredicateName));
    }

    TP_Action_Effect Effect{};
    Effect.predicate_id = InAtom.PredicateId;
    Effect.op = Op;
    Effect.arity = static_cast<uint8>(InAtom.Args.Num());

    for (int32 Index = 0; Index < InAtom.Args.Num(); ++Index) {
        Effect.slots[Index] = ResolveSlot(InAtom.Args[Index], InAtom.PredicateName);
        const UClass* const* SlotClass = SlotClassesValue.Find(InAtom.Args[Index].ParameterName);
        if (SlotClass == nullptr || *SlotClass != InAtom.ArgTypes[Index]) {
            throw Error(FString::Printf(TEXT("parameter type mismatch for predicate: %s"), *InAtom.PredicateName));
        }
    }

    return Effect;
}

inline TP_Numeric_Precondition ActionBuilder::MakeNumericPrecondition(const NumericPrecondition& InCondition) const {
    ValidateNumericSlots(InCondition.Term);

    TP_Numeric_Precondition Result{};
    Result.function_id = InCondition.Term.FunctionId;
    Result.cmp_op = static_cast<uint8>(InCondition.Op);
    Result.arity = static_cast<uint8>(InCondition.Term.Args.Num());
    Result.rhs_value = InCondition.Value;

    for (int32 Index = 0; Index < InCondition.Term.Args.Num(); ++Index) {
        Result.slots[Index] = ResolveSlot(InCondition.Term.Args[Index], InCondition.Term.FunctionName);
    }

    return Result;
}

inline TP_Numeric_Effect ActionBuilder::MakeNumericEffect(const NumericEffect& InEffect) const {
    ValidateNumericSlots(InEffect.Term);

    TP_Numeric_Effect Result{};
    Result.function_id = InEffect.Term.FunctionId;
    Result.op = static_cast<uint8>(InEffect.Op);
    Result.arity = static_cast<uint8>(InEffect.Term.Args.Num());
    Result.rhs_value = InEffect.Value;

    for (int32 Index = 0; Index < InEffect.Term.Args.Num(); ++Index) {
        Result.slots[Index] = ResolveSlot(InEffect.Term.Args[Index], InEffect.Term.FunctionName);
    }

    return Result;
}

inline void ActionBuilder::ValidateNumericSlots(const NumericTerm& InTerm) const {
    if (InTerm.Args.Num() > TP_MAX_ARITY) {
        throw Error(FString::Printf(TEXT("function arity exceeds TP_MAX_ARITY: %s"), *InTerm.FunctionName));
    }

    for (int32 Index = 0; Index < InTerm.Args.Num(); ++Index) {
        const UClass* const* SlotClass = SlotClassesValue.Find(InTerm.Args[Index].ParameterName);
        if (SlotClass == nullptr || *SlotClass != InTerm.ArgTypes[Index]) {
            throw Error(FString::Printf(TEXT("parameter type mismatch for function: %s"), *InTerm.FunctionName));
        }
    }
}

inline int8 ActionBuilder::ResolveSlot(const detail::ArgumentRef& InArg, const FString& InOwnerName) const {
    if (InArg.RefKind != detail::ArgumentRef::Kind::Parameter) {
        throw Error(FString::Printf(TEXT("action atom uses object reference in predicate: %s"), *InOwnerName));
    }

    const int32* FoundSlot = SlotIdsValue.Find(InArg.ParameterName);
    if (FoundSlot == nullptr) {
        throw Error(FString::Printf(TEXT("unknown action parameter '%s' in %s"), *InArg.ParameterName, *InOwnerName));
    }

    return static_cast<int8>(*FoundSlot);
}

inline Action ActionBuilder::Commit() {
    if (PlannerValue == nullptr) {
        throw Error(TEXT("action builder has no planner"));
    }
    return PlannerValue->CommitAction(*this);
}

}  // namespace tpue
