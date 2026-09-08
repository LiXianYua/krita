/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <simpletest.h>

#include <PkFlakeBridge.h>

#include <QAction>

#include <vector>

#include "../tool/KisSelectionToolFactoryBase.h"

namespace {
class SelectionFactory final : public KisSelectionToolFactoryBase
{
public:
    SelectionFactory()
        : KisSelectionToolFactoryBase(PkString("selection"))
    {
    }

    KoToolBase *createTool(KoCanvasBase *) override { return nullptr; }

    PkList<QAction *> actions()
    {
        return createActionsImpl();
    }
};

class PolylineFactory final : public KisToolPolyLineFactoryBase
{
public:
    PolylineFactory()
        : KisToolPolyLineFactoryBase(PkString("polyline"))
    {
    }

    KoToolBase *createTool(KoCanvasBase *) override { return nullptr; }

    PkList<QAction *> actions()
    {
        return createActionsImpl();
    }
};
}

class KisSelectionToolFactoryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void preservesSelectionActionNamesAndFactoryOwnership();
    void preservesPolylineActionNamesAndFactoryOwnership();
};

void KisSelectionToolFactoryTest::preservesSelectionActionNamesAndFactoryOwnership()
{
    SelectionFactory factory;
    const auto actions = factory.actions();
    QCOMPARE(actions.size(), 10);

    const std::vector<PkString> expected {
        PkString("increase_brush_size"),
        PkString("decrease_brush_size"),
        PkString("rotate_brush_tip_clockwise"),
        PkString("rotate_brush_tip_clockwise_precise"),
        PkString("rotate_brush_tip_counter_clockwise"),
        PkString("rotate_brush_tip_counter_clockwise_precise"),
        PkString("selection_tool_mode_add"),
        PkString("selection_tool_mode_replace"),
        PkString("selection_tool_mode_subtract"),
        PkString("selection_tool_mode_intersect")
    };
    for (int i = 0; i < actions.size(); ++i) {
        QCOMPARE(toPkString(actions.at(i)->objectName()), expected.at(i));
        QCOMPARE(actions.at(i)->parent(), static_cast<QObject *>(&factory));
    }
}

void KisSelectionToolFactoryTest::preservesPolylineActionNamesAndFactoryOwnership()
{
    PolylineFactory factory;
    const auto actions = factory.actions();
    QCOMPARE(actions.size(), 8);
    QCOMPARE(toPkString(actions.at(6)->objectName()), PkString("undo_polygon_selection"));
    QCOMPARE(toPkString(actions.at(7)->objectName()), PkString("selection_tool_mode_add"));
    QCOMPARE(actions.at(6)->parent(), static_cast<QObject *>(&factory));
    QCOMPARE(actions.at(7)->parent(), static_cast<QObject *>(&factory));
}

SIMPLE_TEST_MAIN(KisSelectionToolFactoryTest)

#include "kis_selection_tool_factory_test.moc"
