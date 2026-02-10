// ============================================================================
// GameVoid Engine — Entry Point
// ============================================================================
// Boots the engine with a default configuration and enters the main loop.
// Pass --no-editor to skip the CLI editor and run a real-time window loop.
// Pass --api-key <KEY> to configure the Gemini AI module.
// ============================================================================

#include "core/Engine.h"
#include <string>

int main(int argc, char* argv[]) {
    gv::EngineConfig config;
    config.windowTitle  = "GameVoid Engine";
    config.windowWidth  = 1280;
    config.windowHeight = 720;

    // ── Parse command-line flags ────────────────────────────────────────────
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--no-editor") {
            config.enableEditor = false;
        } else if (arg == "--api-key" && i + 1 < argc) {
            config.geminiAPIKey = argv[++i];
        } else if (arg == "--width" && i + 1 < argc) {
            config.windowWidth = static_cast<gv::u32>(std::stoi(argv[++i]));
        } else if (arg == "--height" && i + 1 < argc) {
            config.windowHeight = static_cast<gv::u32>(std::stoi(argv[++i]));
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "GameVoid Engine v0.1.0\n"
                      << "Usage: GameVoid [options]\n"
                      << "  --no-editor          Run real-time loop (skip CLI editor)\n"
                      << "  --api-key <KEY>      Set Google Gemini API key\n"
                      << "  --width <W>          Window width  (default 1280)\n"
                      << "  --height <H>         Window height (default 720)\n"
                      << "  --help, -h           Show this message\n";
            return 0;
        }
    }

    // ── Boot the engine ────────────────────────────────────────────────────
    gv::Engine& engine = gv::Engine::Instance();

    if (!engine.Init(config)) {
        GV_LOG_FATAL("Engine initialisation failed. Exiting.");
        return 1;
    }

    engine.Run();
    engine.Shutdown();

    return 0;
}
