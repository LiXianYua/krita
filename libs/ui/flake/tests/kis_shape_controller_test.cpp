/*
 * SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_shape_controller_test.h"

#include <simpletest.h>

#include "kis_name_server.h"
#include "kis_shape_controller.h"
#include "kis_shape_layer.h"

#include <KoDocumentResourceManager.h>
#include <KoPathShape.h>
#include <kundo2stack.h>


KisDummiesFacadeBase* KisShapeControllerTest::dummiesFacadeFactory()
{
    m_nameServer = new KisNameServer();
    m_undoStack = new KUndo2Stack();
    m_controller = new KisShapeController(m_nameServer, m_undoStack);
    return m_controller;
}

void KisShapeControllerTest::destroyDummiesFacade(KisDummiesFacadeBase *dummiesFacade)
{
    delete dummiesFacade;
    delete m_undoStack;
    delete m_nameServer;
}

void KisShapeControllerTest::testCreatesVectorLayerWithoutDesktopContext()
{
    m_controller->setImage(m_image);

    KoPathShape shape;
    KUndo2Command parentCommand;
    KoShapeContainer *parent =
        m_controller->createParentForShapes({&shape}, false, &parentCommand);

    QVERIFY(parent);
    QVERIFY(dynamic_cast<KisShapeLayer *>(parent));
    QCOMPARE(m_controller->resourceManager()->undoStack(), m_undoStack);
}

SIMPLE_TEST_MAIN(KisShapeControllerTest)
