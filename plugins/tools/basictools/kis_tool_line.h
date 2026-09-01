/*
 *  kis_tool_line.h - part of Krayon
 *
 *  SPDX-FileCopyrightText: 2000 John Califf <jcaliff@comuzone.net>
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TOOL_LINE_H_
#define KIS_TOOL_LINE_H_

#include "kis_tool_shape.h"

#include <PkScopedPointer.h>
#include <kis_global.h>
#include <kis_types.h>
#include <KisToolPaintFactoryBase.h>
#include <flake/kis_node_shape.h>
#include <kis_signal_compressor.h>

class PkPoint;
class KoCanvasBase;
class KisPaintingInformationBuilder;
class KisToolLineHelper;


class KisToolLine : public KisToolShape
{
public:
    KisToolLine(KoCanvasBase * canvas);
    ~KisToolLine() override;

    void requestStrokeCancellation() override;
    void requestStrokeEnd() override;

    void beginPrimaryAction(KoPointerEvent *event) override;
    void continuePrimaryAction(KoPointerEvent *event) override;
    void endPrimaryAction(KoPointerEvent *event) override;
    void activate(const PkSet<KoShape*> &shapes) override;
    void deactivate() override;
    bool primaryActionSupportsHiResEvents() const override;

    void paint(PkPainter& gc, const KoViewConverter &converter) override;

    PkString quickHelp() const override;

    bool supportsPaintingAssistants() const override;

protected:
    void resetCursorStyle() override;

private:
    void updateStroke();

private:
    void paintLine(PkPainter& gc, const PkRect& rc);
    PkPointF straightLine(PkPointF point);
    void updateGuideline();
    void showSize();
    void updatePreviewTimer(bool showGuide);

    void endStroke();
    void cancelStroke();

private:
    bool m_showGuideline {true};

    PkPointF m_startPoint; // start point to use when painting (after the line was snapped to assistant already)
    PkPointF m_endPoint;
    PkPointF m_lastUpdatedPoint;

    bool m_strokeIsRunning {false};
    bool m_altInitiallyHeld {false};

    PkScopedPointer<KisPaintingInformationBuilder> m_infoBuilder;
    PkScopedPointer<KisToolLineHelper> m_helper;
    KisSignalCompressor m_strokeUpdateCompressor;
    KisSignalCompressor m_longStrokeUpdateCompressor;
    PkConnection m_strokeUpdateConnection;
    PkConnection m_longStrokeUpdateConnection;
};


class KisToolLineFactory : public KisToolPaintFactoryBase
{

public:

    KisToolLineFactory()
            : KisToolPaintFactoryBase("KritaShape/KisToolLine") {
        setToolTip(PkString("Line Tool"));
        // Temporarily
        setSection(ToolBoxSection::Shape);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
        setPriority(1);
    }

    ~KisToolLineFactory() override {}

    KoToolBase * createTool(KoCanvasBase *canvas) override {
        return new KisToolLine(canvas);
    }

};




#endif //KIS_TOOL_LINE_H_
