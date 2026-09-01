/*
 *  SPDX-FileCopyrightText: 2018 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISOPENGLUPDATEINFOBUILDER_H
#define KISOPENGLUPDATEINFOBUILDER_H

#include "kritacanvas_export.h"
#include <QBitArray>
#include <QRect>
#include <QScopedPointer>
#include <QSize>

#include <PkRect.h>
#include <PkSharedPointer.h>

#include "kis_types.h"

class KisTextureTileInfoPool;
typedef PkSharedPointer<KisTextureTileInfoPool> KisTextureTileInfoPoolSP;

class KisOpenGLUpdateInfo;
typedef KisSharedPtr<KisOpenGLUpdateInfo> KisOpenGLUpdateInfoSP;

class KoColorSpace;
struct ConversionOptions;


class KRITACANVAS_EXPORT KisOpenGLUpdateInfoBuilder
{
public:
    KisOpenGLUpdateInfoBuilder();
    ~KisOpenGLUpdateInfoBuilder();

    KisOpenGLUpdateInfoSP buildUpdateInfo(const QRect& rect, KisImageSP srcImage, bool convertColorSpace);
    KisOpenGLUpdateInfoSP buildUpdateInfo(const PkRect& rect, KisPaintDeviceSP projection, const PkRect &bounds, int levelOfDetail, bool convertColorSpace);

    PkRect calculatePhysicalTileRect(int col, int row, const PkRect &imageBounds, int levelOfDetail) const;
    QRect calculateEffectiveTileRect(int col, int row, const PkRect &imageBounds) const;
    int xToCol(int x) const;
    int yToRow(int y) const;

    const KoColorSpace* destinationColorSpace() const;

    void setConversionOptions(const ConversionOptions &options);
    void setChannelFlags(const QBitArray &channelFrags, bool onlyOneChannelSelected, int selectedChannelIndex);

    void setTextureBorder(int value);
    void setEffectiveTextureSize(const QSize &size);

    void setTextureInfoPool(KisTextureTileInfoPoolSP pool);
    KisTextureTileInfoPoolSP textureInfoPool() const;

    void setProofingConfig(KisProofingConfigurationSP config);
    KisProofingConfigurationSP proofingConfig() const;

private:
    struct Private;
    const QScopedPointer<Private> m_d;
};

#endif // KISOPENGLUPDATEINFOBUILDER_H
