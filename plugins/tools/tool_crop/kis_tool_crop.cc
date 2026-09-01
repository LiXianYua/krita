/*
 *  kis_tool_crop.cc -- part of Krita
 *
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2005 Michael Thaler <michael.thaler@physik.tu-muenchen.de>
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2007 Adrian Page <adrian@pagenet.plus.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_crop.h"


#include <PkPainter.h>
#include <PkPen.h>
#include <PkRect.h>

#include <kis_debug.h>
#include <ksharedconfig.h>

#include <KoCanvasBase.h>
#include <kis_global.h>
#include <kis_painter.h>
#include <kis_image.h>
#include <kis_undo_adapter.h>
#include <KoPointerEvent.h>
#include <kis_selection.h>
#include <kis_layer.h>
#include <KisCanvasFeedback.h>
#include <kis_group_layer.h>
#include <kis_resources_snapshot.h>

#include <kundo2command.h>
#include <kis_crop_saved_extra_data.h>


struct DecorationLine
{
    PkPointF start;
    PkPointF end;
    enum Relation
    {
        Width,
        Height,
        Smallest,
        Largest
    };
    Relation startXRelation;
    Relation startYRelation;
    Relation endXRelation;
    Relation endYRelation;
};

DecorationLine decors[20] =
{
    //thirds
    {PkPointF(0.0, 0.3333),PkPointF(1.0, 0.3333), DecorationLine::Width, DecorationLine::Height, DecorationLine::Width, DecorationLine::Height},
    {PkPointF(0.0, 0.6666),PkPointF(1.0, 0.6666), DecorationLine::Width, DecorationLine::Height, DecorationLine::Width, DecorationLine::Height},
    {PkPointF(0.3333, 0.0),PkPointF(0.3333, 1.0), DecorationLine::Width, DecorationLine::Height, DecorationLine::Width, DecorationLine::Height},
    {PkPointF(0.6666, 0.0),PkPointF(0.6666, 1.0), DecorationLine::Width, DecorationLine::Height, DecorationLine::Width, DecorationLine::Height},

    //fifths
    {PkPointF(0.0, 0.2),PkPointF(1.0, 0.2), DecorationLine::Width, DecorationLine::Height, DecorationLine::Width, DecorationLine::Height},
    {PkPointF(0.0, 0.4),PkPointF(1.0, 0.4), DecorationLine::Width, DecorationLine::Height, DecorationLine::Width, DecorationLine::Height},
    {PkPointF(0.0, 0.6),PkPointF(1.0, 0.6), DecorationLine::Width, DecorationLine::Height, DecorationLine::Width, DecorationLine::Height},
    {PkPointF(0.0, 0.8),PkPointF(1.0, 0.8), DecorationLine::Width, DecorationLine::Height, DecorationLine::Width, DecorationLine::Height},
    {PkPointF(0.2, 0.0),PkPointF(0.2, 1.0), DecorationLine::Width, DecorationLine::Height, DecorationLine::Width, DecorationLine::Height},
    {PkPointF(0.4, 0.0),PkPointF(0.4, 1.0), DecorationLine::Width, DecorationLine::Height, DecorationLine::Width, DecorationLine::Height},
    {PkPointF(0.6, 0.0),PkPointF(0.6, 1.0), DecorationLine::Width, DecorationLine::Height, DecorationLine::Width, DecorationLine::Height},
    {PkPointF(0.8, 0.0),PkPointF(0.8, 1.0), DecorationLine::Width, DecorationLine::Height, DecorationLine::Width, DecorationLine::Height},

    // Passport photo
    {PkPointF(0.0, 0.45/0.35),PkPointF(1.0, 0.45/0.35), DecorationLine::Width, DecorationLine::Width, DecorationLine::Width, DecorationLine::Width},
    {PkPointF(0.2, 0.05/0.35),PkPointF(0.8, 0.05/0.35), DecorationLine::Width, DecorationLine::Width, DecorationLine::Width, DecorationLine::Width},
    {PkPointF(0.2, 0.40/0.35),PkPointF(0.8, 0.40/0.35), DecorationLine::Width, DecorationLine::Width, DecorationLine::Width, DecorationLine::Width},
    {PkPointF(0.25, 0.07/0.35),PkPointF(0.75, 0.07/0.35), DecorationLine::Width, DecorationLine::Width, DecorationLine::Width, DecorationLine::Width},
    {PkPointF(0.25, 0.38/0.35),PkPointF(0.75, 0.38/0.35), DecorationLine::Width, DecorationLine::Width, DecorationLine::Width, DecorationLine::Width},
    {PkPointF(0.35/0.45, 0.0),PkPointF(0.35/0.45, 1.0), DecorationLine::Height, DecorationLine::Height, DecorationLine::Height, DecorationLine::Height},

    //Crosshair
    {PkPointF(0.0, 0.5),PkPointF(1.0, 0.5), DecorationLine::Width, DecorationLine::Height, DecorationLine::Width, DecorationLine::Height},
    {PkPointF(0.5, 0.0),PkPointF(0.5, 1.0), DecorationLine::Width, DecorationLine::Height, DecorationLine::Width, DecorationLine::Height}
};

#define DECORATION_COUNT 5
const int decorsIndex[DECORATION_COUNT] = {0,4,12,18,20};

KisToolCrop::KisToolCrop(KoCanvasBase * canvas)
        : KisTool(canvas, Qt::ArrowCursor)
{
    setObjectName("tool_crop");
    m_handleSize = 13;
    m_haveCropSelection = false;
    m_cropTypeSelectable = false;
    m_cropType = ImageCropType;
    m_decoration = 1;

    PkObject::connect(&m_finalRect, &KisConstrainedRect::sigValuesChanged,
                      this, &KisToolCrop::slotRectChanged);
    PkObject::connect(&m_finalRect, &KisConstrainedRect::sigLockValuesChanged,
                      this, &KisToolCrop::slotRectChanged);
}

KisToolCrop::~KisToolCrop()
{
}

void KisToolCrop::cropTypeSelectableChanged() { activateSignal<>(this, PkMemberFnKey::from(&KisToolCrop::cropTypeSelectableChanged)); }
void KisToolCrop::cropTypeChanged(int value) { activateSignal<int>(this, PkMemberFnKey::from(&KisToolCrop::cropTypeChanged), value); }
void KisToolCrop::decorationChanged(int value) { activateSignal<int>(this, PkMemberFnKey::from(&KisToolCrop::decorationChanged), value); }
void KisToolCrop::cropXChanged(int value) { activateSignal<int>(this, PkMemberFnKey::from(&KisToolCrop::cropXChanged), value); }
void KisToolCrop::cropYChanged(int value) { activateSignal<int>(this, PkMemberFnKey::from(&KisToolCrop::cropYChanged), value); }
void KisToolCrop::cropWidthChanged(int value) { activateSignal<int>(this, PkMemberFnKey::from(&KisToolCrop::cropWidthChanged), value); }
void KisToolCrop::cropHeightChanged(int value) { activateSignal<int>(this, PkMemberFnKey::from(&KisToolCrop::cropHeightChanged), value); }
void KisToolCrop::ratioChanged(double value) { activateSignal<double>(this, PkMemberFnKey::from(&KisToolCrop::ratioChanged), value); }
void KisToolCrop::lockWidthChanged(bool value) { activateSignal<bool>(this, PkMemberFnKey::from(&KisToolCrop::lockWidthChanged), value); }
void KisToolCrop::lockHeightChanged(bool value) { activateSignal<bool>(this, PkMemberFnKey::from(&KisToolCrop::lockHeightChanged), value); }
void KisToolCrop::lockRatioChanged(bool value) { activateSignal<bool>(this, PkMemberFnKey::from(&KisToolCrop::lockRatioChanged), value); }
void KisToolCrop::canGrowChanged(bool value) { activateSignal<bool>(this, PkMemberFnKey::from(&KisToolCrop::canGrowChanged), value); }
void KisToolCrop::isCenteredChanged(bool value) { activateSignal<bool>(this, PkMemberFnKey::from(&KisToolCrop::isCenteredChanged), value); }

void KisToolCrop::activate(const PkSet<KoShape*> &shapes)
{

    KisTool::activate(shapes);
    configGroup =  KSharedConfig::openConfig()->group(toolId()); // save settings to kritarc

    KisResourcesSnapshotSP resources =
        new KisResourcesSnapshot(image(), currentNode(), this->canvas()->resourceManager()->canvasResourcesInterface());


    // load settings from configuration
    setGrowCenter(configGroup.readEntry("growCenter", false));
    setAllowGrow(configGroup.readEntry("allowGrow", true));

    // Default: thirds decoration
    setDecoration(configGroup.readEntry("decoration", 1));

    // Default: crop the entire image
    setCropType(CropToolType(configGroup.readEntry("cropType", 0)));

    m_finalRect.setCropRect(image()->bounds());

    KisSelectionSP sel = resources->activeSelection();
    if (sel) {
        m_haveCropSelection = true;
        m_finalRect.setRectInitial(sel->selectedExactRect());
    }
    useCursor(cursor());

    //pixel layer
    if(resources->currentNode() && resources->currentNode()->paintDevice()) {
        setCropTypeSelectable(true);
    }
    //vector layer
    else {
        if (m_cropType != ImageCropType && m_cropType != CanvasCropType) {
            setCropType(ImageCropType);
        }
        setCropTypeSelectable(false);
    }
    PkObject::connect(&m_finalRect, &KisConstrainedRect::sigValuesChanged,
                      this, &KisToolCrop::showSizeOnCanvas);
}

void KisToolCrop::cancelStroke()
{
    m_haveCropSelection = false;
    useCursor(cursor());
    doCanvasUpdate(image()->bounds());
}

void KisToolCrop::deactivate()
{
    cancelStroke();
    KisTool::deactivate();
}

void KisToolCrop::requestStrokeEnd()
{
    if (m_haveCropSelection) crop();
}

void KisToolCrop::requestStrokeCancellation()
{
    cancelStroke();
}

void KisToolCrop::requestUndoDuringStroke()
{
    cancelStroke();
}

void KisToolCrop::requestRedoDuringStroke()
{
    cancelStroke();
}

void KisToolCrop::canvasResourceChanged(int key, const PkVariant &res)
{
    KisTool::canvasResourceChanged(key, res);

    //pixel layer
    if(currentNode() && currentNode()->paintDevice()) {
        setCropTypeSelectable(true);
    }
    //vector layer
    else {
        if (m_cropType != ImageCropType && m_cropType != CanvasCropType) {
            setCropType(ImageCropType);
        }
        setCropTypeSelectable(false);
    }
}

void KisToolCrop::paint(PkPainter &painter, const KoViewConverter &converter)
{
    (void)converter;
    paintOutlineWithHandles(painter);
}

PkString KisToolCrop::cropActionsSection() const
{
    return PkString("Crop Tool Actions");
}

PkList<KisToolCrop::CropAction> KisToolCrop::cropActions() const
{
    PkList<CropAction> actions;

    if (m_haveCropSelection) {
        actions.append({CropActionId::ApplyCrop, PkString("Crop"), false, false, 0});
    }

    actions.append({CropActionId::Center, PkString("Center"), true, growCenter(), 1});
    actions.append({CropActionId::Grow, PkString("Grow"), true, allowGrow(), 1});
    actions.append({CropActionId::LockWidth, PkString("Lock Width"), true, lockWidth(), 2});
    actions.append({CropActionId::LockHeight, PkString("Lock Height"), true, lockHeight(), 2});
    actions.append({CropActionId::LockRatio, PkString("Lock Ratio"), true, lockRatio(), 2});
    return actions;
}

bool KisToolCrop::triggerCropAction(CropActionId action, bool checked)
{
    switch (action) {
    case CropActionId::ApplyCrop:
        if (!m_haveCropSelection) {
            return false;
        }
        crop();
        return true;
    case CropActionId::Center:
        setGrowCenter(checked);
        return true;
    case CropActionId::Grow:
        setAllowGrow(checked);
        return true;
    case CropActionId::LockWidth:
        setLockWidth(checked);
        return true;
    case CropActionId::LockHeight:
        setLockHeight(checked);
        return true;
    case CropActionId::LockRatio:
        setLockRatio(checked);
        return true;
    }
    return false;
}

void KisToolCrop::beginPrimaryAction(KoPointerEvent *event)
{
    m_finalRect.setCropRect(image()->bounds());
    setMode(KisTool::PAINT_MODE);

    const PkPointF imagePoint = convertToPixelCoord(event);
    m_mouseOnHandleType = mouseOnHandle(pixelToView(imagePoint));

    if (m_mouseOnHandleType != KisConstrainedRect::None) {
        PkPointF snapPoint = m_finalRect.handleSnapPoint(KisConstrainedRect::HandleType(m_mouseOnHandleType), imagePoint);
        PkPointF snapDocPoint = image()->pixelToDocument(snapPoint);
        m_dragOffsetDoc = snapDocPoint - event->point;
    } else {
        m_dragOffsetDoc = PkPointF();
    }

    PkPointF snappedPoint = convertToPixelCoordAndSnap(event, m_dragOffsetDoc);

    m_dragStart = snappedPoint.toPoint();
    m_resettingStroke = false;

    if (!m_haveCropSelection || m_mouseOnHandleType == None) {
        m_lastCanvasUpdateRect = image()->bounds();
        const int initialWidth = m_finalRect.widthLocked() ? m_finalRect.rect().width() : 1;
        const int initialHeight = m_finalRect.heightLocked() ? m_finalRect.rect().height() : 1;
        const PkRect initialRect = PkRect(m_dragStart, PkSize(initialWidth, initialHeight));
        m_finalRect.setRectInitial(initialRect);
        m_initialDragRect = initialRect;
        m_mouseOnHandleType = KisConstrainedRect::Creation;
        m_resettingStroke = true;
    } else {
        m_initialDragRect = m_finalRect.rect();
    }
}

void KisToolCrop::continuePrimaryAction(KoPointerEvent *event)
{
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);

    const PkPointF pos = convertToPixelCoordAndSnap(event, m_dragOffsetDoc);
    const PkPoint drag = pos.toPoint() - m_dragStart;

    m_finalRect.moveHandle(KisConstrainedRect::HandleType(m_mouseOnHandleType), drag, m_initialDragRect);
}

bool KisToolCrop::tryContinueLastCropAction()
{
    bool result = false;

    const KUndo2Command *lastCommand = image()->undoAdapter()->presentCommand();
    const KisCropSavedExtraData *data = 0;

    if ((lastCommand = image()->undoAdapter()->presentCommand()) &&
        (data = dynamic_cast<const KisCropSavedExtraData*>(lastCommand->extraData()))) {

        bool cropImageConsistent =
            m_cropType == ImageCropType &&
            (data->type() == KisCropSavedExtraData::CROP_IMAGE ||
             data->type() == KisCropSavedExtraData::RESIZE_IMAGE);

        bool cropLayerConsistent =
            m_cropType == LayerCropType &&
            data->type() == KisCropSavedExtraData::CROP_LAYER &&
            currentNode() == data->cropNode();


        if (cropImageConsistent || cropLayerConsistent) {
            image()->undoAdapter()->undoLastCommand();
            image()->waitForDone();

            m_finalRect.setRectInitial(data->cropRect());
            m_haveCropSelection = true;

            result = true;
        }
    }

    return result;
}

void KisToolCrop::endPrimaryAction(KoPointerEvent *event)
{
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);
    setMode(KisTool::HOVER_MODE);

    PkRectF viewCropRect = pixelToView(m_finalRect.rect());
    const bool haveValidRect =
        viewCropRect.width() > m_handleSize &&
        viewCropRect.height() > m_handleSize;


    if (!m_haveCropSelection && !haveValidRect) {
        if (!tryContinueLastCropAction()) {
            m_finalRect.setRectInitial(image()->bounds());
            m_haveCropSelection = true;
        }
    } else if (m_resettingStroke && !haveValidRect) {
        m_lastCanvasUpdateRect = image()->bounds();
        m_haveCropSelection = false;
    } else {
        m_haveCropSelection = true;
    }

    m_finalRect.normalize();

    qint32 type = mouseOnHandle(pixelToView(convertToPixelCoordAndSnap(event, m_dragOffsetDoc)));
    setMoveResizeCursor(type);
}

void KisToolCrop::mouseMoveEvent(KoPointerEvent *event)
{
    PkPointF pos = convertToPixelCoordAndSnap(event);

    if (m_haveCropSelection) {  //if the crop selection is set
        //set resize cursor if we are on one of the handles
        if(mode() == KisTool::PAINT_MODE) {
            //keep the same cursor as the one we clicked with
            setMoveResizeCursor(m_mouseOnHandleType);
        }else{
            //hovering
            qint32 type = mouseOnHandle(pixelToView(pos));
            setMoveResizeCursor(type);
        }
    }
}

void KisToolCrop::beginPrimaryDoubleClickAction(KoPointerEvent *event)
{
    if (m_haveCropSelection) crop();

    // this action will have no continuation
    event->ignore();
}


#define BORDER_LINE_WIDTH 0
#define HALF_BORDER_LINE_WIDTH 0
#define HANDLE_BORDER_LINE_WIDTH 1

PkRectF KisToolCrop::borderLineRect()
{
    PkRectF borderRect = pixelToView(m_finalRect.rect());

    // Draw the border line right next to the crop rectangle perimeter.
    borderRect.adjust(-HALF_BORDER_LINE_WIDTH, -HALF_BORDER_LINE_WIDTH, HALF_BORDER_LINE_WIDTH, HALF_BORDER_LINE_WIDTH);

    return borderRect;
}

#define OUTSIDE_CROP_ALPHA 200

void KisToolCrop::paintOutlineWithHandles(PkPainter& gc)
{
    if (canvas() && (mode() == KisTool::PAINT_MODE || m_haveCropSelection)) {
        gc.save();

        PkRectF wholeImageRect = pixelToView(image()->bounds());
        PkRectF borderRect = borderLineRect();

        PkPainterPath path;

        path.addRect(wholeImageRect);
        path.addRect(borderRect);
        gc.setPen(Qt::NoPen);
        gc.setBrush(PkColor(0, 0, 0, OUTSIDE_CROP_ALPHA));
        gc.drawPath(path);

        // Handles
        PkPen pen(Qt::SolidLine);
        pen.setWidth(HANDLE_BORDER_LINE_WIDTH * decorationThickness());
        pen.setColor(Qt::black);
        pen.setCosmetic(true);
        gc.setPen(pen);
        gc.setBrush(PkColor(200, 200, 200, OUTSIDE_CROP_ALPHA));
        gc.drawPath(handlesPath());

        gc.setClipRect(borderRect, Qt::IntersectClip);

        if (m_decoration > 0) {
            for (int i = decorsIndex[m_decoration-1]; i<decorsIndex[m_decoration]; i++) {
                drawDecorationLine(&gc, &(decors[i]), borderRect);
            }
        }
        gc.restore();
    }
}

void KisToolCrop::crop()
{
    KIS_ASSERT_RECOVER_RETURN(currentImage());
    if (m_finalRect.rect().isEmpty()) return;

    const bool imageCrop = m_cropType == ImageCropType || m_cropType == CanvasCropType;

    if (!imageCrop) {
        //Cropping layer
        if (!nodeEditable()) {
            return;
        }
    }

    m_haveCropSelection = false;
    useCursor(cursor());

    PkRect cropRect = m_finalRect.rect();

    // The visitor adds the undo steps to the macro
    if (imageCrop || !currentNode()->paintDevice()) {
        if (m_cropType == CanvasCropType) {
            currentImage()->resizeImage(cropRect);
        } else {
            currentImage()->cropImage(cropRect);
        }
    } else {
        currentImage()->cropNode(currentNode(), cropRect, m_cropType == FrameCropType);
    }
}

void KisToolCrop::setCropTypeLegacy(int cropType)
{
    setCropType(static_cast<KisToolCrop::CropToolType>(cropType));
}

void KisToolCrop::setCropType(KisToolCrop::CropToolType cropType)
{
    if(m_cropType == cropType)
        return;
    m_cropType = cropType;

    configGroup.writeEntry("cropType", static_cast<int>(cropType));

    cropTypeChanged(m_cropType);
}

KisToolCrop::CropToolType KisToolCrop::cropType() const
{
    return m_cropType;
}

void KisToolCrop::setCropTypeSelectable(bool selectable)
{
    if(selectable == m_cropTypeSelectable)
        return;
    m_cropTypeSelectable = selectable;
    cropTypeSelectableChanged();
}

bool KisToolCrop::cropTypeSelectable() const
{
    return m_cropTypeSelectable;
}

int KisToolCrop::decoration() const
{
    return m_decoration;
}

void KisToolCrop::setDecoration(int i)
{
    // This shouldn't happen, but safety first
    if(i < 0 || i > DECORATION_COUNT)
        return;
    m_decoration = i;
    decorationChanged(decoration());
    updateCanvasViewRect(boundingRect());

    configGroup.writeEntry("decoration", i);
}

void KisToolCrop::doCanvasUpdate(const PkRect &updateRect)
{
    updateCanvasViewRect(updateRect | m_lastCanvasUpdateRect);
    m_lastCanvasUpdateRect = updateRect;
}

void KisToolCrop::slotRectChanged()
{
    cropHeightChanged(cropHeight());
    cropWidthChanged(cropWidth());
    cropXChanged(cropX());
    cropYChanged(cropY());
    ratioChanged(ratio());
    lockHeightChanged(lockHeight());
    lockWidthChanged(lockWidth());
    lockRatioChanged(lockRatio());

    canGrowChanged(allowGrow());
    isCenteredChanged(growCenter());

    doCanvasUpdate(boundingRect().toAlignedRect());
}

void KisToolCrop::setCropX(int x)
{
    if(x == m_finalRect.rect().x())
        return;

    if (!m_haveCropSelection) {
        m_haveCropSelection = true;
        m_finalRect.setRectInitial(image()->bounds());
    }

    PkPoint offset = m_finalRect.rect().topLeft();
    offset.setX(x);
    m_finalRect.setOffset(offset);
}

int KisToolCrop::cropX() const
{
    return m_finalRect.rect().x();
}

void KisToolCrop::setCropY(int y)
{
    if(y == m_finalRect.rect().y())
        return;

    if (!m_haveCropSelection) {
        m_haveCropSelection = true;
        m_finalRect.setRectInitial(image()->bounds());
    }

    PkPoint offset = m_finalRect.rect().topLeft();
    offset.setY(y);
    m_finalRect.setOffset(offset);
}

int KisToolCrop::cropY() const
{
    return m_finalRect.rect().y();
}

void KisToolCrop::setCropWidth(int w)
{
    if(w == m_finalRect.rect().width())
        return;

    if (!m_haveCropSelection) {
        m_haveCropSelection = true;
        m_finalRect.setRectInitial(image()->bounds());
    }

    m_finalRect.setWidth(w);
}

int KisToolCrop::cropWidth() const
{
    return m_finalRect.rect().width();
}

void KisToolCrop::setLockWidth(bool lock)
{
    m_finalRect.setWidthLocked(lock);
}

bool KisToolCrop::lockWidth() const
{
    return m_finalRect.widthLocked();
}

void KisToolCrop::setCropHeight(int h)
{
    if(h == m_finalRect.rect().height())
        return;

    if (!m_haveCropSelection) {
        m_haveCropSelection = true;
        m_finalRect.setRectInitial(image()->bounds());
    }

    m_finalRect.setHeight(h);
}

int KisToolCrop::cropHeight() const
{
    return m_finalRect.rect().height();
}

void KisToolCrop::setLockHeight(bool lock)
{
    m_finalRect.setHeightLocked(lock);
}

bool KisToolCrop::lockHeight() const
{
    return m_finalRect.heightLocked();
}

void KisToolCrop::setAllowGrow(bool g)
{
    m_finalRect.setCanGrow(g);
    m_finalRect.setCropRect(image()->bounds());
    configGroup.writeEntry("allowGrow", g);

    // Do a dummy move for the crop area to snap back to the canvas if grow is no longer allowed
    if (!g) {
        m_finalRect.moveHandle(KisConstrainedRect::HandleType::Inside, PkPoint(0, 0), m_finalRect.rect());
    }

    canGrowChanged(g);
}

bool KisToolCrop::allowGrow() const
{
    return m_finalRect.canGrow();
}

void KisToolCrop::setGrowCenter(bool value)
{
    m_finalRect.setCentered(value);


    configGroup.writeEntry("growCenter", value);

    isCenteredChanged(value);
}

bool KisToolCrop::growCenter() const
{
    return m_finalRect.centered();
}

void KisToolCrop::setRatio(double ratio)
{
    if(ratio == m_finalRect.ratio())
        return;

    if (!m_haveCropSelection) {
        m_haveCropSelection = true;
        m_finalRect.setRectInitial(image()->bounds());
    }

    m_finalRect.setRatio(ratio);
}

double KisToolCrop::ratio() const
{
    return m_finalRect.ratio();
}

void KisToolCrop::setLockRatio(bool lock)
{
    m_finalRect.setRatioLocked(lock);
}

bool KisToolCrop::lockRatio() const
{
    return m_finalRect.ratioLocked();
}

void KisToolCrop::showSizeOnCanvas()
{
    KisCanvasFeedback *feedback = dynamic_cast<KisCanvasFeedback*>(canvas());
    KIS_SAFE_ASSERT_RECOVER_RETURN(feedback);
    if(m_mouseOnHandleType == 9) {
        feedback->showFloatingMessage(PkString("X: %1\nY: %2").arg(cropX()).arg(cropY()),
                                      {}, 1000, KisCanvasFeedback::Priority::High,
                                      Qt::AlignLeft | Qt::TextWordWrap | Qt::AlignVCenter);
    }
    else {
        feedback->showFloatingMessage(PkString("Width: %1\nHeight: %2").arg(cropWidth()).arg(cropHeight()),
                                      {}, 1000, KisCanvasFeedback::Priority::High,
                                      Qt::AlignLeft | Qt::TextWordWrap | Qt::AlignVCenter);
    }
}

PkRectF KisToolCrop::lowerRightHandleRect(PkRectF cropBorderRect)
{
    return PkRectF(cropBorderRect.right() - m_handleSize / 2.0, cropBorderRect.bottom() - m_handleSize / 2.0, m_handleSize, m_handleSize);
}

PkRectF KisToolCrop::upperRightHandleRect(PkRectF cropBorderRect)
{
    return PkRectF(cropBorderRect.right() - m_handleSize / 2.0 , cropBorderRect.top() - m_handleSize / 2.0, m_handleSize, m_handleSize);
}

PkRectF KisToolCrop::lowerLeftHandleRect(PkRectF cropBorderRect)
{
    return PkRectF(cropBorderRect.left() - m_handleSize / 2.0 , cropBorderRect.bottom() - m_handleSize / 2.0, m_handleSize, m_handleSize);
}

PkRectF KisToolCrop::upperLeftHandleRect(PkRectF cropBorderRect)
{
    return PkRectF(cropBorderRect.left() - m_handleSize / 2.0, cropBorderRect.top() - m_handleSize / 2.0, m_handleSize, m_handleSize);
}

PkRectF KisToolCrop::lowerHandleRect(PkRectF cropBorderRect)
{
    return PkRectF(cropBorderRect.left() + (cropBorderRect.width() - m_handleSize) / 2.0 , cropBorderRect.bottom() - m_handleSize / 2.0, m_handleSize, m_handleSize);
}

PkRectF KisToolCrop::rightHandleRect(PkRectF cropBorderRect)
{
    return PkRectF(cropBorderRect.right() - m_handleSize / 2.0 , cropBorderRect.top() + (cropBorderRect.height() - m_handleSize) / 2.0, m_handleSize, m_handleSize);
}

PkRectF KisToolCrop::upperHandleRect(PkRectF cropBorderRect)
{
    return PkRectF(cropBorderRect.left() + (cropBorderRect.width() - m_handleSize) / 2.0 , cropBorderRect.top() - m_handleSize / 2.0, m_handleSize, m_handleSize);
}

PkRectF KisToolCrop::leftHandleRect(PkRectF cropBorderRect)
{
    return PkRectF(cropBorderRect.left() - m_handleSize / 2.0, cropBorderRect.top() + (cropBorderRect.height() - m_handleSize) / 2.0, m_handleSize, m_handleSize);
}

PkPainterPath KisToolCrop::handlesPath()
{
    PkRectF cropBorderRect = borderLineRect();
    PkPainterPath path;

    path.addRect(upperLeftHandleRect(cropBorderRect));
    path.addRect(upperRightHandleRect(cropBorderRect));
    path.addRect(lowerLeftHandleRect(cropBorderRect));
    path.addRect(lowerRightHandleRect(cropBorderRect));
    path.addRect(upperHandleRect(cropBorderRect));
    path.addRect(lowerHandleRect(cropBorderRect));
    path.addRect(leftHandleRect(cropBorderRect));
    path.addRect(rightHandleRect(cropBorderRect));

    return path;
}

qint32 KisToolCrop::mouseOnHandle(PkPointF currentViewPoint)
{
    PkRectF borderRect = borderLineRect();
    qint32 handleType = None;

    if (!m_haveCropSelection) {
        return None;
    }

    if (upperLeftHandleRect(borderRect).contains(currentViewPoint)) {
        handleType = UpperLeft;
    } else if (lowerLeftHandleRect(borderRect).contains(currentViewPoint)) {
        handleType = LowerLeft;
    } else if (upperRightHandleRect(borderRect).contains(currentViewPoint)) {
        handleType = UpperRight;
    } else if (lowerRightHandleRect(borderRect).contains(currentViewPoint)) {
        handleType = LowerRight;
    } else if (upperHandleRect(borderRect).contains(currentViewPoint)) {
        handleType = Upper;
    } else if (lowerHandleRect(borderRect).contains(currentViewPoint)) {
        handleType = Lower;
    } else if (leftHandleRect(borderRect).contains(currentViewPoint)) {
        handleType = Left;
    } else if (rightHandleRect(borderRect).contains(currentViewPoint)) {
        handleType = Right;
    } else if (borderRect.contains(currentViewPoint)) {
        handleType = Inside;
    }

    return handleType;
}

void KisToolCrop::setMoveResizeCursor(qint32 handle)
{
    Qt::CursorShape cursorType = Qt::ArrowCursor;

    switch (handle) {
    case(UpperLeft):
    case(LowerRight):
        cursorType = Qt::SizeFDiagCursor;
        break;
    case(LowerLeft):
    case(UpperRight):
        cursorType = Qt::SizeBDiagCursor;
        break;
    case(Upper):
    case(Lower):
        cursorType = Qt::SizeVerCursor;
        break;
    case(Left):
    case(Right):
        cursorType = Qt::SizeHorCursor;
        break;
    case(Inside):
        cursorType = Qt::SizeAllCursor;
        break;
    default:
        if (m_haveCropSelection) {
            cursorType = Qt::ArrowCursor;
        } else {
            cursorType = cursor();
        }
        break;
    }
    useCursor(cursorType);
}

PkRectF KisToolCrop::boundingRect()
{
    PkRectF rect = handlesPath().boundingRect();
    rect.adjust(-HANDLE_BORDER_LINE_WIDTH, -HANDLE_BORDER_LINE_WIDTH, HANDLE_BORDER_LINE_WIDTH, HANDLE_BORDER_LINE_WIDTH);
    return rect;
}

void KisToolCrop::drawDecorationLine(PkPainter *p, DecorationLine *decorLine, const PkRectF rect)
{
    PkPointF start = rect.topLeft();
    PkPointF end = rect.topLeft();
    qreal small = qMin(rect.width(), rect.height());
    qreal large = qMax(rect.width(), rect.height());

    switch (decorLine->startXRelation) {
    case DecorationLine::Width:
        start.setX(start.x() + decorLine->start.x() * rect.width());
        break;
    case DecorationLine::Height:
        start.setX(start.x() + decorLine->start.x() * rect.height());
        break;
    case DecorationLine::Smallest:
        start.setX(start.x() + decorLine->start.x() * small);
        break;
    case DecorationLine::Largest:
        start.setX(start.x() + decorLine->start.x() * large);
        break;
    }

    switch (decorLine->startYRelation) {
    case DecorationLine::Width:
        start.setY(start.y() + decorLine->start.y() * rect.width());
        break;
    case DecorationLine::Height:
        start.setY(start.y() + decorLine->start.y() * rect.height());
        break;
    case DecorationLine::Smallest:
        start.setY(start.y() + decorLine->start.y() * small);
        break;
    case DecorationLine::Largest:
        start.setY(start.y() + decorLine->start.y() * large);
        break;
    }

    switch (decorLine->endXRelation) {
    case DecorationLine::Width:
        end.setX(end.x() + decorLine->end.x() * rect.width());
        break;
    case DecorationLine::Height:
        end.setX(end.x() + decorLine->end.x() * rect.height());
        break;
    case DecorationLine::Smallest:
        end.setX(end.x() + decorLine->end.x() * small);
        break;
    case DecorationLine::Largest:
        end.setX(end.x() + decorLine->end.x() * large);
        break;
    }

    switch (decorLine->endYRelation) {
    case DecorationLine::Width:
        end.setY(end.y() + decorLine->end.y() * rect.width());
        break;
    case DecorationLine::Height:
        end.setY(end.y() + decorLine->end.y() * rect.height());
        break;
    case DecorationLine::Smallest:
        end.setY(end.y() + decorLine->end.y() * small);
        break;
    case DecorationLine::Largest:
        end.setY(end.y() + decorLine->end.y() * large);
        break;
    }

    p->drawLine(start, end);
}

KoToolBase *KisToolCropFactory::createTool(KoCanvasBase *canvas)
{
    return new KisToolCrop(canvas);
}
