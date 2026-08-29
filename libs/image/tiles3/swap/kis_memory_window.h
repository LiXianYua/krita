/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_MEMORY_WINDOW_H
#define __KIS_MEMORY_WINDOW_H

#include <cstdint>

#include <string>

#include <PkString.h>

#include "kis_chunk_allocator.h"


#define DEFAULT_WINDOW_SIZE (16*MiB)

class KRITAIMAGE_EXPORT KisMemoryWindow
{
public:
    /**
     * @param swapDir If the dir doesn't exist, it'll be created.
     * @param writeWindowSize write window size.
     */
    KisMemoryWindow(const PkString &swapDir, std::uint64_t writeWindowSize = DEFAULT_WINDOW_SIZE);
    ~KisMemoryWindow();

    inline std::uint8_t* getReadChunkPtr(KisChunk readChunk) {
        return getReadChunkPtr(readChunk.data());
    }

    inline std::uint8_t* getWriteChunkPtr(KisChunk writeChunk) {
        return getWriteChunkPtr(writeChunk.data());
    }

    std::uint8_t* getReadChunkPtr(const KisChunkData &readChunk);
    std::uint8_t* getWriteChunkPtr(const KisChunkData &writeChunk);

private:
    struct MappingWindow {
        MappingWindow(std::uint64_t _defaultSize)
            : chunk(0,0),
              window(0),
              defaultSize(_defaultSize)
        {
        }

        std::uint8_t* calculatePointer(const KisChunkData &other) const {
            return window + other.m_begin - chunk.m_begin;
        }

        /**
         * The mapping window. The mapping interface exposes the raw
         * pointer (window) and its length (chunk.size()); the F-16 swap-out
         * path will need both to call msync(MS_ASYNC) and
         * madvise(MADV_DONTNEED) on the mapped region.
         */
        KisChunkData chunk;
        std::uint8_t *window;
        const std::uint64_t defaultSize;
    };


private:
    bool adjustWindow(const KisChunkData &requestedChunk,
                      MappingWindow *adjustingWindow,
                      MappingWindow *otherWindow);

    std::uint8_t* mapFile(std::uint64_t begin, std::uint64_t size);
    void unmapFile(std::uint8_t *window, std::uint64_t size);
    std::uint64_t fileSize() const;
    bool resizeFile(std::uint64_t newSize);

private:
    int m_fileFd;
    std::string m_fileName;

    bool m_valid;
    MappingWindow m_readWindowEx;
    MappingWindow m_writeWindowEx;
};

#endif /* __KIS_MEMORY_WINDOW_H */
