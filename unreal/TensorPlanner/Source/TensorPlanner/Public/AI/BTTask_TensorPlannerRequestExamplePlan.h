#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"

#include "BTTask_TensorPlannerRequestExamplePlan.generated.h"

UCLASS()
class TENSORPLANNER_API UBTTask_TensorPlannerRequestExamplePlan : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_TensorPlannerRequestExamplePlan();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
