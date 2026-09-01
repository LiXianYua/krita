/*
 *  SPDX-FileCopyrightText: 2017 Eugene Ingerman
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TOOL_SMART_PATCH_H_
#define KIS_TOOL_SMART_PATCH_H_

#include <PkScopedPointer.h>
#include <PkPainterPath.h>
#include <PkPainter.h>

#include "kis_tool_paint.h"

#include "KisToolPaintFactoryBase.h"

#include <flake/kis_node_shape.h>
#include <kconfig.h>
#include <kconfiggroup.h>

class KoCanvasBase;
class KisPaintInformation;
class KisSpacingInformation;


class KisToolSmartPatch : public KisToolPaint
{
public:
    KisToolSmartPatch(KoCanvasBase * canvas);
    ~KisToolSmartPatch() override;

    void activatePrimaryAction() override;
    void deactivatePrimaryAction() override;

    void beginPrimaryAction(KoPointerEvent *event) override;
    void continuePrimaryAction(KoPointerEvent *event) override;
    void endPrimaryAction(KoPointerEvent *event) override;
    void paint(PkPainter &painter, const KoViewConverter &converter) override;
    int flags() const override { return KisTool::FLAG_USES_CUSTOM_SIZE | KisTool::FLAG_USES_CUSTOM_PRESET; }

protected:
    void resetCursorStyle() override;

public:
    void activate(const PkSet<KoShape*> &shapes) override;
    void deactivate() override;

private:
    //PkRect inpaintImage(KisPaintDeviceSP maskDev, KisPaintDeviceSP imageDev);
    PkPainterPath getBrushOutlinePath(const PkPointF &documentPos, const KoPointerEvent *event);
    PkPainterPath brushOutline();
    void requestUpdateOutline(const PkPointF &outlineDocPoint, const KoPointerEvent *event) override;

private:
    struct Private;
    class InpaintCommand;
    const PkScopedPointer<Private> m_d;

    void addMaskPath(KoPointerEvent *event);
};


class KisToolSmartPatchFactory : public KisToolPaintFactoryBase
{

public:
    KisToolSmartPatchFactory()
        : KisToolPaintFactoryBase("KritaShape/KisToolSmartPatch")
    {

        setToolTip(PkString("Smart Patch Tool"));

        setSection(ToolBoxSection::Fill);
        setPriority(4);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
    }

    ~KisToolSmartPatchFactory() override {}

    KoToolBase *createTool(KoCanvasBase *canvas) override;

};


#endif // KIS_TOOL_SMART_PATCH_H_
