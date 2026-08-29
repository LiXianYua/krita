/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_CHUNK_LIST_H
#define __KIS_CHUNK_LIST_H

#include <cstdint>
#include <cassert>

#include <list>
#include "kritaimage_export.h"

#define MiB (1ULL << 20)

#define DEFAULT_STORE_SIZE (4096*MiB)
#define DEFAULT_SLAB_SIZE (64*MiB)


//#define DEBUG_SLAB_FAILS

#ifdef DEBUG_SLAB_FAILS

#define WINDOW_SIZE 2000
#define DECLARE_FAIL_COUNTER() std::uint64_t __failCount
#define INIT_FAIL_COUNTER() __failCount = 0
#define START_COUNTING() std::uint64_t __numSteps = 0
#define REGISTER_STEP() if(++__numSteps > WINDOW_SIZE) {__numSteps=0; __failCount++;}
#define REGISTER_FAIL() __failCount++
#define DEBUG_FAIL_COUNTER() infoTiles << "Slab fail count:\t" << __failCount

#else

#define DECLARE_FAIL_COUNTER()
#define INIT_FAIL_COUNTER()
#define START_COUNTING()
#define REGISTER_STEP()
#define REGISTER_FAIL()
#define DEBUG_FAIL_COUNTER()

#endif /* DEBUG_SLAB_FAILS */



class KisChunkData;

typedef std::list<KisChunkData> KisChunkDataList;
typedef KisChunkDataList::iterator KisChunkDataListIterator;

class KRITAIMAGE_EXPORT KisChunkData
{
public:
    KisChunkData(std::uint64_t begin, std::uint64_t size)
    {
        setChunk(begin, size);
    }

    inline void setChunk(std::uint64_t begin, std::uint64_t size) {
        m_begin = begin;
        m_end = begin + size - 1;
    }

    inline std::uint64_t size() const {
        return m_end - m_begin +1;
    }

    bool operator== (const KisChunkData& other) const
    {
        assert(m_begin!=other.m_begin || m_end==other.m_end);

        /**
         * Chunks cannot overlap, so it is enough to check
         * the beginning of the interval only
         */
        return m_begin == other.m_begin;
    }

    std::uint64_t m_begin;
    std::uint64_t m_end;
};

class KRITAIMAGE_EXPORT KisChunk
{
public:
    KisChunk() {}

    KisChunk(KisChunkDataListIterator iterator)
        : m_iterator(iterator)
    {
    }

    inline std::uint64_t begin() const {
        return m_iterator->m_begin;
    }

    inline std::uint64_t end() const {
        return m_iterator->m_end;
    }

    inline std::uint64_t size() const {
        return m_iterator->size();
    }

    inline KisChunkDataListIterator position() {
        return m_iterator;
    }

    inline const KisChunkData& data() {
        return *m_iterator;
    }

private:
    KisChunkDataListIterator m_iterator;
};


class KRITAIMAGE_EXPORT KisChunkAllocator
{
public:
    KisChunkAllocator(std::uint64_t slabSize = DEFAULT_SLAB_SIZE,
                      std::uint64_t storeSize = DEFAULT_STORE_SIZE);
    ~KisChunkAllocator();

    inline std::uint64_t numChunks() const {
        return m_list.size();
    }

    KisChunk getChunk(std::uint64_t size);
    void freeChunk(KisChunk chunk);

    void debugChunks();
    bool sanityCheck(bool pleaseCrash = true);
    double debugFragmentation(bool toStderr = true);

private:
    bool tryInsertChunk(KisChunkDataList &list,
                        KisChunkDataListIterator &iterator,
                        std::uint64_t size);

private:
    std::uint64_t m_storeMaxSize;
    std::uint64_t m_storeSlabSize;


    KisChunkDataList m_list;
    KisChunkDataListIterator m_iterator;
    std::uint64_t m_storeSize;
    DECLARE_FAIL_COUNTER()
};

#endif /* __KIS_CHUNK_ALLOCATOR_H */
