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

    connect(m_shapeManager.data(), &KoShapeManager::selectionChanged, this, &KoSelectedShapesProxy::selectionChanged);
    connect(m_shapeManager.data(), &KoShapeManager::selectionContentChanged, this, &KoSelectedShapesProxy::selectionContentChanged);
    connect(m_shapeManager->selection(), &KoSelection::currentLayerChanged, this, &KoSelectedShapesProxy::currentLayerChanged);
}

KoSelection *KoSelectedShapesProxySimple::selection()
{
    KIS_ASSERT_RECOVER_RETURN_VALUE(m_shapeManager, 0);
    return m_shapeManager->selection();
}

