/*
 *  SPDX-FileCopyrightText: 2025 Agata Cacko
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TOOL_KNIFE_H_
#define KIS_TOOL_KNIFE_H_

#include <QtCore/QtCore>
#include <QtGui/QtGui>
#include <QtWidgets/QtWidgets>
#include <PkFlakeBridge.h>
#include <PkScopedPointer.h>
#include <PkPainterPath.h>
#include <PkPainter.h>

#include "KoInteractionTool.h"

#include "KisToolPaintFactoryBase.h"

#include <flake/kis_node_shape.h>
#include <kconfig.h>
#include <kconfiggroup.h>

class KoCanvasBase;
class KisPaintInformation;
class KisSpacingInformation;


class KisToolKnife : public KoInteractionTool
{
public:
    KisToolKnife(KoCanvasBase * canvas);
    ~KisToolKnife() override;

    void paint(QPainter &painter, const KoViewConverter &converter) override;

public:
    void activate(const QSet<KoShape*> &shapes) override;
    void deactivate() override;
    void mousePressEvent(KoPointerEvent *event) override;
    void mouseMoveEvent(KoPointerEvent *event)  override;
    void mouseReleaseEvent(KoPointerEvent *event) override;

    KoInteractionStrategy *createStrategy(KoPointerEvent *event) override;

    bool isValidForCurrentLayer() const;
private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};


class KisToolKnifeFactory : public KisToolPaintFactoryBase
{

public:
    KisToolKnifeFactory()
        : KisToolPaintFactoryBase("KritaShape/KisToolKnife")
    {

        setToolTip(toQString(PkString("Comic Panel Editing Tool")));

        setSection(ToolBoxSection::Main);
        setPriority(7);
        setActivationShapeId("flake/always,KoPathShape");
    }

    ~KisToolKnifeFactory() override {}

    KoToolBase *createTool(KoCanvasBase *canvas) override;

};


#endif // KIS_TOOL_KNIFE_H_
