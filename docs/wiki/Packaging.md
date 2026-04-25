# Packaging

Tensor Planner includes a POSIX distribution script:

```text
build.sh
```

It builds native libraries and stages selected binding/package outputs.

## Basic usage

```bash
sh ./build.sh -release -target unity cpp sharp jai -o ./dist
```

Arguments:

```text
-release       Build Release artifacts. Default.
-debug         Build Debug artifacts.
-os            One or more target OS values: linux windows.
-target        One or more targets: unity cpp sharp jai.
-o             Output directory. Defaults to ./dist.
-no-clean      Keep build-process files after packaging.
```

Examples:

```bash
sh ./build.sh -release -os linux -target unity cpp sharp jai -o ./dist
sh ./build.sh -release -os windows -target unity cpp sharp -o ./dist
sh ./build.sh -target unity cpp -os linux windows -o ./dist
```

## Target OS

Supported values:

- `linux`
- `windows`

Linux builds use the host C++ toolchain through CMake.

Windows cross-builds require:

```text
x86_64-w64-mingw32-g++
```

## Target: cpp

Stages:

```text
dist/cpp/
├── include/
│   ├── tensor_planner.h
│   └── tensor_planner.hpp
└── lib/
    └── libtensor_planner.so or tensor_planner.dll
```

When building multiple OS targets, output is split by OS:

```text
dist/cpp/linux/
dist/cpp/windows/
```

## Target: sharp

Stages the .NET wrapper assembly and native runtime assets:

```text
dist/sharp/
├── lib/netstandard2.1/TensorPlanner.dll
└── runtimes/
    ├── linux-x64/native/libtensor_planner.so
    └── win-x64/native/tensor_planner.dll
```

## Target: unity

Stages a Unity package folder:

```text
dist/unity/dev.nick.tensor-planner/
├── package.json
├── Runtime/
│   ├── TensorPlanner.cs
│   └── Plugins/x86_64/...
└── Samples~/
```

Unity samples included:

- Basic Usage
- Tower of Hanoi

## Target: jai

Stages a Jai module folder:

```text
dist/jai/Tensor_Planner/
```

The script stages native libraries under:

```text
dist/jai/Tensor_Planner/lib/<os>/
```

Current note: verify Jai packaging when adding platform-specific generated Jai
binding files. The source module currently includes the Linux generated binding.

## Cleanup behavior

By default, the script removes build-process files after staging outputs:

- per-OS CMake build directories,
- C# `bin/<Configuration>`,
- C# `obj`.

Use `-no-clean` to inspect intermediate files:

```bash
sh ./build.sh -debug -target cpp sharp -o ./dist -no-clean
```

## Manual CMake install

The CMake project installs headers, library, and CMake package config files:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --install build --prefix ./install
```

Installed headers:

```text
include/tensor_planner.h
include/tensor_planner.hpp
```
