#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Configuration = "Release"
$OutputRoot = Join-Path $ScriptDir "dist"
$Targets = @()
$TargetOses = @()
$CleanAfter = $true

function Show-Usage {
    @"
Usage:
  pwsh ./build.ps1 [-release|-debug] [-os <linux|windows>...] -target <unity|cpp|sharp|jai|unreal>... [-o <output-dir>] [-no-clean]

Examples:
  pwsh ./build.ps1 -release -os windows -target unity cpp sharp unreal -o ./dist
  pwsh ./build.ps1 -target unity cpp -os windows -o ./dist
  pwsh ./build.ps1 -debug -target cpp -no-clean

Options:
  -release       Build Release artifacts. This is the default.
  -debug         Build Debug artifacts.
  -os            One or more target OS values: linux windows. Defaults to host OS.
  -target        One or more targets: unity cpp sharp jai unreal.
  -o             Output directory. Defaults to ./dist.
  -no-clean      Keep generated build-process files after packaging.
  -h, --help     Show this help.
"@
}

function Fail {
    param([Parameter(Mandatory = $true)][string]$Message)
    Write-Error "error: $Message"
    exit 1
}

function Write-Info {
    param([Parameter(Mandatory = $true)][string]$Message)
    Write-Host $Message
}

function Require-Command {
    param([Parameter(Mandatory = $true)][string]$Name)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        Fail "required command not found: $Name"
    }
}

function Get-HostOs {
    if ([System.Environment]::OSVersion.Platform -eq [System.PlatformID]::Win32NT) {
        return "windows"
    }
    if ([System.Environment]::OSVersion.Platform -eq [System.PlatformID]::Unix) {
        return "linux"
    }
    Fail "unsupported host OS. Pass -os linux or -os windows."
}

function Get-AbsolutePath {
    param([Parameter(Mandatory = $true)][string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Add-UniqueValue {
    param(
        [string[]]$Values,
        [Parameter(Mandatory = $true)][string]$Value
    )
    if ($Values -contains $Value) { return $Values }
    return @($Values + $Value)
}

function Add-Target {
    param([Parameter(Mandatory = $true)][string]$Value)
    switch ($Value) {
        { $_ -in @("unity", "cpp", "sharp", "jai", "unreal") } { $script:Targets = @(Add-UniqueValue -Values $script:Targets -Value $Value); return }
        default { Fail "unknown target: $Value" }
    }
}

function Add-TargetOs {
    param([Parameter(Mandatory = $true)][string]$Value)
    switch ($Value) {
        { $_ -in @("linux", "windows") } { $script:TargetOses = @(Add-UniqueValue -Values $script:TargetOses -Value $Value); return }
        default { Fail "unknown OS: $Value" }
    }
}

function Test-OptionToken {
    param([Parameter(Mandatory = $true)][string]$Value)
    return $Value.StartsWith("-")
}

function Parse-Arguments {
    param([string[]]$Arguments)
    $index = 0
    while ($index -lt $Arguments.Count) {
        switch ($Arguments[$index]) {
            "-release" {
                $script:Configuration = "Release"
                $index += 1
            }
            "-debug" {
                $script:Configuration = "Debug"
                $index += 1
            }
            "-os" {
                $index += 1
                if ($index -ge $Arguments.Count -or (Test-OptionToken ($Arguments[$index]))) {
                    Fail "-os needs at least one value"
                }
                while ($index -lt $Arguments.Count -and -not (Test-OptionToken ($Arguments[$index]))) {
                    Add-TargetOs ($Arguments[$index])
                    $index += 1
                }
            }
            "-target" {
                $index += 1
                if ($index -ge $Arguments.Count -or (Test-OptionToken ($Arguments[$index]))) {
                    Fail "-target needs at least one value"
                }
                while ($index -lt $Arguments.Count -and -not (Test-OptionToken ($Arguments[$index]))) {
                    Add-Target ($Arguments[$index])
                    $index += 1
                }
            }
            "-o" {
                $index += 1
                if ($index -ge $Arguments.Count) { Fail "-o needs an output directory" }
                $script:OutputRoot = Get-AbsolutePath ($Arguments[$index])
                $index += 1
            }
            "-no-clean" {
                $script:CleanAfter = $false
                $index += 1
            }
            { $_ -in @("-h", "--help") } {
                Show-Usage
                exit 0
            }
            default {
                Fail "unknown argument: $($Arguments[$index])"
            }
        }
    }

    if ($script:Targets.Count -eq 0) { Fail "missing -target" }
    if ($script:TargetOses.Count -eq 0) { $script:TargetOses = @(Get-HostOs) }
}

function Test-Target {
    param([Parameter(Mandatory = $true)][string]$Value)
    return $script:Targets -contains $Value
}

function Get-NormalizedPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    return ([System.IO.Path]::GetFullPath($Path)).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
}

function Remove-SafeDirectory {
    param([Parameter(Mandatory = $true)][string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { Fail "refusing to remove an empty path" }

    $fullPath = Get-NormalizedPath $Path
    $scriptPath = Get-NormalizedPath $script:ScriptDir
    $outputPath = Get-NormalizedPath $script:OutputRoot
    $rootPath = (Get-NormalizedPath ([System.IO.Path]::GetPathRoot($fullPath)))

    if ($fullPath -eq $rootPath -or $fullPath -eq $scriptPath -or $fullPath -eq $outputPath) {
        Fail "refusing to remove unsafe path: $Path"
    }

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
}

function Clear-Directory {
    param([Parameter(Mandatory = $true)][string]$Path)
    Remove-SafeDirectory $Path
    New-Item -ItemType Directory -Path $Path -Force | Out-Null
}

function Copy-FileChecked {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        Fail "required file not found: $Source"
    }
    New-Item -ItemType Directory -Path (Split-Path -Parent $Destination) -Force | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

function Copy-DirectoryChecked {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )
    if (-not (Test-Path -LiteralPath $Source -PathType Container)) {
        Fail "required directory not found: $Source"
    }
    New-Item -ItemType Directory -Path (Split-Path -Parent $Destination) -Force | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
}

function Get-BuildDirectoryForOs {
    param([Parameter(Mandatory = $true)][string]$Os)
    return Join-Path $script:ScriptDir ("build/{0}-{1}" -f $script:Configuration, $Os)
}

function Get-RuntimeIdForOs {
    param([Parameter(Mandatory = $true)][string]$Os)
    switch ($Os) {
        "linux" { return "linux-x64" }
        "windows" { return "win-x64" }
    }
}

function Get-LibraryNameForOs {
    param([Parameter(Mandatory = $true)][string]$Os)
    switch ($Os) {
        "linux" { return "libtensor_planner.so" }
        "windows" { return "tensor_planner.dll" }
    }
}

function Get-UnityPluginPathForOs {
    param([Parameter(Mandatory = $true)][string]$Os)
    switch ($Os) {
        "linux" { return "Runtime/Plugins/x86_64/libtensor_planner.so" }
        "windows" { return "Runtime/Plugins/x86_64/tensor_planner.dll" }
    }
}

function Get-UnrealPlatformDirectoryForOs {
    param([Parameter(Mandatory = $true)][string]$Os)
    switch ($Os) {
        "linux" { return "Linux" }
        "windows" { return "Win64" }
    }
}

function Get-NativeLinkLibraryPath {
    param([Parameter(Mandatory = $true)][string]$Os)
    $buildDir = Get-BuildDirectoryForOs $Os

    switch ($Os) {
        "linux" {
            $candidate = Join-Path $buildDir (Get-LibraryNameForOs $Os)
            if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
        }
        "windows" {
            $candidates = @(
                (Join-Path $buildDir "tensor_planner.lib"),
                (Join-Path $buildDir "libtensor_planner.dll.a"),
                (Join-Path $buildDir "Release/tensor_planner.lib"),
                (Join-Path $buildDir "Release/libtensor_planner.dll.a"),
                (Join-Path $buildDir "Debug/tensor_planner.lib"),
                (Join-Path $buildDir "Debug/libtensor_planner.dll.a")
            )

            foreach ($candidate in $candidates) {
                if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
            }
        }
    }

    Fail "native link library for $Os not found in $buildDir"
}

function Require-ToolchainForOs {
    param([Parameter(Mandatory = $true)][string]$Os)
    switch ($Os) {
        "linux" {
            Require-Command "cmake"
        }
        "windows" {
            Require-Command "cmake"
            if ((Get-HostOs) -ne "windows") {
                Require-Command "x86_64-w64-mingw32-g++"
            }
        }
    }
}

function Get-ConfigureArgumentsForOs {
    param([Parameter(Mandatory = $true)][string]$Os)
    switch ($Os) {
        "linux" {
            return @("-DCMAKE_BUILD_TYPE=$script:Configuration")
        }
        "windows" {
            if ((Get-HostOs) -eq "windows") {
                return @("-DCMAKE_BUILD_TYPE=$script:Configuration")
            }
            return @(
                "-DCMAKE_BUILD_TYPE=$script:Configuration",
                "-DCMAKE_SYSTEM_NAME=Windows",
                "-DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++"
            )
        }
    }
}

function Get-NativeLibraryPath {
    param([Parameter(Mandatory = $true)][string]$Os)
    $buildDir = Get-BuildDirectoryForOs $Os
    $name = Get-LibraryNameForOs $Os
    $candidates = @(
        (Join-Path $buildDir $name),
        (Join-Path $buildDir ("lib$name")),
        (Join-Path $buildDir ("Release/$name")),
        (Join-Path $buildDir ("Debug/$name"))
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }

    Fail "native library for $Os not found in $buildDir"
}

function Build-CppForOs {
    param([Parameter(Mandatory = $true)][string]$Os)
    $buildDir = Get-BuildDirectoryForOs $Os
    Require-ToolchainForOs $Os
    Write-Info "Configuring C++ ($script:Configuration, $Os)..."
    $configureArguments = @(Get-ConfigureArgumentsForOs $Os)
    & cmake -S $script:ScriptDir -B $buildDir @configureArguments
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    Write-Info "Building C++ ($script:Configuration, $Os)..."
    & cmake --build $buildDir --config $script:Configuration --parallel
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Build-Sharp {
    Require-Command "dotnet"
    Write-Info "Building C# ($script:Configuration)..."
    & dotnet build (Join-Path $script:ScriptDir "csharp/TensorPlanner/TensorPlanner.csproj") -c $script:Configuration
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Get-CppOutputDirectoryForOs {
    param([Parameter(Mandatory = $true)][string]$Os)
    if ($script:TargetOses.Count -gt 1) {
        return Join-Path $script:OutputRoot "cpp/$Os"
    }
    return Join-Path $script:OutputRoot "cpp"
}

function Stage-CppForOs {
    param([Parameter(Mandatory = $true)][string]$Os)
    $out = Get-CppOutputDirectoryForOs $Os
    $buildDir = Get-BuildDirectoryForOs $Os
    $lib = Get-NativeLibraryPath $Os
    $name = Get-LibraryNameForOs $Os
    Clear-Directory $out
    Copy-DirectoryChecked (Join-Path $script:ScriptDir "include") (Join-Path $out "include")
    Copy-FileChecked $lib (Join-Path $out "lib/$name")

    $config = Join-Path $buildDir "tensor_plannerConfig.cmake"
    if (Test-Path -LiteralPath $config -PathType Leaf) {
        Copy-FileChecked $config (Join-Path $out "lib/cmake/tensor_planner/tensor_plannerConfig.cmake")
    }

    $configVersion = Join-Path $buildDir "tensor_plannerConfigVersion.cmake"
    if (Test-Path -LiteralPath $configVersion -PathType Leaf) {
        Copy-FileChecked $configVersion (Join-Path $out "lib/cmake/tensor_planner/tensor_plannerConfigVersion.cmake")
    }

    Write-Info "Created $out"
}

function Prepare-UnityPackage {
    $out = Join-Path $script:OutputRoot "unity/dev.nick.tensor-planner"
    Clear-Directory $out
    Copy-FileChecked (Join-Path $script:ScriptDir "dev.nick.tensor-planner/package.json") (Join-Path $out "package.json")
    Copy-DirectoryChecked (Join-Path $script:ScriptDir "dev.nick.tensor-planner/Runtime") (Join-Path $out "Runtime")
    Copy-DirectoryChecked (Join-Path $script:ScriptDir "dev.nick.tensor-planner/Samples~") (Join-Path $out "Samples~")
}

function Stage-UnityForOs {
    param([Parameter(Mandatory = $true)][string]$Os)
    $out = Join-Path $script:OutputRoot "unity/dev.nick.tensor-planner"
    $lib = Get-NativeLibraryPath $Os
    $pluginPath = Get-UnityPluginPathForOs $Os
    Copy-FileChecked $lib (Join-Path $out $pluginPath)
    Write-Info "Added Unity native plugin for $Os"
}

function Stage-Sharp {
    $out = Join-Path $script:OutputRoot "sharp"
    $buildOutput = Join-Path $script:ScriptDir "csharp/TensorPlanner/bin/$script:Configuration/netstandard2.1"
    $dll = Join-Path $buildOutput "TensorPlanner.dll"
    $pdb = Join-Path $buildOutput "TensorPlanner.pdb"
    $deps = Join-Path $buildOutput "TensorPlanner.deps.json"

    Clear-Directory $out
    Copy-FileChecked $dll (Join-Path $out "lib/netstandard2.1/TensorPlanner.dll")
    if (Test-Path -LiteralPath $pdb -PathType Leaf) { Copy-FileChecked $pdb (Join-Path $out "lib/netstandard2.1/TensorPlanner.pdb") }
    if (Test-Path -LiteralPath $deps -PathType Leaf) { Copy-FileChecked $deps (Join-Path $out "lib/netstandard2.1/TensorPlanner.deps.json") }
    Copy-FileChecked (Join-Path $script:ScriptDir "README.md") (Join-Path $out "README.md")

    foreach ($os in $script:TargetOses) {
        $rid = Get-RuntimeIdForOs $os
        $name = Get-LibraryNameForOs $os
        $lib = Get-NativeLibraryPath $os
        Copy-FileChecked $lib (Join-Path $out "runtimes/$rid/native/$name")
    }

    Write-Info "Created $out"
}

function Stage-JaiForOs {
    param([Parameter(Mandatory = $true)][string]$Os)
    $out = Join-Path $script:OutputRoot "jai/Tensor_Planner"
    $lib = Get-NativeLibraryPath $Os
    $name = Get-LibraryNameForOs $Os
    Copy-FileChecked $lib (Join-Path $out "lib/$Os/$name")
}

function Stage-Jai {
    $out = Join-Path $script:OutputRoot "jai/Tensor_Planner"
    Clear-Directory $out
    Copy-FileChecked (Join-Path $script:ScriptDir "modules/Tensor_Planner/module.jai") (Join-Path $out "module.jai")
    Copy-FileChecked (Join-Path $script:ScriptDir "modules/Tensor_Planner/platform.jai") (Join-Path $out "platform.jai")
    Copy-FileChecked (Join-Path $script:ScriptDir "modules/Tensor_Planner/status.jai") (Join-Path $out "status.jai")
    Copy-FileChecked (Join-Path $script:ScriptDir "modules/Tensor_Planner/schema_spec.jai") (Join-Path $out "schema_spec.jai")
    Copy-FileChecked (Join-Path $script:ScriptDir "modules/Tensor_Planner/runtime.jai") (Join-Path $out "runtime.jai")
    Copy-FileChecked (Join-Path $script:ScriptDir "modules/Tensor_Planner/generate.jai") (Join-Path $out "generate.jai")
    Copy-DirectoryChecked (Join-Path $script:ScriptDir "modules/Tensor_Planner/generated") (Join-Path $out "generated")
    foreach ($os in $script:TargetOses) {
        Stage-JaiForOs $os
    }
    Write-Info "Created $out"
}

function Prepare-UnrealPackage {
    $out = Join-Path $script:OutputRoot "unreal/TensorPlanner"
    Clear-Directory $out
    Copy-FileChecked (Join-Path $script:ScriptDir "unreal/TensorPlanner/TensorPlanner.uplugin") (Join-Path $out "TensorPlanner.uplugin")
    Copy-FileChecked (Join-Path $script:ScriptDir "unreal/TensorPlanner/README.md") (Join-Path $out "README.md")
    Copy-FileChecked (Join-Path $script:ScriptDir "LICENSE") (Join-Path $out "LICENSE")
    Copy-DirectoryChecked (Join-Path $script:ScriptDir "unreal/TensorPlanner/Source/TensorPlanner") (Join-Path $out "Source/TensorPlanner")
    Copy-DirectoryChecked (Join-Path $script:ScriptDir "include") (Join-Path $out "Source/ThirdParty/TensorPlanner/include")
}

function Stage-UnrealForOs {
    param([Parameter(Mandatory = $true)][string]$Os)
    $out = Join-Path $script:OutputRoot "unreal/TensorPlanner"
    $platformDir = Get-UnrealPlatformDirectoryForOs $Os
    $runtimeLib = Get-NativeLibraryPath $Os
    $runtimeName = Get-LibraryNameForOs $Os
    $linkLib = Get-NativeLinkLibraryPath $Os
    $linkName = Split-Path -Leaf $linkLib

    Copy-FileChecked $runtimeLib (Join-Path $out "Binaries/ThirdParty/TensorPlanner/$platformDir/$runtimeName")
    Copy-FileChecked $linkLib (Join-Path $out "Source/ThirdParty/TensorPlanner/lib/$platformDir/$linkName")
    Write-Info "Added Unreal native files for $Os"
}

function Clear-BuildProcessFiles {
    if (-not $script:CleanAfter) { return }
    Write-Info "Cleaning build-process files..."
    foreach ($os in $script:TargetOses) {
        Remove-SafeDirectory (Get-BuildDirectoryForOs $os)
    }
    Remove-SafeDirectory (Join-Path $script:ScriptDir "csharp/TensorPlanner/bin/$script:Configuration")
    Remove-SafeDirectory (Join-Path $script:ScriptDir "csharp/TensorPlanner/obj")
}

function Main {
    Set-Location $script:ScriptDir
    Parse-Arguments $args
    New-Item -ItemType Directory -Path $script:OutputRoot -Force | Out-Null

    foreach ($os in $script:TargetOses) {
        Build-CppForOs $os
    }

    if (Test-Target "sharp") { Build-Sharp }

    if (Test-Target "unity") {
        Prepare-UnityPackage
        foreach ($os in $script:TargetOses) {
            Stage-UnityForOs $os
        }
        $unityOutput = Join-Path $script:OutputRoot "unity/dev.nick.tensor-planner"
        Write-Info "Created $unityOutput"
    }

    if (Test-Target "unreal") {
        Prepare-UnrealPackage
        foreach ($os in $script:TargetOses) {
            Stage-UnrealForOs $os
        }
        $unrealOutput = Join-Path $script:OutputRoot "unreal/TensorPlanner"
        Write-Info "Created $unrealOutput"
    }

    if (Test-Target "cpp") {
        foreach ($os in $script:TargetOses) {
            Stage-CppForOs $os
        }
    }

    if (Test-Target "sharp") { Stage-Sharp }
    if (Test-Target "jai") { Stage-Jai }

    Clear-BuildProcessFiles
    Write-Info "Done. Output root: $script:OutputRoot"
}

Main @args
