# ============================================================================
# GameVoid Engine — AI Server Setup Script (Windows)
# ============================================================================
# One-click setup: creates Python venv, installs dependencies, starts server.
#
# Usage:
#   .\ai_server\setup_ai_server.ps1                  # Full setup + start
#   .\ai_server\setup_ai_server.ps1 -StartOnly       # Just start the server
#   .\ai_server\setup_ai_server.ps1 -InstallOnly     # Just install deps
# ============================================================================

param(
    [switch]$StartOnly,
    [switch]$InstallOnly,
    [int]$Port = 5000
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$VenvDir = Join-Path $ScriptDir ".venv"
$RequirementsFile = Join-Path $ScriptDir "requirements.txt"
$ServerScript = Join-Path $ScriptDir "server.py"

Write-Host ""
Write-Host "======================================================" -ForegroundColor Cyan
Write-Host "  GameVoid AI Server Setup" -ForegroundColor Cyan
Write-Host "======================================================" -ForegroundColor Cyan
Write-Host ""

# ── Check Python ────────────────────────────────────────────────────────────
function Find-Python {
    $pythonCmds = @("python", "python3", "py")
    foreach ($cmd in $pythonCmds) {
        try {
            $ver = & $cmd --version 2>&1
            if ($ver -match "Python 3\.(\d+)") {
                $minor = [int]$Matches[1]
                if ($minor -ge 10) {
                    Write-Host "  Found: $ver" -ForegroundColor Green
                    return $cmd
                }
            }
        } catch {}
    }
    return $null
}

$PythonCmd = Find-Python
if (-not $PythonCmd) {
    Write-Host "  ERROR: Python 3.10+ is required but not found!" -ForegroundColor Red
    Write-Host "  Please install Python from https://python.org" -ForegroundColor Yellow
    exit 1
}

# ── Create Virtual Environment ──────────────────────────────────────────────
if (-not $StartOnly) {
    if (-not (Test-Path $VenvDir)) {
        Write-Host "  Creating virtual environment..." -ForegroundColor Yellow
        & $PythonCmd -m venv $VenvDir
        Write-Host "  Virtual environment created at: $VenvDir" -ForegroundColor Green
    } else {
        Write-Host "  Virtual environment exists at: $VenvDir" -ForegroundColor Green
    }
}

# ── Activate Venv & Install Dependencies ────────────────────────────────────
$VenvPython = Join-Path $VenvDir "Scripts\python.exe"
$VenvPip = Join-Path $VenvDir "Scripts\pip.exe"

if (-not (Test-Path $VenvPython)) {
    Write-Host "  ERROR: Virtual environment Python not found at $VenvPython" -ForegroundColor Red
    exit 1
}

if (-not $StartOnly) {
    Write-Host ""
    Write-Host "  Installing dependencies (this may take a few minutes)..." -ForegroundColor Yellow
    Write-Host "  Note: PyTorch download is ~2GB on first install" -ForegroundColor DarkYellow
    Write-Host ""

    # Upgrade pip
    & $VenvPython -m pip install --upgrade pip --quiet

    # Install PyTorch (CPU version by default — smaller download)
    # Users with NVIDIA GPU can install CUDA version manually
    Write-Host "  Installing PyTorch (CPU)..." -ForegroundColor Yellow
    & $VenvPip install torch torchvision --index-url https://download.pytorch.org/whl/cpu --quiet 2>$null

    # Install remaining dependencies
    Write-Host "  Installing remaining packages..." -ForegroundColor Yellow
    & $VenvPip install flask Pillow numpy trimesh google-genai rembg timm einops huggingface-hub requests --quiet

    # Try to install open3d (may fail on some Python versions)
    Write-Host "  Installing Open3D (for MiDaS fallback)..." -ForegroundColor Yellow
    & $VenvPip install open3d --quiet 2>$null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  Warning: Open3D install failed. MiDaS fallback won't be available." -ForegroundColor DarkYellow
    }

    Write-Host ""
    Write-Host "  All dependencies installed!" -ForegroundColor Green
}

if ($InstallOnly) {
    Write-Host ""
    Write-Host "  Setup complete. Run with -StartOnly to start the server." -ForegroundColor Cyan
    exit 0
}

# ── Start Server ────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "======================================================" -ForegroundColor Cyan
Write-Host "  Starting GameVoid AI Server on port $Port" -ForegroundColor Cyan
Write-Host "  Press Ctrl+C to stop" -ForegroundColor DarkCyan
Write-Host "======================================================" -ForegroundColor Cyan
Write-Host ""

# Create output directory
$OutputDir = Join-Path $ProjectRoot "generated_models"
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

& $VenvPython $ServerScript --port $Port
