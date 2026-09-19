[CmdletBinding()]
param(
    [string]$BuildDirectory = '',
    [string]$QtRoot = '',
    [string]$CompilerRoot = '',
    [string]$VcpkgRoot = '',
    [string]$InnoSetupRoot = '',
    [string]$Triplet = 'x64-mingw-dynamic',
    [string]$Configuration = 'Release',
    [string]$Version = '0.1.0',
    [ValidateRange(1, 64)]
    [int]$Parallel = 2,
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-AbsolutePath {
    param([Parameter(Mandatory)][string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Resolve-Setting {
    param(
        [string]$ParameterValue,
        [string]$EnvironmentName,
        [Parameter(Mandatory)][string]$Fallback
    )

    if (-not [string]::IsNullOrWhiteSpace($ParameterValue)) {
        return $ParameterValue
    }

    $environmentValue = [Environment]::GetEnvironmentVariable($EnvironmentName)
    if (-not [string]::IsNullOrWhiteSpace($environmentValue)) {
        return $environmentValue
    }

    return $Fallback
}

function Invoke-LoggedCommand {
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [Parameter(Mandatory)][string[]]$ArgumentList,
        [Parameter(Mandatory)][string]$LogPath
    )

    $logDirectory = Split-Path -Parent $LogPath
    New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null

    & $FilePath @ArgumentList > $LogPath 2>&1
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        Write-Host "Command failed ($exitCode): $FilePath"
        if (Test-Path $LogPath) {
            Get-Content -LiteralPath $LogPath -Tail 160
        }
        throw "The command failed. See $LogPath for the complete output."
    }
}

function Find-ZlibRuntime {
    param(
        [Parameter(Mandatory)][string]$VcpkgInstalledDirectory,
        [Parameter(Mandatory)][ValidateSet('Release', 'Debug')][string]$BuildConfiguration
    )

    $zlibBinRelativePath = if ($BuildConfiguration -eq 'Debug') {
        "$Triplet\debug\bin"
    } else {
        "$Triplet\bin"
    }
    $zlibBinDirectory = Join-Path $VcpkgInstalledDirectory $zlibBinRelativePath
    if (-not (Test-Path $zlibBinDirectory)) {
        return $null
    }

    return Get-ChildItem -LiteralPath $zlibBinDirectory -File -Filter '*.dll' |
        Where-Object { $_.BaseName -match '(^|-)zlib|^libz' } |
        Select-Object -First 1
}

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildDirectoryValue = if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    Join-Path $projectRoot 'build'
} else {
    $BuildDirectory
}
$buildDirectory = Resolve-AbsolutePath $buildDirectoryValue

$qtRoot = Resolve-AbsolutePath (Resolve-Setting $QtRoot 'FOLDERSNAP_QT_ROOT' 'C:\Qt\6.11.1\mingw_64')
$compilerRoot = Resolve-AbsolutePath (Resolve-Setting $CompilerRoot 'FOLDERSNAP_COMPILER_ROOT' 'C:\Qt\Tools\mingw1310_64')
$vcpkgRoot = Resolve-AbsolutePath (Resolve-Setting $VcpkgRoot 'VCPKG_ROOT' 'C:\Users\burak\vcpkg')
$innoSetupRoot = Resolve-AbsolutePath (Resolve-Setting $InnoSetupRoot 'FOLDERSNAP_INNO_ROOT' 'C:\Users\burak\AppData\Local\Programs\Inno Setup 6')

$cmakeCommand = Get-Command cmake.exe -ErrorAction Stop
$cmakePath = $cmakeCommand.Source
$vcpkgPath = Join-Path $vcpkgRoot 'vcpkg.exe'
$windeployqtPath = Join-Path $qtRoot 'bin\windeployqt.exe'
$isccPath = Join-Path $innoSetupRoot 'ISCC.exe'
$compilerPath = Join-Path $compilerRoot 'bin\g++.exe'
$toolchainPath = Join-Path $vcpkgRoot 'scripts\buildsystems\vcpkg.cmake'

foreach ($requiredPath in @($vcpkgPath, $windeployqtPath, $isccPath, $compilerPath, $toolchainPath)) {
    if (-not (Test-Path $requiredPath)) {
        throw "Required tool or file was not found: $requiredPath"
    }
}

New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null
$env:PATH = "$(Join-Path $compilerRoot 'bin');$(Join-Path $qtRoot 'bin');$env:PATH"
$vcpkgInstalledDirectory = Join-Path $buildDirectory 'vcpkg_installed'
$zlibHeaderPath = Join-Path $vcpkgInstalledDirectory "$Triplet\include\zlib.h"
$zlibRuntime = Find-ZlibRuntime $vcpkgInstalledDirectory $Configuration

if (-not (Test-Path $zlibHeaderPath) -or $null -eq $zlibRuntime) {
    Push-Location $projectRoot
    try {
        Invoke-LoggedCommand $vcpkgPath @(
            'install',
            "--triplet=$Triplet",
            "--x-install-root=$vcpkgInstalledDirectory",
            '--no-print-usage'
        ) (Join-Path $buildDirectory 'vcpkg-install.log')
    } finally {
        Pop-Location
    }

    $zlibRuntime = Find-ZlibRuntime $vcpkgInstalledDirectory $Configuration
    if ($null -eq $zlibRuntime) {
        throw "vcpkg finished without producing a zlib runtime DLL in $vcpkgInstalledDirectory\$Triplet\bin."
    }
}

$cachePath = Join-Path $buildDirectory 'CMakeCache.txt'
$needsConfigure = -not (Test-Path $cachePath)
if (-not $needsConfigure) {
    $buildTypeLine = Get-Content -LiteralPath $cachePath |
        Where-Object { $_ -like 'CMAKE_BUILD_TYPE:STRING=*' } |
        Select-Object -First 1
    $needsConfigure = $buildTypeLine -ne "CMAKE_BUILD_TYPE:STRING=$Configuration"
}

if ($needsConfigure) {
    Invoke-LoggedCommand $cmakePath @(
        '-S', $projectRoot,
        '-B', $buildDirectory,
        '-G', 'Ninja',
        "-DCMAKE_BUILD_TYPE=$Configuration",
        "-DCMAKE_PREFIX_PATH=$qtRoot",
        "-DCMAKE_CXX_COMPILER=$compilerPath",
        "-DCMAKE_TOOLCHAIN_FILE=$toolchainPath",
        "-DVCPKG_INSTALLED_DIR=$vcpkgInstalledDirectory",
        "-DVCPKG_TARGET_TRIPLET=$Triplet",
        '-DFOLDERSNAP_INSTALL_DEPENDENCIES=OFF',
        '-DBUILD_TESTING=OFF'
    ) (Join-Path $buildDirectory 'cmake-configure.log')
}

$applicationPath = Join-Path $buildDirectory 'FolderSnap.exe'
if (-not $SkipBuild) {
    $runningApplication = Get-Process -Name 'FolderSnap' -ErrorAction SilentlyContinue
    if ($null -ne $runningApplication) {
        throw 'FolderSnap.exe is running. Close the app before building the installer, or pass -SkipBuild to package the existing executable.'
    }

    Invoke-LoggedCommand $cmakePath @(
        '--build', $buildDirectory,
        '--target', 'FolderSnap',
        '--parallel', "$Parallel"
    ) (Join-Path $buildDirectory 'build.log')
}

if (-not (Test-Path $applicationPath)) {
    throw "The application executable was not found: $applicationPath"
}

$deploymentDirectory = Join-Path $buildDirectory 'deploy'
$installerDirectory = Join-Path $buildDirectory 'installer'
if (Test-Path $deploymentDirectory) {
    Remove-Item -LiteralPath $deploymentDirectory -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $deploymentDirectory, $installerDirectory | Out-Null

$deploymentApplicationPath = Join-Path $deploymentDirectory 'FolderSnap.exe'
Copy-Item -LiteralPath $applicationPath -Destination $deploymentApplicationPath

$windeployConfiguration = if ($Configuration -eq 'Debug') { '--debug' } else { '--release' }
Invoke-LoggedCommand $windeployqtPath @(
    $windeployConfiguration,
    '--compiler-runtime',
    '--no-translations',
    '--qmldir', (Join-Path $projectRoot 'src\qml'),
    '--dir', $deploymentDirectory,
    $deploymentApplicationPath
) (Join-Path $buildDirectory 'windeployqt.log')

Copy-Item -LiteralPath $zlibRuntime.FullName -Destination $deploymentDirectory

$installerScript = Join-Path $projectRoot 'installer\FolderSnap.iss'
$installerPath = Join-Path $installerDirectory "FolderSnap-Setup-$Version.exe"
if (Test-Path $installerPath) {
    Remove-Item -LiteralPath $installerPath -Force
}

Invoke-LoggedCommand $isccPath @(
    "/DAppVersion=$Version",
    "/DProjectRoot=$projectRoot",
    "/DSourceDir=$deploymentDirectory",
    "/DOutputDir=$installerDirectory",
    $installerScript
) (Join-Path $buildDirectory 'inno-setup.log')

if (-not (Test-Path $installerPath)) {
    throw "Inno Setup completed without producing $installerPath."
}

Write-Host "Deployment folder: $deploymentDirectory"
Write-Host "Installer: $installerPath"
