/*
 *  SPDX-FileCopyrightText: 2017 Victor Wåhlström <victor.wahlstrom@initiali.se>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_pan.h"
#include <KisCanvasToolServices.h>
#include <KoCanvasBase.h>
#include <KoCanvasController.h>
#include <KoPointerEvent.h>

KisToolPan::KisToolPan(KoCanvasBase *canvas)
    : KisTool(canvas, dynamic_cast<KisCanvasToolServices *>(canvas)->toolOpenHandCursor())
{
}

KisToolPan::~KisToolPan()
{
}

void KisToolPan::beginPrimaryAction(KoPointerEvent *event)
{
    m_lastPosition = event->pos();
    useCursor(dynamic_cast<KisCanvasToolServices *>(canvas())->toolClosedHandCursor());
}

void KisToolPan::continuePrimaryAction(KoPointerEvent *event)
{
    PkPoint pos = event->pos();
    PkPoint delta = m_lastPosition - pos;
    canvas()->canvasController()->pan(delta);
    m_lastPosition = pos;
}

void KisToolPan::endPrimaryAction(KoPointerEvent *event)
{
    (void)event;
    useCursor(dynamic_cast<KisCanvasToolServices *>(canvas())->toolOpenHandCursor());
}

bool KisToolPan::panByKey(int key)
{
    switch (key) {
        case Qt::Key_Up:
            canvas()->canvasController()->panUp();
            return true;
        case Qt::Key_Down:
            canvas()->canvasController()->panDown();
            return true;
        case Qt::Key_Left:
            canvas()->canvasController()->panLeft();
            return true;
        case Qt::Key_Right:
            canvas()->canvasController()->panRight();
            return true;
        default:
            return false;
    }
}

void KisToolPan::paint(PkPainter &painter, const KoViewConverter &converter)
{
    (void)painter;
    (void)converter;
}

bool KisToolPan::wantsAutoScroll() const
{
    return false;
}

KisToolPanFactory::KisToolPanFactory()
    : KoToolFactoryBase("PanTool")
{
    setToolTip(PkString("Pan Tool"));
    setSection(ToolBoxSection::Navigation);
    setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
    setPriority(2);
}

KisToolPanFactory::~KisToolPanFactory()
{
}

KoToolBase* KisToolPanFactory::createTool(KoCanvasBase *canvas)
{
    return new KisToolPan(canvas);
}
