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

#include <QScopedPointer>
#include <kis_global.h>
#include <kis_types.h>
#include <KisToolPaintFactoryBase.h>
#include <flake/kis_node_shape.h>
#include <kis_signal_compressor.h>

class QPoint;
class KoCanvasBase;
class KisPaintingInformationBuilder;
class KisToolLineHelper;


class KisToolLine : public KisToolShape
{
    Q_OBJECT

public:
    KisToolLine(KoCanvasBase * canvas);
    ~KisToolLine() override;

    void requestStrokeCancellation() override;
    void requestStrokeEnd() override;

    void beginPrimaryAction(KoPointerEvent *event) override;
    void continuePrimaryAction(KoPointerEvent *event) override;
    void endPrimaryAction(KoPointerEvent *event) override;
    void activate(const QSet<KoShape*> &shapes) override;
    void deactivate() override;
    bool primaryActionSupportsHiResEvents() const override;

    void paint(QPainter& gc, const KoViewConverter &converter) override;

    QString quickHelp() const override;

    bool supportsPaintingAssistants() const override;

protected Q_SLOTS:
    void resetCursorStyle() override;

private Q_SLOTS:
    void updateStroke();

private:
    void paintLine(QPainter& gc, const QRect& rc);
    QPointF straightLine(QPointF point);
    void updateGuideline();
    void showSize();
    void updatePreviewTimer(bool showGuide);

    void endStroke();
    void cancelStroke();

private:
    bool m_showGuideline {true};

    QPointF m_startPoint; // start point to use when painting (after the line was snapped to assistant already)
    QPointF m_endPoint;
    QPointF m_lastUpdatedPoint;

    bool m_strokeIsRunning {false};
    bool m_altInitiallyHeld {false};

    QScopedPointer<KisPaintingInformationBuilder> m_infoBuilder;
    QScopedPointer<KisToolLineHelper> m_helper;
    KisSignalCompressor m_strokeUpdateCompressor;
    KisSignalCompressor m_longStrokeUpdateCompressor;
};


class KisToolLineFactory : public KisToolPaintFactoryBase
{

public:

    KisToolLineFactory()
            : KisToolPaintFactoryBase("KritaShape/KisToolLine") {
        setToolTip(i18n("Line Tool"));
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
