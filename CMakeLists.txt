cmake_minimum_required(VERSION 3.20)
project(GameVoid VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── Options ────────────────────────────────────────────────────────────────
option(GV_ENABLE_PHYSICS  "Build with physics module"   ON)
option(GV_ENABLE_AI       "Build with AI module"        ON)
option(GV_ENABLE_SCRIPTING "Build with scripting module" ON)
option(GV_ENABLE_EDITOR   "Build with CLI editor"       ON)

# ─── External Dependencies (placeholders – swap with find_package / FetchContent) ─
# find_package(OpenGL REQUIRED)
# find_package(glfw3 REQUIRED)
# find_package(glm REQUIRED)
# find_package(assimp REQUIRED)
# find_package(Lua REQUIRED)
# find_package(CURL REQUIRED)

# ─── Source files ───────────────────────────────────────────────────────────
file(GLOB_RECURSE ENGINE_SOURCES
    "src/core/*.cpp"
    "src/renderer/*.cpp"
    "src/physics/*.cpp"
    "src/assets/*.cpp"
    "src/ai/*.cpp"
    "src/scripting/*.cpp"
    "src/editor/*.cpp"
    "src/editor2d/*.cpp"
    "src/future/*.cpp"
    "src/effects/*.cpp"
    "src/terrain/*.cpp"
    "src/animation/*.cpp"
    "src/input/*.cpp"
    "src/camera/*.cpp"
    "src/main.cpp"
)

add_executable(${PROJECT_NAME} ${ENGINE_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/deps
    ${CMAKE_SOURCE_DIR}/deps/miniaudio
)

# ── Platform-specific libraries ────────────────────────────────────────────
# WinInet for Gemini API integration on Windows
# ws2_32 for Winsock networking
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE wininet ws2_32)
endif()

# target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL glfw glm assimp lua curl)

# ─── Compiler warnings ─────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

message(STATUS "GameVoid Engine v${PROJECT_VERSION} — build configuration complete")
