/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISDABCACHEUTILS_H
#define KISDABCACHEUTILS_H

#include <PkRect.h>
#include <PkSize.h>

#include "kis_types.h"

#include <KoColor.h>
#include <kis_paint_information.h>
#include <KisMirrorProperties.h>
#include "kis_dab_shape.h"

#include "kritapaintop_export.h"
#include <functional>
#include <memory>

class KisBrush;
typedef PkSharedPointer<KisBrush> KisBrushSP;

class KisColorSource;
class KisSharpnessOption;
class KisTextureOption;


namespace KisDabCacheUtils
{

struct PAINTOP_EXPORT DabRenderingResources
{
    DabRenderingResources();
    virtual ~DabRenderingResources();

    virtual void syncResourcesToSeqNo(int seqNo, const KisPaintInformation &info);

    KisBrushSP brush;
    std::unique_ptr<KisColorSource> colorSource;

    std::unique_ptr<KisSharpnessOption> sharpnessOption;
    std::unique_ptr<KisTextureOption> textureOption;

    KisPaintDeviceSP colorSourceDevice;

private:
    DabRenderingResources(const DabRenderingResources &rhs) = delete;
};

typedef std::function<DabRenderingResources*()> ResourcesFactory;

struct PAINTOP_EXPORT DabRequestInfo
{
    DabRequestInfo(const KoColor &_color,
                   const PkPointF &_cursorPoint,
                   const KisDabShape &_shape,
                   const KisPaintInformation &_info,
                   qreal _softnessFactor,
                   qreal _lightnessStrength = 1.0)
        : color(_color),
          cursorPoint(_cursorPoint),
          shape(_shape),
          info(_info),
          softnessFactor(_softnessFactor),
          lightnessStrength(_lightnessStrength)
    {
    }

    const KoColor &color;
    const PkPointF &cursorPoint;
    const KisDabShape &shape;
    const KisPaintInformation &info;
    const qreal softnessFactor;
    const qreal lightnessStrength;

private:
    DabRequestInfo(const DabRequestInfo &rhs);
};

struct PAINTOP_EXPORT DabGenerationInfo
{
    MirrorProperties mirrorProperties;
    KisDabShape shape;
    PkRect dstDabRect;
    PkPointF subPixel;
    bool solidColorFill = true;
    KoColor paintColor;
    KisPaintInformation info;
    qreal softnessFactor = 1.0;
    qreal lightnessStrength = 1.0;

    bool needsPostprocessing = false;
};

PAINTOP_EXPORT PkRect correctDabRectWhenFetchedFromCache(const PkRect &dabRect,
                                                        const PkSize &realDabSize);

PAINTOP_EXPORT void generateDab(const DabGenerationInfo &di,
                                DabRenderingResources *resources,
                                KisFixedPaintDeviceSP *dab,
                                bool forceImageStamp = false);

PAINTOP_EXPORT void postProcessDab(KisFixedPaintDeviceSP dab,
                                   const PkPoint &dabTopLeft,
                                   const KisPaintInformation& info,
                                   DabRenderingResources *resources);

}

template<class T> class PkSharedPointer;
class KisDabRenderingJob;
typedef PkSharedPointer<KisDabRenderingJob> KisDabRenderingJobSP;

#endif // KISDABCACHEUTILS_H
