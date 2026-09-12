/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_UPDATE_INFO_H_
#define KIS_UPDATE_INFO_H_

#include <PkRect.h>

#include "kis_image_patch.h"
#include "kis_shared.h"
#include "kritacanvas_export.h"
#include "opengl/kis_texture_tile_update_info.h"

#include "kis_ui_types.h"

class KRITACANVAS_EXPORT KisUpdateInfo : public KisShared
{
public:
    KisUpdateInfo();
    virtual ~KisUpdateInfo();

    virtual PkRect dirtyViewportRect();
    virtual PkRect dirtyImageRect() const = 0;
    virtual int levelOfDetail() const = 0;
    virtual bool canBeCompressed() const;
};

struct ConversionOptions {
    ConversionOptions() : m_needsConversion(false) {}
    ConversionOptions(const KoColorSpace *destinationColorSpace,
                      KoColorConversionTransformation::Intent renderingIntent,
                      KoColorConversionTransformation::ConversionFlags conversionFlags)
        : m_needsConversion(true),
          m_destinationColorSpace(destinationColorSpace),
          m_renderingIntent(renderingIntent),
          m_conversionFlags(conversionFlags)
    {
    }


    bool m_needsConversion {false};
    const KoColorSpace *m_destinationColorSpace {0};
    KoColorConversionTransformation::Intent m_renderingIntent {KoColorConversionTransformation::IntentPerceptual};
    KoColorConversionTransformation::ConversionFlags m_conversionFlags {KoColorConversionTransformation::Empty};
};

class KisOpenGLUpdateInfo;
typedef KisSharedPtr<KisOpenGLUpdateInfo> KisOpenGLUpdateInfoSP;

class KRITACANVAS_EXPORT KisOpenGLUpdateInfo : public KisUpdateInfo
{
public:
    KisOpenGLUpdateInfo();

    KisTextureTileUpdateInfoSPList tileList;

    PkRect dirtyViewportRect() override;
    PkRect dirtyImageRect() const override;

    void assignDirtyImageRect(const PkRect &rect);
    void assignLevelOfDetail(int lod);

    int levelOfDetail() const override;

    bool tryMergeWith(const KisOpenGLUpdateInfo& rhs);

private:
    PkRect m_dirtyImageRect;
    int m_levelOfDetail;
};


class KRITACANVAS_EXPORT KisPPUpdateInfo : public KisUpdateInfo
{
public:
    enum TransferType {
        DIRECT,
        PATCH
    };

    PkRect dirtyViewportRect() override;
    PkRect dirtyImageRect() const override;
    int levelOfDetail() const override;

    /**
     * The rect that was reported by KisImage as dirty
     */
    PkRect dirtyImageRectVar;

    /**
     * Rect of KisImage corresponding to @ref viewportRect .
     * It is cropped and aligned corresponding to the canvas.
     */
    PkRect imageRect;

    /**
     * Rect of canvas widget corresponding to @ref imageRect
     */
    PkRectF viewportRect;

    qreal scaleX;
    qreal scaleY;

    /**
     * Defines the way the source image is painted onto
     * prescaled PkImage
     */
    TransferType transfer;

    /**
     * Render hints for painting the direct painting/patch painting
     */
    unsigned renderHints {0};

    /**
     * The number of additional pixels those should be added
     * to the patch
     */
    qint32 borderWidth;

    /**
     * Used for temporary storage of KisImage's data
     * by KisProjectionCache
     */
    KisImagePatch patch;
};

class KRITACANVAS_EXPORT KisMarkerUpdateInfo : public KisUpdateInfo
{
public:
    enum Type {
        StartBatch = 0,
        EndBatch,
        BlockLodUpdates,
        UnblockLodUpdates,
    };

public:
    KisMarkerUpdateInfo(Type type, const PkRect &dirtyImageRect);

    Type type() const;

    PkRect dirtyImageRect() const override;
    int levelOfDetail() const override;
    bool canBeCompressed() const override;

private:
    Type m_type;
    PkRect m_dirtyImageRect;
};

#endif /* KIS_UPDATE_INFO_H_ */
