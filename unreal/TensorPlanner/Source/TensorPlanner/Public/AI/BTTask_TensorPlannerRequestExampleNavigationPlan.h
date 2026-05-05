#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"

#include "BTTask_TensorPlannerRequestExampleNavigationPlan.generated.h"

UCLASS()
class TENSORPLANNER_API UBTTask_TensorPlannerRequestExampleNavigationPlan : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_TensorPlannerRequestExampleNavigationPlan();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
