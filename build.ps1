# ============================================================================
# GameVoid Engine — Build Script (Window Mode)
# ============================================================================
# Compiles the engine with GLFW + OpenGL window support.
# Run setup.ps1 first to download GLFW.
#
# Usage:
#   .\build.ps1              Build with window support (default)
#   .\build.ps1 -CliOnly     Build without window (CLI-only, no GLFW needed)
# ============================================================================
param(
    [switch]$CliOnly
)

$ErrorActionPreference = "Stop"

$commonSources = @(
    "src/main.cpp",
    "src/core/Engine.cpp",
    "src/renderer/Renderer.cpp",
    "src/renderer/Camera.cpp",
    "src/physics/Physics.cpp",
    "src/assets/Assets.cpp",
    "src/ai/AIManager.cpp",
    "src/scripting/ScriptEngine.cpp",
    "src/editor/CLIEditor.cpp",
    "src/future/Placeholders.cpp"
)

if ($CliOnly) {
    # ── CLI-only build (no GLFW, no window) ─────────────────────────────────
    Write-Host "=== Building GameVoid Engine (CLI-Only) ===" -ForegroundColor Cyan
    $srcList = ($commonSources + "src/core/Window.cpp", "src/core/GLLoader.cpp") -join " "
    $cmd = "g++ -std=c++17 -Iinclude -o GameVoid.exe $srcList"
    Write-Host $cmd
    Invoke-Expression $cmd
} else {
    # ── Window build (GLFW + OpenGL) ────────────────────────────────────────
    if (-not (Test-Path "deps/glfw/lib/libglfw3.a")) {
        Write-Host "ERROR: GLFW not found in deps/glfw/.  Run setup.ps1 first." -ForegroundColor Red
        exit 1
    }

    Write-Host "=== Building GameVoid Engine (Window Mode) ===" -ForegroundColor Cyan
    $windowSources = @("src/core/Window.cpp", "src/core/GLLoader.cpp")
    $srcList = ($commonSources + $windowSources) -join " "
    $cmd = "g++ -std=c++17 -DGV_HAS_GLFW -O2 -Iinclude -Ideps/glfw/include -Ldeps/glfw/lib -o GameVoid.exe $srcList -lglfw3 -lopengl32 -lgdi32"
    Write-Host $cmd
    Invoke-Expression $cmd
}

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "Build successful!" -ForegroundColor Green
    if (-not $CliOnly) {
        Write-Host "  .\GameVoid.exe --no-editor    Window mode"
    }
    Write-Host "  .\GameVoid.exe                 CLI editor mode"
} else {
    Write-Host "Build FAILED." -ForegroundColor Red
    exit 1
}
