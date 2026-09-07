/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2011 Silvio Heinrich <plassy@web.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_COORDINATES_CONVERTER_H
#define KIS_COORDINATES_CONVERTER_H

#include <optional>
#include <type_traits>

// R-38 约定：真 Qt 头在前，PkFlakeBridge 才走真 Qt 分支（toQTransform 等可见）。
#include <QtCore/QtCore>
#include <QtGui/QtGui>
#include <pk/geometry/PkTransform.h>
#include <PkFlakeBridge.h>
#include <KoZoomHandler.h>

#include "kritacanvas_export.h"
#include "kis_types.h"

class KoViewTransformStillPoint;

#define EPSILON 1e-6

#define SCALE_LESS_THAN(scX, scY, value)                        \
    (scX < (value) - EPSILON && scY < (value) - EPSILON)
#define SCALE_MORE_OR_EQUAL_TO(scX, scY, value)                 \
    (scX > (value) - EPSILON && scY > (value) - EPSILON)

namespace _Private
{
    template<class T> struct Traits
    {
        typedef T Result;
        // 按类型精确分发：Pk 类型走 PkTransform 原生；Qt 类型走 toQTransform 后的
        // 精确重载（不能笼统 map(obj)：QRect 会隐式转 QRegion/QPolygon 造成歧义）。
        // 需 PkRectF/PkRect 特化保留在下方（mapRect 语义不同：整数版四角取整）。
        static T map(const PkTransform& transform, const T& obj)
        {
            if constexpr (std::is_same_v<T, PkRectF> || std::is_same_v<T, PkRect>) {
                return transform.mapRect(obj);
            } else if constexpr (std::is_same_v<T, PkPoint> || std::is_same_v<T, PkPointF> ||
                                 std::is_same_v<T, PkLineF> || std::is_same_v<T, PkPolygonF> ||
                                 std::is_same_v<T, PkPainterPath>) {
                return transform.map(obj);
            } else if constexpr (std::is_same_v<T, QRect> || std::is_same_v<T, QRectF>) {
                return toQTransform(transform).mapRect(obj);
            } else if constexpr (std::is_same_v<T, QPoint> || std::is_same_v<T, QPointF> ||
                                 std::is_same_v<T, QPolygon> || std::is_same_v<T, QPolygonF> ||
                                 std::is_same_v<T, QLineF> || std::is_same_v<T, QRegion>) {
                return toQTransform(transform).map(obj);
            } else if constexpr (std::is_same_v<T, QLine>) {
                return toQTransform(transform).map(QLineF(obj)).toLine();
            } else {
                static_assert(sizeof(T) == 0, "Traits<T>::map: unsupported type");
            }
        }
    };

    template<> struct Traits<PkRectF>
    {
        typedef PkRectF Result;
        static PkRectF map(const PkTransform& transform, const PkRectF& rc)  { return transform.mapRect(rc); }
    };


}

class KRITACANVAS_EXPORT KisCoordinatesConverter: public KoZoomHandler
{
public:
    KisCoordinatesConverter();
    ~KisCoordinatesConverter() override;

    PkSizeF getCanvasWidgetSize() const;
    QSize viewportDevicePixelSize() const;

    void setCanvasWidgetSize(PkSizeF size);
    void setDevicePixelRatio(qreal value);
    void setImage(KisImageWSP image);
    void setExtraReferencesBounds(const QRect &imageRect);
    void setImageBounds(const QRect &rect, const PkPointF oldImageStillPoint, const PkPointF newImageStillPoint);
    void setImageResolution(qreal xRes, qreal yRes);
    void setDocumentOffset(const PkPointF &offset);

    qreal devicePixelRatio() const;
    QPoint documentOffset() const;
    PkPointF documentOffsetF() const;
    qreal rotationAngle() const;


    /**
     * \brief returns the point in image coordinates which is supposed to be
     *        the default still point for the transformations of the canvas.
     *        It returns the point of the image that is "roughly" mapped to
     *        to the center of the canvas widget.
     *
     * Some of the methods of the converter accept std::optional<KoViewTransformStillPoint>
     * for a still point, over which the transformation should happen. When this argument is
     * std::nullotp, then preferredTransformationCenter() is used.
     *
     * One important property of preferredTransformationCenter() is that it is **not**
     * changed when the canvas is transformed over it, even when pixel alignment happens.
     * It means that preferredTransformationCenter() may **not** exactly map to the center
     * of the widget, due to hardware-pixel-alignment.
     *
     * Keeping this value unchanged allows us to avoid drifts of the offset when zooming
     * and rotating the canvas.
     */
    PkPointF preferredTransformationCenter() const;

    // Use the begin/end interface to rotate the canvas in one transformation.
    // This method is more accurate and doesn't amplify numerical errors from very small angles.
    void beginRotation();
    void endRotation();

    void enableNatureGestureFlag();

    /**
     * \brief rotates the canvas
     *
     * For the meaning of \p stillPoint \see setZoom()
     */
    void rotate(const std::optional<KoViewTransformStillPoint> &stillPoint, qreal angle);

    /**
     * \brief mirrors the canvas
     *
     * For the meaning of \p stillPoint \see setZoom()
     */
    void mirror(const std::optional<KoViewTransformStillPoint> &stillPoint, bool mirrorXAxis, bool mirrorYAxis);

    bool xAxisMirrored() const;
    bool yAxisMirrored() const;

    /**
     * \brief resets canvas rotation
     *
     * For the meaning of \p stillPoint \see setZoom()
     */
    void resetRotation(const std::optional<KoViewTransformStillPoint> &stillPoint);

    void setZoom(qreal zoom) override;

    void zoomTo(const PkRectF &widgetRect);

    /**
     * \brief changes the zoom mode of the canvas
     *
     * When \p mode is KoZoomMode::ZOOM_CONSTANT, \p stillPoint instructs the converter
     * to keep the passed point "still", i.e. to make sure that stillPoint.docPoint()
     * maps to stillPoint.widgetPoint() on screen.
     *
     * Please make sure that there is **no guarantee** that the still point will map
     * to the passed points exactly! The still point may be offset by at most
     * (0.5 * sqrt(2) / devicePixelRatio()) due to hardware pixel alignment.
     *
     * This alignment is the reason why we pass both document and view points
     * as a still point, because both values should be kept constant during iterative
     * zoom operations. Otherwise the canvas will drift to the side because of the
     * pixel alignmentl.
     *
     * If \p stillPoint is std::nullopt, then the zooming happens over
     * preferredTransformationCenter(), which is basically the center of
     * canvas widget, but with some guards against the drifting.
     */
    void setZoom(KoZoomMode::Mode mode, qreal zoom, qreal resolutionX, qreal resolutionY, const std::optional<KoViewTransformStillPoint> &stillPoint);

    void setCanvasWidgetSizeKeepZoom(const PkSizeF &size);

    /**
     * A composition of to scale methods: zoom level + image resolution
     */
    qreal effectiveZoom() const;
    qreal effectivePhysicalZoom() const;

    template<class T> typename _Private::Traits<T>::Result
    imageToViewport(const T& obj) const { return _Private::Traits<T>::map(imageToViewportTransform(), obj); }
    template<class T> typename _Private::Traits<T>::Result
    viewportToImage(const T& obj) const { return _Private::Traits<T>::map(imageToViewportTransform().inverted(), obj); }

    template<class T> typename _Private::Traits<T>::Result
    flakeToWidget(const T& obj) const { return _Private::Traits<T>::map(flakeToWidgetTransform(), obj); }
    template<class T> typename _Private::Traits<T>::Result
    widgetToFlake(const T& obj) const { return _Private::Traits<T>::map(flakeToWidgetTransform().inverted(), obj); }

    template<class T> typename _Private::Traits<T>::Result
    widgetToViewport(const T& obj) const { return _Private::Traits<T>::map(viewportToWidgetTransform().inverted(), obj); }
    template<class T> typename _Private::Traits<T>::Result
    viewportToWidget(const T& obj) const { return _Private::Traits<T>::map(viewportToWidgetTransform(), obj); }

    template<class T> typename _Private::Traits<T>::Result
    documentToWidget(const T& obj) const { return _Private::Traits<T>::map(documentToWidgetTransform(), obj); }
    template<class T> typename _Private::Traits<T>::Result
    widgetToDocument(const T& obj) const { return _Private::Traits<T>::map(documentToWidgetTransform().inverted(), obj); }

    template<class T> typename _Private::Traits<T>::Result
    imageToDocument(const T& obj) const { return _Private::Traits<T>::map(imageToDocumentTransform(), obj); }
    template<class T> typename _Private::Traits<T>::Result
    documentToImage(const T& obj) const { return _Private::Traits<T>::map(imageToDocumentTransform().inverted(), obj); }

    template<class T> typename _Private::Traits<T>::Result
    documentToFlake(const T& obj) const { return _Private::Traits<T>::map(documentToFlakeTransform(), obj); }
    template<class T> typename _Private::Traits<T>::Result
    flakeToDocument(const T& obj) const { return _Private::Traits<T>::map(documentToFlakeTransform().inverted(), obj); }

    template<class T> typename _Private::Traits<T>::Result
    imageToWidget(const T& obj) const { return _Private::Traits<T>::map(imageToWidgetTransform(), obj); }
    template<class T> typename _Private::Traits<T>::Result
    widgetToImage(const T& obj) const { return _Private::Traits<T>::map(imageToWidgetTransform().inverted(), obj); }

    PkTransform imageToWidgetTransform() const;
    PkTransform imageToDocumentTransform() const;
    PkTransform documentToFlakeTransform() const;
    PkTransform imageToViewportTransform() const;
    PkTransform viewportToWidgetTransform() const;
    PkTransform flakeToWidgetTransform() const;
    PkTransform documentToWidgetTransform() const;

    void getQPainterCheckersInfo(PkTransform *transform,
                                 PkPointF *brushOrigin,
                                 PkPolygonF *polygon,
                                 const bool scrollCheckers) const;

    void getOpenGLCheckersInfo(const PkRectF &viewportRect,
                               PkTransform *textureTransform,
                               PkTransform *modelTransform,
                               PkRectF *textureRect,
                               PkRectF *modelRect,
                               const bool scrollCheckers) const;

    PkPointF imageCenterInWidgetPixel() const;
    PkRectF imageRectInWidgetPixels() const;
    PkRectF imageRectInViewportPixels() const;
    PkSizeF imageSizeInFlakePixels() const;
    PkRectF widgetRectInFlakePixels() const;
    PkRectF widgetRectInImagePixels() const;
    QRect imageRectInImagePixels() const;
    PkRectF imageRectInDocumentPixels() const;

    PkPointF flakeCenterPoint() const;
    PkPointF widgetCenterPoint() const;

    void imageScale(qreal *scaleX, qreal *scaleY) const;
    void imagePhysicalScale(qreal *scaleX, qreal *scaleY) const;

    PkPointF snapToDevicePixel(const PkPointF &point) const;
    PkSizeF snapWidgetSizeToDevicePixel(const PkSizeF &size) const;

    QPoint minimumOffset() const;
    QPoint maximumOffset() const;

    qreal minZoom() const;
    qreal maxZoom() const;
    qreal clampZoom(qreal zoom) const;
    PkVector<qreal> standardZoomLevels() const;

    static qreal findNextZoom(qreal currentZoom, const PkVector<qreal> &zoomLevels);
    static qreal findPrevZoom(qreal currentZoom, const PkVector<qreal> &zoomLevels);

    KoViewTransformStillPoint makeWidgetStillPoint(const PkPointF &viewPoint) const override;
    KoViewTransformStillPoint makeDocStillPoint(const PkPointF &docPoint) const override;

public:
    // overrides from KoViewConverter
    PkTransform viewToWidget() const override;
    PkTransform widgetToView() const override;

private:
    friend class KisZoomAndPanTest;
    friend class KisCoordinatesConverterTest;

    PkPointF centeringCorrection() const;
    void correctOffsetToTransformationAndSnap();
    void correctTransformationToOffset();
    void resetPreferredTransformationCenter();
    void recalculateTransformations();
    void recalculateZoomLevelLimits();
    void recalculateOffsetBoundsAndCrop();

private:
    struct Private;
    Private * const m_d;
};

#endif /* KIS_COORDINATES_CONVERTER_H */
