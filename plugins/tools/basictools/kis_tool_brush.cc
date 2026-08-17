/*
 *  kis_tool_brush.cc - part of Krita
 *
 *  SPDX-FileCopyrightText: 2003-2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2015 Moritz Molch <kde@moritzmolch.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_brush.h"

#include <klocalizedstring.h>
#include <QAction>

#include <KoCanvasBase.h>

#include <KisCanvasToolServices.h>
#include "kis_image_config.h"
#include "kundo2magicstring.h"

#include "kis_types.h"
#include "kis_tool.h"

void KisToolBrush::addSmoothingAction(int enumId, const QString &id)
{
    /**
     * KisToolBrush is the base of several tools, but the actions
     * should be unique, so let's be careful with them
     */
    QAction *a = action(id);
    connect(a, SIGNAL(triggered()), &m_signalMapper, SLOT(map()));
    m_signalMapper.setMapping(a, enumId);
}

KisToolBrush::KisToolBrush(KoCanvasBase * canvas)
    : KisToolFreehand(canvas,
                      dynamic_cast<KisCanvasToolServices *>(canvas)->toolLoadCursor("tool_freehand_cursor.xpm", 2, 2),
                      kundo2_i18n("Freehand Brush Stroke"))
{
    setObjectName("tool_brush");
    setIsOpacityPresetMode(true);

    connect(this, SIGNAL(smoothingTypeChanged()), this, SLOT(resetCursorStyle()));

    addSmoothingAction(KisSmoothingOptions::NO_SMOOTHING, "set_no_brush_smoothing");
    addSmoothingAction(KisSmoothingOptions::SIMPLE_SMOOTHING, "set_simple_brush_smoothing");
    addSmoothingAction(KisSmoothingOptions::WEIGHTED_SMOOTHING, "set_weighted_brush_smoothing");
    addSmoothingAction(KisSmoothingOptions::STABILIZER, "set_stabilizer_brush_smoothing");
    addSmoothingAction(KisSmoothingOptions::PIXEL_PERFECT, "set_pixel_perfect_smoothing");
}

KisToolBrush::~KisToolBrush()
{
}

void KisToolBrush::activate(const QSet<KoShape*> &shapes)
{
    KisToolFreehand::activate(shapes);
    connect(&m_signalMapper, SIGNAL(mapped(int)), SLOT(slotSetSmoothingType(int)), Qt::UniqueConnection);

    KisImageConfig cfg(true);
    slotSetSmoothingType(cfg.lineSmoothingType());
}

void KisToolBrush::deactivate()
{
    disconnect(&m_signalMapper, 0, this, 0);

    KisToolFreehand::deactivate();
}

int KisToolBrush::smoothingType() const
{
    return smoothingOptions()->smoothingType();
}

bool KisToolBrush::smoothPressure() const
{
    return smoothingOptions()->smoothPressure();
}

int KisToolBrush::smoothnessQualityMin() const
{
    return smoothingOptions()->smoothnessDistanceMin();
}

int KisToolBrush::smoothnessQualityMax() const
{
    return smoothingOptions()->smoothnessDistanceMax();
}

qreal KisToolBrush::smoothnessFactor() const
{
    return smoothingOptions()->tailAggressiveness();
}

void KisToolBrush::slotSetSmoothingType(int index)
{
    switch (index) {
    case KisSmoothingOptions::NO_SMOOTHING:
        smoothingOptions()->setSmoothingType(KisSmoothingOptions::NO_SMOOTHING);
        break;
    case KisSmoothingOptions::SIMPLE_SMOOTHING:
        smoothingOptions()->setSmoothingType(KisSmoothingOptions::SIMPLE_SMOOTHING);
        break;
    case KisSmoothingOptions::WEIGHTED_SMOOTHING:
        smoothingOptions()->setSmoothingType(KisSmoothingOptions::WEIGHTED_SMOOTHING);
        break;
    case KisSmoothingOptions::STABILIZER:
        smoothingOptions()->setSmoothingType(KisSmoothingOptions::STABILIZER);
        break;
    case KisSmoothingOptions::PIXEL_PERFECT:
    default:
        smoothingOptions()->setSmoothingType(KisSmoothingOptions::PIXEL_PERFECT);
    }

    Q_EMIT smoothingTypeChanged();
}

void KisToolBrush::slotSetSmoothnessDistanceMin(qreal distance)
{
    smoothingOptions()->setSmoothnessDistanceMin(distance);
    Q_EMIT smoothnessQualityChanged();
}

void KisToolBrush::slotSetSmoothnessDistanceMax(qreal distance)
{
    smoothingOptions()->setSmoothnessDistanceMax(distance);
    Q_EMIT smoothnessQualityChanged();
}

void KisToolBrush::slotSetSmoothnessDistanceKeepAspectRatio(bool value)
{
    smoothingOptions()->setSmoothnessDistanceKeepAspectRatio(value);
}

void KisToolBrush::slotSetTailAggressiveness(qreal argh_rhhrr)
{
    smoothingOptions()->setTailAggressiveness(argh_rhhrr);
    Q_EMIT smoothnessFactorChanged();
}

// used with weighted smoothing
void KisToolBrush::setSmoothPressure(bool value)
{
    smoothingOptions()->setSmoothPressure(value);
}

bool KisToolBrush::useScalableDistance() const
{
    return smoothingOptions()->useScalableDistance();
}

// used with weighted smoothing
void KisToolBrush::setUseScalableDistance(bool value)
{
    smoothingOptions()->setUseScalableDistance(value);

    Q_EMIT useScalableDistanceChanged();
}

void KisToolBrush::resetCursorStyle()
{
    KisImageConfig cfg(true);
    CursorStyle cursorStyle = cfg.newCursorStyle();

    // When the stabilizer is in use, we avoid using the brush outline cursor,
    // because it would hide the real position of the cursor to the user,
    // yielding unexpected results.
    if (smoothingOptions()->smoothingType() == KisSmoothingOptions::STABILIZER &&
            smoothingOptions()->useDelayDistance() &&
            cursorStyle == CURSOR_STYLE_NO_CURSOR) {

        useCursor(dynamic_cast<KisCanvasToolServices *>(canvas())->toolCursor(CURSOR_STYLE_SMALL_ROUND));
    } else {
        KisToolFreehand::resetCursorStyle();
    }

    overrideCursorIfNotEditable();
}

// stabilizer brush settings
bool KisToolBrush::useDelayDistance() const
{
    return smoothingOptions()->useDelayDistance();
}

qreal KisToolBrush::delayDistance() const
{
    return smoothingOptions()->delayDistance();
}

void KisToolBrush::setUseDelayDistance(bool value)
{
    smoothingOptions()->setUseDelayDistance(value);

    Q_EMIT useDelayDistanceChanged();
}

void KisToolBrush::setDelayDistance(qreal value)
{
    smoothingOptions()->setDelayDistance(value);
    Q_EMIT delayDistanceChanged();
}

void KisToolBrush::setFinishStabilizedCurve(bool value)
{
    smoothingOptions()->setFinishStabilizedCurve(value);

    Q_EMIT finishStabilizedCurveChanged();
}

bool KisToolBrush::finishStabilizedCurve() const
{
    return smoothingOptions()->finishStabilizedCurve();
}

void KisToolBrush::setStabilizeSensors(bool value)
{
    smoothingOptions()->setStabilizeSensors(value);
    Q_EMIT stabilizeSensorsChanged();
}

bool KisToolBrush::stabilizeSensors() const
{
    return smoothingOptions()->stabilizeSensors();
}

void KisToolBrush::updateSettingsViews()
{
    Q_EMIT smoothnessQualityChanged();
    Q_EMIT smoothnessFactorChanged();
    Q_EMIT smoothPressureChanged();
    Q_EMIT smoothingTypeChanged();
    Q_EMIT useScalableDistanceChanged();
    Q_EMIT useDelayDistanceChanged();
    Q_EMIT delayDistanceChanged();
    Q_EMIT finishStabilizedCurveChanged();
    Q_EMIT stabilizeSensorsChanged();

    KisTool::updateSettingsViews();
}


QList<QAction *> KisToolBrushFactory::createActionsImpl()
{

    QList<QAction *> actions = KisToolPaintFactoryBase::createActionsImpl();

    { QAction *action = new QAction(this); action->setObjectName("set_no_brush_smoothing"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("set_simple_brush_smoothing"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("set_weighted_brush_smoothing"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("set_stabilizer_brush_smoothing"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("set_pixel_perfect_smoothing"); actions << action; }

    return actions;

}
