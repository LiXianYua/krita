#pragma once
#include <string>

// 级别取值刻意与 spdlog::level::level_enum 无关——PkLogBackend.cpp 做映射，
// 这样"换后端"只动那一个翻译单元。
enum PkLogLevel {
    PkLogDebug = 0,
    PkLogInfo,
    PkLogWarning,
    PkLogCritical,
    PkLogFatal
};

// QMessageLogContext 的对应物。只保留有调用点的字段：version 没有调用点，不要。
struct PkLogContext {
    const char *file = nullptr;
    int line = 0;
    const char *function = nullptr;
    const char *category = "default";
};
