#pragma once

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "TensorPlannerModule.h"
#include "TensorPlannerPlanComponent.h"

namespace TensorPlannerAI
{
inline UTensorPlannerPlanComponent* FindPlanComponent(const UBehaviorTreeComponent& OwnerComp)
{
    if (AAIController* Controller = OwnerComp.GetAIOwner())
    {
        if (UTensorPlannerPlanComponent* ControllerComponent = Controller->FindComponentByClass<UTensorPlannerPlanComponent>())
        {
            return ControllerComponent;
        }

        if (APawn* Pawn = Controller->GetPawn())
        {
            return Pawn->FindComponentByClass<UTensorPlannerPlanComponent>();
        }
    }

    return nullptr;
}

inline FString DescribeAIContext(const UBehaviorTreeComponent& OwnerComp)
{
    const AAIController* Controller = OwnerComp.GetAIOwner();
    const APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
    return FString::Printf(
        TEXT("controller=%s pawn=%s owner=%s"),
        *GetNameSafe(Controller),
        *GetNameSafe(Pawn),
        *GetNameSafe(OwnerComp.GetOwner()));
}

inline void LogSetupError(const TCHAR* Source, const FString& Message, const FString& Fix)
{
    UE_LOG(LogTensorPlanner, Error, TEXT("[%s] %s Fix: %s"), Source, *Message, *Fix);
}

inline void LogMissingPlanComponent(const TCHAR* Source, const UBehaviorTreeComponent& OwnerComp)
{
    LogSetupError(
        Source,
        FString::Printf(TEXT("No UTensorPlannerPlanComponent found (%s)."), *DescribeAIContext(OwnerComp)),
        TEXT("Add a TensorPlannerPlanComponent to the AIController or its controlled Pawn so each NPC has its own non-global plan state."));
}

inline void LogMissingBlackboard(const TCHAR* Source, const UBehaviorTreeComponent& OwnerComp)
{
    LogSetupError(
        Source,
        FString::Printf(TEXT("Behavior Tree has no BlackboardComponent (%s)."), *DescribeAIContext(OwnerComp)),
        TEXT("Assign and initialize a Blackboard asset on the AIController/Behavior Tree before using TensorPlanner BT services."));
}
}
