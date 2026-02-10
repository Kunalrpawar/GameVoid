# ============================================================================
# GameVoid Engine — Dependency Setup
# ============================================================================
# Downloads GLFW 3.3.8 source and compiles it with your local gcc.
# This avoids ABI incompatibilities with pre-built MinGW binaries.
# Custom DirectInput/XInput stub headers in deps/dxstubs/ are used.
# Run from the project root:  .\setup.ps1
# ============================================================================
$ErrorActionPreference = "Stop"

$glfwVersion = "3.3.8"
$srcUrl      = "https://github.com/glfw/glfw/releases/download/$glfwVersion/glfw-$glfwVersion.zip"
$zipFile     = "glfw-src.zip"
$srcDir      = "glfw-$glfwVersion"

Write-Host "=== GameVoid Engine - Dependency Setup ===" -ForegroundColor Cyan

# ── Build GLFW from source ──────────────────────────────────────────────────
if (Test-Path "deps/glfw/lib/libglfw3.a") {
    Write-Host "GLFW already installed in deps/glfw/. Skipping." -ForegroundColor Yellow
} else {
    Write-Host "Downloading GLFW $glfwVersion source..."
    curl.exe -L --fail --progress-bar -o $zipFile $srcUrl
    if (-not (Test-Path $zipFile)) {
        Write-Host "ERROR: Download failed." -ForegroundColor Red
        exit 1
    }
    Write-Host "Extracting..."
    Expand-Archive -Path $zipFile -DestinationPath "." -Force

    # Compile all GLFW Win32 source files
    Write-Host "Compiling GLFW from source (this may take a moment)..."
    New-Item -ItemType Directory -Path "glfw_build" -Force | Out-Null
    Push-Location "glfw_build"

    $glfwSources = @(
        "context.c", "init.c", "input.c", "monitor.c", "vulkan.c", "window.c",
        "win32_init.c", "win32_joystick.c", "win32_monitor.c", "win32_thread.c",
        "win32_time.c", "win32_window.c", "wgl_context.c", "egl_context.c",
        "osmesa_context.c"
    ) | ForEach-Object { "..\$srcDir\src\$_" }

    $gccArgs = @("-c", "-O2", "-D_GLFW_WIN32", "-I..\deps\dxstubs", "-I..\$srcDir\include") + $glfwSources
    & gcc $gccArgs 2>&1 | ForEach-Object {
        if ($_ -match 'error:') { Write-Host $_ -ForegroundColor Red }
    }

    $oFiles = (Get-ChildItem *.o -ErrorAction SilentlyContinue).FullName
    if (-not $oFiles -or $oFiles.Count -lt 10) {
        Pop-Location
        Write-Host "ERROR: GLFW compilation failed." -ForegroundColor Red
        exit 1
    }

    # Create static library
    New-Item -ItemType Directory -Path "..\deps\glfw\lib" -Force | Out-Null
    New-Item -ItemType Directory -Path "..\deps\glfw\include" -Force | Out-Null
    ar rcs "..\deps\glfw\lib\libglfw3.a" $oFiles 2>&1 | Out-Null

    Pop-Location

    # Copy headers
    Copy-Item -Path "$srcDir\include\GLFW" -Destination "deps\glfw\include\GLFW" -Recurse -Force

    # Cleanup
    Remove-Item -Path $zipFile      -Force -ErrorAction SilentlyContinue
    Remove-Item -Path $srcDir       -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -Path "glfw_build"  -Recurse -Force -ErrorAction SilentlyContinue

    Write-Host "GLFW $glfwVersion built and installed to deps/glfw/" -ForegroundColor Green
}

Write-Host ""
Write-Host "Setup complete!  Next steps:" -ForegroundColor Cyan
Write-Host "  .\build.ps1                   Build with window support"
Write-Host "  .\GameVoid.exe --no-editor    Run in window mode"
Write-Host "  .\GameVoid.exe                Run CLI editor (no window)"
