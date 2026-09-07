/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2011 Silvio Heinrich <plassy@web.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cmath>

#include <QDebug>
#include <QPoint>
#include <pk/geometry/PkPoint.h>
#include <QRect>
#include <pk/geometry/PkRect.h>
#include <QSize>
#include <pk/geometry/PkSize.h>
#include <pk/geometry/PkTransform.h>
#include <QtMath>

#include "kis_coordinates_converter.h"
#include "KoViewTransformStillPoint.h"
#include <KoViewConverter.h>

#include "KisCanvasConfig.h"
#include <kis_image.h>
#include <kis_algebra_2d.h>
#include <kis_assert.h>
#include <KisValueCache.h>
#include <KisPortingUtils.h>
#include <PkFlakeBridge.h>


struct KisCoordinatesConverter::Private {
    Private():
        imageXRes(1.0),
        imageYRes(1.0),
        isXAxisMirrored(false),
        isYAxisMirrored(false),
        isRotating(false),
        isNativeGesture(false),
        rotationAngle(0.0),
        rotationBaseAngle(0.0),
        rotationIsOrthogonal(true),
        devicePixelRatio(1.0),
        standardZoomLevels(this)
    {
    }

    QRect imageBounds;
    QRect extraReferencesBounds;
    qreal imageXRes;
    qreal imageYRes;

    bool isXAxisMirrored;
    bool isYAxisMirrored;
    bool isRotating;
    bool isNativeGesture;
    qreal rotationAngle;
    qreal rotationBaseAngle;
    bool rotationIsOrthogonal;
    PkSizeF canvasWidgetSize;
    qreal devicePixelRatio;
    PkPointF documentOffset;
    PkPointF preferredTransformationCenterImage;
    QPoint minimumOffset;
    QPoint maximumOffset;

    qreal minZoom {0.01};
    qreal maxZoom {9.0};

    struct StandardZoomLevelsInitializer {
        StandardZoomLevelsInitializer(Private *d) : m_d(d) {}
        PkVector<qreal> initialize() const;
        Private *m_d;
    };

    KisValueCache<StandardZoomLevelsInitializer> standardZoomLevels;

    PkTransform flakeToWidget;
    PkTransform rotationBaseTransform;
    PkTransform imageToDocument;
    PkTransform documentToFlake;
    PkTransform widgetToViewport;

    PkPointF preferredTransformationCenterInDocumentPixels() const {
        return imageToDocument.map(preferredTransformationCenterImage);
    }
};

/**
 * When vastScrolling value is less than 0.5 it is possible
 * that the whole scrolling area (viewport) will be smaller than
 * the size of the widget. In such cases the image should be
 * centered in the widget. Previously we used a special parameter
 * documentOrigin for this purpose, now the value for this
 * centering is calculated dynamically, helping the offset to
 * center the image inside the widget
 *
 * Note that the correction is null when the size of the document
 * plus vast scrolling reserve is larger than the widget. This
 * is always true for vastScrolling parameter > 0.5.
 */

PkPointF KisCoordinatesConverter::centeringCorrection() const
{
    KisCanvasConfig cfg(true);

    QSize documentSize = toQSize(imageRectInWidgetPixels().toAlignedRect().size());
    PkPointF dPoint(documentSize.width(), documentSize.height());
    PkPointF wPoint(m_d->canvasWidgetSize.width(), m_d->canvasWidgetSize.height());

    PkPointF minOffset = -cfg.vastScrolling() * wPoint;
    PkPointF maxOffset = dPoint - wPoint + cfg.vastScrolling() * wPoint;

    PkPointF range = maxOffset - minOffset;

    range.rx() = pkMin(range.x(), (qreal)0.0);
    range.ry() = pkMin(range.y(), (qreal)0.0);

    range /= 2;

    return -range;
}

void KisCoordinatesConverter::recalculateOffsetBoundsAndCrop()
{
    if (!m_d->canvasWidgetSize.isValid()) return;

    KisCanvasConfig cfg(true);

    const QRect refRect = imageToWidget(m_d->extraReferencesBounds);

    QRect documentRect = toQRect(imageRectInWidgetPixels().toAlignedRect());
    PkPointF dPointMax(pkMax(documentRect.width(), refRect.right() + 1 - documentRect.x()),
                      pkMax(documentRect.height(),  refRect.bottom() + 1 - documentRect.y()));
    PkPointF dPointMin(pkMin(0, refRect.left() - documentRect.x()),
                      pkMin(0,  refRect.top() - documentRect.y()));
    PkPointF wPoint(m_d->canvasWidgetSize.width(), m_d->canvasWidgetSize.height());

    PkPointF minOffset = dPointMin - cfg.vastScrolling() * wPoint;
    PkPointF maxOffset = dPointMax - wPoint + cfg.vastScrolling() * wPoint;

    m_d->minimumOffset = toQPoint(minOffset.toPoint());
    m_d->maximumOffset = toQPoint(maxOffset.toPoint());

    const PkRectF limitRect(toPkPoint(m_d->minimumOffset), toPkPoint(m_d->maximumOffset));

    if (!limitRect.contains(m_d->documentOffset)) {
        m_d->documentOffset = snapToDevicePixel(KisAlgebra2D::clampPoint(m_d->documentOffset, limitRect));
        qDebug() << "    corrected offset:" << m_d->documentOffset;
        correctTransformationToOffset();
    }
}

QPoint KisCoordinatesConverter::minimumOffset() const
{
    return m_d->minimumOffset;
}

QPoint KisCoordinatesConverter::maximumOffset() const
{
    return m_d->maximumOffset;
}

/**
 * The document offset and the position of the top left corner of the
 * image must always coincide, that is why we need to correct them to
 * and fro.
 *
 * When we change zoom level, the calculation of the new offset is
 * done by KoCanvasControllerWidget, that is why we just passively fix
 * the flakeToWidget transform to conform the offset and wait until
 * the canvas controller will recenter us.
 *
 * But when we do our own transformations of the canvas, like rotation
 * and mirroring, we cannot rely on the centering of the canvas
 * controller and we do it ourselves. Then we just set new offset and
 * return its value to be set in the canvas controller explicitly.
 */

void KisCoordinatesConverter::correctOffsetToTransformationAndSnap()
{
    m_d->documentOffset = snapToDevicePixel(-(imageRectInWidgetPixels().topLeft() -
          centeringCorrection()));
}

void KisCoordinatesConverter::correctTransformationToOffset()
{
    PkPointF topLeft = imageRectInWidgetPixels().topLeft();
    PkPointF diff = (-topLeft) - m_d->documentOffset;
    diff += centeringCorrection();
    m_d->flakeToWidget *= PkTransform::fromTranslate(diff.x(), diff.y());
}

void KisCoordinatesConverter::resetPreferredTransformationCenter()
{
    m_d->preferredTransformationCenterImage = widgetToImage(this->widgetCenterPoint());
}

void KisCoordinatesConverter::recalculateTransformations()
{
    m_d->imageToDocument = PkTransform::fromScale(1 / m_d->imageXRes,
                                                 1 / m_d->imageYRes);

    qreal zoomX, zoomY;
    KoZoomHandler::zoom(&zoomX, &zoomY);
    m_d->documentToFlake = PkTransform::fromScale(zoomX, zoomY);

    correctTransformationToOffset();
    recalculateOffsetBoundsAndCrop();

    PkRectF irect = imageRectInWidgetPixels();
    PkRectF wrect = PkRectF(toPkPoint(QPoint(0,0)), toPkSizeF(m_d->canvasWidgetSize));
    PkRectF rrect = irect & wrect;

    PkTransform reversedTransform = flakeToWidgetTransform().inverted();
    PkRectF     canvasBounds      = reversedTransform.mapRect(rrect);
    PkPointF    offset            = canvasBounds.topLeft();

    m_d->widgetToViewport = reversedTransform * PkTransform::fromTranslate(-offset.x(), -offset.y());
}


KisCoordinatesConverter::KisCoordinatesConverter()
    : m_d(new Private) { }

KisCoordinatesConverter::~KisCoordinatesConverter()
{
    delete m_d;
}

PkSizeF KisCoordinatesConverter::getCanvasWidgetSize() const
{
    return m_d->canvasWidgetSize;
}

void KisCoordinatesConverter::setCanvasWidgetSize(PkSizeF size)
{
    m_d->canvasWidgetSize = snapWidgetSizeToDevicePixel(size);
    recalculateTransformations();

    // the widget center has changed, hence the preferred
    // center changes as well
    resetPreferredTransformationCenter();
}

void KisCoordinatesConverter::setDevicePixelRatio(qreal value)
{
    m_d->devicePixelRatio = value;
}

void KisCoordinatesConverter::setImage(KisImageWSP image)
{
    m_d->imageXRes = image->xRes();
    m_d->imageYRes = image->yRes();

    // we should **not** call setResolution() here, since
    // it is a different kind of resolution that is used
    // to convert the image to the physical size of the display

    const PkRect imageBounds = image->bounds();
    m_d->imageBounds = QRect(imageBounds.x(), imageBounds.y(),
                             imageBounds.width(), imageBounds.height());
    recalculateZoomLevelLimits();
    recalculateTransformations();

    if (m_d->canvasWidgetSize.isEmpty()) {
        // if setImage() comes before setCanvasWidgetSize(), then just remember the
        // proposed mode and postpone the actual recentering of the image
        // (this case is supposed to happen in unittests only)
        KoZoomHandler::setZoomMode(KoZoomMode::ZOOM_PAGE);
        m_d->preferredTransformationCenterImage = toPkPointF(m_d->imageBounds.center());
    } else {
        // the default mode after initialization is "Zoom Page"
        setZoom(KoZoomMode::ZOOM_PAGE, 777.7, resolutionX(), resolutionY(), std::nullopt);
    }
}

void KisCoordinatesConverter::setExtraReferencesBounds(const QRect &imageRect)
{
    if (imageRect == m_d->extraReferencesBounds) return;

    // this value affects scroll range only, so no need to do extra
    // still point tracking
    m_d->extraReferencesBounds = imageRect;
    recalculateTransformations();
}

void KisCoordinatesConverter::setImageBounds(const QRect &rect, const PkPointF oldImageStillPoint, const PkPointF newImageStillPoint)
{
    if (rect == m_d->imageBounds) return;

    const PkPointF oldWidgetStillPoint = imageToWidget(oldImageStillPoint);

    // we reset zoom mode to constant to make sure that
    // the image changes visually for the user
    setZoomMode(KoZoomMode::ZOOM_CONSTANT);

    m_d->imageBounds = rect;
    recalculateZoomLevelLimits();
    recalculateTransformations();

    const PkPointF newWidgetStillPoint = imageToWidget(newImageStillPoint);
    m_d->documentOffset = snapToDevicePixel(m_d->documentOffset + newWidgetStillPoint - oldWidgetStillPoint);
    recalculateTransformations();

    resetPreferredTransformationCenter();
}

void KisCoordinatesConverter::setImageResolution(qreal xRes, qreal yRes)
{
    // we consiter the center of the image to be the still point
    // on the canvas

    if (pkQtFuzzyCompare(xRes, m_d->imageXRes) && pkQtFuzzyCompare(yRes, m_d->imageYRes)) return;

    const PkPointF oldImageCenter = imageCenterInWidgetPixel();

    // we should **not** call setResolution() here, since
    // it is a different kind of resolution that is used
    // to convert the image to the physical size of the display

    // we reset zoom mode to constant to make sure that
    // the image changes visually for the user
    setZoomMode(KoZoomMode::ZOOM_CONSTANT);

    m_d->imageXRes = xRes;
    m_d->imageYRes = yRes;
    recalculateZoomLevelLimits();
    recalculateTransformations();

    const PkPointF newImageCenter = imageCenterInWidgetPixel();
    m_d->documentOffset = snapToDevicePixel(m_d->documentOffset + newImageCenter - oldImageCenter);
    recalculateTransformations();

    resetPreferredTransformationCenter();
}

void KisCoordinatesConverter::setDocumentOffset(const PkPointF& offset)
{
    // when changing the offset manually, the mode is explicitly
    // reset to constant
    setZoomMode(KoZoomMode::ZOOM_CONSTANT);

    // The given offset is in widget logical pixels. In order to prevent fuzzy
    // canvas rendering at 100% pixel-perfect zoom level when devicePixelRatio
    // is not integral, we adjusts the offset to map to whole device pixels.

    // Steps to reproduce the issue (when no snapping):
    // 1) Download an image with 1px vertical black and white stripes
    // 2) Enable fractional HiDPI support in Krita
    // 3) Set display scaling to 1.5 or 2.5
    // 4) Try to change offset of the image. If offset is unaligned, then
    //    the image will disappear on the canvas.

    m_d->documentOffset = snapToDevicePixel(offset);
    recalculateTransformations();

    resetPreferredTransformationCenter();
}

qreal KisCoordinatesConverter::devicePixelRatio() const
{
    return m_d->devicePixelRatio;
}

QPoint KisCoordinatesConverter::documentOffset() const
{
    return QPoint(int(m_d->documentOffset.x()), int(m_d->documentOffset.y()));
}

PkPointF KisCoordinatesConverter::documentOffsetF() const
{
    return m_d->documentOffset;
}

PkPointF KisCoordinatesConverter::preferredTransformationCenter() const
{
    return m_d->preferredTransformationCenterImage;
}

qreal KisCoordinatesConverter::rotationAngle() const
{
    return m_d->rotationAngle;
}

void KisCoordinatesConverter::setZoom(qreal zoom)
{
    // when changing the offset manually, the mode is explicitly
    // reset to constant, this method is used in unittests mostly
    setZoomMode(KoZoomMode::ZOOM_CONSTANT);

    KoZoomHandler::setZoom(zoom);
    recalculateTransformations();
    resetPreferredTransformationCenter();
}

void KisCoordinatesConverter::setCanvasWidgetSizeKeepZoom(const PkSizeF &size)
{
    setCanvasWidgetSize(size);

    if (zoomMode() == KoZoomMode::ZOOM_CONSTANT) {
        // in constant mode we just preserve the document offset
        // (as much as we can in relation to the vast scroll factor)
    } else {
        /**
         * WARNING: we can safely call setZoom() after changing widget size **only**
         * for non-constant modes, because they have no still points, they always
         * align to the center of the widget. Constant mode, reads the state of the
         * canvas before transformation to calculate the position of the still point,
         * hence we cannot change the state separately.
         */
        setZoom(zoomMode(), 777.0, resolutionX(), resolutionY(), std::nullopt);
    }
}

PkSizeF KisCoordinatesConverter::snapWidgetSizeToDevicePixel(const PkSizeF &size) const
{
    if (pkQtFuzzyCompare(m_d->devicePixelRatio, 1.0)) return size;

    // This is how QOpenGLCanvas sets the FBO and the viewport size. If
    // devicePixelRatioF() is non-integral, the result is truncated.
    // *Correction*: The FBO size is actually rounded, but the glViewport call
    // uses integer truncation and that's what really matters.
    const int viewportWidth = static_cast<int>(size.width() * m_d->devicePixelRatio);
    const int viewportHeight = static_cast<int>(size.height() * m_d->devicePixelRatio);

    // The widget size may be an integer but here we actually want to give
    // KisCoordinatesConverter the logical viewport size aligned to device
    // pixels.
    return PkSizeF(viewportWidth, viewportHeight) / m_d->devicePixelRatio;
}

QSize KisCoordinatesConverter::viewportDevicePixelSize() const
{
    // TODO: add an assert and a unittest to verify that there is no
    //       actual rounding happens, only intolerances!
    return toQSize(pkQtFuzzyCompare(m_d->devicePixelRatio, 1.0) ?
        m_d->canvasWidgetSize.toSize() :
        (m_d->canvasWidgetSize * m_d->devicePixelRatio).toSize());
}

void KisCoordinatesConverter::setZoom(KoZoomMode::Mode mode, qreal zoom, qreal resolutionX, qreal resolutionY, const std::optional<KoViewTransformStillPoint> &stillPoint)
{
    const int cfgMargin = zoomMarginSize();

    auto updateDisplayResolution = [&]() {
        if (!pkQtFuzzyCompare(resolutionX, this->resolutionX()) || !pkQtFuzzyCompare(resolutionY, this->resolutionY())) {
            setResolution(resolutionX, resolutionY);
            recalculateZoomLevelLimits();
        }
    };

    if(mode == KoZoomMode::ZOOM_CONSTANT) {
        if(pkQtFuzzyIsNull(zoom)) return;

        /// only constant mode is a subject for clamping,
        /// fit-modes are allowed to zoom as much as needed
        zoom = clampZoom(zoom);

        KoViewTransformStillPoint effectiveStillPoint =
            stillPoint ? *stillPoint :
            KoViewTransformStillPoint(m_d->preferredTransformationCenterInDocumentPixels(), widgetCenterPoint());

        updateDisplayResolution();
        KoZoomHandler::setZoom(zoom);
        KoZoomHandler::setZoomMode(mode);
        recalculateTransformations();

        const PkPointF newStillPoint = documentToWidget(effectiveStillPoint.docPoint());
        const PkPointF offset = newStillPoint - effectiveStillPoint.viewPoint();
        m_d->documentOffset = snapToDevicePixel(m_d->documentOffset + offset);
        recalculateTransformations();

        if (stillPoint) {
            resetPreferredTransformationCenter();
        }

    } else if (mode == KoZoomMode::ZOOM_PAGE || mode == KoZoomMode::ZOOM_WIDTH || mode == KoZoomMode::ZOOM_HEIGHT) {
        updateDisplayResolution();
        recalculateTransformations();

        KIS_SAFE_ASSERT_RECOVER_RETURN(!m_d->canvasWidgetSize.isEmpty());

        const PkSizeF documentSize = imageRectInWidgetPixels().size();
        const qreal zoomCoeffX = (m_d->canvasWidgetSize.width() - 2 * cfgMargin) / documentSize.width();
        const qreal zoomCoeffY = (m_d->canvasWidgetSize.height() - 2 * cfgMargin) / documentSize.height();

        const bool fitToWidth = [&]() {
            if (mode == KoZoomMode::ZOOM_PAGE) {
                return zoomCoeffX < zoomCoeffY;
            } else if (mode == KoZoomMode::ZOOM_HEIGHT) {
                return false;
            } else if (mode == KoZoomMode::ZOOM_WIDTH) {
                return true;
            }
            Q_UNREACHABLE_RETURN(true);
        }();

        KoZoomHandler::setZoom(this->zoom() * (fitToWidth ? zoomCoeffX : zoomCoeffY));
        KoZoomHandler::setZoomMode(mode);
        recalculateTransformations();

        const PkPointF offset = imageCenterInWidgetPixel() - widgetCenterPoint();

        PkPointF newDocumentOffset = m_d->documentOffset + offset;

        // just explicitly set minimal axis offset to zero to
        // avoid imperfections of floating point numbers
        if (fitToWidth) {
            newDocumentOffset.setX(-cfgMargin);
        } else {
            newDocumentOffset.setY(-cfgMargin);
        }

        m_d->documentOffset = snapToDevicePixel(newDocumentOffset);
        recalculateTransformations();

        resetPreferredTransformationCenter();
    }
}

void KisCoordinatesConverter::zoomTo(const PkRectF &zoomRectWidget)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(!zoomRectWidget.isEmpty());

    const PkPointF zoomPortionCenterInImagePixels = widgetToImage(zoomRectWidget.center());

    const qreal zoomCoeffX = m_d->canvasWidgetSize.width() / zoomRectWidget.width();
    const qreal zoomCoeffY = m_d->canvasWidgetSize.height() / zoomRectWidget.height();

    const bool fitToWidth = zoomCoeffX < zoomCoeffY;

    KoZoomHandler::setZoom(this->zoom() * (fitToWidth ? zoomCoeffX : zoomCoeffY));
    KoZoomHandler::setZoomMode(KoZoomMode::ZOOM_CONSTANT);
    recalculateTransformations();

    const PkPointF offset = imageToWidget(zoomPortionCenterInImagePixels) - widgetCenterPoint();
    PkPointF newDocumentOffset = m_d->documentOffset + offset;

    m_d->documentOffset = snapToDevicePixel(newDocumentOffset);
    recalculateTransformations();

    resetPreferredTransformationCenter();
}

qreal KisCoordinatesConverter::effectiveZoom() const
{
    qreal scaleX, scaleY;
    this->imageScale(&scaleX, &scaleY);

    if (scaleX != scaleY) {
        qWarning() << "WARNING: Zoom is not isotropic!"  << ppVar(scaleX) << ppVar(scaleY) << ppVar(pkQtFuzzyCompare(scaleX, scaleY));
    }

    // zoom by average of x and y
    return 0.5 * (scaleX + scaleY);
}

qreal KisCoordinatesConverter::effectivePhysicalZoom() const
{
    qreal scaleX, scaleY;
    this->imagePhysicalScale(&scaleX, &scaleY);

    if (scaleX != scaleY) {
        qWarning() << "WARNING: Zoom is not isotropic!"  << ppVar(scaleX) << ppVar(scaleY) << ppVar(pkQtFuzzyCompare(scaleX, scaleY));
    }

    // zoom by average of x and y
    return 0.5 * (scaleX + scaleY);
}

void KisCoordinatesConverter::enableNatureGestureFlag()
{
    m_d->isNativeGesture = true;
}

void KisCoordinatesConverter::beginRotation()
{
    KIS_SAFE_ASSERT_RECOVER_NOOP(!m_d->isRotating);

    // Save the current transformation and angle to use as the base of the ongoing rotation.
    m_d->rotationBaseTransform = m_d->flakeToWidget;
    m_d->rotationBaseAngle = m_d->rotationAngle;
    m_d->isRotating = true;
}

void KisCoordinatesConverter::endRotation()
{
    KIS_SAFE_ASSERT_RECOVER_NOOP(m_d->isRotating);

    m_d->isNativeGesture = false;
    m_d->isRotating = false;
}

void KisCoordinatesConverter::rotate(const std::optional<KoViewTransformStillPoint> &stillPoint, qreal angle)
{
    setZoomMode(KoZoomMode::ZOOM_CONSTANT);

    KoViewTransformStillPoint effectiveStillPoint =
        stillPoint ? *stillPoint :
        KoViewTransformStillPoint(m_d->preferredTransformationCenterInDocumentPixels(), widgetCenterPoint());

    PkTransform rot;
    rot.rotate(angle);

    if (!m_d->isNativeGesture && m_d->isRotating)
    {
        // Modal (begin/end) rotation. Transform from the stable base.
        m_d->flakeToWidget = m_d->rotationBaseTransform;
        m_d->rotationAngle = std::fmod(m_d->rotationBaseAngle + angle, 360.0);
    }
    else
    {
        // Immediate rotation, directly applied to the canvas transformation.
        m_d->rotationAngle = std::fmod(m_d->rotationAngle + angle, 360.0);
    }

    {
        const qreal numQuadrants = m_d->rotationAngle / 90.0;
        m_d->rotationIsOrthogonal = std::floor(numQuadrants) == numQuadrants;
    }

    m_d->flakeToWidget *= rot;
    correctOffsetToTransformationAndSnap();
    recalculateTransformations();

    const PkPointF newStillPoint = documentToWidget(effectiveStillPoint.docPoint());
    const PkPointF offset = newStillPoint - effectiveStillPoint.viewPoint();
    m_d->documentOffset = snapToDevicePixel(m_d->documentOffset + offset);
    recalculateTransformations();

    if (stillPoint) {
        resetPreferredTransformationCenter();
    }
}

void KisCoordinatesConverter::mirror(const std::optional<KoViewTransformStillPoint> &stillPoint, bool mirrorXAxis, bool mirrorYAxis)
{
    bool keepOrientation = false; // XXX: Keep here for now, maybe some day we can restore the parameter again.

    KoViewTransformStillPoint effectiveStillPoint =
        stillPoint ? *stillPoint :
        KoViewTransformStillPoint(m_d->preferredTransformationCenterInDocumentPixels(), widgetCenterPoint());


    if (kisSquareDistance(toPkPointF(effectiveStillPoint.viewPoint()),
                          toPkPointF(widgetCenterPoint())) > 2.0) {
        // when mirroring not against the center, reset the zoom mode
        setZoomMode(KoZoomMode::ZOOM_CONSTANT);
    }

    const PkPointF oldDocumentOffset = m_d->documentOffset;

    bool       doXMirroring = m_d->isXAxisMirrored ^ mirrorXAxis;
    bool       doYMirroring = m_d->isYAxisMirrored ^ mirrorYAxis;
    qreal      scaleX       = doXMirroring ? -1.0 : 1.0;
    qreal      scaleY       = doYMirroring ? -1.0 : 1.0;
    PkTransform mirror       = PkTransform::fromScale(scaleX, scaleY);

    PkTransform rot;
    rot.rotate(m_d->rotationAngle);

    m_d->flakeToWidget *= PkTransform::fromTranslate(-effectiveStillPoint.viewPoint().x(),-effectiveStillPoint.viewPoint().y());

    if (keepOrientation) {
        m_d->flakeToWidget *= rot.inverted();
    }

    m_d->flakeToWidget *= mirror;

    if (keepOrientation) {
        m_d->flakeToWidget *= rot;
    }

    m_d->flakeToWidget *= PkTransform::fromTranslate(effectiveStillPoint.viewPoint().x(),effectiveStillPoint.viewPoint().y());


    if (!keepOrientation && (doXMirroring ^ doYMirroring)) {
        m_d->rotationAngle = -m_d->rotationAngle;
    }

    m_d->isXAxisMirrored = mirrorXAxis;
    m_d->isYAxisMirrored = mirrorYAxis;

    correctOffsetToTransformationAndSnap();

    if (zoomMode() != KoZoomMode::ZOOM_CONSTANT) {
        // we were "centered", so let's try to keep the offset as before
        m_d->documentOffset = oldDocumentOffset;
    } else {
        recalculateTransformations();
        const PkPointF newStillPoint = documentToWidget(effectiveStillPoint.docPoint());
        const PkPointF offset = newStillPoint - effectiveStillPoint.viewPoint();
        m_d->documentOffset = snapToDevicePixel(m_d->documentOffset + offset);
    }

    recalculateTransformations();

    if (stillPoint) {
        resetPreferredTransformationCenter();
    }
}

bool KisCoordinatesConverter::xAxisMirrored() const
{
    return m_d->isXAxisMirrored;
}

bool KisCoordinatesConverter::yAxisMirrored() const
{
    return m_d->isYAxisMirrored;
}

void KisCoordinatesConverter::resetRotation(const std::optional<KoViewTransformStillPoint> &stillPoint)
{
    KoViewTransformStillPoint effectiveStillPoint =
        stillPoint ? *stillPoint :
        KoViewTransformStillPoint(m_d->preferredTransformationCenterInDocumentPixels(), widgetCenterPoint());

    PkTransform rot;
    rot.rotate(-m_d->rotationAngle);

    m_d->flakeToWidget *= rot;
    m_d->rotationAngle = 0.0;
    m_d->rotationIsOrthogonal = true;

    correctOffsetToTransformationAndSnap();
    recalculateTransformations();

    const PkPointF newStillPoint = documentToWidget(effectiveStillPoint.docPoint());
    const PkPointF offset = newStillPoint - effectiveStillPoint.viewPoint();
    m_d->documentOffset = snapToDevicePixel(m_d->documentOffset + offset);
    recalculateTransformations();

    if (stillPoint) {
        resetPreferredTransformationCenter();
    }
}

PkTransform KisCoordinatesConverter::imageToWidgetTransform() const {
    return m_d->imageToDocument * m_d->documentToFlake * m_d->flakeToWidget;
}

PkTransform KisCoordinatesConverter::imageToDocumentTransform() const {
    return m_d->imageToDocument;
}

PkTransform KisCoordinatesConverter::documentToFlakeTransform() const {
    return m_d->documentToFlake;
}

PkTransform KisCoordinatesConverter::flakeToWidgetTransform() const {
    return m_d->flakeToWidget;
}

PkTransform KisCoordinatesConverter::documentToWidgetTransform() const {
    return m_d->documentToFlake * m_d->flakeToWidget;
}

PkTransform KisCoordinatesConverter::viewportToWidgetTransform() const {
    return m_d->widgetToViewport.inverted();
}

PkTransform KisCoordinatesConverter::imageToViewportTransform() const {
    return m_d->imageToDocument * m_d->documentToFlake * m_d->flakeToWidget * m_d->widgetToViewport;
}

void KisCoordinatesConverter::getQPainterCheckersInfo(PkTransform *transform,
                                                      PkPointF *brushOrigin,
                                                      PkPolygonF *polygon,
                                                      const bool scrollCheckers) const
{
    /**
     * Qt has different rounding for QPainter::drawRect/drawImage.
     * The image is rounded mathematically, while rect in aligned
     * to the next integer. That causes transparent line appear on
     * the canvas.
     *
     * See: https://bugreports.qt.nokia.com/browse/QTBUG-22827
     */

    PkRectF imageRect = imageRectInViewportPixels();
    imageRect.adjust(0,0,-0.5,-0.5);

    if (scrollCheckers) {
        *transform = viewportToWidgetTransform();
        *polygon = imageRect;
        *brushOrigin = imageToViewport(PkPointF(0,0));
    }
    else {
        *transform = PkTransform();
        *polygon = viewportToWidgetTransform().map(imageRect);
        *brushOrigin = toPkPointF(QPoint(0,0));
    }
}

void KisCoordinatesConverter::getOpenGLCheckersInfo(const PkRectF &viewportRect,
                                                    PkTransform *textureTransform,
                                                    PkTransform *modelTransform,
                                                    PkRectF *textureRect,
                                                    PkRectF *modelRect,
                                                    const bool scrollCheckers) const
{
    if(scrollCheckers) {
        *textureTransform = PkTransform();
        *textureRect = PkRectF(0, 0, viewportRect.width(),viewportRect.height());
    }
    else {
        *textureTransform = viewportToWidgetTransform();
        *textureRect = viewportRect;
    }

    *modelTransform = viewportToWidgetTransform();
    *modelRect = viewportRect;
}

PkPointF KisCoordinatesConverter::imageCenterInWidgetPixel() const
{
    PkPolygonF poly = toPkPolygonF(imageToWidget(QPolygon(m_d->imageBounds)));
    return (poly[0] + poly[1] + poly[2] + poly[3]) / 4.0;
}


// these functions return a bounding rect if the canvas is rotated

PkRectF KisCoordinatesConverter::imageRectInWidgetPixels() const
{
    return toPkRect(imageToWidget(m_d->imageBounds));
}

PkRectF KisCoordinatesConverter::imageRectInViewportPixels() const
{
    return toPkRect(imageToViewport(m_d->imageBounds));
}

QRect KisCoordinatesConverter::imageRectInImagePixels() const
{
    return m_d->imageBounds;
}

PkRectF KisCoordinatesConverter::imageRectInDocumentPixels() const
{
    return toPkRect(imageToDocument(m_d->imageBounds));
}

PkSizeF KisCoordinatesConverter::imageSizeInFlakePixels() const
{
    qreal scaleX, scaleY;
    imageScale(&scaleX, &scaleY);
    QSize imageSize = m_d->imageBounds.size();

    return PkSizeF(imageSize.width() * scaleX, imageSize.height() * scaleY);
}

PkRectF KisCoordinatesConverter::widgetRectInFlakePixels() const
{
    return widgetToFlake(PkRectF(toPkPoint(QPoint(0,0)), toPkSizeF(m_d->canvasWidgetSize)));
}

PkRectF KisCoordinatesConverter::widgetRectInImagePixels() const
{
    return widgetToImage(PkRectF(toPkPoint(QPoint(0,0)), toPkSizeF(m_d->canvasWidgetSize)));
}

PkPointF KisCoordinatesConverter::flakeCenterPoint() const
{
    PkRectF widgetRect = widgetRectInFlakePixels();
    return PkPointF(widgetRect.left() + widgetRect.width() / 2,
                   widgetRect.top() + widgetRect.height() / 2);
}

PkPointF KisCoordinatesConverter::widgetCenterPoint() const
{
    return PkPointF(m_d->canvasWidgetSize.width() / 2.0, m_d->canvasWidgetSize.height() / 2.0);
}

void KisCoordinatesConverter::imageScale(qreal *scaleX, qreal *scaleY) const
{
    // get the x and y zoom level of the canvas
    qreal zoomX, zoomY;
    KoZoomHandler::zoom(&zoomX, &zoomY);

    // Get the KisImage resolution
    qreal resX = m_d->imageXRes;
    qreal resY = m_d->imageYRes;

    // Compute the scale factors
    *scaleX = zoomX / resX;
    *scaleY = zoomY / resY;
}

void KisCoordinatesConverter::imagePhysicalScale(qreal *scaleX, qreal *scaleY) const
{
    imageScale(scaleX, scaleY);
    *scaleX *= m_d->devicePixelRatio;
    *scaleY *= m_d->devicePixelRatio;
}

/**
 * @brief Adjust a given pair of coordinates to the nearest device pixel
 *        according to the value of `devicePixelRatio`.
 * @param point a point in logical pixel space
 * @return The point in logical pixel space but adjusted to the nearest device
 *         pixel
 */

PkPointF KisCoordinatesConverter::snapToDevicePixel(const PkPointF &point) const
{
    if (!m_d->rotationIsOrthogonal) {
        return point;
    }

    PkPoint devicePixel = (point * m_d->devicePixelRatio).toPoint();
    // These adjusted coords will be in logical pixel but is aligned in device
    // pixel space for pixel-perfect rendering.
    return toPkPointF(devicePixel) / m_d->devicePixelRatio;
}

PkTransform KisCoordinatesConverter::viewToWidget() const
{
    return flakeToWidgetTransform();
}

PkTransform KisCoordinatesConverter::widgetToView() const
{
    return flakeToWidgetTransform().inverted();
}

qreal KisCoordinatesConverter::minZoom() const
{
    return m_d->minZoom;
}
qreal KisCoordinatesConverter::maxZoom() const
{
    return m_d->maxZoom;
}
qreal KisCoordinatesConverter::clampZoom(qreal zoom) const
{
    return std::clamp(zoom, minZoom(), maxZoom());
}

PkVector<qreal> KisCoordinatesConverter::standardZoomLevels() const
{
    return m_d->standardZoomLevels.value();
}

PkVector<qreal> KisCoordinatesConverter::Private::StandardZoomLevelsInitializer::initialize() const
{
    return KoZoomMode::generateStandardZoomLevels(m_d->minZoom, m_d->maxZoom);
}

void KisCoordinatesConverter::recalculateZoomLevelLimits()
{
    qreal minDimension = 0.0;

    if (m_d->imageBounds.width() < m_d->imageBounds.height()) {
        minDimension = m_d->imageBounds.width() * resolutionX() / m_d->imageXRes;
    } else {
        minDimension = m_d->imageBounds.height() * resolutionY() / m_d->imageYRes;
    }

    m_d->minZoom = pkMin(100.0 / minDimension, 0.1);
    m_d->maxZoom = 90.0;
    m_d->standardZoomLevels.clear(); // TODO: reset only on real change!
}

qreal KisCoordinatesConverter::findNextZoom(qreal currentZoom, const PkVector<qreal> &zoomLevels)
{
    return KoZoomMode::findNextZoom(currentZoom, zoomLevels);
}

qreal KisCoordinatesConverter::findPrevZoom(qreal currentZoom, const PkVector<qreal> &zoomLevels)
{
    return KoZoomMode::findPrevZoom(currentZoom, zoomLevels);
}

KoViewTransformStillPoint KisCoordinatesConverter::makeWidgetStillPoint(const PkPointF &viewPoint) const
{
    return {widgetToDocument(viewPoint), viewPoint};
}

KoViewTransformStillPoint KisCoordinatesConverter::makeDocStillPoint(const PkPointF &docPoint) const
{
    return {docPoint, documentToWidget(docPoint)};
}
