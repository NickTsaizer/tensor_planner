#include "TensorPlannerModule.h"

DEFINE_LOG_CATEGORY(LogTensorPlanner);

void FTensorPlannerModule::StartupModule()
{
    UE_LOG(LogTensorPlanner, Verbose, TEXT("TensorPlanner module started"));
}

void FTensorPlannerModule::ShutdownModule()
{
    UE_LOG(LogTensorPlanner, Verbose, TEXT("TensorPlanner module stopped"));
}

IMPLEMENT_MODULE(FTensorPlannerModule, TensorPlanner)
