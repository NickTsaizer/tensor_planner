# Tensor Planner Unreal Plugin

This package is a source plugin wrapper for Tensor Planner.

## What it includes

- `Source/TensorPlanner/` Unreal runtime module
- `Source/ThirdParty/TensorPlanner/include/` native C/C++ headers
- `Source/ThirdParty/TensorPlanner/lib/<Platform>/` native link libraries
- `Binaries/ThirdParty/TensorPlanner/<Platform>/` runtime shared libraries

## API shape

The Unreal wrapper keeps the fluent planner shape, but uses Unreal-native types:

- `FString`
- `TArray`
- `TMap`
- `TSoftObjectPtr<>`

Entry point:

- `Source/TensorPlanner/Public/TensorPlannerUnreal.h`

## Packaging notes

- Build this package with `build.sh` or `build.ps1` using `-target unreal`.
- Windows plugin consumers should prefer a package produced on Windows so the staged import library matches the MSVC toolchain.
- Soft object pointers do not auto-load assets. Use `LoadSynchronous()` only when blocking is acceptable.
