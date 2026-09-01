/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2009 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "kis_prescaled_projection.h"

#include <math.h>

#include <QImage>
#include <QBitArray>
#include <QColor>
#include <QRect>
#include <QPoint>
#include <QSharedPointer>
#include <QSize>
#include <QVector>
#include <QPainter>

#include <PkObject.h>
#include <PkRect.h>
#include <PkSize.h>
#include <PkVector.h>

#include <KoColorProfile.h>
#include <KoViewConverter.h>

#include "kis_image_config.h"
#include "kis_config_notifier.h"
#include "kis_image.h"
#include "krita_utils.h"

#include "kis_coordinates_converter.h"

// This consumer uses only the generic update-info interfaces. Avoid instantiating
// the unrelated OpenGL tile implementation whose producer-native private types
// are intentionally outside R-44's writable closure.
class KisTextureTileUpdateInfo;
typedef QSharedPointer<KisTextureTileUpdateInfo> KisTextureTileUpdateInfoSP;
typedef QVector<KisTextureTileUpdateInfoSP> KisTextureTileUpdateInfoSPList;
#define KIS_TEXTURE_TILE_UPDATE_INFO_H_
#include "kis_projection_backend.h"
#include "kis_image_pyramid.h"
#include "kis_display_filter.h"
#include <KisDisplayConfig.h>

#include <KisCanvasState.h>

namespace {

PkRect toPkRect(const QRect &rect)
{
    return PkRect(rect.x(), rect.y(), rect.width(), rect.height());
}

PkSize toPkSize(const QSize &size)
{
    return PkSize(size.width(), size.height());
}

QRect toQRect(const PkRect &rect)
{
    return QRect(rect.x(), rect.y(), rect.width(), rect.height());
}

/**
 * RelevantCanvasState represents a part of the canvas state
 * which is relevant for the prescaled projection. Basically,
 * if this structure changes, then the prescaled projection
 * needs an update.
 */
struct RelevantCanvasState
{
    qreal zoom = 1.0;
    qreal rotation = 0.0;
    QPointF viewportOffsetF;

    std::tuple<qreal,qreal> transformations() const {
        return {zoom, rotation};
    }

    bool operator==(const RelevantCanvasState& other) const {
        return qFuzzyCompare(zoom, other.zoom) &&
               qFuzzyCompare(rotation, other.rotation) &&
               viewportOffsetF == other.viewportOffsetF;
    }

    bool operator!=(const RelevantCanvasState& other) const {
        return !(*this == other);
    }

    static RelevantCanvasState fromCanvasState(const KisCanvasState &state) {
        return {state.effectiveZoom, state.rotation, state.viewportOffsetF};
    }
};
}

#define ceiledSize(sz) QSize(ceil((sz).width()), ceil((sz).height()))

inline void copyQImageBuffer(uchar* dst, const uchar* src , qint32 deltaX, qint32 width)
{
    if (deltaX >= 0) {
        memcpy(dst + 4 * deltaX, src, 4 *(width - deltaX) * sizeof(uchar));
    } else {
        memcpy(dst, src - 4 * deltaX, 4 *(width + deltaX) * sizeof(uchar));
    }
}

void copyQImage(qint32 deltaX, qint32 deltaY, QImage* dstImage, const QImage& srcImage)
{
    qint32 height = dstImage->height();
    qint32 width = dstImage->width();
    Q_ASSERT(dstImage->width() == srcImage.width() && dstImage->height() == srcImage.height());
    if (deltaY >= 0) {
        for (int y = 0; y < height - deltaY; y ++) {
            const uchar* src = srcImage.scanLine(y);
            uchar* dst = dstImage->scanLine(y + deltaY);
            copyQImageBuffer(dst, src, deltaX, width);
        }
    } else {
        for (int y = 0; y < height + deltaY; y ++) {
            const uchar* src = srcImage.scanLine(y - deltaY);
            uchar* dst = dstImage->scanLine(y);
            copyQImageBuffer(dst, src, deltaX, width);
        }
    }
}

struct KisPrescaledProjection::Private {
    Private()
        : viewportSize(0, 0)
        , projectionBackend(0) {
    }

    QImage prescaledQImage;

    std::optional<RelevantCanvasState> currentRelevantCanvasState;
    QSize updatePatchSize;
    QSize canvasSize;
    QSize viewportSize;
    KisImageWSP image;
    KisCoordinatesConverter *coordinatesConverter {0};
    KisProjectionBackend *projectionBackend {0};
};

KisPrescaledProjection::KisPrescaledProjection()
        : QObject(0)
        , m_d(new Private())
{
    updateSettings();

    // we disable building the pyramid with setting its height to 1
    // XXX: setting it higher than 1 is broken because it's not updated until you show/hide the layer
    m_d->projectionBackend = new KisImagePyramid(1);

    KisConfigNotifier *notifier = KisConfigNotifier::instance();
    PkConnection configConnection = PkObject::connect(
        notifier, &KisConfigNotifier::configChanged, notifier,
        [this]() { updateSettings(); });
    QObject::connect(this, &QObject::destroyed,
                     [configConnection](QObject *) mutable {
                         PkObject::disconnect(configConnection);
                     });
}

KisPrescaledProjection::~KisPrescaledProjection()
{
    delete m_d->projectionBackend;
    delete m_d;
}


void KisPrescaledProjection::setImage(KisImageWSP image)
{
    Q_ASSERT(image);
    m_d->image = image;
    m_d->projectionBackend->setImage(image);
}

QImage KisPrescaledProjection::prescaledQImage() const
{
    return m_d->prescaledQImage;
}

void KisPrescaledProjection::setCoordinatesConverter(KisCoordinatesConverter *coordinatesConverter)
{
    m_d->coordinatesConverter = coordinatesConverter;
    m_d->currentRelevantCanvasState = RelevantCanvasState::fromCanvasState(KisCanvasState::fromConverter(*coordinatesConverter));
}

void KisPrescaledProjection::updateSettings()
{
    KisImageConfig imageConfig(false);
    m_d->updatePatchSize.setWidth(imageConfig.updatePatchWidth());
    m_d->updatePatchSize.setHeight(imageConfig.updatePatchHeight());
}

void KisPrescaledProjection::notifyCanvasStateChanged(const KisCanvasState &state)
{
    auto relevantState = RelevantCanvasState::fromCanvasState(state);

    if (m_d->currentRelevantCanvasState == relevantState) return;

    if (!m_d->currentRelevantCanvasState ||
        m_d->currentRelevantCanvasState->transformations() != relevantState.transformations()) {

        updateViewportSize();
        preScale();
    } else {
        const QPointF moveOffset = m_d->currentRelevantCanvasState->viewportOffsetF - relevantState.viewportOffsetF;
        viewportMoved(-moveOffset);
    }

    m_d->currentRelevantCanvasState = relevantState;
}

void KisPrescaledProjection::viewportMoved(const QPointF &offset)
{
    // FIXME: \|/
    if (m_d->prescaledQImage.isNull()) return;
    if (offset.isNull()) return;

    QPoint alignedOffset = offset.toPoint();

    if(offset != alignedOffset) {
        /**
         * We can't optimize anything when offset is float :(
         * Just prescale entire image.
         */
        dbgRender << "prescaling the entire image because the offset is float";
        preScale();
        return;
    }

    QImage newImage = QImage(m_d->viewportSize, QImage::Format_ARGB32);
    newImage.fill(0);

    /**
     * TODO: viewport rects should be cropped by the borders of
     * the image, because it may be requested to read/write
     * outside QImage and copyQImage will not catch it
     */
    QRect newViewportRect = QRect(QPoint(0,0), m_d->viewportSize);
    QRect oldViewportRect = newViewportRect.translated(alignedOffset);

    QRegion updateRegion = newViewportRect;
    QRect savedArea = newViewportRect & oldViewportRect;
    if(!savedArea.isEmpty()) {
        copyQImage(alignedOffset.x(), alignedOffset.y(), &newImage, m_d->prescaledQImage);
        updateRegion -= savedArea;
    }

    QPainter gc(&newImage);
    auto rc = updateRegion.begin();
    while (rc != updateRegion.end()) {
        QRect rect = *rc;
        QRect imageRect =
            m_d->coordinatesConverter->viewportToImage(rect).toAlignedRect();
        const PkVector<PkRect> patches = KritaUtils::splitRectIntoPatches(
            toPkRect(imageRect), toPkSize(m_d->updatePatchSize));

        Q_FOREACH (const PkRect &pkRect, patches) {
            const QRect rc = toQRect(pkRect);
            QRect viewportPatch =
                m_d->coordinatesConverter->imageToViewport(rc).toAlignedRect();

            KisPPUpdateInfoSP info = getInitialUpdateInformation(QRect());
            fillInUpdateInformation(viewportPatch, info);
            drawUsingBackend(gc, info);
        }
        rc++;
    }

    m_d->prescaledQImage = newImage;
}

void KisPrescaledProjection::slotImageSizeChanged(qint32 w, qint32 h)
{
    m_d->projectionBackend->setImageSize(w, h);
    // viewport size is cropped by the size of the image
    // so we need to update it as well
    updateViewportSize();
}

KisUpdateInfoSP KisPrescaledProjection::updateCache(const QRect &dirtyImageRect)
{
    if (!m_d->image) {
        dbgRender.noquote() << "Calling updateCache without an image:" << kisBacktrace();
        // return invalid info
        return new KisPPUpdateInfo();
    }

    /**
     * We needn't this stuff outside KisImage's area. We're not displaying
     * anything painted outside the image anyway.
     */
    QRect croppedImageRect = dirtyImageRect & toQRect(m_d->image->bounds());
    if (croppedImageRect.isEmpty()) return new KisPPUpdateInfo();

    KisPPUpdateInfoSP info = getInitialUpdateInformation(croppedImageRect);
    m_d->projectionBackend->updateCache(croppedImageRect);

    return info;
}

void KisPrescaledProjection::recalculateCache(KisUpdateInfoSP info)
{
    KisPPUpdateInfoSP ppInfo = dynamic_cast<KisPPUpdateInfo*>(info.data());
    if(!ppInfo) return;

    QRect rawViewRect =
        m_d->coordinatesConverter->
        imageToViewport(ppInfo->dirtyImageRectVar).toAlignedRect();

    fillInUpdateInformation(rawViewRect, ppInfo);

    m_d->projectionBackend->recalculateCache(ppInfo);

    if(!info->dirtyViewportRect().isEmpty())
        updateScaledImage(ppInfo);
}

void KisPrescaledProjection::preScale()
{
    if (!m_d->image) return;

    m_d->prescaledQImage.fill(0);

    QRect viewportRect(QPoint(0, 0), m_d->viewportSize);
    QRect imageRect =
        m_d->coordinatesConverter->viewportToImage(viewportRect).toAlignedRect();

    const PkVector<PkRect> patches = KritaUtils::splitRectIntoPatches(
        toPkRect(imageRect), toPkSize(m_d->updatePatchSize));

    Q_FOREACH (const PkRect &pkRect, patches) {
        const QRect rc = toQRect(pkRect);
        QRect viewportPatch = m_d->coordinatesConverter->imageToViewport(rc).toAlignedRect();
        KisPPUpdateInfoSP info = getInitialUpdateInformation(QRect());
        fillInUpdateInformation(viewportPatch, info);
        QPainter gc(&m_d->prescaledQImage);
        gc.setCompositionMode(QPainter::CompositionMode_Source);
        drawUsingBackend(gc, info);
    }

}

void KisPrescaledProjection::setDisplayConfig(const KisDisplayConfig &config)
{
    m_d->projectionBackend->setMonitorProfile(config.profile, config.intent, config.conversionFlags);
}

void KisPrescaledProjection::setChannelFlags(const QBitArray &channelFlags)
{
    m_d->projectionBackend->setChannelFlags(channelFlags);
}

void KisPrescaledProjection::setDisplayFilter(QSharedPointer<KisDisplayFilter> displayFilter)
{
    m_d->projectionBackend->setDisplayFilter(displayFilter);
}


void KisPrescaledProjection::updateViewportSize()
{
    QRect imageRect = m_d->coordinatesConverter->imageRectInWidgetPixels().toAlignedRect();
    QSizeF minimalSize(qMin(imageRect.width(), m_d->canvasSize.width()),
                       qMin(imageRect.height(), m_d->canvasSize.height()));
    QRectF minimalRect(QPointF(0,0), minimalSize);

    m_d->viewportSize = m_d->coordinatesConverter->widgetToViewport(minimalRect).toAlignedRect().size();

    if (m_d->prescaledQImage.isNull() ||
        m_d->prescaledQImage.size() != m_d->viewportSize) {

        m_d->prescaledQImage = QImage(m_d->viewportSize, QImage::Format_ARGB32);
        m_d->prescaledQImage.fill(0);
    }
}

void KisPrescaledProjection::notifyCanvasSizeChanged(const QSize &widgetSize)
{
    m_d->canvasSize = widgetSize;
    updateViewportSize();
    preScale();
}

KisPPUpdateInfoSP KisPrescaledProjection::getInitialUpdateInformation(const QRect &dirtyImageRect)
{
    /**
     * This update information has nothing more than an information
     * about dirty image rect. All the other information used for
     * scaling will be fetched in fillUpdateInformation() later,
     * when we are working in the context of the UI thread
     */

    KisPPUpdateInfoSP info = new KisPPUpdateInfo();
    info->dirtyImageRectVar = dirtyImageRect;

    return info;
}

void KisPrescaledProjection::fillInUpdateInformation(const QRect &viewportRect,
                                                     KisPPUpdateInfoSP info)
{
    m_d->coordinatesConverter->imageScale(&info->scaleX, &info->scaleY);

    // first, crop the part of the view rect that is outside of the canvas
    QRect croppedViewRect = viewportRect.intersected(QRect(QPoint(0, 0), m_d->viewportSize));

    // second, align this rect to the KisImage's pixels and pixels
    // of projection backend.
    info->imageRect = m_d->coordinatesConverter->viewportToImage(QRectF(croppedViewRect)).toAlignedRect();

    /**
     * To avoid artifacts while scaling we use mechanism like
     * changeRect/needRect for layers. Here we grow the rect to update
     * pixels which depend on the dirty rect (like changeRect), and
     * later we request a bit more pixels for the patch to make the
     * scaling safe (like needRect).
     */
    const int borderSize = BORDER_SIZE(qMax(info->scaleX, info->scaleY));
    info->imageRect.adjust(-borderSize, -borderSize, borderSize, borderSize);

    info->imageRect = info->imageRect & toQRect(m_d->image->bounds());

    m_d->projectionBackend->alignSourceRect(info->imageRect, info->scaleX);

    // finally, compute the dirty rect of the canvas
    info->viewportRect = m_d->coordinatesConverter->imageToViewport(info->imageRect);

    info->borderWidth = 0;
    if (SCALE_MORE_OR_EQUAL_TO(info->scaleX, info->scaleY, 1.0)) {
        if (SCALE_LESS_THAN(info->scaleX, info->scaleY, 2.0)) {
            dbgRender << "smoothBetween100And200Percent";
            info->renderHints = QPainter::SmoothPixmapTransform;
            info->borderWidth = borderSize;
        }
        info->transfer = KisPPUpdateInfo::DIRECT;
    } else { // <100%
        info->renderHints = QPainter::SmoothPixmapTransform;
        info->borderWidth = borderSize;
        info->transfer = KisPPUpdateInfo::PATCH;
    }

    dbgRender << "#####################################";
    dbgRender << ppVar(info->scaleX) << ppVar(info->scaleY);
    dbgRender << ppVar(info->borderWidth) << ppVar(info->renderHints);
    dbgRender << ppVar(info->transfer);
    dbgRender << ppVar(info->dirtyImageRectVar);
    dbgRender << "Not aligned rect of the canvas (raw):\t" << croppedViewRect;
    dbgRender << "Update rect in KisImage's pixels:\t" << info->imageRect;
    dbgRender << "Update rect in canvas' pixels:\t" << info->viewportRect;
    dbgRender << "#####################################";
}

void KisPrescaledProjection::updateScaledImage(KisPPUpdateInfoSP info)
{
    QPainter gc(&m_d->prescaledQImage);
    gc.setCompositionMode(QPainter::CompositionMode_Source);
    drawUsingBackend(gc, info);
}

void KisPrescaledProjection::drawUsingBackend(QPainter &gc, KisPPUpdateInfoSP info)
{
    if (info->imageRect.isEmpty()) return;

    if (info->transfer == KisPPUpdateInfo::DIRECT) {
        m_d->projectionBackend->drawFromOriginalImage(gc, info);
    } else /* if info->transfer == KisPPUpdateInformation::PATCH */ {
        KisImagePatch patch = m_d->projectionBackend->getNearestPatch(info);
        // prescale the patch because otherwise we'd scale using QPainter, which gives
        // a crap result compared to QImage's smoothscale
        patch.preScale(info->viewportRect);
        patch.drawMe(gc, info->viewportRect, info->renderHints);
    }
}
