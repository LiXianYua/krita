/*
 * SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISSHAPECONTROLLER_TEST_H
#define KISSHAPECONTROLLER_TEST_H

#include "kis_dummies_facade_base_test.h"

class KisNameServer;
class KisShapeController;
class KUndo2Stack;


class KisShapeControllerTest : public KisDummiesFacadeBaseTest
{
    Q_OBJECT

protected:
    KisDummiesFacadeBase* dummiesFacadeFactory() override;
    void destroyDummiesFacade(KisDummiesFacadeBase *dummiesFacade) override;

private Q_SLOTS:
    void testCreatesVectorLayerWithoutDesktopContext();
    void testFindsShapeManagersForVectorLayerAndSelectionMask();

private:
    KisShapeController *m_controller;
    KisNameServer *m_nameServer;
    KUndo2Stack *m_undoStack;
};

#endif
