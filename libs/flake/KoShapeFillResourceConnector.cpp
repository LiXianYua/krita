/*
 *  SPDX-FileCopyrightText: 2018 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include "KoShapeFillResourceConnector.h"

#include <KoCanvasResourceProvider.h>
#include <KoSelectedShapesProxy.h>

#include "kis_assert.h"
#include "KisQtConnectionsStore.h"

#include <KoColor.h>
#include <KoFlake.h>
#include <KoShapeFillWrapper.h>
#include <KoSelection.h>
#include <KoCanvasBase.h>

// S-08 过渡期：libs/pigment 的 KoColor 已剥离掉 Q_DECLARE_METATYPE，而本 TU 用
// QVariant::value<KoColor>()（资源系统以 QVariant 存 KoColor），在此补声明。
Q_DECLARE_METATYPE(KoColor)



struct KoShapeFillResourceConnector::Private
{
    KoCanvasBase *canvas;
    KisQtConnectionsStore resourceManagerConnections;

    void applyShapeColoring(KoFlake::FillVariant fillVariant, const KoColor &color);
};

KoShapeFillResourceConnector::KoShapeFillResourceConnector(QObject *parent)
    : QObject(parent),
      m_d(new Private())
{
}

KoShapeFillResourceConnector::~KoShapeFillResourceConnector()
{
}

void KoShapeFillResourceConnector::connectToCanvas(KoCanvasBase *canvas)
{
    m_d->resourceManagerConnections.clear();
    m_d->canvas = 0;

    KIS_SAFE_ASSERT_RECOVER_RETURN(!canvas || canvas->resourceManager());
    KIS_SAFE_ASSERT_RECOVER_RETURN(!canvas || canvas->selectedShapesProxy());

    m_d->canvas = canvas;

    if (m_d->canvas) {
        m_d->resourceManagerConnections.addConnection(
            canvas->resourceManager(), &KoCanvasResourceProvider::canvasResourceChanged,
            this, &KoShapeFillResourceConnector::slotCanvasResourceChanged);
    }
}

void KoShapeFillResourceConnector::disconnect()
{
    connectToCanvas(0);
}

void KoShapeFillResourceConnector::slotCanvasResourceChanged(int key, const QVariant &value)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_d->canvas);

    if (key == KoCanvasResource::ForegroundColor) {
        m_d->applyShapeColoring(KoFlake::Fill, value.value<KoColor>());
    } else if (key == KoCanvasResource::BackgroundColor) {
        m_d->applyShapeColoring(KoFlake::StrokeFill, value.value<KoColor>());
    }
}


void KoShapeFillResourceConnector::Private::applyShapeColoring(KoFlake::FillVariant fillVariant, const KoColor &color)
{
    QList<KoShape *> selectedEditableShapes = canvas->selectedShapesProxy()->selection()->selectedEditableShapes();

    if (selectedEditableShapes.isEmpty()) {
        return;
    }

    KoShapeFillWrapper wrapper(selectedEditableShapes, fillVariant);
    // KoColor::toQColor() 在剥离后返回 PkColor，经桥接转回真 Qt 颜色再喂给 setColor。
    KUndo2Command *command = wrapper.setColor(toQColor(color.toQColor()));

    if (command) {
        canvas->addCommand(command);
    }
}
