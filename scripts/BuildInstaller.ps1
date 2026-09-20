[CmdletBinding()]
param(
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
        [string]$Fallback
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

function Find-CommandPath {
    param([Parameter(Mandatory)][string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue |
        Where-Object { $_.CommandType -eq 'Application' } |
        Select-Object -First 1
    if ($null -eq $command) {
        return $null
    }

    if (-not [string]::IsNullOrWhiteSpace($command.Path)) {
        return $command.Path
    }
    return $command.Source
}

function Write-Step {
    param([Parameter(Mandatory)][string]$Message)

    Write-Host "[FolderSnap] $Message"
}

function Invoke-LoggedCommand {
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [Parameter(Mandatory)][string[]]$ArgumentList,
        [Parameter(Mandatory)][string]$LogPath,
        [string]$Description = ''
    )

    $logDirectory = Split-Path -Parent $LogPath
    New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null

    if (-not [string]::IsNullOrWhiteSpace($Description)) {
        Write-Step "${Description}..."
    }

    & $FilePath @ArgumentList > $LogPath 2>&1
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        if (-not [string]::IsNullOrWhiteSpace($Description)) {
            Write-Host "[FolderSnap] ${Description} failed." -ForegroundColor Red
        }
        Write-Host "Command failed ($exitCode): $FilePath"
        if (Test-Path $LogPath) {
            Get-Content -LiteralPath $LogPath -Tail 160
        }
        throw "The command failed. See $LogPath for the complete output."
    }

    if (-not [string]::IsNullOrWhiteSpace($Description)) {
        Write-Step "${Description} complete."
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
$buildDirectory = Join-Path $projectRoot 'build'

Write-Step 'Preparing installer build.'
Write-Host "  Project: $projectRoot"
Write-Host "  Build:   $buildDirectory"
Write-Step 'Checking required build tools.'

$missingRequirements = @()

$cmakePath = Find-CommandPath 'cmake.exe'
if ($null -eq $cmakePath) {
    $missingRequirements += 'cmake.exe (not supplied and not found on PATH)'
}

$qtSetting = Resolve-Setting $QtRoot 'FOLDERSNAP_QT_ROOT' ''
if ([string]::IsNullOrWhiteSpace($qtSetting)) {
    $windeployqtPath = Find-CommandPath 'windeployqt.exe'
    if ($null -eq $windeployqtPath) {
        $missingRequirements += 'windeployqt.exe (QtRoot not supplied and not found on PATH)'
        $qtRoot = $null
    } else {
        $qtRoot = Split-Path -Parent (Split-Path -Parent $windeployqtPath)
    }
} else {
    $qtRoot = Resolve-AbsolutePath $qtSetting
    $windeployqtPath = Join-Path $qtRoot 'bin\windeployqt.exe'
    if (-not (Test-Path $windeployqtPath)) {
        $missingRequirements += "windeployqt.exe (not found under QtRoot: $qtRoot)"
    }
}

$compilerSetting = Resolve-Setting $CompilerRoot 'FOLDERSNAP_COMPILER_ROOT' ''
if ([string]::IsNullOrWhiteSpace($compilerSetting)) {
    $compilerPath = Find-CommandPath 'g++.exe'
    if ($null -eq $compilerPath) {
        $missingRequirements += 'g++.exe (CompilerRoot not supplied and not found on PATH)'
        $compilerRoot = $null
    } else {
        $compilerRoot = Split-Path -Parent (Split-Path -Parent $compilerPath)
    }
} else {
    $compilerRoot = Resolve-AbsolutePath $compilerSetting
    $compilerPath = Join-Path $compilerRoot 'bin\g++.exe'
    if (-not (Test-Path $compilerPath)) {
        $missingRequirements += "g++.exe (not found under CompilerRoot: $compilerRoot)"
    }
}

$vcpkgSetting = Resolve-Setting $VcpkgRoot 'VCPKG_ROOT' ''
if ([string]::IsNullOrWhiteSpace($vcpkgSetting)) {
    $vcpkgPath = Find-CommandPath 'vcpkg.exe'
    if ($null -eq $vcpkgPath) {
        $missingRequirements += 'vcpkg.exe (VcpkgRoot not supplied and not found on PATH)'
        $vcpkgRoot = $null
    } else {
        $vcpkgRoot = Split-Path -Parent $vcpkgPath
    }
} else {
    $vcpkgRoot = Resolve-AbsolutePath $vcpkgSetting
    $vcpkgPath = Join-Path $vcpkgRoot 'vcpkg.exe'
    if (-not (Test-Path $vcpkgPath)) {
        $missingRequirements += "vcpkg.exe (not found under VcpkgRoot: $vcpkgRoot)"
    }
}

$toolchainPath = if ($null -ne $vcpkgRoot) {
    Join-Path $vcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
} else {
    $null
}
if ($null -ne $toolchainPath -and -not (Test-Path $toolchainPath)) {
    $missingRequirements += "vcpkg.cmake toolchain (not found under VcpkgRoot: $vcpkgRoot)"
}

$innoSetting = Resolve-Setting $InnoSetupRoot 'FOLDERSNAP_INNO_ROOT' ''
if ([string]::IsNullOrWhiteSpace($innoSetting)) {
    $localApplicationData = [Environment]::GetFolderPath([Environment+SpecialFolder]::LocalApplicationData)
    $innoSetupRoot = Join-Path $localApplicationData 'Programs\Inno Setup 6'
} else {
    $innoSetupRoot = Resolve-AbsolutePath $innoSetting
}
$isccPath = Join-Path $innoSetupRoot 'ISCC.exe'
if (-not (Test-Path $isccPath)) {
    $missingRequirements += "ISCC.exe (not found at the Inno Setup path: $innoSetupRoot)"
}

if ($missingRequirements.Count -gt 0) {
    Write-Host 'Missing required build tools or paths:' -ForegroundColor Red
    foreach ($missingRequirement in $missingRequirements) {
        Write-Host "  - $missingRequirement" -ForegroundColor Red
    }
    throw 'Install the missing tools or provide their root paths, then run the installer script again.'
}

Write-Step 'Required build tools found.'
New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null
$env:PATH = "$(Join-Path $compilerRoot 'bin');$(Join-Path $qtRoot 'bin');$env:PATH"
$vcpkgInstalledDirectory = Join-Path $buildDirectory 'vcpkg_installed'
$zlibHeaderPath = Join-Path $vcpkgInstalledDirectory "$Triplet\include\zlib.h"
$zlibRuntime = Find-ZlibRuntime $vcpkgInstalledDirectory $Configuration

Write-Step 'Checking zlib dependency.'
if (-not (Test-Path $zlibHeaderPath) -or $null -eq $zlibRuntime) {
    Push-Location $projectRoot
    try {
        Invoke-LoggedCommand $vcpkgPath @(
            'install',
            "--triplet=$Triplet",
            "--x-install-root=$vcpkgInstalledDirectory",
            '--no-print-usage'
        ) (Join-Path $buildDirectory 'vcpkg-install.log') 'Installing zlib dependency'
    } finally {
        Pop-Location
    }

    $zlibRuntime = Find-ZlibRuntime $vcpkgInstalledDirectory $Configuration
    if ($null -eq $zlibRuntime) {
        throw "vcpkg finished without producing a zlib runtime DLL in $vcpkgInstalledDirectory\$Triplet\bin."
    }
    Write-Step 'zlib dependency ready.'
} else {
    Write-Step 'zlib dependency already available.'
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
    ) (Join-Path $buildDirectory 'cmake-configure.log') 'Configuring CMake'
} else {
    Write-Step 'CMake configuration is up to date.'
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
    ) (Join-Path $buildDirectory 'build.log') 'Building FolderSnap target'
} else {
    Write-Step 'Skipping application build (-SkipBuild).'
}

if (-not (Test-Path $applicationPath)) {
    throw "The application executable was not found: $applicationPath"
}

$deploymentDirectory = Join-Path $buildDirectory 'deploy'
$installerDirectory = Join-Path $buildDirectory 'installer'
Write-Step 'Preparing deployment directory.'
if (Test-Path $deploymentDirectory) {
    Remove-Item -LiteralPath $deploymentDirectory -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $deploymentDirectory, $installerDirectory | Out-Null

$deploymentApplicationPath = Join-Path $deploymentDirectory 'FolderSnap.exe'
Copy-Item -LiteralPath $applicationPath -Destination $deploymentApplicationPath
Write-Step 'Copied application executable.'

$windeployConfiguration = if ($Configuration -eq 'Debug') { '--debug' } else { '--release' }
Invoke-LoggedCommand $windeployqtPath @(
    $windeployConfiguration,
    '--compiler-runtime',
    '--no-translations',
    '--qmldir', (Join-Path $projectRoot 'src\qml'),
    '--dir', $deploymentDirectory,
    $deploymentApplicationPath
) (Join-Path $buildDirectory 'windeployqt.log') 'Deploying Qt runtime'

Copy-Item -LiteralPath $zlibRuntime.FullName -Destination $deploymentDirectory
Write-Step 'Copied zlib runtime.'

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
) (Join-Path $buildDirectory 'inno-setup.log') 'Building installer package'

if (-not (Test-Path $installerPath)) {
    throw "Inno Setup completed without producing $installerPath."
}

Write-Step 'Installer packaging complete.'
Write-Host "Deployment folder: $deploymentDirectory"
Write-Host "Installer: $installerPath"
