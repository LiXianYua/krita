/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_CHUNK_LIST_H
#define __KIS_CHUNK_LIST_H

#include <cstdint>
#include <type_traits>
#include <cstdlib>
#include <cstdio>

#ifndef PK_TILES_ASSERT
#  if defined(QT_NO_DEBUG) || defined(NDEBUG)
#    define PK_TILES_ASSERT(condition) ((void)sizeof(condition))
#    define PK_TILES_ASSERT_X(condition, where, what) \
        ((void)sizeof(condition), (void)sizeof(where), (void)sizeof(what))
#  else
#    define PK_TILES_ASSERT(condition) \
        ((condition) ? static_cast<void>(0) : std::abort())
#    define PK_TILES_ASSERT_X(condition, where, what) \
        ((condition) ? static_cast<void>(0) : \
         (std::fprintf(stderr, "%s: %s\n", (where), (what)), std::abort()))
#  endif
#endif

#include <list>
#include "kritaimage_export.h"

#define MiB (1ULL << 20)

#define DEFAULT_STORE_SIZE (4096*MiB)
#define DEFAULT_SLAB_SIZE (64*MiB)

// Qt's qint64/quint64 are long long on the supported LP64 build.  Keep the
// exported tiles3 signatures ABI-identical instead of inheriting libc's
// platform-dependent std::(u)int64_t spelling.
using PkTilesQuint64 = unsigned long long;
using PkTilesQint64 = long long;
static_assert(std::is_same<PkTilesQuint64, unsigned long long>::value, "quint64 ABI");
static_assert(std::is_same<PkTilesQint64, long long>::value, "qint64 ABI");


//#define DEBUG_SLAB_FAILS

#ifdef DEBUG_SLAB_FAILS

#define WINDOW_SIZE 2000
#define DECLARE_FAIL_COUNTER() unsigned long long __failCount
#define INIT_FAIL_COUNTER() __failCount = 0
#define START_COUNTING() unsigned long long __numSteps = 0
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
    KisChunkData(PkTilesQuint64 begin, PkTilesQuint64 size)
    {
        setChunk(begin, size);
    }

    inline void setChunk(PkTilesQuint64 begin, PkTilesQuint64 size) {
        m_begin = begin;
        m_end = begin + size - 1;
    }

    inline PkTilesQuint64 size() const {
        return m_end - m_begin +1;
    }

    bool operator== (const KisChunkData& other) const
    {
        PK_TILES_ASSERT(m_begin!=other.m_begin || m_end==other.m_end);

        /**
         * Chunks cannot overlap, so it is enough to check
         * the beginning of the interval only
         */
        return m_begin == other.m_begin;
    }

    PkTilesQuint64 m_begin;
    PkTilesQuint64 m_end;
};

class KRITAIMAGE_EXPORT KisChunk
{
public:
    KisChunk() {}

    KisChunk(KisChunkDataListIterator iterator)
        : m_iterator(iterator)
    {
    }

    inline PkTilesQuint64 begin() const {
        return m_iterator->m_begin;
    }

    inline PkTilesQuint64 end() const {
        return m_iterator->m_end;
    }

    inline PkTilesQuint64 size() const {
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
    KisChunkAllocator(PkTilesQuint64 slabSize = DEFAULT_SLAB_SIZE,
                      PkTilesQuint64 storeSize = DEFAULT_STORE_SIZE);
    ~KisChunkAllocator();

    inline PkTilesQuint64 numChunks() const {
        return m_list.size();
    }

    KisChunk getChunk(PkTilesQuint64 size);
    void freeChunk(KisChunk chunk);

    void debugChunks();
    bool sanityCheck(bool pleaseCrash = true);
    double debugFragmentation(bool toStderr = true);

private:
    bool tryInsertChunk(KisChunkDataList &list,
                        KisChunkDataListIterator &iterator,
                        PkTilesQuint64 size);

private:
    PkTilesQuint64 m_storeMaxSize;
    PkTilesQuint64 m_storeSlabSize;


    KisChunkDataList m_list;
    KisChunkDataListIterator m_iterator;
    PkTilesQuint64 m_storeSize;
    DECLARE_FAIL_COUNTER()
};

#endif /* __KIS_CHUNK_ALLOCATOR_H */
