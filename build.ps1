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
    "src/core/FPSCamera.cpp",
    "src/core/SceneSerializer.cpp",
    "src/renderer/Renderer.cpp",
    "src/renderer/Camera.cpp",
    "src/renderer/Material.cpp",
    "src/renderer/MaterialComponent.cpp",
    "src/physics/Physics.cpp",
    "src/assets/Assets.cpp",
    "src/ai/AIManager.cpp",
    "src/scripting/ScriptEngine.cpp",
    "src/scripting/NodeGraph.cpp",
    "src/scripting/NativeScript.cpp",
    "src/editor/CLIEditor.cpp",
    "src/editor/OrbitCamera.cpp",
    "src/terrain/Terrain.cpp",
    "src/effects/ParticleSystem.cpp",
    "src/animation/Animation.cpp",
    "src/animation/SkeletalAnimation.cpp",
    "src/future/Placeholders.cpp"
)

if ($CliOnly) {
    # ── CLI-only build (no GLFW, no window) ─────────────────────────────────
    Write-Host "=== Building GameVoid Engine (CLI-Only) ===" -ForegroundColor Cyan
    $srcList = ($commonSources + "src/core/Window.cpp", "src/core/GLLoader.cpp") -join " "
    $cmd = "g++ -std=c++17 -Iinclude -o GameVoid.exe $srcList -lwininet -lws2_32"
    Write-Host $cmd
    Invoke-Expression $cmd
} else {
    # ── Window build (GLFW + OpenGL) ────────────────────────────────────────
    if (-not (Test-Path "deps/glfw/lib/libglfw3.a")) {
        Write-Host "ERROR: GLFW not found in deps/glfw/.  Run setup.ps1 first." -ForegroundColor Red
        exit 1
    }

    Write-Host "=== Building GameVoid Engine (Window Mode) ===" -ForegroundColor Cyan
    $windowSources = @(
        "src/core/Window.cpp",
        "src/core/GLLoader.cpp",
        "src/editor/EditorUI.cpp",
        "src/camera/EditorCamera.cpp",
        "src/input/ViewportInput.cpp"
    )
    $imguiSources = @(
        "deps/imgui/imgui.cpp",
        "deps/imgui/imgui_draw.cpp",
        "deps/imgui/imgui_tables.cpp",
        "deps/imgui/imgui_widgets.cpp",
        "deps/imgui/imgui_demo.cpp",
        "deps/imgui/imgui_impl_glfw.cpp",
        "deps/imgui/imgui_impl_opengl3.cpp"
    )
    $srcList = ($commonSources + $windowSources + $imguiSources) -join " "
    $cmd = "g++ -std=c++17 -DGV_HAS_GLFW -DIMGUI_DISABLE_WIN32_FUNCTIONS -O2 -Iinclude -Ideps -Ideps/glfw/include -Ideps/imgui -Ideps/miniaudio -Ldeps/glfw/lib -o GameVoid.exe $srcList -lglfw3 -lopengl32 -lgdi32 -lwininet -lws2_32 -lcomdlg32 -lole32 -lshell32"
    Write-Host $cmd
    Invoke-Expression $cmd
}

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "Build successful!" -ForegroundColor Green
    if (-not $CliOnly) {
        Write-Host "  .\GameVoid.exe --editor-gui    GUI editor (Dear ImGui)"
        Write-Host "  .\GameVoid.exe --no-editor     Window mode (game loop)"
    }
    Write-Host "  .\GameVoid.exe                 CLI editor mode"
} else {
    Write-Host "Build FAILED." -ForegroundColor Red
    exit 1
}
