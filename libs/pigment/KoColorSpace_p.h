/*
 *  SPDX-FileCopyrightText: 2005 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me> *
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _KOCOLORSPACE_P_H_
#define _KOCOLORSPACE_P_H_

#include "KoColorSpace.h"
#include "KoColorSpaceEngine.h"
#include "KoColorConversionTransformation.h"
#include <PkThreadStorage.h>
#include <PkPolygon.h>
#include <PkStringHash.h>
#include <PkMap.h>

struct KoColorSpace::Private {
    /**
     * Returns the thread-local conversion cache. If it doesn't exist
     * yet, it is created. If it is currently too small, it is resized.
     */
    struct ThreadLocalCache {
        PkVector<quint8> * get(quint32 size)
        {
            PkVector<quint8> * ba = nullptr;
            if (!m_cache.hasLocalData()) {
                ba = new PkVector<quint8>(size, '0');
                m_cache.setLocalData(ba);
            } else {
                ba = m_cache.localData();
                if ((quint8)ba->size() < size)
                    ba->resize(size);
            }
            return ba;
        }
    private:
        PkThreadStorage<PkVector<quint8>> m_cache;
    };


    PkString id;
    quint32 idNumber;
    PkString name;
    PkHash<PkString, KoCompositeOp*> compositeOps;
    PkList<KoChannelInfo *> channels;
    KoMixColorsOp* mixColorsOp;
    KoConvolutionOp* convolutionOp;
    PkHash<PkString, PkMap<DitherType, KisDitherOp*>> ditherOps;

    mutable ThreadLocalCache conversionCache;
    mutable ThreadLocalCache channelFlagsApplicationCache;

    mutable KoColorConversionTransformation* transfoToRGBA16;
    mutable KoColorConversionTransformation* transfoFromRGBA16;
    mutable KoColorConversionTransformation* transfoToLABA16;
    mutable KoColorConversionTransformation* transfoFromLABA16;
    
    PkPolygonF gamutXYY;
    PkPolygonF TRCXYY;
    PkVector<qreal> colorants;
    PkVector<qreal> lumaCoefficients;

    KoColorSpaceEngine *iccEngine;

    Deletability deletability;
};

#endif
