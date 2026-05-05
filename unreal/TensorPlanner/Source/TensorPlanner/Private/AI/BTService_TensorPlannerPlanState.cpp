#include "AI/BTService_TensorPlannerPlanState.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"

#include "AI/TensorPlannerAIHelpers.h"

namespace
{
void SetBool(UBlackboardComponent* Blackboard, const FBlackboardKeySelector& Key, const bool bValue)
{
    if (Blackboard != nullptr && Key.SelectedKeyName != NAME_None)
    {
        Blackboard->SetValueAsBool(Key.SelectedKeyName, bValue);
    }
}

void SetInt(UBlackboardComponent* Blackboard, const FBlackboardKeySelector& Key, const int32 Value)
{
    if (Blackboard != nullptr && Key.SelectedKeyName != NAME_None)
    {
        Blackboard->SetValueAsInt(Key.SelectedKeyName, Value);
    }
}

void SetName(UBlackboardComponent* Blackboard, const FBlackboardKeySelector& Key, const FName Value)
{
    if (Blackboard != nullptr && Key.SelectedKeyName != NAME_None)
    {
        Blackboard->SetValueAsName(Key.SelectedKeyName, Value);
    }
}
}

UBTService_TensorPlannerPlanState::UBTService_TensorPlannerPlanState()
{
    NodeName = TEXT("Tensor Planner Plan State");
    Interval = 0.25f;
    RandomDeviation = 0.0f;
}

void UBTService_TensorPlannerPlanState::InitializeFromAsset(UBehaviorTree& Asset)
{
    Super::InitializeFromAsset(Asset);

    if (Asset.BlackboardAsset == nullptr)
    {
        UE_LOG(
            LogTensorPlanner,
            Error,
            TEXT("[UBTService_TensorPlannerPlanState::InitializeFromAsset] Blackboard asset is not set on the Behavior Tree. Fix: assign a Blackboard asset to this Behavior Tree and bind the TensorPlanner keys before running the service."));
        return;
    }

    HasPlanKey.ResolveSelectedKey(*Asset.BlackboardAsset);
    PlanCompleteKey.ResolveSelectedKey(*Asset.BlackboardAsset);
    CurrentStepDoneKey.ResolveSelectedKey(*Asset.BlackboardAsset);
    CurrentStepIndexKey.ResolveSelectedKey(*Asset.BlackboardAsset);
    CurrentStepNameKey.ResolveSelectedKey(*Asset.BlackboardAsset);
}

void UBTService_TensorPlannerPlanState::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

    UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
    if (Blackboard == nullptr)
    {
        TensorPlannerAI::LogMissingBlackboard(TEXT("UBTService_TensorPlannerPlanState::TickNode"), OwnerComp);
        return;
    }

    UTensorPlannerPlanComponent* PlanComponent = TensorPlannerAI::FindPlanComponent(OwnerComp);

    if (PlanComponent == nullptr)
    {
        TensorPlannerAI::LogMissingPlanComponent(TEXT("UBTService_TensorPlannerPlanState::TickNode"), OwnerComp);
        SetBool(Blackboard, HasPlanKey, false);
        SetBool(Blackboard, PlanCompleteKey, false);
        SetBool(Blackboard, CurrentStepDoneKey, false);
        SetInt(Blackboard, CurrentStepIndexKey, INDEX_NONE);
        SetName(Blackboard, CurrentStepNameKey, NAME_None);
        return;
    }

    SetBool(Blackboard, HasPlanKey, PlanComponent->HasPlan());
    SetBool(Blackboard, PlanCompleteKey, PlanComponent->IsPlanComplete());
    SetBool(Blackboard, CurrentStepDoneKey, PlanComponent->IsCurrentStepDone());
    SetInt(Blackboard, CurrentStepIndexKey, PlanComponent->GetCurrentStepIndex());
    SetName(Blackboard, CurrentStepNameKey, PlanComponent->GetCurrentStepName());
}
