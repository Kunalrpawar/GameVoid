$ErrorActionPreference = 'SilentlyContinue'
$cmd = "g++ -std=c++17 -DGV_HAS_GLFW -DIMGUI_DISABLE_WIN32_FUNCTIONS -O2 -Iinclude -Ideps -Ideps/glfw/include -Ideps/imgui -Ideps/miniaudio -Ldeps/glfw/lib -o GameVoid.exe src/main.cpp src/core/Engine.cpp src/core/FPSCamera.cpp src/core/SceneSerializer.cpp src/renderer/Renderer.cpp src/renderer/Camera.cpp src/renderer/Material.cpp src/renderer/MaterialComponent.cpp src/physics/Physics.cpp src/assets/Assets.cpp src/ai/AIManager.cpp src/scripting/ScriptEngine.cpp src/scripting/NodeGraph.cpp src/scripting/NativeScript.cpp src/editor/CLIEditor.cpp src/editor/OrbitCamera.cpp src/terrain/Terrain.cpp src/effects/ParticleSystem.cpp src/animation/Animation.cpp src/animation/SkeletalAnimation.cpp src/future/Placeholders.cpp src/core/Window.cpp src/core/GLLoader.cpp src/editor/EditorUI.cpp src/camera/EditorCamera.cpp src/input/ViewportInput.cpp src/editor2d/Editor2DCamera.cpp src/editor2d/Editor2DViewport.cpp deps/imgui/imgui.cpp deps/imgui/imgui_draw.cpp deps/imgui/imgui_tables.cpp deps/imgui/imgui_widgets.cpp deps/imgui/imgui_demo.cpp deps/imgui/imgui_impl_glfw.cpp deps/imgui/imgui_impl_opengl3.cpp -lglfw3 -lopengl32 -lgdi32 -lwininet -lws2_32 -lcomdlg32 -lole32 -lshell32"
cmd /c "$cmd 2>build_real_errors.txt"
Write-Host "EXIT CODE: $LASTEXITCODE"
if (Test-Path GameVoid.exe) {
    Write-Host "BUILD SUCCESS"
} else {
    Write-Host "BUILD FAILED"
    Get-Content build_real_errors.txt
}
