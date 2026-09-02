/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_liquify_transform_strategy.h"

#include <algorithm>

#include <PkPoint.h>
#include <PkPainter.h>
#include <PkPainterPath.h>

#include "KoPointerEvent.h"

#include "kis_coordinates_converter.h"
#include "tool_transform_args.h"
#include "transform_transaction_properties.h"
#include "krita_utils.h"
#include "kis_transform_utils.h"
#include "kis_algebra_2d.h"
#include "kis_liquify_paint_helper.h"
#include "kis_liquify_transform_worker.h"
#include "KoCanvasResourceProvider.h"
#include <KisCanvasToolServices.h>
#include <KisStandardBrushSizes.h>


struct KisLiquifyTransformStrategy::Private
{
    Private(KisLiquifyTransformStrategy *_q,
            const KisCoordinatesConverter *_converter,
            ToolTransformArgs &_currentArgs,
            TransformTransactionProperties &_transaction,
            const KoCanvasResourceProvider *_manager,
            KisCanvasToolServices *_canvasServices)
        : manager(_manager),
          canvasServices(_canvasServices),
          q(_q),
          converter(_converter),
          currentArgs(_currentArgs),
          transaction(_transaction),
          helper(_converter),
          recalculateOnNextRedraw(false)
    {
    }

    const KoCanvasResourceProvider *manager;
    KisCanvasToolServices *canvasServices;

    KisLiquifyTransformStrategy * const q;

    /// standard members ///

    const KisCoordinatesConverter *converter;

    //////
    ToolTransformArgs &currentArgs;
    //////
    TransformTransactionProperties &transaction;

    PkTransform paintingTransform;
    PkPointF paintingOffset;

    PkTransform handlesTransform;

    /// custom members ///

    PkImage transformedImage;

    // size-gesture-related
    PkPointF lastMouseWidgetPos;
    PkPointF startResizeImagePos;
    PkPoint startResizeGlobalCursorPos;

    // for increase/decrease brush size
    PkPointF lastDocPos;
    KisStandardBrushSizes standardBrushSizes{int(KisLiquifyProperties::minSize()),
                                              int(KisLiquifyProperties::maxSize())};

    KisLiquifyPaintHelper helper;

    bool recalculateOnNextRedraw;

    void recalculateTransformations();
    inline PkPointF imageToThumb(const PkPointF &pt, bool useFlakeOptimization);
};

KisLiquifyTransformStrategy::KisLiquifyTransformStrategy(const KisCoordinatesConverter *converter,
                                                         ToolTransformArgs &currentArgs,
                                                         TransformTransactionProperties &transaction,
                                                         const KoCanvasResourceProvider *manager,
                                                         KisCanvasToolServices *canvasServices)

    : m_d(new Private(this, converter, currentArgs, transaction, manager, canvasServices))
{
}

KisLiquifyTransformStrategy::~KisLiquifyTransformStrategy()
{
}

PkPainterPath KisLiquifyTransformStrategy::getCursorOutline() const
{
    return m_d->helper.brushOutline(*m_d->currentArgs.liquifyProperties());
}

void KisLiquifyTransformStrategy::setTransformFunction(const PkPointF &mousePos, bool perspectiveModifierActive, bool shiftModifierActive)
{
    (void)mousePos;
    (void)perspectiveModifierActive;
    (void)shiftModifierActive;
}

TransformCursorDescriptor KisLiquifyTransformStrategy::getCurrentCursor() const
{
    return TransformCursorDescriptor{TransformCursorKind::Blank};
}

void KisLiquifyTransformStrategy::paint(TransformToolPainter &gc)
{
    // Draw preview image

    if (m_d->recalculateOnNextRedraw) {
        m_d->recalculateTransformations();
        m_d->recalculateOnNextRedraw = false;
    }

    gc.save();

    gc.setOpacity(m_d->transaction.basePreviewOpacity());
    gc.setTransform(m_d->paintingTransform, true);
    gc.drawImage(m_d->paintingOffset, m_d->transformedImage);

    gc.restore();
}

void KisLiquifyTransformStrategy::externalConfigChanged()
{
    if (!m_d->currentArgs.liquifyWorker()) return;
    m_d->recalculateTransformations();
}

bool KisLiquifyTransformStrategy::acceptsClicks() const
{
    return true;
}

bool KisLiquifyTransformStrategy::beginPrimaryAction(KoPointerEvent *event)
{
    m_d->lastDocPos = event->point;
    m_d->helper.configurePaintOp(*m_d->currentArgs.liquifyProperties(), m_d->currentArgs.liquifyWorker());
    m_d->helper.startPaint(event, m_d->manager);

    m_d->recalculateTransformations();

    return true;
}

void KisLiquifyTransformStrategy::continuePrimaryAction(KoPointerEvent *event)
{
    m_d->lastDocPos = event->point;
    m_d->helper.continuePaint(event);

    // the updates should be compressed
    m_d->recalculateOnNextRedraw = true;
    requestCanvasUpdate();
}

bool KisLiquifyTransformStrategy::endPrimaryAction(KoPointerEvent *event)
{
    m_d->lastDocPos = event->point;
    if (m_d->helper.endPaint(event)) {
        m_d->recalculateTransformations();
        requestCanvasUpdate();
    }

    return true;
}

void KisLiquifyTransformStrategy::hoverActionCommon(KoPointerEvent *event)
{
    m_d->lastDocPos = event->point;
    m_d->helper.hoverPaint(event);
}

void KisLiquifyTransformStrategy::activateAlternateAction(KisTool::AlternateAction action)
{
    if (action == KisTool::SampleFgNode || action == KisTool::SampleBgNode ||
        action == KisTool::SampleFgImage || action == KisTool::SampleBgImage) {

        KisLiquifyProperties *props = m_d->currentArgs.liquifyProperties();
        props->setReverseDirection(!props->reverseDirection());
        requestUpdateOptionWidget();
    }
}

void KisLiquifyTransformStrategy::deactivateAlternateAction(KisTool::AlternateAction action)
{
    if (action == KisTool::SampleFgNode || action == KisTool::SampleBgNode ||
        action == KisTool::SampleFgImage || action == KisTool::SampleBgImage) {

        KisLiquifyProperties *props = m_d->currentArgs.liquifyProperties();
        props->setReverseDirection(!props->reverseDirection());
        requestUpdateOptionWidget();
    }
}

bool KisLiquifyTransformStrategy::beginAlternateAction(KoPointerEvent *event, KisTool::AlternateAction action)
{
    m_d->lastDocPos = event->point;
    if (action == KisTool::ChangeSize || action == KisTool::ChangeSizeSnap) {
        PkPointF widgetPoint = m_d->converter->documentToWidget(event->point);
        m_d->lastMouseWidgetPos = widgetPoint;
        m_d->startResizeImagePos = m_d->converter->documentToImage(event->point);
        m_d->startResizeGlobalCursorPos = event->globalPos();
        return true;
    } else if (action == KisTool::SampleFgNode || action == KisTool::SampleBgNode ||
               action == KisTool::SampleFgImage || action == KisTool::SampleBgImage) {

        return beginPrimaryAction(event);
    }

    return false;
}

void KisLiquifyTransformStrategy::continueAlternateAction(KoPointerEvent *event, KisTool::AlternateAction action)
{
    m_d->lastDocPos = event->point;
    if (action == KisTool::ChangeSize || action == KisTool::ChangeSizeSnap) {
        PkPointF widgetPoint = m_d->converter->documentToWidget(event->point);

        PkPointF diff = widgetPoint - m_d->lastMouseWidgetPos;

        KisLiquifyProperties *props = m_d->currentArgs.liquifyProperties();
        const qreal linearizedOffset = diff.x() / KisTransformUtils::scaleFromAffineMatrix(m_d->converter->imageToWidgetTransform());
        const qreal newSize = qBound(props->minSize(), props->size() + linearizedOffset, props->maxSize());
        if (action == KisTool::ChangeSizeSnap) {
            props->setSize(floor(newSize));
        } else {
            props->setSize(newSize);
        }
        m_d->currentArgs.saveLiquifyTransformMode();

        m_d->lastMouseWidgetPos = widgetPoint;

        requestCursorOutlineUpdate(m_d->startResizeImagePos);
    } else if (action == KisTool::SampleFgNode || action == KisTool::SampleBgNode ||
               action == KisTool::SampleFgImage || action == KisTool::SampleBgImage) {

        return continuePrimaryAction(event);
    }
}

bool KisLiquifyTransformStrategy::endAlternateAction(KoPointerEvent *event, KisTool::AlternateAction action)
{
    m_d->lastDocPos = event->point;

    if (action == KisTool::ChangeSize || action == KisTool::ChangeSizeSnap) {
        m_d->canvasServices->toolSetCursorPosition(m_d->startResizeGlobalCursorPos);
        return true;
    } else if (action == KisTool::SampleFgNode || action == KisTool::SampleBgNode ||
               action == KisTool::SampleFgImage || action == KisTool::SampleBgImage) {
        return endPrimaryAction(event);
    }

    return false;
}

void KisLiquifyTransformStrategy::increaseBrushSize(KoCanvasBase *canvas)
{
    changeBrushSize(canvas, true);
}

void KisLiquifyTransformStrategy::decreaseBrushSize(KoCanvasBase *canvas)
{
    changeBrushSize(canvas, false);
}

void KisLiquifyTransformStrategy::changeBrushSize(KoCanvasBase *canvas, bool increase)
{
    KisLiquifyProperties *props = m_d->currentArgs.liquifyProperties();
    qreal oldSize = props->size();

    int newSize;
    if (increase) {
        newSize = m_d->standardBrushSizes.increaseBrushSize(oldSize);
    } else {
        newSize = m_d->standardBrushSizes.decreaseBrushSize(oldSize);
    }

    props->setSize(newSize);
    m_d->canvasServices->toolShowBrushSize(newSize);
    requestCursorOutlineUpdate( m_d->converter->documentToImage(m_d->lastDocPos));
    requestUpdateOptionWidget();
}

inline PkPointF KisLiquifyTransformStrategy::Private::imageToThumb(const PkPointF &pt, bool useFlakeOptimization)
{
    return useFlakeOptimization ? converter->imageToDocument(converter->documentToFlake((pt))) : q->thumbToImageTransform().inverted().map(pt);
}

void KisLiquifyTransformStrategy::Private::recalculateTransformations()
{
    KIS_ASSERT_RECOVER_RETURN(currentArgs.liquifyWorker());

    PkTransform scaleTransform = KisTransformUtils::imageToFlakeTransform(converter);

    PkTransform resultThumbTransform = q->thumbToImageTransform() * scaleTransform;
    qreal scale = KisTransformUtils::scaleFromAffineMatrix(resultThumbTransform);
    bool useFlakeOptimization = scale < 1.0 &&
        !KisTransformUtils::thumbnailTooSmall(resultThumbTransform, q->originalImage().rect());

    paintingOffset = transaction.originalTopLeft();
    if (!q->originalImage().isNull()) {
        if (useFlakeOptimization) {
            transformedImage = q->originalImage().transformed(resultThumbTransform);
            paintingTransform = PkTransform();
        } else {
            transformedImage = q->originalImage();
            paintingTransform = resultThumbTransform;
        }

        PkTransform imageToRealThumbTransform =
            useFlakeOptimization ?
            scaleTransform :
            q->thumbToImageTransform().inverted();

        PkPointF origTLInFlake =
            imageToRealThumbTransform.map(transaction.originalTopLeft());

        transformedImage =
            currentArgs.liquifyWorker()->runOnQImage(transformedImage,
                                                     origTLInFlake,
                                                     imageToRealThumbTransform,
                                                     &paintingOffset);
    } else {
        transformedImage = q->originalImage();
        paintingOffset = imageToThumb(transaction.originalTopLeft(), false);
        paintingTransform = resultThumbTransform;
    }

    handlesTransform = scaleTransform;
    q->requestImageRecalculation();
}
