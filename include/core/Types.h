// ============================================================================
// GameVoid Engine — Core Type Aliases & Forward Declarations
// ============================================================================
// Provides engine-wide type aliases, forward declarations, and utility macros.
// Every module should include this header for consistent typing.
// ============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <sstream>
#include <cassert>
#include <cmath>

namespace gv {

// ─── Numeric type aliases ──────────────────────────────────────────────────
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;

// ─── Smart-pointer aliases ─────────────────────────────────────────────────
template <typename T> using Unique = std::unique_ptr<T>;
template <typename T> using Shared = std::shared_ptr<T>;
template <typename T> using Weak   = std::weak_ptr<T>;

template <typename T, typename... Args>
constexpr Unique<T> MakeUnique(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
constexpr Shared<T> MakeShared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

// ─── Logging helpers (placeholder – swap for spdlog / custom logger) ───────
enum class LogLevel { Trace, Debug, Info, Warn, Error, Fatal };

inline void Log(LogLevel level, const std::string& msg) {
    const char* tags[] = { "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL" };
    std::cout << "[GameVoid][" << tags[static_cast<int>(level)] << "] " << msg << "\n";
}

#define GV_LOG_TRACE(msg) ::gv::Log(::gv::LogLevel::Trace, msg)
#define GV_LOG_DEBUG(msg) ::gv::Log(::gv::LogLevel::Debug, msg)
#define GV_LOG_INFO(msg)  ::gv::Log(::gv::LogLevel::Info,  msg)
#define GV_LOG_WARN(msg)  ::gv::Log(::gv::LogLevel::Warn,  msg)
#define GV_LOG_ERROR(msg) ::gv::Log(::gv::LogLevel::Error, msg)
#define GV_LOG_FATAL(msg) ::gv::Log(::gv::LogLevel::Fatal, msg)

} // namespace gv
