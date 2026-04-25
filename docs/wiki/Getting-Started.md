# Getting Started

This page gets Tensor Planner built, tested, and packaged.

## Requirements

Core native build:

- CMake 3.20+
- C++20 compiler

Optional binding checks:

- .NET SDK for C# smoke tests and wrapper builds
- Unity 2019.4+ for the Unity package
- Jai compiler for the Jai module and example
- `x86_64-w64-mingw32-g++` for Windows cross-builds from Linux

## Clone

```bash
git clone https://github.com/NickTsaizer/tensor_planner.git
cd tensor_planner
```

## Build native library and tests

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The build creates the shared library and smoke-test executables:

- `tensor_planner`
- `tensor_planner_solver_smoke`
- `tensor_planner_logistics_smoke`
- `tensor_planner_crafting_smoke`
- `tensor_planner_cpp_fluent_smoke`

## Run C# smoke test

```bash
dotnet run --project csharp/TensorPlanner.Smoke/TensorPlanner.Smoke.csproj -c Release
```

The C# wrapper uses `DllImport("tensor_planner")`, so the native library must be
discoverable by the runtime when used outside the repository's normal build/test
setup.

## Run Jai crafting example

The Jai example expects native libraries in module-local locations.

```bash
cmake -S . -B build/Release-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release-linux --parallel
cp build/Release-linux/libtensor_planner.so modules/Tensor_Planner/generated/libtensor_planner.so
cp build/Release-linux/libtensor_planner.so modules/Tensor_Planner/libtensor_planner.so
jai modules/Tensor_Planner/tests/crafting.jai
modules/Tensor_Planner/tests/crafting
```

Expected behavior: the example solves a crafting progression and logs a plan
ending with a `craft3` action that creates `diamond_pickaxe`.

## Build distribution artifacts

Use the POSIX package script:

```bash
sh ./build.sh -release -target unity cpp sharp jai -o ./dist
```

Defaults:

- `-release` is the default if neither `-release` nor `-debug` is passed.
- target OS defaults to the host OS.
- build-process files are cleaned after packaging unless `-no-clean` is passed.

Useful variants:

```bash
sh ./build.sh -target unity cpp -os linux windows -o ./dist
sh ./build.sh -debug -target cpp sharp -o ./dist -no-clean
```

## Output layout

Depending on selected targets, `dist/` can contain:

```text
dist/
├── cpp/       headers, native library, optional CMake config
├── sharp/     TensorPlanner.dll plus native runtime assets
├── unity/     dev.nick.tensor-planner Unity package
└── jai/       Tensor_Planner module staging
```

## First example to read

Read these in order:

1. `dev.nick.tensor-planner/Samples~/Basic Usage/BasicUsage.cs`
2. `tests/cpp_fluent_smoke.cpp`
3. `tests/crafting_smoke.cpp`
4. `modules/Tensor_Planner/tests/crafting.jai`

Then continue to [Core Concepts](Core-Concepts).
