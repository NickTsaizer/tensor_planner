#include "AI/BTTask_TensorPlannerRequestExamplePlan.h"

#include "AI/TensorPlannerAIHelpers.h"

UBTTask_TensorPlannerRequestExamplePlan::UBTTask_TensorPlannerRequestExamplePlan()
{
    NodeName = TEXT("Tensor Planner Request Example Plan");
}

EBTNodeResult::Type UBTTask_TensorPlannerRequestExamplePlan::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UTensorPlannerPlanComponent* PlanComponent = TensorPlannerAI::FindPlanComponent(OwnerComp);
    if (PlanComponent == nullptr)
    {
        TensorPlannerAI::LogMissingPlanComponent(TEXT("UBTTask_TensorPlannerRequestExamplePlan::ExecuteTask"), OwnerComp);
        return EBTNodeResult::Failed;
    }

    if (PlanComponent->HasPlan())
    {
        return EBTNodeResult::Succeeded;
    }

    if (!PlanComponent->BuildExamplePlan())
    {
        TensorPlannerAI::LogSetupError(
            TEXT("UBTTask_TensorPlannerRequestExamplePlan::ExecuteTask"),
            FString::Printf(TEXT("Failed to build example plan (%s)."), *TensorPlannerAI::DescribeAIContext(OwnerComp)),
            TEXT("Ensure the plan component is attached to the correct NPC owner and the plugin runtime library is staged correctly."));
        return EBTNodeResult::Failed;
    }

    return EBTNodeResult::Succeeded;
}
