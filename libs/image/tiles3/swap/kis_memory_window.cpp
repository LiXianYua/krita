/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cstdint>

#include "kis_debug.h"
#include "kis_memory_window.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <vector>

#define SWP_PREFIX "KRITA_SWAP_FILE_XXXXXX"

namespace {

bool ensureDirExists(const std::string &path)
{
    struct stat st;
    if (::stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }

    // mkdir -p style: create intermediate directories
    std::string p;
    for (std::string::const_iterator it = path.begin(); it != path.end(); ++it) {
        p += *it;
        if (*it == '/') {
            if (p.size() > 1 && ::stat(p.c_str(), &st) != 0) {
                ::mkdir(p.c_str(), 0700);
            }
        }
    }
    ::mkdir(path.c_str(), 0700);

    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

} // namespace

KisMemoryWindow::KisMemoryWindow(const PkString &swapDir, PkTilesQuint64 writeWindowSize)
    : m_fileFd(-1),
      m_readWindowEx(writeWindowSize / 4),
      m_writeWindowEx(writeWindowSize)
{
    m_valid = true;

    // swapDir will never be empty, as KisImageConfig::swapDir() always provides
    // us with a (platform specific) default directory, even if none is explicitly
    // configured by the user; also we do not want any logic that determines the
    // default swap dir here.
    KIS_SAFE_ASSERT_RECOVER_NOOP(!swapDir.isEmpty());

    const std::string dir = swapDir.PkToUtf8();
    if (!ensureDirExists(dir)) {
        m_valid = false;
    }

    if (m_valid) {
        std::string swapFileTemplate = dir + '/' + SWP_PREFIX;
        std::vector<char> nameBuf(swapFileTemplate.begin(), swapFileTemplate.end());
        nameBuf.push_back('\0');

        const int fd = ::mkstemp(nameBuf.data());
        if (fd < 0) {
            m_valid = false;
        } else {
            m_fileFd = fd;
            m_fileName = nameBuf.data();
        }
    }

    if (!m_valid) {
        qWarning() << "Could not create or open swapfile; disabling swapfile" << swapDir;
    }
}

KisMemoryWindow::~KisMemoryWindow()
{
    if (m_fileFd >= 0) {
        if (m_readWindowEx.window) {
            munmap(m_readWindowEx.window, m_readWindowEx.chunk.size());
        }
        if (m_writeWindowEx.window) {
            munmap(m_writeWindowEx.window, m_writeWindowEx.chunk.size());
        }
        ::close(m_fileFd);
        if (!m_fileName.empty()) {
            ::unlink(m_fileName.c_str());
        }
        m_fileFd = -1;
    }
}

std::uint8_t* KisMemoryWindow::getReadChunkPtr(const KisChunkData &readChunk)
{
    if (!adjustWindow(readChunk, &m_readWindowEx, &m_writeWindowEx)) {
        return nullptr;
    }

    return m_readWindowEx.calculatePointer(readChunk);
}

std::uint8_t* KisMemoryWindow::getWriteChunkPtr(const KisChunkData &writeChunk)
{
    if (!adjustWindow(writeChunk, &m_writeWindowEx, &m_readWindowEx)) {
        return nullptr;
    }

    return m_writeWindowEx.calculatePointer(writeChunk);
}

std::uint8_t* KisMemoryWindow::mapFile(PkTilesQuint64 begin, PkTilesQuint64 size)
{
    void *ptr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fileFd,
                       static_cast<off_t>(begin));
    return ptr == MAP_FAILED ? nullptr : static_cast<std::uint8_t*>(ptr);
}

void KisMemoryWindow::unmapFile(std::uint8_t *window, PkTilesQuint64 size)
{
    if (window) {
        ::munmap(window, size);
    }
}

PkTilesQuint64 KisMemoryWindow::fileSize() const
{
    struct stat st;
    if (::fstat(m_fileFd, &st) == 0) {
        return static_cast<PkTilesQuint64>(st.st_size);
    }
    return 0;
}

bool KisMemoryWindow::resizeFile(PkTilesQuint64 newSize)
{
    return ::ftruncate(m_fileFd, static_cast<off_t>(newSize)) == 0;
}

bool KisMemoryWindow::adjustWindow(const KisChunkData &requestedChunk,
                                   MappingWindow *adjustingWindow,
                                   MappingWindow *otherWindow)
{
    (void)(otherWindow);

    if(!(adjustingWindow->window) ||
       !(requestedChunk.m_begin >= adjustingWindow->chunk.m_begin &&
         requestedChunk.m_end <= adjustingWindow->chunk.m_end))
    {
        unmapFile(adjustingWindow->window, adjustingWindow->chunk.size());
        adjustingWindow->window = 0;

        PkTilesQuint64 windowSize = adjustingWindow->defaultSize;
        if(requestedChunk.size() > windowSize) {
            warnKrita <<
                "KisMemoryWindow: the requested chunk is too "
                "big to fit into the mapping! "
                "Adjusting mapping to avoid SIGSEGV...";

            windowSize = requestedChunk.size();
        }

        adjustingWindow->chunk.setChunk(requestedChunk.m_begin, windowSize);

        if(adjustingWindow->chunk.m_end >= fileSize()) {
            // Align by 32 bytes
            PkTilesQuint64 newSize = (adjustingWindow->chunk.m_end + 1 + 32) & (~31ULL);

            if (!resizeFile(newSize)) {
                return false;
            }
        }

        /**
         * Note: with raw mmap there is no Qt-internal map-handle cache to
         * invalidate, so no other-window unmapping/re-mapping is needed after
         * resize (that was a workaround for the Qt file-engine internals).
         */
        adjustingWindow->window = mapFile(adjustingWindow->chunk.m_begin,
                                          adjustingWindow->chunk.size());

        if (!adjustingWindow->window) {
            return false;
        }
    }

	return true;
}
