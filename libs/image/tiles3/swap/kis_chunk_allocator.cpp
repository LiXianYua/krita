/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cstdint>
#include <cassert>

#include "kis_debug.h"
#include "kis_chunk_allocator.h"
#include <iterator>


#define GAP_SIZE(low, high) ((high) - (low) > 0 ? (high) - (low) - 1 : 0)

#define HAS_NEXT(list,iter) ((iter)!=(list).end())
#define HAS_PREVIOUS(list,iter) ((iter)!=(list).begin())

#define PEEK_NEXT(iter) (*(iter))
#define PEEK_PREVIOUS(iter) (*std::prev(iter))
#define WRAP_PREVIOUS_CHUNK_DATA(iter) (KisChunk(std::prev(iter)))


KisChunkAllocator::KisChunkAllocator(PkTilesQuint64 slabSize, PkTilesQuint64 storeSize)
{
    m_storeMaxSize = storeSize;
    m_storeSlabSize = slabSize;

    m_iterator = m_list.begin();
    m_storeSize = m_storeSlabSize;
    INIT_FAIL_COUNTER();
}

KisChunkAllocator::~KisChunkAllocator()
{
}

KisChunk KisChunkAllocator::getChunk(PkTilesQuint64 size)
{
    KisChunkDataListIterator startPosition = m_iterator;
    START_COUNTING();

    for(;;) {
        if(tryInsertChunk(m_list, m_iterator, size))
            return WRAP_PREVIOUS_CHUNK_DATA(m_iterator);

        if(m_iterator == m_list.end())
            break;

        m_iterator++;
        REGISTER_STEP();
    }

    REGISTER_FAIL();
    m_iterator = m_list.begin();

    for(;;) {
        if(tryInsertChunk(m_list, m_iterator, size))
            return WRAP_PREVIOUS_CHUNK_DATA(m_iterator);

        if(m_iterator == m_list.end() || m_iterator == startPosition)
            break;

        m_iterator++;
        REGISTER_STEP();
    }

    REGISTER_FAIL();
    m_iterator = m_list.end();

    while ((m_storeSize += m_storeSlabSize) <= m_storeMaxSize) {
        if(tryInsertChunk(m_list, m_iterator, size))
            return WRAP_PREVIOUS_CHUNK_DATA(m_iterator);
    }

    qFatal("KisChunkAllocator: out of swap space");

    // just let gcc be happy! :)
    return KisChunk(m_list.end());
}

bool KisChunkAllocator::tryInsertChunk(KisChunkDataList &list,
                                       KisChunkDataListIterator &iterator,
                                       PkTilesQuint64 size)
{
    bool result = false;
    PkTilesQuint64 highBound = m_storeSize;
    PkTilesQuint64 lowBound = 0;
    PkTilesQuint64 shift = 0;

    if(HAS_NEXT(list, iterator))
        highBound = PEEK_NEXT(iterator).m_begin;

    if(HAS_PREVIOUS(list, iterator)) {
        lowBound = PEEK_PREVIOUS(iterator).m_end;
        shift = 1;
    }

    if(GAP_SIZE(lowBound, highBound) >= size) {
        list.insert(iterator, KisChunkData(lowBound + shift, size));
        result = true;
    }

    return result;
}

void KisChunkAllocator::freeChunk(KisChunk chunk)
{
    if(m_iterator != m_list.end() && m_iterator == chunk.position()) {
        m_iterator = m_list.erase(m_iterator);
        return;
    }

    PK_TILES_ASSERT(chunk.position()->m_begin == chunk.begin());
    m_list.erase(chunk.position());
}



/**************************************************************/
/*******             Debugging features                ********/
/**************************************************************/


void KisChunkAllocator::debugChunks()
{
    PkTilesQuint64 idx = 0;
    KisChunkDataListIterator i;

    for(i = m_list.begin(); i != m_list.end(); ++i) {
        infoTiles << "chunk #" << idx++ << ": [" << i->m_begin << i->m_end << "]";
    }
}

bool KisChunkAllocator::sanityCheck(bool pleaseCrash)
{
    bool failed = false;
    KisChunkDataListIterator i;

    for(i = m_list.begin(); i != m_list.end(); ++i) {
        if(HAS_PREVIOUS(m_list, i)) {
            if(PEEK_PREVIOUS(i).m_end >= i->m_begin) {
                qWarning("Chunks overlapped: [%lld %lld], [%lld %lld]", PEEK_PREVIOUS(i).m_begin, PEEK_PREVIOUS(i).m_end, i->m_begin, i->m_end);
                failed = true;
                break;
            }
        }
    }

    i = m_list.end();
    if(HAS_PREVIOUS(m_list, i)) {
        if(PEEK_PREVIOUS(i).m_end >= m_storeSize) {
            warnKrita << "Last chunk exceeds the store size!";
            failed = true;
        }
    }

    if(failed && pleaseCrash)
        qFatal("KisChunkAllocator: sanity check failed!");

    return !failed;
}

double KisChunkAllocator::debugFragmentation(bool toStderr)
{
    KisChunkDataListIterator i;

    PkTilesQuint64 totalSize = 0;
    PkTilesQuint64 allocated = 0;
    PkTilesQuint64 free = 0;
    double fragmentation = 0;

    for(i = m_list.begin(); i != m_list.end(); ++i) {
        allocated += i->m_end - i->m_begin + 1;

        if(HAS_PREVIOUS(m_list, i))
            free += GAP_SIZE(PEEK_PREVIOUS(i).m_end, i->m_begin);
        else
            free += i->m_begin;
    }

    i = m_list.end();
    if(HAS_PREVIOUS(m_list, i))
        totalSize = PEEK_PREVIOUS(i).m_end + 1;

    if(totalSize)
        fragmentation = double(free) / totalSize;

    if(toStderr) {
        infoTiles << "Hard store limit:\t" << m_storeMaxSize;
        infoTiles << "Slab size:\t\t" << m_storeSlabSize;
        infoTiles << "Num slabs:\t\t" << m_storeSize / m_storeSlabSize;
        infoTiles << "Store size:\t\t" << m_storeSize;
        infoTiles << "Total used:\t\t" << totalSize;
        infoTiles << "Allocated:\t\t" << allocated;
        infoTiles << "Free:\t\t\t" << free;
        infoTiles << "Fragmentation:\t\t" << fragmentation;
        DEBUG_FAIL_COUNTER();
    }

    PK_TILES_ASSERT(totalSize == allocated + free);

    return fragmentation;
}
