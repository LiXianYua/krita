/*
 *  kis_tool_crop.h - part of Krita
 *
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TOOL_CROP_H_
#define KIS_TOOL_CROP_H_

#include <PkPoint.h>
#include <PkList.h>
#include <PkPainter.h>
#include <PkPainterPath.h>
#include <PkSet.h>
#include <PkString.h>
#include <PkVariant.h>


#include <kconfig.h>
#include <kconfiggroup.h>

#include <KoToolFactoryBase.h>
#include "kis_tool.h"
#include "flake/kis_node_shape.h"
#include "kis_constrained_rect.h"

struct DecorationLine;


/**
 * Crop tool
 */
class KisToolCrop : public KisTool
{

public:
    enum CropToolType {
        ImageCropType,
        CanvasCropType,
        LayerCropType,
        FrameCropType
    };

    KisToolCrop(KoCanvasBase * canvas);
    ~KisToolCrop() override;

    void beginPrimaryAction(KoPointerEvent *event) override;
    void continuePrimaryAction(KoPointerEvent *event) override;
    void endPrimaryAction(KoPointerEvent *event) override;
    void beginPrimaryDoubleClickAction(KoPointerEvent *event) override;

    void mouseMoveEvent(KoPointerEvent *e) override;
    void canvasResourceChanged(int key, const PkVariant &res) override;

    void paint(PkPainter &painter, const KoViewConverter &converter) override;

    CropToolType cropType() const;
    bool cropTypeSelectable() const;
    int cropX() const;
    int cropY() const;
    int cropWidth() const;
    bool lockWidth() const;
    int cropHeight() const;
    bool lockHeight() const;
    double ratio() const;
    bool lockRatio() const;
    int decoration() const;
    bool growCenter() const;
    bool allowGrow() const;

public:
    void cropTypeSelectableChanged();
    void cropTypeChanged(int value);
    void decorationChanged(int value);

    void cropXChanged(int value);
    void cropYChanged(int value);
    void cropWidthChanged(int value);
    void cropHeightChanged(int value);

    void ratioChanged(double value);

    void lockWidthChanged(bool value);
    void lockHeightChanged(bool value);
    void lockRatioChanged(bool value);

    void canGrowChanged(bool value);
    void isCenteredChanged(bool value);

public:

    void activate(const PkSet<KoShape*> &shapes) override;
    void deactivate() override;

    void requestStrokeEnd() override;
    void requestStrokeCancellation() override;
    void requestUndoDuringStroke() override;
    void requestRedoDuringStroke() override;

    void crop();

    void showSizeOnCanvas();

    void setCropTypeLegacy(int cropType);
    void setCropType(CropToolType cropType);
    void setCropTypeSelectable(bool selectable);
    void setCropX(int x);
    void setCropY(int y);
    void setCropWidth(int x);
    void setLockWidth(bool lock);
    void setCropHeight(int y);
    void setLockHeight(bool lock);
    void setRatio(double ratio);
    void setLockRatio(bool lock);
    void setDecoration(int i);
    void setAllowGrow(bool g);
    void setGrowCenter(bool g);

    void slotRectChanged();

private:
    void doCanvasUpdate(const PkRect &updateRect);

private:
    void cancelStroke();
    PkRectF boundingRect();
    PkRectF borderLineRect();
    PkPainterPath handlesPath();
    void paintOutlineWithHandles(PkPainter& gc);
    qint32 mouseOnHandle(const PkPointF currentViewPoint);
    void setMoveResizeCursor(qint32 handle);
    PkRectF lowerRightHandleRect(PkRectF cropBorderRect);
    PkRectF upperRightHandleRect(PkRectF cropBorderRect);
    PkRectF lowerLeftHandleRect(PkRectF cropBorderRect);
    PkRectF upperLeftHandleRect(PkRectF cropBorderRect);
    PkRectF lowerHandleRect(PkRectF cropBorderRect);
    PkRectF rightHandleRect(PkRectF cropBorderRect);
    PkRectF upperHandleRect(PkRectF cropBorderRect);
    PkRectF leftHandleRect(PkRectF cropBorderRect);
    void drawDecorationLine(PkPainter *p, DecorationLine *decorLine, PkRectF rect);

    bool tryContinueLastCropAction();

private:
    PkPoint m_dragStart;

    qint32 m_handleSize {13};
    bool m_haveCropSelection {false};
    qint32 m_mouseOnHandleType {0};

    CropToolType m_cropType {ImageCropType};
    bool m_cropTypeSelectable {false};

    int m_decoration {1};
    bool m_resettingStroke {false};
    PkRect m_lastCanvasUpdateRect;

    KConfigGroup configGroup;

    enum handleType {
        None = 0,
        UpperLeft = 1,
        UpperRight = 2,
        LowerLeft = 3,
        LowerRight = 4,
        Upper = 5,
        Lower = 6,
        Left = 7,
        Right = 8,
        Inside = 9
    };
    PkList<DecorationLine *> m_decorations;

    KisConstrainedRect m_finalRect;
    PkRect m_initialDragRect;
    PkPointF m_dragOffsetDoc;
};

class KisToolCropFactory : public KoToolFactoryBase
{

public:
    KisToolCropFactory()
            : KoToolFactoryBase("KisToolCrop") {
        setToolTip(PkString("Crop Tool"));
        setSection(ToolBoxSection::Transform);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
        setPriority(11);
        setShortcut(PkString("C"));
    }

    ~KisToolCropFactory() override {}

    KoToolBase *createTool(KoCanvasBase *canvas) override;

};



#endif // KIS_TOOL_CROP_H_
