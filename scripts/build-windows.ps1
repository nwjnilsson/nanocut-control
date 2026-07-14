<#
.SYNOPSIS
    Build script for NanoCut Control on Windows.
.DESCRIPTION
    Guides users through installing dependencies, configuring, and building
    NanoCut Control. Runs interactively by default with prompts before each
    action. Use -Yes to auto-accept all prompts (CI mode).
.EXAMPLE
    .\scripts\build-windows.ps1
    .\scripts\build-windows.ps1 -Yes
    .\scripts\build-windows.ps1 -VcpkgPath C:\vcpkg
#>

param(
    [switch]$Yes,
    [string]$VcpkgPath
)

$ErrorActionPreference = "Stop"
$Script:StepNum = 0
$Script:TotalSteps = 4

# ---------------------------------------------------------------------------
# Helper functions
# ---------------------------------------------------------------------------

function Write-Banner {
    Write-Host ""
    Write-Host "  ╔══════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "  ║         NanoCut Control Builder          ║" -ForegroundColor Cyan
    Write-Host "  ╚══════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  This script will check for required tools," -ForegroundColor Gray
    Write-Host "  install any that are missing, and build" -ForegroundColor Gray
    Write-Host "  NanoCut Control from source." -ForegroundColor Gray
    Write-Host ""
}

function Write-Step {
    param([string]$Message)
    $Script:StepNum++
    Write-Host ""
    Write-Host "[$Script:StepNum/$Script:TotalSteps] $Message" -ForegroundColor Cyan
    Write-Host ("-" * 50) -ForegroundColor DarkGray
}

function Exit-WithError {
    param([string]$Message)
    Write-Host ""
    Write-Host "ERROR: $Message" -ForegroundColor Red
    Write-Host ""
    exit 1
}

function Test-Command {
    param([string]$Name)
    $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Prompt-User {
    param(
        [string]$Question,
        [string]$Command
    )
    if ($Command) {
        Write-Host "  Command: " -ForegroundColor DarkGray -NoNewline
        Write-Host $Command -ForegroundColor Yellow
    }
    if ($Yes) {
        Write-Host "  $Question (y/n): y [auto]" -ForegroundColor DarkGray
        return $true
    }
    $answer = Read-Host "  $Question (y/n)"
    return ($answer -eq 'y' -or $answer -eq 'Y' -or $answer -eq 'yes')
}

function Refresh-Path {
    $machinePath = [System.Environment]::GetEnvironmentVariable("Path", "Machine")
    $userPath = [System.Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path = "$machinePath;$userPath"
}

function Invoke-Checked {
    param(
        [string]$Description,
        [scriptblock]$Block
    )
    Write-Host "  > $Description" -ForegroundColor Gray
    & $Block
    if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) {
        Exit-WithError "$Description failed with exit code $LASTEXITCODE."
    }
}

function Get-VsInstallPath {
    # Locate a Visual Studio / Build Tools install that has the C++ toolset.
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $path = & $vsWhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath 2>$null
        if ($path) { return ($path | Select-Object -First 1).Trim() }
    }
    # Fall back to an already-active developer environment (e.g. a Developer
    # PowerShell prompt).
    if ($env:VSINSTALLDIR) { return $env:VSINSTALLDIR.TrimEnd('\') }
    return $null
}

function Import-VsDevEnvironment {
    # Import the MSVC "x64 Native Tools" environment into this process. This is
    # what puts cl.exe on PATH and sets INCLUDE/LIB so the compiler can actually
    # find the standard headers and libraries. Without it, CMake's Ninja
    # generator either can't find a compiler at all ("CMAKE_CXX_COMPILER not
    # set") or, if pointed at cl.exe manually, fails the compiler check because
    # INCLUDE/LIB are missing.
    param([string]$InstallPath)

    $vcvars = Join-Path $InstallPath "VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvars)) {
        Exit-WithError "Could not find vcvars64.bat at '$vcvars'. Your Visual Studio install may be missing the C++ build tools (VC.Tools.x86.x64)."
    }

    Write-Host "  Importing MSVC build environment (vcvars64.bat)" -ForegroundColor Gray

    # Run vcvars64.bat in a child cmd, then dump the resulting environment with
    # `set` and copy each variable back into this PowerShell process.
    $applied = $false
    cmd.exe /c "call `"$vcvars`" && set" | ForEach-Object {
        if ($_ -match '^([^=\s]+)=(.*)$') {
            [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
            $applied = $true
        }
    }

    if (-not $applied) {
        Exit-WithError "Failed to import the MSVC build environment from '$vcvars'."
    }
    if (-not (Test-Command "cl")) {
        Exit-WithError "cl.exe is still not on PATH after importing the MSVC environment. Try opening 'Developer PowerShell for VS 2022' and running this script again."
    }
}

# ---------------------------------------------------------------------------
# Resolve project root (script lives in scripts/)
# ---------------------------------------------------------------------------

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$ProjectRoot = Split-Path -Parent $ScriptDir
Push-Location $ProjectRoot

try {

Write-Banner

# ===================================================================
# Phase 1: Check & install build tools
# ===================================================================

Write-Step "Checking build tools"

$tools = @(
    @{ Name = "CMake";  Cmd = "cmake";  WingetId = "Kitware.CMake" },
    @{ Name = "Ninja";  Cmd = "ninja";  WingetId = "Ninja-build.Ninja" }
)

foreach ($tool in $tools) {
    if (Test-Command $tool.Cmd) {
        Write-Host "  [OK] $($tool.Name) found" -ForegroundColor Green
    } else {
        Write-Host "  [!!] $($tool.Name) not found" -ForegroundColor Yellow
        if (Prompt-User "Install $($tool.Name) via winget?" "winget install --id $($tool.WingetId) -e --accept-source-agreements --accept-package-agreements") {
            Invoke-Checked "Installing $($tool.Name)" {
                winget install --id $tool.WingetId -e --accept-source-agreements --accept-package-agreements
            }
            Refresh-Path
            if (-not (Test-Command $tool.Cmd)) {
                Exit-WithError "$($tool.Name) was installed but is not on PATH. Please restart your terminal and run this script again."
            }
            Write-Host "  [OK] $($tool.Name) installed" -ForegroundColor Green
        } else {
            Exit-WithError "$($tool.Name) is required to build NanoCut. Please install it and try again."
        }
    }
}

# Visual Studio Build Tools / cl.exe
$vsInstallPath = Get-VsInstallPath

if ($vsInstallPath) {
    Write-Host "  [OK] Visual Studio Build Tools found" -ForegroundColor Green
} else {
    Write-Host "  [!!] Visual Studio Build Tools not found" -ForegroundColor Yellow
    if (Prompt-User "Install Visual Studio 2022 Build Tools via winget?" "winget install --id Microsoft.VisualStudio.2022.BuildTools -e --accept-source-agreements --accept-package-agreements") {
        Invoke-Checked "Installing Visual Studio Build Tools" {
            winget install --id Microsoft.VisualStudio.2022.BuildTools -e --accept-source-agreements --accept-package-agreements `
                --override "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
        }
        Refresh-Path
        $vsInstallPath = Get-VsInstallPath
        if (-not $vsInstallPath) {
            Exit-WithError "Visual Studio Build Tools were installed but could not be located. Please restart your terminal and run this script again."
        }
        Write-Host "  [OK] Visual Studio Build Tools installed" -ForegroundColor Green
    } else {
        Exit-WithError "Visual Studio Build Tools are required to build NanoCut. Please install them and try again."
    }
}

# Activate the MSVC toolchain in this session so CMake/Ninja and vcpkg can find
# cl.exe with a working INCLUDE/LIB. On the GitHub windows-latest runner a
# MinGW gcc happens to be on PATH, which is why this step was never needed
# there; a plain local PowerShell has no compiler environment at all.
Import-VsDevEnvironment $vsInstallPath

# ===================================================================
# Phase 2: vcpkg setup
# ===================================================================

Write-Step "Setting up vcpkg"

$vcpkgToolchain = $null

if ($VcpkgPath) {
    # User provided a path
    $candidateToolchain = Join-Path $VcpkgPath "scripts\buildsystems\vcpkg.cmake"
    if (Test-Path $candidateToolchain) {
        $vcpkgToolchain = $candidateToolchain
        Write-Host "  [OK] Using vcpkg at: $VcpkgPath" -ForegroundColor Green
    } else {
        Exit-WithError "Provided vcpkg path is invalid: could not find scripts\buildsystems\vcpkg.cmake in '$VcpkgPath'. Please provide the root vcpkg directory."
    }
} elseif (Test-Path ".\vcpkg\scripts\buildsystems\vcpkg.cmake") {
    # Already cloned in project directory
    Write-Host "  Found existing vcpkg in .\vcpkg" -ForegroundColor Gray
    if (Prompt-User "Use existing vcpkg installation?") {
        $vcpkgToolchain = (Resolve-Path ".\vcpkg\scripts\buildsystems\vcpkg.cmake").Path
        Write-Host "  [OK] Reusing existing vcpkg" -ForegroundColor Green
    }
}

if (-not $vcpkgToolchain) {
    # Fresh clone
    if (Prompt-User "Clone and bootstrap vcpkg?" "git clone https://github.com/microsoft/vcpkg.git") {
        if (Test-Path ".\vcpkg") {
            Remove-Item -Recurse -Force ".\vcpkg"
        }
        Invoke-Checked "Cloning vcpkg" {
            git clone https://github.com/microsoft/vcpkg.git
        }
        Invoke-Checked "Bootstrapping vcpkg" {
            & .\vcpkg\bootstrap-vcpkg.bat -disableMetrics
        }
        $vcpkgToolchain = (Resolve-Path ".\vcpkg\scripts\buildsystems\vcpkg.cmake").Path
        Write-Host "  [OK] vcpkg ready" -ForegroundColor Green
    } else {
        Exit-WithError "vcpkg is required. Provide a path with -VcpkgPath or allow the script to clone it."
    }
}

# Install vcpkg packages
Write-Host ""
Write-Host "  Installing vcpkg packages (glfw3, zlib)..." -ForegroundColor Gray
$vcpkgExe = Join-Path (Split-Path (Split-Path (Split-Path $vcpkgToolchain))) "vcpkg.exe"
if (-not (Test-Path $vcpkgExe)) {
    $vcpkgExe = Join-Path (Split-Path (Split-Path (Split-Path $vcpkgToolchain))) "vcpkg"
}
Invoke-Checked "Installing vcpkg packages" {
    & $vcpkgExe install glfw3 zlib
}
Write-Host "  [OK] vcpkg packages installed" -ForegroundColor Green

# ===================================================================
# Phase 3: Configure (CMake)
# ===================================================================

Write-Step "Configuring with CMake"

$buildDir = Join-Path $ProjectRoot "build"

if (Test-Path $buildDir) {
    Write-Host "  Build directory already exists." -ForegroundColor Gray
    if (Prompt-User "Clean and reconfigure?" "Remove-Item -Recurse build") {
        Remove-Item -Recurse -Force $buildDir
    }
}

if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

$cmakeArgs = @(
    ".."
    "-G", "Ninja"
    "-DCMAKE_TOOLCHAIN_FILE=$vcpkgToolchain"
    "-DCMAKE_BUILD_TYPE=Release"
    "-DCMAKE_C_COMPILER=cl"
    "-DCMAKE_CXX_COMPILER=cl"
)

Write-Host "  Command: cmake $($cmakeArgs -join ' ')" -ForegroundColor DarkGray

Push-Location $buildDir
try {
    Invoke-Checked "CMake configure" {
        cmake @cmakeArgs
    }
} finally {
    Pop-Location
}

Write-Host "  [OK] CMake configuration complete" -ForegroundColor Green

# ===================================================================
# Phase 4: Build & Install
# ===================================================================

Write-Step "Building and installing NanoCut"

Invoke-Checked "CMake build" {
    cmake --build $buildDir
}

Invoke-Checked "CMake install" {
    cmake --install $buildDir --prefix $ProjectRoot
}

Write-Host "  [OK] Build and install complete" -ForegroundColor Green

# ===================================================================
# Summary
# ===================================================================

$exePath = Join-Path $ProjectRoot "bin\NanoCut.exe"

Write-Host ""
Write-Host "  ╔══════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "  ║            Build Successful!             ║" -ForegroundColor Green
Write-Host "  ╚══════════════════════════════════════════╝" -ForegroundColor Green
Write-Host ""
Write-Host "  Executable: $exePath" -ForegroundColor Cyan
Write-Host ""

} finally {
    Pop-Location
}
