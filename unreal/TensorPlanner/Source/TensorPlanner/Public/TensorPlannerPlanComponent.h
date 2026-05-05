#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "TensorPlannerPlanComponent.generated.h"

USTRUCT(BlueprintType)
struct FTensorPlannerPlanStep
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tensor Planner")
    FName ActionName = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tensor Planner")
    float DurationSeconds = 10.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tensor Planner")
    bool bCompleted = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tensor Planner")
    TMap<FName, TSoftObjectPtr<UObject>> ObjectArguments;
};

UCLASS(ClassGroup = (AI), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class TENSORPLANNER_API UTensorPlannerPlanComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTensorPlannerPlanComponent();

    UFUNCTION(BlueprintCallable, Category = "Tensor Planner")
    bool BuildExamplePlan();

    UFUNCTION(BlueprintCallable, Category = "Tensor Planner")
    bool BuildExampleNavigationPlan();

    UFUNCTION(BlueprintCallable, Category = "Tensor Planner")
    void ClearPlan();

    UFUNCTION(BlueprintPure, Category = "Tensor Planner")
    bool HasPlan() const;

    UFUNCTION(BlueprintPure, Category = "Tensor Planner")
    bool HasCurrentStep() const;

    UFUNCTION(BlueprintPure, Category = "Tensor Planner")
    bool IsCurrentStepDone() const;

    UFUNCTION(BlueprintPure, Category = "Tensor Planner")
    bool IsPlanComplete() const;

    UFUNCTION(BlueprintPure, Category = "Tensor Planner")
    int32 GetCurrentStepIndex() const;

    UFUNCTION(BlueprintPure, Category = "Tensor Planner")
    FName GetCurrentStepName() const;

    UFUNCTION(BlueprintPure, Category = "Tensor Planner")
    float GetCurrentStepDurationSeconds() const;

    UFUNCTION(BlueprintPure, Category = "Tensor Planner")
    AActor* GetCurrentStepActorArgument(FName ParamName) const;

    UFUNCTION(BlueprintPure, Category = "Tensor Planner")
    float GetCurrentStepAcceptanceRadius() const;

    UFUNCTION(BlueprintCallable, Category = "Tensor Planner")
    bool MarkCurrentStepDone();

    UFUNCTION(BlueprintCallable, Category = "Tensor Planner")
    bool AdvanceToNextStep();

    UFUNCTION(BlueprintPure, Category = "Tensor Planner")
    bool CurrentStepMatchesActionPrefix(FName Prefix) const;

    const FTensorPlannerPlanStep* GetCurrentStep() const;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tensor Planner|Example")
    float ExampleStepDurationSeconds = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tensor Planner|Navigation Example")
    FName ExampleNavigationTargetTag = TEXT("TensorPlanner.NavTarget");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tensor Planner|Navigation Example")
    float ExampleMoveAcceptanceRadius = 75.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tensor Planner")
    TArray<FTensorPlannerPlanStep> PlanSteps;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tensor Planner")
    int32 CurrentStepIndex = INDEX_NONE;

private:
    AActor* ResolvePlanningActor() const;
    bool HasValidCurrentStepIndex() const;
    void SetPlanSteps(TArray<FTensorPlannerPlanStep> InSteps);
    bool BuildExampleWaitPlan(AActor* PlanningActor);
};
