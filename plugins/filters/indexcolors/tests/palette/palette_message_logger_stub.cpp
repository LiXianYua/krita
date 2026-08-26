/*
 * SPDX-FileCopyrightText: 2026 S-09-b Task 1 palette handover fixture
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Minimal PkMessageLogger stub for the standalone palette byte-compat fixture.
 *
 * The production palettegeneratorconfig.cpp logs an error in the
 * fromByteArray() else-branch via the PkMessageLogger printf-style overload:
 *
 *   qDebug("PaletteGeneratorConfig::FromByteArray: Unsupported data version");
 *
 * The real implementation (pk/log/PkMessageLogger.cpp) routes through
 * PkLogBackend.cpp, which depends on <spdlog/spdlog.h> -- not installed in the
 * CI environment used for this standalone fixture. The tested version==0 path
 * never reaches the log call, so this stub only needs to satisfy the linker
 * for the constructor and the one debug(const char*, ...) overload.
 */

#include "PkMessageLogger.h"

#include <cstdarg>
#include <cstdio>

PkMessageLogger::PkMessageLogger(const char *file, int line, const char *fn,
                                 const PkLoggingCategory *cat)
    : _ctx{file, line, fn, cat ? "category" : "default"}
{
    (void)cat;
}

void PkMessageLogger::debug(const char *msg, ...) const
{
    va_list args;
    va_start(args, msg);
    std::vfprintf(stderr, msg, args);
    va_end(args);
    std::fputc('\n', stderr);
}
