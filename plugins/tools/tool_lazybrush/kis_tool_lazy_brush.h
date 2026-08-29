/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TOOL_LAZY_BRUSH_H_
#define KIS_TOOL_LAZY_BRUSH_H_

#include <PkSet.h>

#include <PkScopedPointer.h>
#include <PkVariant.h>
#include <PkNamespace.h>
#include "kis_tool_freehand.h"

#include "KisToolPaintFactoryBase.h"

#include <flake/kis_node_shape.h>

#include <kconfig.h>
#include <kconfiggroup.h>

namespace PkNs = Qt;
using PkToolCursorShape = PkNs::CursorShape;
constexpr PkToolCursorShape PkToolArrowCursor = PkNs::ArrowCursor;
constexpr PkToolCursorShape PkToolPointingHandCursor = PkNs::PointingHandCursor;

class KoCanvasBase;

class KisToolLazyBrush : public KisToolFreehand
{
public:
    KisToolLazyBrush(KoCanvasBase * canvas);
    ~KisToolLazyBrush() override;

    void activatePrimaryAction() override;
    void deactivatePrimaryAction() override;

    void beginPrimaryAction(KoPointerEvent *event) override;
    void continuePrimaryAction(KoPointerEvent *event) override;
    void endPrimaryAction(KoPointerEvent *event) override;

    void activateAlternateAction(AlternateAction action) override;
    void deactivateAlternateAction(AlternateAction action) override;

    void beginAlternateAction(KoPointerEvent *event, AlternateAction action) override;
    void continueAlternateAction(KoPointerEvent *event, AlternateAction action) override;
    void endAlternateAction(KoPointerEvent *event, AlternateAction action) override;

    void explicitUserStrokeEndRequest() override;

protected:
    void resetCursorStyle() override;

public:
    void activate(const PkSet<KoShape*> &shapes) override;
    void deactivate() override;

private:
    void slotCanvasResourceChanged(int key, const PkVariant &value);
    void slotCurrentNodeChanged(KisNodeSP node);

private:
    bool colorizeMaskActive() const;
    bool canCreateColorizeMask() const;
    bool shouldActivateKeyStrokes() const;
    void tryCreateColorizeMask();

    void tryDisableKeyStrokesOnMask();

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};


class KisToolLazyBrushFactory : public KisToolPaintFactoryBase
{

public:
    KisToolLazyBrushFactory()
            : KisToolPaintFactoryBase("KritaShape/KisToolLazyBrush") {

        setToolTip(PkString("Colorize Mask Editing Tool"));

        // Temporarily
        setSection(ToolBoxSection::Fill);
        setPriority(3);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
    }

    ~KisToolLazyBrushFactory() override {}

    KoToolBase *createTool(KoCanvasBase *canvas) override;

};


#endif // KIS_TOOL_LAZY_BRUSH_H_
