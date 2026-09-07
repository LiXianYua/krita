/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2022 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISPATHENCLOSINGPRODUCER
#define KISPATHENCLOSINGPRODUCER

#include <KoCreatePathTool.h>
#include <kis_pixel_selection.h>
#include <kis_delegated_tool_policies.h>
#include <kis_tool_shape.h>
#include <KisCanvasToolServices.h>
#include <PkScopedPointer.h>

#include "KisDynamicDelegatedTool.h"

class KisPathEnclosingProducer;

class KisToolPathLocalTool : public KoCreatePathTool {
public:
    KisToolPathLocalTool(KoCanvasBase * canvas, KisPathEnclosingProducer* parentTool);

    void paintPath(KoPathShape &pathShape, PkPainter &painter, const KoViewConverter &converter) override;
    void addPathShape(KoPathShape* pathShape) override;
    void beginShape() override;
    void endShape() override;

    using KoCreatePathTool::createOptionWidgets;
    using KoCreatePathTool::endPathWithoutLastPoint;
    using KoCreatePathTool::endPath;
    using KoCreatePathTool::cancelPath;
    using KoCreatePathTool::removeLastPoint;

private:
    KisPathEnclosingProducer* const m_parentTool;
};

class DelegatedPathTool : public KisToolShape
{
public:
    DelegatedPathTool(KoCanvasBase *canvas,
                      const QCursor &cursor,
                      KisToolPathLocalTool *delegateTool)
        : KisToolShape(canvas, cursor)
        , m_localTool(delegateTool)
    {
    }

    KisToolPathLocalTool *localTool() const
    {
        return m_localTool.data();
    }

    void activate(const PkSet<KoShape*> &shapes) override
    {
        KisToolShape::activate(shapes);
        m_localTool->activate(shapes);
        DeselectShapesActivationPolicy::onActivate(canvas());
        dynamic_cast<KisCanvasToolServices*>(canvas())
            ->toolSetPriorityEventFilter(this, true);
    }

    void deactivate() override
    {
        m_localTool->deactivate();
        KisToolShape::deactivate();
        dynamic_cast<KisCanvasToolServices*>(canvas())
            ->toolSetPriorityEventFilter(this, false);
    }

    void mousePressEvent(KoPointerEvent *event) override;
    void mouseDoubleClickEvent(KoPointerEvent *event) override;

    void mouseMoveEvent(KoPointerEvent *event) override
    {
        m_localTool->mouseMoveEvent(event);
        KisToolShape::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(KoPointerEvent *event) override;

    void paint(PkPainter &painter, const KoViewConverter &converter) override
    {
        m_localTool->paint(painter, converter);
    }

    PkList<PkPointer<QWidget>> createOptionWidgets() override
    {
        PkList<PkPointer<QWidget>> widgets = KisToolShape::createOptionWidgets();
        widgets.append(m_localTool->createOptionWidgets());
        return widgets;
    }

protected:
    PkScopedPointer<KisToolPathLocalTool> m_localTool;
};

class KisPathEnclosingProducer : public KisDynamicDelegateTool<DelegatedPathTool>
{
public:
    KisPathEnclosingProducer(KoCanvasBase *canvas);
    ~KisPathEnclosingProducer() override;
    
    bool hasUserInteractionRunning() const;

    void mousePressEvent(KoPointerEvent *event) override;
    void beginPrimaryAction(KoPointerEvent* event) override;
    void continuePrimaryAction(KoPointerEvent *event) override;
    void endPrimaryAction(KoPointerEvent *event) override;
    void beginAlternateAction(KoPointerEvent *event, AlternateAction action) override;
    // reimplementing KisTool's method because that method calls beginPrimaryAction
    // which now is used to start the path tool.
    void beginPrimaryDoubleClickAction(KoPointerEvent* event) override;

    KisPopupWidgetInterface* popupWidget() override;
    void enclosingMaskProduced(KisPixelSelectionSP enclosingMask);
    
protected:
    void requestStrokeCancellation() override;
    void requestStrokeEnd() override;

    void addPathShape(KoPathShape* pathShape);
    void beginShape() override;
    void endShape() override;

    friend class KisToolPathLocalTool;

private:
    bool m_hasUserInteractionRunning {false};

protected:
    void resetCursorStyle() override;
};

#endif
