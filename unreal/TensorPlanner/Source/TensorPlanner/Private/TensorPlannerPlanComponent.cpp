#include "TensorPlannerPlanComponent.h"

#include "AIController.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "TensorPlannerModule.h"
#include "TensorPlannerUnreal.h"

namespace
{
FTensorPlannerPlanStep MakeStep(const tpue::PlanStep& InStep, const float InDurationSeconds)
{
    FTensorPlannerPlanStep Step;
    Step.ActionName = FName(*InStep.Name());
    Step.DurationSeconds = InDurationSeconds;

    for (const FString& ParamName : InStep.GetAction().ParamNames)
    {
        Step.ObjectArguments.Add(FName(*ParamName), InStep.ArgObject(ParamName).Object);
    }

    return Step;
}
}

UTensorPlannerPlanComponent::UTensorPlannerPlanComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UTensorPlannerPlanComponent::BuildExamplePlan()
{
    AActor* PlanningActor = ResolvePlanningActor();
    if (PlanningActor == nullptr)
    {
        UE_LOG(
            LogTensorPlanner,
            Error,
            TEXT("[UTensorPlannerPlanComponent::BuildExamplePlan] Planning actor is null for owner=%s. Fix: attach this component to an AIController with a possessed Pawn, or attach it directly to the Pawn/Actor that should own per-NPC plan state."),
            *GetNameSafe(GetOwner()));
        ClearPlan();
        return false;
    }

    return BuildExampleWaitPlan(PlanningActor);
}

bool UTensorPlannerPlanComponent::BuildExampleNavigationPlan()
{
    AActor* PlanningActor = ResolvePlanningActor();
    if (PlanningActor == nullptr)
    {
        UE_LOG(
            LogTensorPlanner,
            Error,
            TEXT("[UTensorPlannerPlanComponent::BuildExampleNavigationPlan] Planning actor is null for owner=%s. Fix: attach this component to an AIController with a possessed Pawn, or attach it directly to the Pawn/Actor that should own per-NPC plan state."),
            *GetNameSafe(GetOwner()));
        ClearPlan();
        return false;
    }

    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        UE_LOG(
            LogTensorPlanner,
            Error,
            TEXT("[UTensorPlannerPlanComponent::BuildExampleNavigationPlan] World is null for owner=%s. Fix: request navigation plans only from a live world/AI runtime context."),
            *GetNameSafe(GetOwner()));
        ClearPlan();
        return false;
    }

    TArray<AActor*> NavigationTargets;
    UGameplayStatics::GetAllActorsWithTag(World, ExampleNavigationTargetTag, NavigationTargets);
    if (NavigationTargets.Num() < 2)
    {
        UE_LOG(
            LogTensorPlanner,
            Error,
            TEXT("[UTensorPlannerPlanComponent::BuildExampleNavigationPlan] Found only %d navigation targets with tag '%s'. Fix: place at least two target actors in the level and give them the tag '%s' so the example walking plan has reachable destinations."),
            NavigationTargets.Num(),
            *ExampleNavigationTargetTag.ToString(),
            *ExampleNavigationTargetTag.ToString());
        ClearPlan();
        return false;
    }

    NavigationTargets.Sort([](const AActor& Left, const AActor& Right)
    {
        return Left.GetName() < Right.GetName();
    });

    AActor* FirstTarget = NavigationTargets[0];
    AActor* SecondTarget = NavigationTargets[1];
    if (FirstTarget == nullptr || SecondTarget == nullptr)
    {
        UE_LOG(
            LogTensorPlanner,
            Error,
            TEXT("[UTensorPlannerPlanComponent::BuildExampleNavigationPlan] Navigation target lookup returned null actors. Fix: delete and recreate the test target actors in the level."));
        ClearPlan();
        return false;
    }

    try
    {
        tpue::Planner Planner;
        const tpue::Predicate Ready = Planner.PredicateOf<AActor>(TEXT("ready"));
        const tpue::Predicate ReachedFirst = Planner.PredicateOf<AActor>(TEXT("reached_first"));
        const tpue::Predicate PlanFinished = Planner.PredicateOf<AActor>(TEXT("plan_finished"));

        Planner.ActionOf(TEXT("move_to_target_01"))
            .Param<AActor>(TEXT("npc"))
            .Param<AActor>(TEXT("target"))
            .Require(Ready(TEXT("npc")))
            .Removes(Ready(TEXT("npc")))
            .Adds(ReachedFirst(TEXT("npc")))
            .Commit();

        Planner.ActionOf(TEXT("move_to_target_02"))
            .Param<AActor>(TEXT("npc"))
            .Param<AActor>(TEXT("target"))
            .Require(ReachedFirst(TEXT("npc")))
            .Removes(ReachedFirst(TEXT("npc")))
            .Adds(PlanFinished(TEXT("npc")))
            .Commit();

        const tpue::SolveResult Result = Planner.Solve(
            Planner.State()
                .Fact(Ready(PlanningActor))
                .Goal(PlanFinished(PlanningActor)));

        if (!Result.Solved())
        {
            UE_LOG(
                LogTensorPlanner,
                Error,
                TEXT("[UTensorPlannerPlanComponent::BuildExamplePlan] Example plan solve failed for actor=%s. Fix: verify your planner domain/actions/goals are valid before requesting a plan."),
                *GetNameSafe(PlanningActor));
            ClearPlan();
            return false;
        }

        TArray<FTensorPlannerPlanStep> NewPlanSteps;
        NewPlanSteps.Reserve(Result.Steps().Num());

        for (int32 StepIndex = 0; StepIndex < Result.Steps().Num(); ++StepIndex)
        {
            FTensorPlannerPlanStep Step = MakeStep(Result.Steps()[StepIndex], ExampleStepDurationSeconds);
            Step.DurationSeconds = 0.0f;
            Step.ObjectArguments.Add(TEXT("target"), StepIndex == 0 ? TSoftObjectPtr<UObject>(FirstTarget) : TSoftObjectPtr<UObject>(SecondTarget));
            NewPlanSteps.Add(MoveTemp(Step));
        }

        SetPlanSteps(MoveTemp(NewPlanSteps));
        return HasPlan();
    }
    catch (const tpue::Error&)
    {
        UE_LOG(
            LogTensorPlanner,
            Error,
            TEXT("[UTensorPlannerPlanComponent::BuildExamplePlan] TensorPlanner wrapper threw while building example plan for actor=%s. Fix: verify the plugin native library is packaged correctly and the example planning domain is valid."),
            *GetNameSafe(PlanningActor));
        ClearPlan();
        return false;
    }
}

bool UTensorPlannerPlanComponent::BuildExampleWaitPlan(AActor* PlanningActor)
{
    try
    {
        tpue::Planner Planner;
        const tpue::Predicate Ready = Planner.PredicateOf<AActor>(TEXT("ready"));
        const tpue::Predicate StepOneDone = Planner.PredicateOf<AActor>(TEXT("step_one_done"));
        const tpue::Predicate PlanFinished = Planner.PredicateOf<AActor>(TEXT("plan_finished"));

        Planner.ActionOf(TEXT("wait_decoy_step_01"))
            .Param<AActor>(TEXT("npc"))
            .Require(Ready(TEXT("npc")))
            .Removes(Ready(TEXT("npc")))
            .Adds(StepOneDone(TEXT("npc")))
            .Commit();

        Planner.ActionOf(TEXT("wait_decoy_step_02"))
            .Param<AActor>(TEXT("npc"))
            .Require(StepOneDone(TEXT("npc")))
            .Removes(StepOneDone(TEXT("npc")))
            .Adds(PlanFinished(TEXT("npc")))
            .Commit();

        const tpue::SolveResult Result = Planner.Solve(
            Planner.State()
                .Fact(Ready(PlanningActor))
                .Goal(PlanFinished(PlanningActor)));

        if (!Result.Solved())
        {
            UE_LOG(
                LogTensorPlanner,
                Error,
                TEXT("[UTensorPlannerPlanComponent::BuildExamplePlan] Example plan solve failed for actor=%s. Fix: verify your planner domain/actions/goals are valid before requesting a plan."),
                *GetNameSafe(PlanningActor));
            ClearPlan();
            return false;
        }

        TArray<FTensorPlannerPlanStep> NewPlanSteps;
        NewPlanSteps.Reserve(Result.Steps().Num());

        for (const tpue::PlanStep& Step : Result.Steps())
        {
            NewPlanSteps.Add(MakeStep(Step, ExampleStepDurationSeconds));
        }

        SetPlanSteps(MoveTemp(NewPlanSteps));
        return HasPlan();
    }
    catch (const tpue::Error&)
    {
        UE_LOG(
            LogTensorPlanner,
            Error,
            TEXT("[UTensorPlannerPlanComponent::BuildExamplePlan] TensorPlanner wrapper threw while building example plan for actor=%s. Fix: verify the plugin native library is packaged correctly and the example planning domain is valid."),
            *GetNameSafe(PlanningActor));
        ClearPlan();
        return false;
    }
}

void UTensorPlannerPlanComponent::ClearPlan()
{
    PlanSteps.Reset();
    CurrentStepIndex = INDEX_NONE;
}

bool UTensorPlannerPlanComponent::HasPlan() const
{
    return HasCurrentStep();
}

bool UTensorPlannerPlanComponent::HasCurrentStep() const
{
    return HasValidCurrentStepIndex() && !IsPlanComplete();
}

bool UTensorPlannerPlanComponent::IsCurrentStepDone() const
{
    const FTensorPlannerPlanStep* Step = GetCurrentStep();
    return Step != nullptr && Step->bCompleted;
}

bool UTensorPlannerPlanComponent::IsPlanComplete() const
{
    return PlanSteps.Num() > 0 && CurrentStepIndex >= PlanSteps.Num();
}

int32 UTensorPlannerPlanComponent::GetCurrentStepIndex() const
{
    return HasCurrentStep() ? CurrentStepIndex : INDEX_NONE;
}

FName UTensorPlannerPlanComponent::GetCurrentStepName() const
{
    const FTensorPlannerPlanStep* Step = GetCurrentStep();
    return Step != nullptr ? Step->ActionName : NAME_None;
}

float UTensorPlannerPlanComponent::GetCurrentStepDurationSeconds() const
{
    const FTensorPlannerPlanStep* Step = GetCurrentStep();
    return Step != nullptr ? Step->DurationSeconds : 0.0f;
}

AActor* UTensorPlannerPlanComponent::GetCurrentStepActorArgument(const FName ParamName) const
{
    const FTensorPlannerPlanStep* Step = GetCurrentStep();
    if (Step == nullptr)
    {
        return nullptr;
    }

    const TSoftObjectPtr<UObject>* Found = Step->ObjectArguments.Find(ParamName);
    return Found != nullptr ? Cast<AActor>(Found->Get()) : nullptr;
}

float UTensorPlannerPlanComponent::GetCurrentStepAcceptanceRadius() const
{
    return ExampleMoveAcceptanceRadius;
}

bool UTensorPlannerPlanComponent::MarkCurrentStepDone()
{
    if (!HasCurrentStep())
    {
        UE_LOG(
            LogTensorPlanner,
            Error,
            TEXT("[UTensorPlannerPlanComponent::MarkCurrentStepDone] No current step exists for owner=%s. Fix: request/build a plan first and only mark completion while a valid step is active."),
            *GetNameSafe(GetOwner()));
        return false;
    }

    PlanSteps[CurrentStepIndex].bCompleted = true;
    return true;
}

bool UTensorPlannerPlanComponent::AdvanceToNextStep()
{
    if (!HasCurrentStep() || !IsCurrentStepDone())
    {
        UE_LOG(
            LogTensorPlanner,
            Error,
            TEXT("[UTensorPlannerPlanComponent::AdvanceToNextStep] Cannot advance plan for owner=%s because the current step is missing or not completed. Fix: ensure a plan exists and call MarkCurrentStepDone before advancing."),
            *GetNameSafe(GetOwner()));
        return false;
    }

    ++CurrentStepIndex;
    return true;
}

bool UTensorPlannerPlanComponent::CurrentStepMatchesActionPrefix(const FName Prefix) const
{
    const FTensorPlannerPlanStep* Step = GetCurrentStep();
    return Step != nullptr && Step->ActionName.ToString().StartsWith(Prefix.ToString());
}

const FTensorPlannerPlanStep* UTensorPlannerPlanComponent::GetCurrentStep() const
{
    return HasCurrentStep() ? &PlanSteps[CurrentStepIndex] : nullptr;
}

AActor* UTensorPlannerPlanComponent::ResolvePlanningActor() const
{
    if (const AAIController* Controller = Cast<AAIController>(GetOwner()))
    {
        if (APawn* Pawn = Controller->GetPawn())
        {
            return Pawn;
        }
    }

    return GetOwner();
}

bool UTensorPlannerPlanComponent::HasValidCurrentStepIndex() const
{
    return CurrentStepIndex >= 0 && CurrentStepIndex < PlanSteps.Num();
}

void UTensorPlannerPlanComponent::SetPlanSteps(TArray<FTensorPlannerPlanStep> InSteps)
{
    PlanSteps = MoveTemp(InSteps);
    CurrentStepIndex = PlanSteps.IsEmpty() ? INDEX_NONE : 0;
}
