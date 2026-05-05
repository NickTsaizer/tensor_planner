#include "AI/BTTask_TensorPlannerExecuteCurrentStep.h"

#include "BehaviorTree/BehaviorTreeComponent.h"

#include "AI/TensorPlannerAIHelpers.h"

UBTTask_TensorPlannerExecuteCurrentStep::UBTTask_TensorPlannerExecuteCurrentStep()
{
    NodeName = TEXT("Tensor Planner Execute Current Step");
    bNotifyTick = true;
}

uint16 UBTTask_TensorPlannerExecuteCurrentStep::GetInstanceMemorySize() const
{
    return sizeof(FTensorPlannerExecuteStepMemory);
}

EBTNodeResult::Type UBTTask_TensorPlannerExecuteCurrentStep::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UTensorPlannerPlanComponent* PlanComponent = TensorPlannerAI::FindPlanComponent(OwnerComp);
    if (PlanComponent == nullptr)
    {
        TensorPlannerAI::LogMissingPlanComponent(TEXT("UBTTask_TensorPlannerExecuteCurrentStep::ExecuteTask"), OwnerComp);
        return EBTNodeResult::Failed;
    }

    if (!PlanComponent->HasCurrentStep())
    {
        TensorPlannerAI::LogSetupError(
            TEXT("UBTTask_TensorPlannerExecuteCurrentStep::ExecuteTask"),
            FString::Printf(TEXT("No current plan step is available (%s)."), *TensorPlannerAI::DescribeAIContext(OwnerComp)),
            TEXT("Request/build a plan before executing a TensorPlanner step, or guard this task with a Blackboard 'HasPlan' decorator."));
        return EBTNodeResult::Failed;
    }

    if (PlanComponent->IsCurrentStepDone())
    {
        return EBTNodeResult::Succeeded;
    }

    if (!PlanComponent->CurrentStepMatchesActionPrefix(SupportedActionPrefix))
    {
        TensorPlannerAI::LogSetupError(
            TEXT("UBTTask_TensorPlannerExecuteCurrentStep::ExecuteTask"),
            FString::Printf(TEXT("Current step '%s' does not match supported prefix '%s' (%s)."), *PlanComponent->GetCurrentStepName().ToString(), *SupportedActionPrefix.ToString(), *TensorPlannerAI::DescribeAIContext(OwnerComp)),
            TEXT("Use this task only for decoy wait steps, or add another BT task that handles the current TensorPlanner action name."));
        return EBTNodeResult::Failed;
    }

    UWorld* World = OwnerComp.GetWorld();
    if (World == nullptr)
    {
        TensorPlannerAI::LogSetupError(
            TEXT("UBTTask_TensorPlannerExecuteCurrentStep::ExecuteTask"),
            FString::Printf(TEXT("World is null while starting a plan step (%s)."), *TensorPlannerAI::DescribeAIContext(OwnerComp)),
            TEXT("Run this task only from a live AI Behavior Tree instance with a valid world context."));
        return EBTNodeResult::Failed;
    }

    FTensorPlannerExecuteStepMemory* Memory = reinterpret_cast<FTensorPlannerExecuteStepMemory*>(NodeMemory);
    Memory->StartTimeSeconds = World->GetTimeSeconds();
    Memory->DurationSeconds = DurationOverrideSeconds >= 0.0f ? DurationOverrideSeconds : PlanComponent->GetCurrentStepDurationSeconds();
    return EBTNodeResult::InProgress;
}

void UBTTask_TensorPlannerExecuteCurrentStep::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

    UTensorPlannerPlanComponent* PlanComponent = TensorPlannerAI::FindPlanComponent(OwnerComp);
    if (PlanComponent == nullptr)
    {
        TensorPlannerAI::LogMissingPlanComponent(TEXT("UBTTask_TensorPlannerExecuteCurrentStep::TickTask"), OwnerComp);
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    if (!PlanComponent->HasCurrentStep())
    {
        TensorPlannerAI::LogSetupError(
            TEXT("UBTTask_TensorPlannerExecuteCurrentStep::TickTask"),
            FString::Printf(TEXT("Current plan step disappeared while ticking (%s)."), *TensorPlannerAI::DescribeAIContext(OwnerComp)),
            TEXT("Keep per-NPC plan state alive on the TensorPlannerPlanComponent until the step finishes, or re-request the plan before execution."));
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    if (PlanComponent->IsCurrentStepDone())
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        return;
    }

    UWorld* World = OwnerComp.GetWorld();
    if (World == nullptr)
    {
        TensorPlannerAI::LogSetupError(
            TEXT("UBTTask_TensorPlannerExecuteCurrentStep::TickTask"),
            FString::Printf(TEXT("World is null while ticking a plan step (%s)."), *TensorPlannerAI::DescribeAIContext(OwnerComp)),
            TEXT("Run this task only from a live AI Behavior Tree instance with a valid world context."));
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    const FTensorPlannerExecuteStepMemory* Memory = reinterpret_cast<const FTensorPlannerExecuteStepMemory*>(NodeMemory);
    if ((World->GetTimeSeconds() - Memory->StartTimeSeconds) < Memory->DurationSeconds)
    {
        return;
    }

    if (!PlanComponent->MarkCurrentStepDone())
    {
        TensorPlannerAI::LogSetupError(
            TEXT("UBTTask_TensorPlannerExecuteCurrentStep::TickTask"),
            FString::Printf(TEXT("Failed to mark the current step done after waiting (%s)."), *TensorPlannerAI::DescribeAIContext(OwnerComp)),
            TEXT("Verify the current step still exists on the TensorPlannerPlanComponent before this task finishes."));
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
}
