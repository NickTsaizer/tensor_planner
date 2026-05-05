#include "AI/BTTask_TensorPlannerRequestExampleNavigationPlan.h"

#include "AI/TensorPlannerAIHelpers.h"

UBTTask_TensorPlannerRequestExampleNavigationPlan::UBTTask_TensorPlannerRequestExampleNavigationPlan()
{
    NodeName = TEXT("Tensor Planner Request Example Navigation Plan");
}

EBTNodeResult::Type UBTTask_TensorPlannerRequestExampleNavigationPlan::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UTensorPlannerPlanComponent* PlanComponent = TensorPlannerAI::FindPlanComponent(OwnerComp);
    if (PlanComponent == nullptr)
    {
        TensorPlannerAI::LogMissingPlanComponent(TEXT("UBTTask_TensorPlannerRequestExampleNavigationPlan::ExecuteTask"), OwnerComp);
        return EBTNodeResult::Failed;
    }

    if (PlanComponent->HasPlan())
    {
        return EBTNodeResult::Succeeded;
    }

    if (!PlanComponent->BuildExampleNavigationPlan())
    {
        TensorPlannerAI::LogSetupError(
            TEXT("UBTTask_TensorPlannerRequestExampleNavigationPlan::ExecuteTask"),
            FString::Printf(TEXT("Failed to build the example navigation plan (%s)."), *TensorPlannerAI::DescribeAIContext(OwnerComp)),
            TEXT("Place at least two actors tagged 'TensorPlanner.NavTarget' on the NavMesh and ensure this AI owns a TensorPlannerPlanComponent."));
        return EBTNodeResult::Failed;
    }

    return EBTNodeResult::Succeeded;
}
