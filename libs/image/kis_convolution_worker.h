/*
 *  SPDX-FileCopyrightText: 2005, 2008 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2010 Edward Apap <schumifer@hotmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_CONVOLUTION_WORKER_H
#define KIS_CONVOLUTION_WORKER_H

#include <KoUpdater.h>
#include <PkContainerAlgo.h>
#include <PkPoint.h>
#include <PkRect.h>
#include <PkSize.h>

#include "kis_iterator_ng.h"
#include "kis_repeat_iterators_pixel.h"
#include "kis_painter.h"
#include <PkBitArray.h>

struct StandardIteratorFactory {
    typedef KisHLineIteratorSP HLineIterator;
    typedef KisVLineIteratorSP VLineIterator;
    typedef KisHLineConstIteratorSP HLineConstIterator;
    typedef KisVLineConstIteratorSP VLineConstIterator;
    inline static KisHLineIteratorSP createHLineIterator(KisPaintDeviceSP src, qint32 x, qint32 y, qint32 w, const PkRect&) {
        return src->createHLineIteratorNG(x, y, w);
    }
    inline static KisVLineIteratorSP createVLineIterator(KisPaintDeviceSP src, qint32 x, qint32 y, qint32 h, const PkRect&) {
        return src->createVLineIteratorNG(x, y, h);
    }
    inline static KisHLineConstIteratorSP createHLineConstIterator(KisPaintDeviceSP src, qint32 x, qint32 y, qint32 w, const PkRect&) {
        return src->createHLineConstIteratorNG(x, y, w);
    }
    inline static KisVLineConstIteratorSP createVLineConstIterator(KisPaintDeviceSP src, qint32 x, qint32 y, qint32 h, const PkRect&) {
        return src->createVLineConstIteratorNG(x, y, h);
    }
};

struct RepeatIteratorFactory {
    typedef KisHLineIteratorSP HLineIterator;
    typedef KisVLineIteratorSP VLineIterator;
    typedef KisRepeatHLineConstIteratorSP HLineConstIterator;
    typedef KisRepeatVLineConstIteratorSP VLineConstIterator;
    inline static KisHLineIteratorSP createHLineIterator(KisPaintDeviceSP src, qint32 x, qint32 y, qint32 w, const PkRect&) {
        return src->createHLineIteratorNG(x, y, w);
    }
    inline static KisVLineIteratorSP createVLineIterator(KisPaintDeviceSP src, qint32 x, qint32 y, qint32 h, const PkRect&) {
        return src->createVLineIteratorNG(x, y, h);
    }
    inline static HLineConstIterator createHLineConstIterator(KisPaintDeviceSP src, qint32 x, qint32 y, qint32 w, const PkRect& _dataRect) {
        return src->createRepeatHLineConstIterator(x, y, w, _dataRect);
    }
    inline static VLineConstIterator createVLineConstIterator(KisPaintDeviceSP src, qint32 x, qint32 y, qint32 h, const PkRect& _dataRect) {
        return src->createRepeatVLineConstIterator(x, y, h, _dataRect);
    }
};

template <class _IteratorFactory_>
class KisConvolutionWorker
{
public:
    KisConvolutionWorker(KisPainter *painter, KoUpdater *progress)
    {
        m_painter = painter;
        m_progress = progress;
    }

    virtual ~KisConvolutionWorker()
    {
    }

    virtual void execute(const KisConvolutionKernelSP kernel, const KisPaintDeviceSP src, PkPoint srcPos, PkPoint dstPos, PkSize areaSize, const PkRect& dataRect) = 0;

protected:
    PkList<KoChannelInfo *> convolvableChannelList(const KisPaintDeviceSP src)
    {
        PkBitArray painterChannelFlags = m_painter->channelFlags();
        if (painterChannelFlags.isEmpty()) {
            painterChannelFlags = PkBitArray(src->colorSpace()->channelCount(), true);
        }
        Q_ASSERT(static_cast<quint32>(painterChannelFlags.size()) == src->colorSpace()->channelCount());
        const PkList<KoChannelInfo *> channelInfo = src->colorSpace()->channels();
        PkList<KoChannelInfo *> convChannelList;

        for (qint32 c = 0; c < channelInfo.count(); ++c) {
            if (painterChannelFlags.testBit(c)) {
                convChannelList.append(channelInfo[c]);
            }
        }

        return convChannelList;
    }

protected:
    KisPainter* m_painter;
    KoUpdater* m_progress;
};


#endif
