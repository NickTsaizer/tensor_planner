#include "AI/BTTask_TensorPlannerAdvancePlan.h"

#include "AI/TensorPlannerAIHelpers.h"

UBTTask_TensorPlannerAdvancePlan::UBTTask_TensorPlannerAdvancePlan()
{
    NodeName = TEXT("Tensor Planner Advance Plan");
}

EBTNodeResult::Type UBTTask_TensorPlannerAdvancePlan::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UTensorPlannerPlanComponent* PlanComponent = TensorPlannerAI::FindPlanComponent(OwnerComp);
    if (PlanComponent == nullptr)
    {
        TensorPlannerAI::LogMissingPlanComponent(TEXT("UBTTask_TensorPlannerAdvancePlan::ExecuteTask"), OwnerComp);
        return EBTNodeResult::Failed;
    }

    if (!PlanComponent->AdvanceToNextStep())
    {
        TensorPlannerAI::LogSetupError(
            TEXT("UBTTask_TensorPlannerAdvancePlan::ExecuteTask"),
            FString::Printf(TEXT("Failed to advance to the next plan step (%s)."), *TensorPlannerAI::DescribeAIContext(OwnerComp)),
            TEXT("Only call this task after the current step has been marked done, and keep plan state on the per-NPC TensorPlannerPlanComponent."));
        return EBTNodeResult::Failed;
    }

    return EBTNodeResult::Succeeded;
}
