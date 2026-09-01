/*
 *  SPDX-FileCopyrightText: 2017 Eugene Ingerman
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_smart_patch.h"

#include <PkPainter.h>
#include <PkPainterPath.h>

#include <klocalizedstring.h>
#include <KoColor.h>
#include <KoCanvasBase.h>
#include <KoPointerEvent.h>
#include <KisCanvasFeedback.h>
#include <kis_coordinates_converter.h>
#include "kis_painter.h"
#include "kis_paintop_preset.h"

#include "kundo2magicstring.h"
#include "kundo2stack.h"
#include "commands_new/kis_transaction_based_command.h"
#include "kis_transaction.h"

#include "kis_processing_applicator.h"
#include "kis_datamanager.h"

#include "KoColorSpaceRegistry.h"
#include <KisCursorOverrideLock.h>

#include "libs/image/kis_paint_device_debug_utils.h"

#include "kis_paint_layer.h"
#include "kis_algebra_2d.h"
#include "kis_resources_snapshot.h"

PkRect patchImage(KisPaintDeviceSP imageDev, KisPaintDeviceSP maskDev, int radius, int accuracy, KisSelectionSP selection);

class KisToolSmartPatch::InpaintCommand : public KisTransactionBasedCommand {
public:
    InpaintCommand( KisPaintDeviceSP maskDev, KisPaintDeviceSP imageDev, int accuracy, int patchRadius, KisSelectionSP selection) :
        m_maskDev(maskDev), m_imageDev(imageDev), m_accuracy(accuracy), m_patchRadius(patchRadius), m_selection(selection) {}

    KUndo2Command* paint() override {
        KisTransaction transaction(m_imageDev);
        patchImage(m_imageDev, m_maskDev, m_patchRadius, m_accuracy, m_selection);
        return transaction.endAndTake();
    }

private:
    KisPaintDeviceSP m_maskDev, m_imageDev;
    int m_accuracy, m_patchRadius;
    KisSelectionSP m_selection;
};

struct KisToolSmartPatch::Private {
    KisPaintDeviceSP maskDev = nullptr;
    KisPainter maskDevPainter;
    float brushRadius = 50.; //initial default. actually read from ui.
    PkRectF oldOutlineRect;
    PkPainterPath brushOutline;
};


KisToolSmartPatch::KisToolSmartPatch(KoCanvasBase * canvas)
    : KisToolPaint(canvas, Qt::BlankCursor),
      m_d(new Private)
{
    setSupportOutline(true);
    setObjectName("tool_SmartPatch");
    m_d->maskDev = new KisPaintDevice(KoColorSpaceRegistry::instance()->rgb8());
    m_d->maskDevPainter.begin( m_d->maskDev );

    m_d->maskDevPainter.setPaintColor(KoColor(Qt::magenta, m_d->maskDev->colorSpace()));
    m_d->maskDevPainter.setBackgroundColor(KoColor(Qt::white, m_d->maskDev->colorSpace()));
    m_d->maskDevPainter.setFillStyle( KisPainter::FillStyleForegroundColor );
}

KisToolSmartPatch::~KisToolSmartPatch()
{
    m_d->maskDevPainter.end();
}

void KisToolSmartPatch::activate(const PkSet<KoShape*> &shapes)
{
    KisToolPaint::activate(shapes);
}

void KisToolSmartPatch::deactivate()
{
    KisToolPaint::deactivate();
}

void KisToolSmartPatch::resetCursorStyle()
{
    KisToolPaint::resetCursorStyle();
}

void KisToolSmartPatch::activatePrimaryAction()
{
    setOutlineVisible(true);
    KisToolPaint::activatePrimaryAction();
}

void KisToolSmartPatch::deactivatePrimaryAction()
{
    setOutlineVisible(false);
    KisToolPaint::deactivatePrimaryAction();
}

void KisToolSmartPatch::addMaskPath( KoPointerEvent *event )
{
    const KisCoordinatesConverter *converter = dynamic_cast<const KisCoordinatesConverter *>(canvas()->viewConverter());
    KIS_ASSERT(converter);

    PkPointF imagePos = currentImage()->documentToPixel(event->point);
    PkPainterPath currentBrushOutline = brushOutline().translated(KisAlgebra2D::alignForZoom(imagePos, converter->effectivePhysicalZoom()));
    m_d->maskDevPainter.fillPainterPath(currentBrushOutline);

    canvas()->updateCanvas(currentImage()->pixelToDocument(m_d->maskDev->exactBounds()));
}

void KisToolSmartPatch::beginPrimaryAction(KoPointerEvent *event)
{
    //we can only apply inpaint operation to paint layer
    if ( currentNode().isNull() || !currentNode()->inherits("KisPaintLayer") || nodePaintAbility()!=NodePaintAbility::PAINT ) {
        KisCanvasFeedback *feedback = dynamic_cast<KisCanvasFeedback*>(canvas());
        KIS_SAFE_ASSERT_RECOVER_RETURN(feedback);
        feedback->showFloatingMessage(
            PkString("Select a paint layer to use this tool"),
            {}, 2000, KisCanvasFeedback::Priority::Medium, Qt::AlignCenter);
        event->ignore();
        return;
    }

    addMaskPath(event);
    setMode(KisTool::PAINT_MODE);
    KisToolPaint::beginPrimaryAction(event);
}

void KisToolSmartPatch::continuePrimaryAction(KoPointerEvent *event)
{
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);
    addMaskPath(event);
    KisToolPaint::continuePrimaryAction(event);
}

void KisToolSmartPatch::endPrimaryAction(KoPointerEvent *event)
{
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);
    addMaskPath(event);
    KisToolPaint::endPrimaryAction(event);
    setMode(KisTool::HOVER_MODE);

    KisCursorOverrideLock cursorLock(Qt::WaitCursor);

    const int accuracy = 50; //default accuracy - middle value
    const int patchRadius = 4; //default radius, which works well for most cases tested

    KisResourcesSnapshotSP resources =
        new KisResourcesSnapshot(image(), currentNode(), this->canvas()->resourceManager()->canvasResourcesInterface());

    KisProcessingApplicator applicator( image(), currentNode(), KisProcessingApplicator::NONE, KisImageSignalVector(),
                                        kundo2_i18n("Smart Patch"));

    //actual inpaint operation. filling in areas masked by user
    applicator.applyCommand( new InpaintCommand( KisPainter::convertToAlphaAsAlpha(m_d->maskDev),
                                                 currentNode()->paintDevice(),
                                                 accuracy, patchRadius,
                                                 resources->activeSelection()),
                             KisStrokeJobData::BARRIER, KisStrokeJobData::EXCLUSIVE );

    applicator.end();
    image()->waitForDone();

    m_d->maskDev->clear();
}

PkPainterPath KisToolSmartPatch::brushOutline( void )
{
    const qreal diameter = m_d->brushRadius;
    PkPainterPath outline;
    outline.addEllipse(PkPointF(0,0), -0.5 * diameter, -0.5 * diameter );
    return outline;
}

PkPainterPath KisToolSmartPatch::getBrushOutlinePath(const PkPointF &documentPos,
                                          const KoPointerEvent *event)
{
    (void)event;

    PkPointF imagePos = currentImage()->documentToPixel(documentPos);
    PkPainterPath path = brushOutline();

    const KisCoordinatesConverter *converter = dynamic_cast<const KisCoordinatesConverter *>(canvas()->viewConverter());
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(converter, PkPainterPath());

    return path.translated(KisAlgebra2D::alignForZoom(imagePos, converter->effectivePhysicalZoom()));
}

void KisToolSmartPatch::requestUpdateOutline(const PkPointF &outlineDocPoint, const KoPointerEvent *event)
{
    static PkPointF lastDocPoint = PkPointF(0,0);
    if( event )
        lastDocPoint=outlineDocPoint;

    m_d->brushRadius = currentPaintOpPreset()->settings()->paintOpSize();
    m_d->brushOutline = getBrushOutlinePath(lastDocPoint, event);

    PkRectF outlinePixelRect = m_d->brushOutline.boundingRect();
    PkRectF outlineDocRect = currentImage()->pixelToDocument(outlinePixelRect);

    // This adjusted call is needed as we paint with a 3 pixel wide brush and the pen is outside the bounds of the path
    // Pen uses view coordinates so we have to zoom the document value to match 2 pixel in view coordinates
    // See BUG 275829
    qreal zoomX;
    qreal zoomY;
    canvas()->viewConverter()->zoom(&zoomX, &zoomY);
    qreal xoffset = 2.0/zoomX;
    qreal yoffset = 2.0/zoomY;

    if (!outlineDocRect.isEmpty()) {
        outlineDocRect.adjust(-xoffset,-yoffset,xoffset,yoffset);
    }

    if (!m_d->oldOutlineRect.isEmpty()) {
        canvas()->updateCanvas(m_d->oldOutlineRect);
    }

    if (!outlineDocRect.isEmpty()) {
        canvas()->updateCanvas(outlineDocRect);
    }

    m_d->oldOutlineRect = outlineDocRect;
}

void KisToolSmartPatch::paint(PkPainter &painter, const KoViewConverter &converter)
{
    (void)converter;

    painter.save();
    PkPainterPath path = pixelToView(m_d->brushOutline);
    paintToolOutline(&painter, path);
    painter.restore();

    painter.save();
    painter.setBrush(Qt::magenta);
    PkImage img = m_d->maskDev->convertToQImage(0);
    if( !img.size().isEmpty() ){
        painter.drawImage(pixelToView(m_d->maskDev->exactBounds()), img);
    }
    painter.restore();
}

KoToolBase *KisToolSmartPatchFactory::createTool(KoCanvasBase *canvas)
{
    return new KisToolSmartPatch(canvas);
}
