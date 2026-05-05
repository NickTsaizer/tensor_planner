#include "AI/BTTask_TensorPlannerMoveToCurrentStepTarget.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"

#include "AI/TensorPlannerAIHelpers.h"

UBTTask_TensorPlannerMoveToCurrentStepTarget::UBTTask_TensorPlannerMoveToCurrentStepTarget()
{
    NodeName = TEXT("Tensor Planner Move To Current Step Target");
    bNotifyTick = true;
}

uint16 UBTTask_TensorPlannerMoveToCurrentStepTarget::GetInstanceMemorySize() const
{
    return sizeof(FTensorPlannerMoveMemory);
}

EBTNodeResult::Type UBTTask_TensorPlannerMoveToCurrentStepTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* Controller = OwnerComp.GetAIOwner();
    if (Controller == nullptr)
    {
        TensorPlannerAI::LogSetupError(
            TEXT("UBTTask_TensorPlannerMoveToCurrentStepTarget::ExecuteTask"),
            TEXT("AIController is null for the active Behavior Tree."),
            TEXT("Run this task from an AI-controlled pawn with a valid AAIController."));
        return EBTNodeResult::Failed;
    }

    APawn* Pawn = Controller->GetPawn();
    if (Pawn == nullptr)
    {
        TensorPlannerAI::LogSetupError(
            TEXT("UBTTask_TensorPlannerMoveToCurrentStepTarget::ExecuteTask"),
            FString::Printf(TEXT("Controlled pawn is null for controller=%s."), *GetNameSafe(Controller)),
            TEXT("Possess a valid Character/Pawn before running TensorPlanner navigation tasks."));
        return EBTNodeResult::Failed;
    }

    UTensorPlannerPlanComponent* PlanComponent = TensorPlannerAI::FindPlanComponent(OwnerComp);
    if (PlanComponent == nullptr)
    {
        TensorPlannerAI::LogMissingPlanComponent(TEXT("UBTTask_TensorPlannerMoveToCurrentStepTarget::ExecuteTask"), OwnerComp);
        return EBTNodeResult::Failed;
    }

    if (!PlanComponent->HasCurrentStep())
    {
        TensorPlannerAI::LogSetupError(
            TEXT("UBTTask_TensorPlannerMoveToCurrentStepTarget::ExecuteTask"),
            FString::Printf(TEXT("No current plan step is available (%s)."), *TensorPlannerAI::DescribeAIContext(OwnerComp)),
            TEXT("Request/build a navigation plan before running the move task."));
        return EBTNodeResult::Failed;
    }

    AActor* TargetActor = PlanComponent->GetCurrentStepActorArgument(TargetArgumentName);
    if (TargetActor == nullptr)
    {
        TensorPlannerAI::LogSetupError(
            TEXT("UBTTask_TensorPlannerMoveToCurrentStepTarget::ExecuteTask"),
            FString::Printf(TEXT("Current step '%s' is missing actor argument '%s' (%s)."), *PlanComponent->GetCurrentStepName().ToString(), *TargetArgumentName.ToString(), *TensorPlannerAI::DescribeAIContext(OwnerComp)),
            TEXT("Build the navigation example plan with valid tagged target actors, or ensure your custom plan step stores an actor argument named 'target'."));
        return EBTNodeResult::Failed;
    }

    const float AcceptanceRadius = AcceptanceRadiusOverride >= 0.0f ? AcceptanceRadiusOverride : PlanComponent->GetCurrentStepAcceptanceRadius();
    const EPathFollowingRequestResult::Type MoveResult = Controller->MoveToActor(TargetActor, AcceptanceRadius, true, true, true, nullptr, true);
    if (MoveResult == EPathFollowingRequestResult::Failed)
    {
        TensorPlannerAI::LogSetupError(
            TEXT("UBTTask_TensorPlannerMoveToCurrentStepTarget::ExecuteTask"),
            FString::Printf(TEXT("MoveToActor failed for target=%s (%s)."), *GetNameSafe(TargetActor), *TensorPlannerAI::DescribeAIContext(OwnerComp)),
            TEXT("Ensure the target actor sits on a NavMesh-reachable area and the controlled character has a working movement component."));
        return EBTNodeResult::Failed;
    }

    if (MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
    {
        return PlanComponent->MarkCurrentStepDone() ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
    }

    FTensorPlannerMoveMemory* Memory = reinterpret_cast<FTensorPlannerMoveMemory*>(NodeMemory);
    Memory->AcceptanceRadius = AcceptanceRadius;
    Memory->TargetActor = TargetActor;
    return EBTNodeResult::InProgress;
}

void UBTTask_TensorPlannerMoveToCurrentStepTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

    AAIController* Controller = OwnerComp.GetAIOwner();
    UTensorPlannerPlanComponent* PlanComponent = TensorPlannerAI::FindPlanComponent(OwnerComp);
    const FTensorPlannerMoveMemory* Memory = reinterpret_cast<const FTensorPlannerMoveMemory*>(NodeMemory);
    AActor* TargetActor = Memory->TargetActor.Get();

    if (Controller == nullptr || PlanComponent == nullptr || TargetActor == nullptr)
    {
        TensorPlannerAI::LogSetupError(
            TEXT("UBTTask_TensorPlannerMoveToCurrentStepTarget::TickTask"),
            FString::Printf(TEXT("Navigation move lost controller, plan component, or target (%s)."), *TensorPlannerAI::DescribeAIContext(OwnerComp)),
            TEXT("Keep the AIController, TensorPlannerPlanComponent, and target actors alive while navigation is in progress."));
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    APawn* Pawn = Controller->GetPawn();
    if (Pawn == nullptr)
    {
        TensorPlannerAI::LogSetupError(
            TEXT("UBTTask_TensorPlannerMoveToCurrentStepTarget::TickTask"),
            FString::Printf(TEXT("Controlled pawn is null while moving to target=%s."), *GetNameSafe(TargetActor)),
            TEXT("Keep the NPC possessed and alive until the navigation step completes."));
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    const float Distance = FVector::Dist2D(Pawn->GetActorLocation(), TargetActor->GetActorLocation());
    if (Distance <= Memory->AcceptanceRadius)
    {
        Controller->StopMovement();
        FinishLatentTask(OwnerComp, PlanComponent->MarkCurrentStepDone() ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
        return;
    }

    if (Controller->GetMoveStatus() == EPathFollowingStatus::Idle)
    {
        TensorPlannerAI::LogSetupError(
            TEXT("UBTTask_TensorPlannerMoveToCurrentStepTarget::TickTask"),
            FString::Printf(TEXT("Move ended before reaching target=%s (distance=%0.2f)."), *GetNameSafe(TargetActor), Distance),
            TEXT("Ensure the target is reachable on the NavMesh and the NPC movement/path-following setup is valid."));
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
    }
}
