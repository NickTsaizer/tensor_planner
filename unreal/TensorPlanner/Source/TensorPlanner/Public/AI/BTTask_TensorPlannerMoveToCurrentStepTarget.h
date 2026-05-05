#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"

#include "BTTask_TensorPlannerMoveToCurrentStepTarget.generated.h"

UCLASS()
class TENSORPLANNER_API UBTTask_TensorPlannerMoveToCurrentStepTarget : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_TensorPlannerMoveToCurrentStepTarget();

    virtual uint16 GetInstanceMemorySize() const override;
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

protected:
    UPROPERTY(EditAnywhere, Category = "Tensor Planner")
    FName TargetArgumentName = TEXT("target");

    UPROPERTY(EditAnywhere, Category = "Tensor Planner")
    float AcceptanceRadiusOverride = -1.0f;

private:
    struct FTensorPlannerMoveMemory
    {
        float AcceptanceRadius = 0.0f;
        TWeakObjectPtr<AActor> TargetActor;
    };
};
