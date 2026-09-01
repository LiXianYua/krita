/*
 *  kis_tool_line.h - part of Krayon
 *
 *  SPDX-FileCopyrightText: 2000 John Califf <jcaliff@comuzone.net>
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2004 Adrian Page <adrian@pagenet.plus.com>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TOOL_GRADIENT_H_
#define KIS_TOOL_GRADIENT_H_

#include <PkPainter.h>
#include <PkString.h>

#include <KisToolPaintFactoryBase.h>

#include <kis_tool_paint.h>
#include <kis_global.h>
#include <kis_types.h>
#include <kis_gradient_painter.h>
#include <flake/kis_node_shape.h>


class PkPoint;

class KisToolGradient : public KisToolPaint
{

public:
    KisToolGradient(KoCanvasBase * canvas);
    ~KisToolGradient() override;

    void beginPrimaryAction(KoPointerEvent *event) override;
    void continuePrimaryAction(KoPointerEvent *event) override;
    void endPrimaryAction(KoPointerEvent *event) override;

    void paint(PkPainter &painter, const KoViewConverter &converter) override;

public:
    void activate(const PkSet<KoShape*> &shapes) override;
protected:
    void resetCursorStyle() override;

private:

    void areaDone(const PkRect & rc) {
        currentNode()->setDirty(rc); // Starts computing the projection for the area we've done.

    }

private:

    void paintLine(PkPainter& gc);
    void updateGuideline();

    PkPointF straightLine(PkPointF point);

    PkPointF m_startPos;
    PkPointF m_endPos;

    KisGradientPainter::enumGradientShape m_shape;
    KisGradientPainter::enumGradientRepeat m_repeat;

    bool m_dither {false};
    bool m_reverse {false};
    double m_antiAliasThreshold {0.0};
};

class KisToolGradientFactory : public KisToolPaintFactoryBase
{

public:
    KisToolGradientFactory()
            : KisToolPaintFactoryBase("KritaFill/KisToolGradient") {
        setToolTip(PkString("Gradient Tool"));
        setSection(ToolBoxSection::Fill);
        setShortcut(PkString("G"));
        setPriority(1);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
    }

    ~KisToolGradientFactory() override {}

    KoToolBase * createTool(KoCanvasBase *canvas) override {
        return  new KisToolGradient(canvas);
    }

};

#endif //KIS_TOOL_GRADIENT_H_
