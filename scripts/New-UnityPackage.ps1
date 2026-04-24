[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,
    [string]$PackageRoot = "dev.nick.tensor-planner",
    [string]$WindowsLibrary,
    [string]$LinuxLibrary,
    [string]$MacLibrary
)

$ErrorActionPreference = "Stop"

function Copy-NativeLibrary {
    param([string]$SourcePath, [Parameter(Mandatory = $true)][string]$DestinationPath)
    if ([string]::IsNullOrWhiteSpace($SourcePath)) { return }
    if (-not (Test-Path -LiteralPath $SourcePath)) { throw "Native library not found: $SourcePath" }
    New-Item -ItemType Directory -Path (Split-Path -Parent $DestinationPath) -Force | Out-Null
    Copy-Item -LiteralPath $SourcePath -Destination $DestinationPath -Force
}

$resolvedPackageRoot = (Resolve-Path $PackageRoot).Path
$resolvedOutputDirectory = if ([System.IO.Path]::IsPathRooted($OutputDirectory)) { [System.IO.Path]::GetFullPath($OutputDirectory) } else { [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDirectory)) }
$packageName = Split-Path -Leaf $resolvedPackageRoot
$stagingRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("tensor-planner-unity-" + [Guid]::NewGuid().ToString("N"))
$stagedPackage = Join-Path $stagingRoot $packageName

try {
    New-Item -ItemType Directory -Path $stagedPackage -Force | Out-Null
    Copy-Item -Path (Join-Path $resolvedPackageRoot "*") -Destination $stagedPackage -Recurse -Force
    Copy-NativeLibrary -SourcePath $WindowsLibrary -DestinationPath (Join-Path $stagedPackage "Runtime\Plugins\x86_64\tensor_planner.dll")
    Copy-NativeLibrary -SourcePath $LinuxLibrary -DestinationPath (Join-Path $stagedPackage "Runtime\Plugins\x86_64\libtensor_planner.so")
    Copy-NativeLibrary -SourcePath $MacLibrary -DestinationPath (Join-Path $stagedPackage "Runtime\Plugins\libtensor_planner.dylib")
    New-Item -ItemType Directory -Path $resolvedOutputDirectory -Force | Out-Null
    Copy-Item -Path $stagedPackage -Destination $resolvedOutputDirectory -Recurse -Force
}
finally {
    if (Test-Path -LiteralPath $stagingRoot) { Remove-Item -LiteralPath $stagingRoot -Recurse -Force }
}
