/*
 *  SPDX-FileCopyrightText: 2015 Jouni Pentikäinen <joupent@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_ANIMATION_FRAME_CACHE_H
#define KIS_ANIMATION_FRAME_CACHE_H

#include <PkImage.h>
#include <PkObject.h>
#include <PkRect.h>
#include <PkSharedPointer.h>
#include <PkScopedPointer.h>
#include <PkList.h>

#include "kritaanimation_export.h"
#include "kis_types.h"
#include "kis_shared.h"

class KisImage;
class KisImageAnimationInterface;
class KisTimeSpan;
class KisRegion;

class KisOpenGLUpdateInfo;
typedef KisSharedPtr<KisOpenGLUpdateInfo> KisOpenGLUpdateInfoSP;
class KisOpenGLUpdateInfoBuilder;

class KRITAANIMATION_EXPORT KisAnimationFrameCacheSource
{
public:
    virtual ~KisAnimationFrameCacheSource();

    /**
     * Returns a stable identity for the backing texture/cache instance.
     * Sources wrapping the same instance must return the same key, while
     * different canvas texture sets for one image must return different keys.
     */
    virtual const void *cacheKey() const = 0;
    virtual KisImageWSP image() const = 0;
    virtual KisOpenGLUpdateInfoBuilder &updateInfoBuilder() = 0;
    virtual KisOpenGLUpdateInfoSP fetchFrameData(const PkRect &rect, KisImageSP image) = 0;
    virtual void uploadFrameData(KisOpenGLUpdateInfoSP info) = 0;
};

using KisAnimationFrameCacheSourceSP = PkSharedPointer<KisAnimationFrameCacheSource>;

class KRITAANIMATION_EXPORT KisAnimationFrameCache : public PkObject, public KisShared
{
public:

    static KisAnimationFrameCacheSP getFrameCache(KisAnimationFrameCacheSourceSP source);
    static const PkList<KisAnimationFrameCache*> caches();
    static const KisAnimationFrameCacheSP cacheForImage(KisImageWSP image);

    explicit KisAnimationFrameCache(KisAnimationFrameCacheSourceSP source);
    ~KisAnimationFrameCache() override;

    PkImage getFrame(int time);
    bool uploadFrame(int time);

    bool shouldUploadNewFrame(int newTime, int oldTime) const;

    enum CacheStatus {
        Cached,
        Uncached,
    };

    CacheStatus frameStatus(int time) const;
    bool tryGlueSameFrames(const KisTimeSpan &range);


    KisImageWSP image();

    KisOpenGLUpdateInfoSP fetchFrameData(int time, KisImageSP image, const KisRegion &requestedRegion) const;
    void addConvertedFrameData(KisOpenGLUpdateInfoSP info, int time);

    /**
     * Drops all the frames with worse level of detail values than the current
     * desired level of detail.
     */
    void dropLowQualityFrames(const KisTimeSpan &range, const PkRect &regionOfInterest, const PkRect &minimalRect);

    bool framesHaveValidRoi(const KisTimeSpan &range, const PkRect &regionOfInterest);

    void changed();

private:

    struct Private;
    PkScopedPointer<Private> m_d;

private:
    void framesChanged(const KisTimeSpan &range, const PkRect &rect);
    void slotConfigChanged();
};

#endif
