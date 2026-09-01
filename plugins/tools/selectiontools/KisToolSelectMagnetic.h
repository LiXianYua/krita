/*
 *  SPDX-FileCopyrightText: 2019 Kuntal Majumder <hellozee@disroot.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TOOL_SELECT_MAGNETIC_H_
#define KIS_TOOL_SELECT_MAGNETIC_H_

#include <PkPoint.h>
#include "KisSelectionToolFactoryBase.h"
#include <kis_tool_select_base.h>
#include <kis_signal_compressor.h>
#include "KisMagneticWorker.h"
#include <PkPainter.h>
#include <PkPainterPath.h>
#include <PkScopedPointer.h>
#include <PkString.h>

class PkPainterPath;

class KisToolSelectMagnetic : public KisToolSelect
{
public:
    KisToolSelectMagnetic(KoCanvasBase *canvas);
    ~KisToolSelectMagnetic() override;
    void beginPrimaryAction(KoPointerEvent *event) override;
    void continuePrimaryAction(KoPointerEvent *event) override;
    void endPrimaryAction(KoPointerEvent *event) override;
    void paint(PkPainter& gc, const KoViewConverter &converter) override;

    //void beginPrimaryDoubleClickAction(KoPointerEvent *event) override;

    void mouseMoveEvent(KoPointerEvent *event) override;

    void resetCursorStyle() override;
    void requestStrokeEnd() override;
    void requestStrokeCancellation() override;

public:
    void deactivate() override;
    void activate(const PkSet<KoShape *> &shapes) override;
    void undoPoints();
    void slotSetFilterRadius(qreal);
    void slotSetThreshold(int);
    void slotSetSearchRadius(int);
    void slotSetAnchorGap(int);
    void slotCalculateEdge();
    void setContinuedModeModifierPressed(bool pressed);

protected:
    using KisToolSelectBase::m_widgetHelper;

private:
    void finishSelectionAction();
    void updateFeedback();
    void updateContinuedMode();
    void updateCanvas();
    void updatePaintPath();
    void resetVariables();
    void drawAnchors(PkPainter &gc);
    void checkIfAnchorIsSelected(PkPointF pt);
    PkVector<PkPointF> computeEdgeWrapper(PkPoint a, PkPoint b);
    void reEvaluatePoints();
    void calculateCheckPoints(PkVector<PkPointF> points);
    void deleteSelectedAnchor();
    void updateSelectedAnchor();
    int updateInitialAnchorBounds(PkPoint pt);
    void updateContinuedModeFromModifiers(Qt::KeyboardModifiers modifiers);

    PkPainterPath m_paintPath;
    PkVector<PkPointF> m_points;
    PkVector<PkPoint> m_anchorPoints;
    bool m_continuedMode {false};
    PkPointF m_lastCursorPos, m_cursorOnPress;
    PkPoint m_lastAnchor;
    bool m_complete {false};
    bool m_selected {false};
    bool m_finished {false};
    PkScopedPointer<KisMagneticWorker> m_worker;
    int m_threshold {70};
    int m_searchRadius {30};
    int m_selectedAnchor {0};
    int m_anchorGap {30};
    qreal m_filterRadius {3.0};
    PkRectF m_snapBound;
    KConfigGroup m_configGroup;
    PkVector<PkVector<PkPointF>> m_pointCollection;
    KisSignalCompressor m_mouseHoverCompressor;
    PkConnection m_mouseHoverConnection;
};

class KisToolSelectMagneticFactory : public KisSelectionToolFactoryBase
{
public:
    KisToolSelectMagneticFactory()
        : KisSelectionToolFactoryBase("KisToolSelectMagnetic")
    {
        setToolTip(PkString("Magnetic Selection Tool"));
        setSection(ToolBoxSection::Select);
        setPriority(8);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
    }

    ~KisToolSelectMagneticFactory() override { }

    KoToolBase * createTool(KoCanvasBase *canvas) override
    {
        return new KisToolSelectMagnetic(canvas);
    }

};


#endif // __selecttoolmagnetic_h__
