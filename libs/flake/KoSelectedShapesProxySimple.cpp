/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KoSelectedShapesProxySimple.h"

#include "kis_assert.h"
#include <KoShapeManager.h>
#include <KoShapeLayer.h>
#include <KoSelection.h>

KoSelectedShapesProxySimple::KoSelectedShapesProxySimple(KoShapeManager *shapeManager)
    : m_shapeManager(shapeManager)
{
    KIS_ASSERT_RECOVER_RETURN(m_shapeManager);

    const PkPointer<KoSelectedShapesProxySimple> guard(this);
    m_connections.append(PkObject::connect(
        m_shapeManager.data(), &KoShapeManager::selectionChanged, m_shapeManager.data(),
        [guard] { if (guard) guard->selectionChanged(); }));
    m_connections.append(PkObject::connect(
        m_shapeManager.data(), &KoShapeManager::selectionContentChanged, m_shapeManager.data(),
        [guard] { if (guard) guard->selectionContentChanged(); }));
    KoSelection *selection = m_shapeManager->selection();
    m_connections.append(PkObject::connect(
        selection, &KoSelection::currentLayerChanged, selection,
        [guard](const KoShapeLayer *layer) { if (guard) guard->currentLayerChanged(layer); }));
}

KoSelectedShapesProxySimple::~KoSelectedShapesProxySimple()
{
    for (PkConnection &connection : m_connections) {
        PkObject::disconnect(connection);
    }
}

KoSelection *KoSelectedShapesProxySimple::selection()
{
    KIS_ASSERT_RECOVER_RETURN_VALUE(m_shapeManager, 0);
    return m_shapeManager->selection();
}
