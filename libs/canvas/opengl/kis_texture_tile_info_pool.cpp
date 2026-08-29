/*
 *  SPDX-FileCopyrightText: 2017 Bernhard Liebl <poke1024@gmx.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_texture_tile_info_pool.h"

KisTextureTileInfoPoolWorker::KisTextureTileInfoPoolWorker(KisTextureTileInfoPool *pool)
    : m_pool(pool)
    , m_compressor(1000, KisSignalCompressor::POSTPONE)
{
    m_compressorConnection =
        PkObject::connect(&m_compressor, &KisSignalCompressor::timeout,
                          &m_compressorReceiver,
                          [this]() { slotDelayedPurge(); });
}

KisTextureTileInfoPoolWorker::~KisTextureTileInfoPoolWorker()
{
    PkObject::disconnect(m_compressorConnection);
}

void KisTextureTileInfoPoolWorker::slotPurge(int pixelSize, int numFrees)
{
    m_compressorReceiver.moveToThread(PkThread::currentThreadId());
    m_purge[pixelSize] = numFrees;
    m_compressor.start();
}

void KisTextureTileInfoPoolWorker::slotDelayedPurge()
{
    for (QMap<int, int>::const_iterator i = m_purge.constBegin(); i != m_purge.constEnd(); i++) {
        m_pool->tryPurge(i.key(), i.value());
    }

    m_purge.clear();
}
