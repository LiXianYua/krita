/*
 *  SPDX-FileCopyrightText: 2003-2004 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TOOL_BRUSH_H_
#define KIS_TOOL_BRUSH_H_

#include "kis_tool_freehand.h"

#include <PkConnection.h>
#include <PkList.h>
#include <PkString.h>

#include "KisToolPaintFactoryBase.h"

#include <flake/kis_node_shape.h>



class KoCanvasBase;

class KisToolBrush : public KisToolFreehand
{
public:
    KisToolBrush(KoCanvasBase * canvas);
    ~KisToolBrush() override;

    int smoothnessQualityMin() const;
    int smoothnessQualityMax() const;
    qreal smoothnessFactor() const;
    bool smoothPressure() const;
    int smoothingType() const;
    bool useScalableDistance() const;

    bool useDelayDistance() const;
    qreal delayDistance() const;

    bool finishStabilizedCurve() const;
    bool stabilizeSensors() const;

protected:
    void resetCursorStyle() override;

public:
    void activate(const PkSet<KoShape*> &shapes) override;
    void deactivate() override;
    void slotSetSmoothnessDistanceMin(qreal distance);
    void slotSetSmoothnessDistanceMax(qreal distance);
    void slotSetSmoothnessDistanceKeepAspectRatio(bool value);
    void slotSetSmoothingType(int index);
    void slotSetTailAggressiveness(qreal argh_rhhrr);
    void setSmoothPressure(bool value);
    void setUseScalableDistance(bool value);

    void setUseDelayDistance(bool value);
    void setDelayDistance(qreal value);

    void setStabilizeSensors(bool value);

    void setFinishStabilizedCurve(bool value);

    void updateSettingsViews() override;

    PkList<PkString> smoothingActionIds() const;
    bool triggerSmoothingAction(const PkString &id);

    void smoothnessQualityChanged();
    void smoothnessFactorChanged();
    void smoothPressureChanged();
    void smoothingTypeChanged();
    void useScalableDistanceChanged();

    void useDelayDistanceChanged();
    void delayDistanceChanged();
    void finishStabilizedCurveChanged();
    void stabilizeSensorsChanged();

private:
    PkConnection m_smoothingCursorConnection;
};


class KisToolBrushFactory : public KisToolPaintFactoryBase
{

public:
    KisToolBrushFactory()
            : KisToolPaintFactoryBase("KritaShape/KisToolBrush") {

        setToolTip(PkString("Freehand Brush Tool"));

        // Temporarily
        setSection(ToolBoxSection::Shape);
        setShortcut(PkString("B"));
        setPriority(0);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
    }

    KisToolBrushFactory(const PkString &id)
        : KisToolPaintFactoryBase(id)
    {
    }

    ~KisToolBrushFactory() override {}

    KoToolBase * createTool(KoCanvasBase *canvas) override {
        return new KisToolBrush(canvas);
    }

    void showFloatingMessage();

};


#endif // KIS_TOOL_BRUSH_H_
