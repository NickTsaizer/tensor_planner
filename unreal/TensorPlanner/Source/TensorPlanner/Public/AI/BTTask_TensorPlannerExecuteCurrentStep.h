#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"

#include "BTTask_TensorPlannerExecuteCurrentStep.generated.h"

UCLASS()
class TENSORPLANNER_API UBTTask_TensorPlannerExecuteCurrentStep : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_TensorPlannerExecuteCurrentStep();

    virtual uint16 GetInstanceMemorySize() const override;
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

protected:
    UPROPERTY(EditAnywhere, Category = "Tensor Planner")
    FName SupportedActionPrefix = TEXT("wait_decoy_step_");

    UPROPERTY(EditAnywhere, Category = "Tensor Planner")
    float DurationOverrideSeconds = -1.0f;

private:
    struct FTensorPlannerExecuteStepMemory
    {
        float StartTimeSeconds = 0.0f;
        float DurationSeconds = 0.0f;
    };
};
