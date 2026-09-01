/*
 *  kis_tool_brush.cc - part of Krita
 *
 *  SPDX-FileCopyrightText: 2003-2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2015 Moritz Molch <kde@moritzmolch.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_brush.h"

#include <KoCanvasBase.h>

#include <KisCanvasToolServices.h>
#include "kis_image_config.h"
#include "kundo2magicstring.h"

#include "kis_types.h"
#include "kis_tool.h"

KisToolBrush::KisToolBrush(KoCanvasBase * canvas)
    : KisToolFreehand(canvas,
                      dynamic_cast<KisCanvasToolServices *>(canvas)->toolLoadCursor("tool_freehand_cursor.xpm", 2, 2),
                      kundo2_i18n("Freehand Brush Stroke"))
{
    setObjectName("tool_brush");
    setIsOpacityPresetMode(true);

    m_smoothingCursorConnection = PkObject::connect(
        this, &KisToolBrush::smoothingTypeChanged,
        this, &KisToolBrush::resetCursorStyle);
}

KisToolBrush::~KisToolBrush()
{
}

void KisToolBrush::activate(const PkSet<KoShape*> &shapes)
{
    KisToolFreehand::activate(shapes);

    KisImageConfig cfg(true);
    slotSetSmoothingType(cfg.lineSmoothingType());
}

void KisToolBrush::deactivate()
{
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

    smoothingTypeChanged();
}

void KisToolBrush::slotSetSmoothnessDistanceMin(qreal distance)
{
    smoothingOptions()->setSmoothnessDistanceMin(distance);
    smoothnessQualityChanged();
}

void KisToolBrush::slotSetSmoothnessDistanceMax(qreal distance)
{
    smoothingOptions()->setSmoothnessDistanceMax(distance);
    smoothnessQualityChanged();
}

void KisToolBrush::slotSetSmoothnessDistanceKeepAspectRatio(bool value)
{
    smoothingOptions()->setSmoothnessDistanceKeepAspectRatio(value);
}

void KisToolBrush::slotSetTailAggressiveness(qreal argh_rhhrr)
{
    smoothingOptions()->setTailAggressiveness(argh_rhhrr);
    smoothnessFactorChanged();
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

    useScalableDistanceChanged();
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

    useDelayDistanceChanged();
}

void KisToolBrush::setDelayDistance(qreal value)
{
    smoothingOptions()->setDelayDistance(value);
    delayDistanceChanged();
}

void KisToolBrush::setFinishStabilizedCurve(bool value)
{
    smoothingOptions()->setFinishStabilizedCurve(value);

    finishStabilizedCurveChanged();
}

bool KisToolBrush::finishStabilizedCurve() const
{
    return smoothingOptions()->finishStabilizedCurve();
}

void KisToolBrush::setStabilizeSensors(bool value)
{
    smoothingOptions()->setStabilizeSensors(value);
    stabilizeSensorsChanged();
}

bool KisToolBrush::stabilizeSensors() const
{
    return smoothingOptions()->stabilizeSensors();
}

void KisToolBrush::updateSettingsViews()
{
    smoothnessQualityChanged();
    smoothnessFactorChanged();
    smoothPressureChanged();
    smoothingTypeChanged();
    useScalableDistanceChanged();
    useDelayDistanceChanged();
    delayDistanceChanged();
    finishStabilizedCurveChanged();
    stabilizeSensorsChanged();

    KisTool::updateSettingsViews();
}


PkList<PkString> KisToolBrush::smoothingActionIds() const
{
    return {
        PkString("set_no_brush_smoothing"),
        PkString("set_simple_brush_smoothing"),
        PkString("set_weighted_brush_smoothing"),
        PkString("set_stabilizer_brush_smoothing"),
        PkString("set_pixel_perfect_smoothing")
    };
}

bool KisToolBrush::triggerSmoothingAction(const PkString &id)
{
    const PkList<PkString> ids = smoothingActionIds();
    for (int index = 0; index < ids.size(); ++index) {
        if (ids[index] == id) {
            slotSetSmoothingType(index);
            return true;
        }
    }
    return false;
}

void KisToolBrush::smoothnessQualityChanged() { activateSignal<>(this, PkMemberFnKey::from(&KisToolBrush::smoothnessQualityChanged)); }
void KisToolBrush::smoothnessFactorChanged() { activateSignal<>(this, PkMemberFnKey::from(&KisToolBrush::smoothnessFactorChanged)); }
void KisToolBrush::smoothPressureChanged() { activateSignal<>(this, PkMemberFnKey::from(&KisToolBrush::smoothPressureChanged)); }
void KisToolBrush::smoothingTypeChanged() { activateSignal<>(this, PkMemberFnKey::from(&KisToolBrush::smoothingTypeChanged)); }
void KisToolBrush::useScalableDistanceChanged() { activateSignal<>(this, PkMemberFnKey::from(&KisToolBrush::useScalableDistanceChanged)); }
void KisToolBrush::useDelayDistanceChanged() { activateSignal<>(this, PkMemberFnKey::from(&KisToolBrush::useDelayDistanceChanged)); }
void KisToolBrush::delayDistanceChanged() { activateSignal<>(this, PkMemberFnKey::from(&KisToolBrush::delayDistanceChanged)); }
void KisToolBrush::finishStabilizedCurveChanged() { activateSignal<>(this, PkMemberFnKey::from(&KisToolBrush::finishStabilizedCurveChanged)); }
void KisToolBrush::stabilizeSensorsChanged() { activateSignal<>(this, PkMemberFnKey::from(&KisToolBrush::stabilizeSensorsChanged)); }
