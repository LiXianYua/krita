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

inline tmsize_t kisTiffStreamRead(thandle_t handle, void *data, tmsize_t size)
{
    if (!handle || size < 0) return -1;
    return static_cast<PkStream *>(handle)->read(static_cast<char *>(data), size);
}

inline tmsize_t kisTiffStreamWrite(thandle_t handle, void *data, tmsize_t size)
{
    if (!handle || size < 0) return -1;
    return static_cast<PkStream *>(handle)->write(static_cast<const char *>(data), size);
}

inline toff_t kisTiffStreamSeek(thandle_t handle, toff_t offset, int whence)
{
    auto *stream = static_cast<PkStream *>(handle);
    if (!stream) return static_cast<toff_t>(-1);
    const qint64 signedOffset = static_cast<qint64>(offset);
    qint64 base = 0;
    if (whence == SEEK_CUR) base = stream->pos();
    else if (whence == SEEK_END) base = stream->size();
    else if (whence != SEEK_SET) return static_cast<toff_t>(-1);
    if (base < 0 || signedOffset == std::numeric_limits<qint64>::min() ||
        (signedOffset > 0 && base > std::numeric_limits<qint64>::max() - signedOffset) ||
        (signedOffset < 0 && base < -signedOffset)) {
        return static_cast<toff_t>(-1);
    }
    const qint64 position = base + signedOffset;
    return stream->seek(position) ? static_cast<toff_t>(position) : static_cast<toff_t>(-1);
}

inline int kisTiffStreamClose(thandle_t) { return 0; }
inline toff_t kisTiffStreamSize(thandle_t handle)
{
    const auto size = handle ? static_cast<PkStream *>(handle)->size() : -1;
    return size >= 0 ? static_cast<toff_t>(size) : 0;
}
inline int kisTiffStreamMap(thandle_t, void **, toff_t *) { return 0; }
inline void kisTiffStreamUnmap(thandle_t, void *, toff_t) {}

inline TIFF *kisTiffOpenStream(PkStream *stream, const char *mode)
{
    if (!stream || !stream->seek(0)) return nullptr;
    return TIFFClientOpen("PkStream",
                          mode,
                          static_cast<thandle_t>(stream),
                          kisTiffStreamRead,
                          kisTiffStreamWrite,
                          kisTiffStreamSeek,
                          kisTiffStreamClose,
                          kisTiffStreamSize,
                          kisTiffStreamMap,
                          kisTiffStreamUnmap);
}

#endif
