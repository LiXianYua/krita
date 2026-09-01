/*
 *  SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TOOL_PATH_H_
#define KIS_TOOL_PATH_H_

#include <KoCreatePathTool.h>
#include <KoToolFactoryBase.h>
#include <PkPainter.h>
#include <PkString.h>

#include "flake/kis_node_shape.h"
#include "kis_tool_shape.h"
#include "kis_delegated_tool.h"

class KoCanvasBase;
class KisToolPath;

enum class KisToolPathInputType {
    MouseButtonPress,
    MouseButtonDoubleClick,
    TabletPress
};


class __KisToolPathLocalTool : public KoCreatePathTool {
public:
    __KisToolPathLocalTool(KoCanvasBase * canvas, KisToolPath* parentTool);

    void paintPath(KoPathShape &path, PkPainter &painter, const KoViewConverter &converter) override;
    void addPathShape(KoPathShape* pathShape) override;

    using KoCreatePathTool::createOptionWidgets;
    using KoCreatePathTool::endPathWithoutLastPoint;
    using KoCreatePathTool::endPath;
    using KoCreatePathTool::cancelPath;
    using KoCreatePathTool::removeLastPoint;

private:
    KisToolPath* const m_parentTool;
};

typedef KisDelegatedTool<KisToolShape,
                         __KisToolPathLocalTool,
                         DeselectShapesActivationPolicy> DelegatedPathTool;

class KisToolPath : public DelegatedPathTool
{
public:
    KisToolPath(KoCanvasBase * canvas);
    void mousePressEvent(KoPointerEvent *event) override;

    /** Handle input that the priority filter may consume before the delegate. */
    bool handlePathInput(KisToolPathInputType type, Qt::MouseButton button);

    void beginPrimaryAction(KoPointerEvent* event) override;
    void continuePrimaryAction(KoPointerEvent *event) override;
    void endPrimaryAction(KoPointerEvent *event) override;

    void beginAlternateAction(KoPointerEvent *event, AlternateAction action) override;

    // reimplementing KisTool's method because that method calls beginPrimaryAction
    // which now is used to start the path tool.
    void beginPrimaryDoubleClickAction(KoPointerEvent* event) override;

    KisPopupWidgetInterface* popupWidget() override;

protected:
    void requestStrokeCancellation() override;
    void requestStrokeEnd() override;

protected:
    void resetCursorStyle() override;

private:
    friend class __KisToolPathLocalTool;
};

class KisToolPathFactory : public KisToolPaintFactoryBase
{

public:
    KisToolPathFactory()
            : KisToolPaintFactoryBase("KisToolPath") {
        setToolTip(PkString("Bezier Curve Tool: Shift-mouseclick ends the curve."));
        setSection(ToolBoxSection::Shape);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
        setPriority(7);
    }

    ~KisToolPathFactory() override {}

    KoToolBase * createTool(KoCanvasBase *canvas) override {
        return new KisToolPath(canvas);
    }
};



#endif // KIS_TOOL_PATH_H_
