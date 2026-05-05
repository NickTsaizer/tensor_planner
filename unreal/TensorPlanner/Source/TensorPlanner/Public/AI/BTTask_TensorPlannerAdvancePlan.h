#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"

#include "BTTask_TensorPlannerAdvancePlan.generated.h"

UCLASS()
class TENSORPLANNER_API UBTTask_TensorPlannerAdvancePlan : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_TensorPlannerAdvancePlan();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
