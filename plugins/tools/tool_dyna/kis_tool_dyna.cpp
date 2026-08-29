/*
 *  kis_tool_dyna.cpp - part of Krita
 *
 *  SPDX-FileCopyrightText: 2009-2011 Lukáš Tvrdý <LukasT.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_dyna.h"

#include <PkRect.h>
#include <ksharedconfig.h>

#include "KoPointerEvent.h"
#include "kundo2magicstring.h"



#define MAXIMUM_SMOOTHNESS 1000
#define MAXIMUM_MAGNETISM 1000

#define MIN_MASS 1.0
#define MAX_MASS 160.0
#define MIN_DRAG 0.0
#define MAX_DRAG 0.5
#define MIN_ACC 0.000001
#define MIN_VEL 0.000001


KisToolDyna::KisToolDyna(KoCanvasBase * canvas)
        : KisToolFreehand(canvas, PkToolArrowCursor, kundo2_text("Dynamic Brush Stroke"), false)
{
    setObjectName("tool_dyna");
    setIsOpacityPresetMode(true);
    initDyna();
}


void KisToolDyna::initDyna()
{
    /* dynadraw init */
    m_curmass = 0.5;
    m_curdrag = 0.15;
    m_mouse.fixedangle = false;
    m_width = 1.5;
    m_xangle = 0.60;
    m_yangle = 0.20;
    m_widthRange = 0.05;
}


KisToolDyna::~KisToolDyna()
{
}

void KisToolDyna::resetCursorStyle()
{
    KisToolFreehand::resetCursorStyle();

    overrideCursorIfNotEditable();
}

void KisToolDyna::activate(const PkSet<KoShape*> &shapes)
{
    KisToolPaint::activate(shapes);
    m_configGroup =  KSharedConfig::openConfig()->group(toolId());

    // Was read in createOptionWidget() (now deleted) when the options panel
    // was created; that ran on every tool activation, so these are the
    // effective defaults with no panel too -- same keys, same fallbacks.
    // Note these differ from the initDyna() constants (mass 0.5, drag 0.15,
    // angle giving xangle/yangle 0.60/0.20): createOptionWidget() always ran
    // on activation and overwrote them with the values below, so those were
    // the values actually seen in practice.
    slotSetMass(m_configGroup.readEntry("massAmount", 0.01));
    slotSetDrag(m_configGroup.readEntry("dragAmount", .98));
    slotSetFixedAngle(m_configGroup.readEntry("useFixedAngle", false));
    slotSetAngle(m_configGroup.readEntry("angleAmount", 20));
}

void KisToolDyna::initStroke(KoPointerEvent *event)
{
    PkRectF imageSize = PkRectF(PkPointF(0.0,0.0),currentImage()->size());
    PkRectF documentSize = currentImage()->pixelToDocument(imageSize);
    m_surfaceWidth = documentSize.width();
    m_surfaceHeight = documentSize.height();
    setMousePosition(event->point);
    m_mouse.init(m_mousePos.x(), m_mousePos.y());

    KisToolFreehand::initStroke(event);
}

void KisToolDyna::beginPrimaryAction(KoPointerEvent *event)
{
    setMousePosition(event->point);
    m_mouse.init(m_mousePos.x(), m_mousePos.y());
    m_odelx = m_mousePos.x();
    m_odely = m_mousePos.y();

    KisToolFreehand::beginPrimaryAction(event);
}

void KisToolDyna::continuePrimaryAction(KoPointerEvent *event)
{
    setMousePosition(event->point);

    if (applyFilter(m_mousePos.x(), m_mousePos.y())) {
        KoPointerEvent newEvent = filterEvent(event);
        KisToolFreehand::continuePrimaryAction(&newEvent);
    }
}

// dyna algorithm
int KisToolDyna::applyFilter(qreal mx, qreal my)
{
    /* calculate mass and drag */
    qreal mass = flerp(MIN_MASS, MAX_MASS, m_curmass);
    qreal drag = flerp(MIN_DRAG, MAX_DRAG, m_curdrag * m_curdrag);

    /* calculate force and acceleration */
    qreal fx = mx - m_mouse.curx;
    qreal fy = my - m_mouse.cury;

    m_mouse.acc = sqrt(fx * fx + fy * fy);

    if (m_mouse.acc < MIN_ACC) {
        return 0;
    }

    m_mouse.accx = fx / mass;
    m_mouse.accy = fy / mass;

    /* calculate new velocity */
    m_mouse.velx += m_mouse.accx;
    m_mouse.vely += m_mouse.accy;
    m_mouse.vel = sqrt(m_mouse.velx * m_mouse.velx + m_mouse.vely * m_mouse.vely);
    m_mouse.angx = -m_mouse.vely;
    m_mouse.angy = m_mouse.velx;
    if (m_mouse.vel < MIN_VEL) {
        return 0;
    }

    /* calculate angle of drawing tool */
    if (m_mouse.fixedangle) {
        m_mouse.angx = m_xangle;
        m_mouse.angy = m_yangle;
    } else {
        m_mouse.angx /= m_mouse.vel;
        m_mouse.angy /= m_mouse.vel;
    }

    m_mouse.velx = m_mouse.velx * (1.0 - drag);
    m_mouse.vely = m_mouse.vely * (1.0 - drag);

    m_mouse.lastx = m_mouse.curx;
    m_mouse.lasty = m_mouse.cury;
    m_mouse.curx = m_mouse.curx + m_mouse.velx;
    m_mouse.cury = m_mouse.cury + m_mouse.vely;

    return 1;
}


KoPointerEvent KisToolDyna::filterEvent(KoPointerEvent* event)
{
    qreal wid = m_widthRange - m_mouse.vel;

    wid = wid * m_width;

    if (wid < 0.00001) {
        wid = 0.00001;
    }

    qreal delx = m_mouse.angx * wid;
    qreal dely = m_mouse.angy * wid;

    qreal px = m_mouse.lastx;
    qreal py = m_mouse.lasty;
    qreal nx = m_mouse.curx;
    qreal ny = m_mouse.cury;

    PkPointF prev(px , py);         // previous position
    PkPointF now(nx , ny);           // new position

    PkPointF prevr(px + m_odelx , py + m_odely);
    PkPointF prevl(px - m_odelx , py - m_odely);

    PkPointF nowl(nx - delx , ny - dely);
    PkPointF nowr(nx + delx , ny + dely);

    // transform coords from float points into image points
    prev.rx() *= m_surfaceWidth;
    prevr.rx() *= m_surfaceWidth;
    prevl.rx() *= m_surfaceWidth;
    now.rx()  *= m_surfaceWidth;
    nowl.rx() *= m_surfaceWidth;
    nowr.rx() *= m_surfaceWidth;

    prev.ry() *= m_surfaceHeight;
    prevr.ry() *= m_surfaceHeight;
    prevl.ry() *= m_surfaceHeight;
    now.ry()  *= m_surfaceHeight;
    nowl.ry() *= m_surfaceHeight;
    nowr.ry() *= m_surfaceHeight;

#if 0

    qreal xTilt, yTilt;
    qreal m_rotation;
    qreal m_tangentialPressure;

    // some funny debugging
    dbgPlugins << "m_mouse.vel: " << m_mouse.vel;
    dbgPlugins << "m_mouse.velx: " << m_mouse.velx;
    dbgPlugins << "m_mouse.vely: " << m_mouse.vely;
    dbgPlugins << "m_mouse.accx: " << m_mouse.accx;
    dbgPlugins << "m_mouse.accy: " << m_mouse.accy;


    dbgPlugins << "fixed: " << m_mouse.fixedangle;
    dbgPlugins << "drag: " << m_curdrag;
    dbgPlugins << "mass: " << m_curmass;
    dbgPlugins << "xAngle: " << m_xangle;
    dbgPlugins << "yAngle: " << m_yangle;

#endif

    m_odelx = delx;
    m_odely = dely;

    // how to change pressure in the KoPointerEvent???
    return KoPointerEvent(event,now);
}


void KisToolDyna::slotSetDrag(qreal drag)
{
    m_curdrag = drag;
    m_configGroup.writeEntry("dragAmount", drag);
}


void KisToolDyna::slotSetMass(qreal mass)
{
    m_curmass = mass;
    m_configGroup.writeEntry("massAmount", mass);
}


void KisToolDyna::slotSetDynaWidth(double width)
{
    m_width = width;
    m_configGroup.writeEntry("initWidth", width);
}


void KisToolDyna::slotSetWidthRange(double widthRange)
{
    m_widthRange = widthRange;
    m_configGroup.writeEntry("initWidthRange", widthRange);
}


void KisToolDyna::slotSetFixedAngle(bool fixedAngle)
{
    m_mouse.fixedangle = fixedAngle;
    // Was m_angleSelector->setEnabled(fixedAngle) -- the options panel that
    // owned m_angleSelector has been removed, so there is no widget to update.
    m_configGroup.writeEntry("useFixedAngle", fixedAngle);
}

void KisToolDyna::slotSetAngle(qreal angle)
{
    m_xangle = cos(angle * M_PI/180.0);
    m_yangle = sin(angle * M_PI/180.0);

    m_configGroup.writeEntry("angleAmount", angle);
}

KoToolBase *KisToolDynaFactory::createTool(KoCanvasBase *canvas)
{
    return new KisToolDyna(canvas);
}
