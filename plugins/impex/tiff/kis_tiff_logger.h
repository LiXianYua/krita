/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2022 L. E. Segovia <amy@amyspark.me>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TIFF_LOGGER_H
#define KIS_TIFF_LOGGER_H

#include <PkString.h>
#include <PkAuxTypes.h>
#include <PkStream.h>

#include <cstdio>
#include <limits>
#include <tiffio.h>

#include <kis_debug.h>
#include "tiff_stream_adapter.h"

inline PkString formatVarArgs(const char *fmt, va_list args)
{
    int size = 4096;
    PkByteArray buf;
    buf.resize(size);
    va_list copy;
    va_copy(copy, args);
#ifdef _WIN32
    int n = vsnprintf_s(buf.data(), size, size - 1, fmt, copy);
#else
    int n = vsnprintf(buf.data(), size, fmt, copy);
#endif
    va_end(copy);
    while (n >= size || buf.data()[size - 2]) {
        if (size > (1 << 20)) {
            return {};
        }
        size *= 2;
        buf.resize(size);
        buf.data()[size - 1] = 0;
        buf.data()[size - 2] = 0;
        va_copy(copy, args);
#ifdef _WIN32
        n = vsnprintf_s(buf.data(), size, size - 1, fmt, copy);
#else
        n = vsnprintf(buf.data(), size, fmt, copy);
#endif
        va_end(copy);
    }

    if (n > 0) {
        return PkString::PkFromUtf8(buf.constData(), n);
    } else {
        return {};
    }
}

inline void KisTiffErrorHandler(const char *module, const char *fmt, va_list args)
{
    PkString msg("%1: %2");

    errFile << msg.arg(module, formatVarArgs(fmt, args));
}

inline void KisTiffWarningHandler(const char *module, const char *fmt, va_list args)
{
    PkString msg("%1: %2");

    warnFile << msg.arg(module, formatVarArgs(fmt, args));
}

#endif
