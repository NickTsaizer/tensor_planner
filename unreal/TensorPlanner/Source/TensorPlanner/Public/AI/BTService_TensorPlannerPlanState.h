#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTreeTypes.h"

#include "BTService_TensorPlannerPlanState.generated.h"

UCLASS()
class TENSORPLANNER_API UBTService_TensorPlannerPlanState : public UBTService
{
    GENERATED_BODY()

public:
    UBTService_TensorPlannerPlanState();

    virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

protected:
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector HasPlanKey;

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector PlanCompleteKey;

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector CurrentStepDoneKey;

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector CurrentStepIndexKey;

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector CurrentStepNameKey;
};
