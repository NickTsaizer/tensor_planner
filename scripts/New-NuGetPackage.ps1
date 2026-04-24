[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,
    [string]$ProjectPath = "csharp/TensorPlanner/TensorPlanner.csproj",
    [string]$PackageVersion,
    [string]$WindowsLibrary,
    [string]$LinuxLibrary,
    [string]$MacLibrary,
    [string]$MacArm64Library
)

$ErrorActionPreference = "Stop"

function Copy-NativeLibrary {
    param([string]$SourcePath, [Parameter(Mandatory = $true)][string]$DestinationPath)
    if ([string]::IsNullOrWhiteSpace($SourcePath)) { return $null }
    if (-not (Test-Path -LiteralPath $SourcePath)) { throw "Native library not found: $SourcePath" }
    New-Item -ItemType Directory -Path (Split-Path -Parent $DestinationPath) -Force | Out-Null
    Copy-Item -LiteralPath $SourcePath -Destination $DestinationPath -Force
    return $DestinationPath
}

$resolvedProjectPath = (Resolve-Path $ProjectPath).Path
$resolvedOutputDirectory = if ([System.IO.Path]::IsPathRooted($OutputDirectory)) { [System.IO.Path]::GetFullPath($OutputDirectory) } else { [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDirectory)) }
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("tensor-planner-nuget-" + [Guid]::NewGuid().ToString("N"))

try {
    $windowsPath = Copy-NativeLibrary -SourcePath $WindowsLibrary -DestinationPath (Join-Path $tempRoot "win-x64\tensor_planner.dll")
    $linuxPath = Copy-NativeLibrary -SourcePath $LinuxLibrary -DestinationPath (Join-Path $tempRoot "linux-x64\libtensor_planner.so")
    $macPath = Copy-NativeLibrary -SourcePath $MacLibrary -DestinationPath (Join-Path $tempRoot "osx-x64\libtensor_planner.dylib")
    $macArm64Path = Copy-NativeLibrary -SourcePath $MacArm64Library -DestinationPath (Join-Path $tempRoot "osx-arm64\libtensor_planner.dylib")
    New-Item -ItemType Directory -Path $resolvedOutputDirectory -Force | Out-Null
    $arguments = @("pack", $resolvedProjectPath, "-c", "Release", "-o", $resolvedOutputDirectory, "-p:ContinuousIntegrationBuild=true")
    if (-not [string]::IsNullOrWhiteSpace($PackageVersion)) { $arguments += "-p:PackageVersion=$PackageVersion" }
    if ($windowsPath) { $arguments += "-p:TensorPlannerNativeWindowsPath=$windowsPath" }
    if ($linuxPath) { $arguments += "-p:TensorPlannerNativeLinuxPath=$linuxPath" }
    if ($macPath) { $arguments += "-p:TensorPlannerNativeMacPath=$macPath" }
    if ($macArm64Path) { $arguments += "-p:TensorPlannerNativeMacArm64Path=$macArm64Path" }
    & dotnet @arguments
}
finally {
    if (Test-Path -LiteralPath $tempRoot) { Remove-Item -LiteralPath $tempRoot -Recurse -Force }
}
