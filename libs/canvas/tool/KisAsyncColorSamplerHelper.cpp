/*
 *  SPDX-FileCopyrightText: 2022 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2025 Carsten Hartenfels <carsten.hartenfels@pm.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkFlakeBridge.h>

#include "KisAsyncColorSamplerHelper.h"

#include <QApplication>
#include <QPalette>
#include <QTimer>
#include <pk/geometry/PkTransform.h>

#include <klocalizedstring.h>

#include <KConfigGroup>
#include <KSharedConfig>

#include "KoCanvasBase.h"
#include "KoCanvasResourcesIds.h"
#include "KoCanvasResourceProvider.h"
#include "KoViewConverter.h"
#include <QIcon>
#include "KisCanvasFeedback.h"
#include "KisColorSamplingCanvas.h"
#include "kis_image.h"
#include "kis_signal_compressor_with_param.h"
#include "kis_image_interfaces.h"
#include "kis_node.h"
#include "strokes/kis_color_sampler_stroke_strategy.h"


namespace {
enum class ColorSamplerPreviewStyle {
    None,
    Circle,
    RectangleLeft,
    RectangleRight,
    RectangleAbove,
    Count,
};

ColorSamplerPreviewStyle readColorSamplerPreviewStyle()
{
    const KConfigGroup cfg = KSharedConfig::openConfig()->group("");
    const int style = cfg.readEntry(
        "colorSamplerPreviewStyle", int(ColorSamplerPreviewStyle::Circle));

    if (style >= 0 && style < int(ColorSamplerPreviewStyle::Count)) {
        return ColorSamplerPreviewStyle(style);
    }

    return ColorSamplerPreviewStyle::Circle;
}

QColor colorWithAlpha(QColor color, int alpha)
{
    color.setAlpha(alpha);
    return color;
}
}

struct KisAsyncColorSamplerHelper::Private
{
    static constexpr qreal PREVIEW_RECT_SIZE = 48.0;

    Private(KoCanvasBase *_canvas, KisColorSamplingCanvas *_samplingCanvas)
        : canvas(_canvas)
        , samplingCanvas(_samplingCanvas)
    {}

    KoCanvasBase *canvas;
    KisColorSamplingCanvas *samplingCanvas;

    int sampleResourceId {0};
    bool sampleCurrentLayer {true};
    bool updateGlobalColor {true};

    bool isActive {false};
    bool showPreview {false};
    bool haveSample {false};

    KisStrokeId strokeId;
    typedef KisSignalCompressorWithParam<PkPointF> SamplingCompressor;
    PkScopedPointer<SamplingCompressor> samplingCompressor;

    QTimer activationDelayTimer;

    ColorSamplerPreviewStyle style = ColorSamplerPreviewStyle::Circle;
    int circlePreviewDiameter {180};
    qreal circlePreviewThickness {0.12};
    bool circlePreviewOutlineEnabled {true};
    bool circlePreviewExtraCircles {true};
    PkRectF previewDocRect;

    QColor currentColor;
    QColor baseColor;

    KisStrokesFacade *strokesFacade() const {
        return samplingCanvas->samplingImage().data();
    }

    const KoViewConverter &converter() const {
        return *canvas->viewConverter();
    }

    PkRectF colorPreviewRectForRectangle() const
    {
        // Offsetting to the sides is both vertical and horizontal, when
        // offsetting above it's only vertical, so it needs a bit more space.
        constexpr qreal OFFSET = 32.0;
        constexpr qreal OFFSET_ABOVE = OFFSET * 1.5;
        constexpr qreal SIZE = PREVIEW_RECT_SIZE;

        bool mirrored = samplingCanvas->samplingCanvasMirroredHorizontally();
        bool flipped = samplingCanvas->samplingCanvasMirroredVertically();

        ColorSamplerPreviewStyle effectiveStyle;
        if (mirrored && style == ColorSamplerPreviewStyle::RectangleLeft) {
            effectiveStyle = ColorSamplerPreviewStyle::RectangleRight;
        } else if (mirrored && style == ColorSamplerPreviewStyle::RectangleRight) {
            effectiveStyle = ColorSamplerPreviewStyle::RectangleLeft;
        } else {
            effectiveStyle = style;
        }

        qreal width = haveSample ? SIZE * 2.0 : SIZE;

        qreal x, y;
        switch (effectiveStyle) {
        case ColorSamplerPreviewStyle::RectangleLeft:
            x = -(OFFSET + width);
            y = flipped ? -(OFFSET + SIZE) : OFFSET;
            break;
        case ColorSamplerPreviewStyle::RectangleRight:
            x = OFFSET;
            y = flipped ? -(OFFSET + SIZE) : OFFSET;
            break;
        default:
            x = width / -2.0;
            y = flipped ? OFFSET_ABOVE : -(OFFSET_ABOVE + SIZE);
            break;
        }

        PkRectF rect(x, y, width, SIZE);

        qreal canvasRotationAngle = samplingCanvas->samplingCanvasRotation();
        if (!pkQtFuzzyIsNull(canvasRotationAngle)) {
            PkTransform tf;
            tf.rotate(mirrored ? canvasRotationAngle : -canvasRotationAngle);
            rect = tf.mapRect(rect);
        }

        return rect;
    }

    PkRectF colorPreviewRectForCircle()
    {
        return PkRectF(-circlePreviewDiameter / 2.0, -circlePreviewDiameter / 2.0, circlePreviewDiameter, circlePreviewDiameter);
    }

    PkRectF colorPreviewDocRect(const PkPointF &outlineDocPoint)
    {
        PkRectF colorPreviewViewRect;
        switch (style) {
        case ColorSamplerPreviewStyle::None:
            return PkRectF();
        case ColorSamplerPreviewStyle::RectangleLeft:
        case ColorSamplerPreviewStyle::RectangleRight:
        case ColorSamplerPreviewStyle::RectangleAbove:
            colorPreviewViewRect = colorPreviewRectForRectangle();
            break;
        default:
            // Showing a preview without sampling a color (by just holding a
            // modifier) is used to compare the foreground color with the
            // canvas. The circle doesn't work well for that purpose, so we
            // use the handedness-independent rectangle above instead.
            if (haveSample) {
                colorPreviewViewRect = colorPreviewRectForCircle();
            } else {
                colorPreviewViewRect = colorPreviewRectForRectangle();
            }
            break;
        }

        const PkRectF colorPreviewDocumentRect = converter().viewToDocument(colorPreviewViewRect);
        return colorPreviewDocumentRect.translated(outlineDocPoint);
    }
};

KisAsyncColorSamplerHelper::KisAsyncColorSamplerHelper(
    KoCanvasBase *canvas,
    KisColorSamplingCanvas *samplingCanvas)
    : m_d(new Private(canvas, samplingCanvas))
{
    KIS_ASSERT(m_d->samplingCanvas);

    using namespace std::placeholders; // For _1 placeholder
    std::function<void(PkPointF)> callback =
        std::bind(&KisAsyncColorSamplerHelper::slotAddSamplingJob, this, _1);
    m_d->samplingCompressor.reset(
        new Private::SamplingCompressor(100, callback, KisSignalCompressor::FIRST_ACTIVE));

    m_d->activationDelayTimer.setInterval(100);
    m_d->activationDelayTimer.setSingleShot(true);
    QObject::connect(&m_d->activationDelayTimer, SIGNAL(timeout()), this, SLOT(activateDelayedPreview()));
}

KisAsyncColorSamplerHelper::~KisAsyncColorSamplerHelper()
{
    KIS_SAFE_ASSERT_RECOVER_NOOP(!m_d->strokeId);
}

bool KisAsyncColorSamplerHelper::isActive() const
{
    return m_d->isActive;
}

void KisAsyncColorSamplerHelper::activate(bool sampleCurrentLayer, bool pickFgColor)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(!m_d->isActive);
    m_d->isActive = true;

    m_d->sampleResourceId =
        pickFgColor ?
            KoCanvasResource::ForegroundColor :
            KoCanvasResource::BackgroundColor;

    m_d->sampleCurrentLayer = sampleCurrentLayer;
    m_d->haveSample = false;


    const KConfigGroup cfg = KSharedConfig::openConfig()->group("");
    m_d->style = readColorSamplerPreviewStyle();
    m_d->circlePreviewDiameter =
        cfg.readEntry("colorSamplerPreviewCircleDiameter", 180);
    m_d->circlePreviewThickness =
        cfg.readEntry("colorSamplerPreviewCircleThickness", qreal(12)) / 100.0;
    m_d->circlePreviewOutlineEnabled =
        cfg.readEntry("colorSamplerPreviewCircleOutlineEnabled", true);
    m_d->circlePreviewExtraCircles =
        cfg.readEntry("colorSamplerPreviewCircleExtraCirclesEnabled", true);

    m_d->activationDelayTimer.start();
}

void KisAsyncColorSamplerHelper::activateDelayedPreview()
{
    // the event may come after we have started or even
    // finished color picking if the user is quick
    if (!m_d->isActive || m_d->showPreview) {
        return;
    }

    activatePreview();

    Q_EMIT sigRequestUpdateOutline();
}

void KisAsyncColorSamplerHelper::activatePreview()
{
    m_d->activationDelayTimer.stop();
    m_d->showPreview = true;

    const KoColor currentColor =
        m_d->canvas->resourceManager()->koColorResource(m_d->sampleResourceId);
    const QColor previewColor = m_d->samplingCanvas->samplingPreviewColor(currentColor);

    m_d->currentColor = previewColor;
    m_d->baseColor = previewColor;
    updateCursor(m_d->sampleCurrentLayer, m_d->sampleResourceId == KoCanvasResource::ForegroundColor);
}

void KisAsyncColorSamplerHelper::updateCursor(bool sampleCurrentLayer, bool pickFgColor)
{
    Q_EMIT sigRequestCursor(
        m_d->samplingCanvas->samplingCursor(sampleCurrentLayer, pickFgColor));
}

void KisAsyncColorSamplerHelper::setUpdateGlobalColor(bool value)
{
    m_d->updateGlobalColor = value;
}

bool KisAsyncColorSamplerHelper::updateGlobalColor() const
{
    return m_d->updateGlobalColor;
}

void KisAsyncColorSamplerHelper::deactivate()
{
    KIS_SAFE_ASSERT_RECOVER(!m_d->strokeId) {
        endAction();
    }

    m_d->activationDelayTimer.stop();

    m_d->showPreview = false;
    m_d->haveSample = false;

    m_d->previewDocRect = PkRectF();
    m_d->currentColor = QColor();
    m_d->baseColor = QColor();
    m_d->isActive = false;

    Q_EMIT sigRequestCursorReset();
    Q_EMIT sigRequestUpdateOutline();
}

void KisAsyncColorSamplerHelper::startAction(const PkPointF &docPoint, int radius, int blend)
{
    KisColorSamplerStrokeStrategy *strategy = new KisColorSamplerStrokeStrategy(radius, blend);
    QObject::connect(strategy, &KisColorSamplerStrokeStrategy::sigColorUpdated,
            this, &KisAsyncColorSamplerHelper::slotColorSamplingFinished);
    QObject::connect(strategy, &KisColorSamplerStrokeStrategy::sigFinalColorSelected,
            this, &KisAsyncColorSamplerHelper::sigFinalColorSelected);

    activatePreview();
    m_d->haveSample = true;
    m_d->strokeId = m_d->strokesFacade()->startStroke(strategy);
    m_d->samplingCompressor->start(docPoint);
}

void KisAsyncColorSamplerHelper::continueAction(const PkPointF &docPoint)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_d->strokeId);
    m_d->samplingCompressor->start(docPoint);
}

void KisAsyncColorSamplerHelper::endAction()
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_d->strokeId);

    m_d->strokesFacade()->addJob(m_d->strokeId,
        new KisColorSamplerStrokeStrategy::FinalizeData());

    m_d->strokesFacade()->endStroke(m_d->strokeId);
    m_d->strokeId = nullptr;
}

PkRectF KisAsyncColorSamplerHelper::colorPreviewDocRect(const PkPointF &docPoint)
{
    if (!m_d->showPreview) return PkRectF();

    m_d->style = readColorSamplerPreviewStyle();
    m_d->previewDocRect = m_d->colorPreviewDocRect(docPoint);
    return m_d->previewDocRect;
}

void KisAsyncColorSamplerHelper::paint(PkPainter &gc, const KoViewConverter &converter)
{
    if (!m_d->showPreview) {
        return;
    }

    PkRectF viewRectF = converter.documentToView(m_d->previewDocRect);
    const PkColor currentColor = toPkColor(colorWithAlpha(m_d->currentColor, OPACITY_OPAQUE_U8));
    const PkColor baseColor = m_d->haveSample
        ? toPkColor(colorWithAlpha(m_d->baseColor, OPACITY_OPAQUE_U8))
        : currentColor;

    switch (m_d->style) {
    case ColorSamplerPreviewStyle::RectangleLeft:
    case ColorSamplerPreviewStyle::RectangleRight:
    case ColorSamplerPreviewStyle::RectangleAbove:
        paintRectangle(gc, viewRectF, currentColor, baseColor);
        break;
    default:
        // See comment in colorPreviewDocRect.
        if (m_d->haveSample) {
            paintCircle(gc, viewRectF, currentColor, baseColor);
        } else {
            paintRectangle(gc, viewRectF, currentColor, baseColor);
        }
        break;
    }
}

void KisAsyncColorSamplerHelper::paintRectangle(PkPainter &gc,
                                                const PkRectF &viewRectF,
                                                const PkColor &currentColor,
                                                const PkColor &baseColor)
{
    PkTransform contentTransform;
    const PkPointF center = viewRectF.center();
    contentTransform.translate(center.x(), center.y());
    const qreal canvasRotationAngle = m_d->samplingCanvas->samplingCanvasRotation();
    contentTransform.rotate(m_d->samplingCanvas->samplingCanvasMirroredHorizontally()
                                ? canvasRotationAngle
                                : -canvasRotationAngle);
    contentTransform.translate(-center.x(), -center.y());

    if (!m_d->haveSample) {
        PkPainterPath currentPath;
        currentPath.addRect(viewRectF);
        gc.fillPath(contentTransform.map(currentPath), PkBrush(currentColor));
        return;
    }

    const qreal centerX = viewRectF.center().x();
    PkRectF currentRect(viewRectF.topLeft(), PkPointF(centerX, viewRectF.bottom()));
    PkRectF baseRect(PkPointF(centerX, viewRectF.top()), viewRectF.bottomRight());
    if (m_d->samplingCanvas->samplingCanvasMirroredHorizontally()) {
        std::swap(currentRect, baseRect);
    }
    PkPainterPath currentPath;
    currentPath.addRect(currentRect);
    PkPainterPath basePath;
    basePath.addRect(baseRect);
    gc.fillPath(contentTransform.map(currentPath), PkBrush(currentColor));
    gc.fillPath(contentTransform.map(basePath), PkBrush(baseColor));
}

void KisAsyncColorSamplerHelper::paintCircle(PkPainter &gc,
                                             const PkRectF &viewRectF,
                                             const PkColor &currentColor,
                                             const PkColor &baseColor)
{
    if (!m_d->haveSample) {
        return;
    }



    gc.save();

    const qreal penWidth = m_d->circlePreviewDiameter > 100 ? 2.0 : 1.0;
    const PkColor outlineColor = toPkColor(
        colorWithAlpha(qApp->palette().color(QPalette::Base), OPACITY_OPAQUE_U8 / 2 + 1));
    const PkRectF outerRect = viewRectF.adjusted(penWidth, penWidth, -penWidth, -penWidth);

    qreal canvasRotationAngle = m_d->samplingCanvas->samplingCanvasRotation();
    if (m_d->samplingCanvas->samplingCanvasMirroredHorizontally()) {
        canvasRotationAngle = -canvasRotationAngle;
    }
    PkTransform contentTransform;
    const PkPointF center = viewRectF.center();
    contentTransform.translate(center.x(), center.y());
    contentTransform.rotate(-canvasRotationAngle);
    contentTransform.translate(-center.x(), -center.y());

    const qreal innerMarginX = outerRect.width() * m_d->circlePreviewThickness;
    const qreal innerMarginY = outerRect.height() * m_d->circlePreviewThickness;
    const PkRectF innerRect = outerRect.adjusted(innerMarginX, innerMarginY, -innerMarginX, -innerMarginY);
    PkPainterPath innerEllipse;
    innerEllipse.addEllipse(innerRect);
    PkPainterPath innerPath = innerEllipse;

    if (m_d->circlePreviewThickness < 0.5 && m_d->circlePreviewExtraCircles) {
        const qreal extraMargin = 0.1 * m_d->circlePreviewThickness * innerRect.width();
        const PkPointF leftCenter(innerRect.left() - extraMargin,
                                  innerRect.top() + innerRect.height() / 2.0);
        const PkPointF rightCenter(innerRect.right() + extraMargin,
                                   innerRect.top() + innerRect.height() / 2.0);
        innerPath.setFillRule(Pk::OddEvenFill);
        const qreal radius = m_d->circlePreviewThickness * viewRectF.width();
        innerPath.addEllipse(leftCenter, radius, radius);
        innerPath.addEllipse(rightCenter, radius, radius);
        innerPath = innerPath.intersected(innerEllipse);
    }
    innerPath = contentTransform.map(innerPath);

    PkPainterPath outerPath;
    outerPath.addEllipse(outerRect);
    const PkPainterPath ringPath = outerPath.subtracted(innerPath);

    const bool needsDualColor = currentColor != baseColor;
    if (needsDualColor) {
        const bool flipped = m_d->samplingCanvas->samplingCanvasMirroredVertically();
        PkPainterPath clipPath;
        clipPath.addRect(PkRectF(viewRectF.left(), viewRectF.top(), viewRectF.width(), viewRectF.height() / 2.0 + 1.0));
        gc.setClipPath(contentTransform.map(clipPath));
        gc.fillPath(ringPath, PkBrush(flipped ? baseColor : currentColor));

        clipPath.clear();
        clipPath.addRect(PkRectF(viewRectF.left(), viewRectF.center().y(), viewRectF.width(), viewRectF.height() / 2.0));
        gc.setClipPath(contentTransform.map(clipPath));
        gc.fillPath(ringPath, PkBrush(flipped ? currentColor : baseColor));
        gc.setClipPath(PkPainterPath(), Pk::NoClip);
    } else {
        gc.fillPath(ringPath, PkBrush(currentColor));
    }

    if (m_d->circlePreviewOutlineEnabled) {
        const PkPen outlinePen(outlineColor, penWidth);
        gc.strokePath(outerPath, outlinePen);
        gc.strokePath(innerPath, outlinePen);
    }

    gc.restore();
}

void KisAsyncColorSamplerHelper::slotAddSamplingJob(const PkPointF &docPoint)
{
    /**
     * The actual sampling is delayed by a compressor, so we can get this
     * event when the stroke is already closed
     */
    if (!m_d->strokeId) return;

    KisImageSP image = m_d->samplingCanvas->samplingImage();

    const PkPoint imagePoint = image->documentToImagePixelFloored(toPkPointF(docPoint));

    if (!m_d->sampleCurrentLayer) {
        const std::optional<KoColor> referenceColor =
            m_d->samplingCanvas->sampleVisibleReferenceColor(QPoint(imagePoint.x(), imagePoint.y()));
        if (referenceColor) {
            slotColorSamplingFinished(*referenceColor);
            return;
        }
    }

    KisPaintDeviceSP device;
    if (m_d->sampleCurrentLayer) {
        KisNodeSP currentNode = m_d->canvas->resourceManager()
                                    ->canvasResourcesInterface()
                                    ->resource(KoCanvasResource::CurrentKritaNode)
                                    .value<KisNodeWSP>();
        if (currentNode) {
            device = currentNode->colorSampleSourceDevice();
        }
    } else {
        device = image->projection();
    }

    if (device) {
        // Used for color sampler blending.
        const KoColor currentColor =
            m_d->canvas->resourceManager()->koColorResource(m_d->sampleResourceId);

        m_d->strokesFacade()->addJob(m_d->strokeId,
            new KisColorSamplerStrokeStrategy::Data(device, imagePoint, currentColor));
    } else {
        QString message = i18n("Color sampler does not work on this layer.");
        if (KisCanvasFeedback *feedback =
                dynamic_cast<KisCanvasFeedback *>(m_d->canvas)) {
            feedback->showFloatingMessage(toPkString(message), QIcon());
        }
    }
}

void KisAsyncColorSamplerHelper::slotColorSamplingFinished(const KoColor &rawColor)
{
    KoColor color(rawColor);

    color.setOpacity(OPACITY_OPAQUE_U8);

    if (m_d->updateGlobalColor) {
        m_d->canvas->resourceManager()->setResource(m_d->sampleResourceId, color);
    }

    Q_EMIT sigRawColorSelected(rawColor);
    Q_EMIT sigColorSelected(color);

    if (!m_d->showPreview) return;

    const QColor previewColor = m_d->samplingCanvas->samplingPreviewColor(color);

    if (!m_d->haveSample || m_d->currentColor != previewColor) {
        m_d->haveSample = true;
        m_d->currentColor = previewColor;
    }

    Q_EMIT sigRequestUpdateOutline();
}
