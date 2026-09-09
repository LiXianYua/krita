/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <simpletest.h>

#include <PkPainterPath.h>
#include <PkPen.h>
#include <PkFlakeBridge.h>

#include <QAction>
#include <KLocalizedString>
#include <QPointer>

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
    void preservesSelectionActionNames();
    void preservesPolylineActionNames();
    void directActionsFollowFactoryLifetime();
    void collectionOwnsActions();
    void duplicateCreationReusesCollectionActions();
};

void KisSelectionToolFactoryTest::preservesSelectionActionNames()
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
    const std::vector<QString> expectedPaintActionText {
        i18n("Increase Brush Size"),
        i18n("Decrease Brush Size"),
        i18n("Rotate brush tip clockwise"),
        i18n("Rotate brush tip clockwise (precise)"),
        i18n("Rotate brush tip counter-clockwise"),
        i18n("Rotate brush tip counter-clockwise (precise)")
    };
    for (int i = 0; i < actions.size(); ++i) {
        QCOMPARE(toPkString(actions.at(i)->objectName()), expected.at(i));
        if (i < static_cast<int>(expectedPaintActionText.size())) {
            QCOMPARE(actions.at(i)->text(), expectedPaintActionText.at(i));
        }
    }
}

void KisSelectionToolFactoryTest::preservesPolylineActionNames()
{
    PolylineFactory factory;
    const auto actions = factory.actions();
    QCOMPARE(actions.size(), 8);
    QCOMPARE(toPkString(actions.at(6)->objectName()), PkString("undo_polygon_selection"));
    QCOMPARE(toPkString(actions.at(7)->objectName()), PkString("selection_tool_mode_add"));
}

void KisSelectionToolFactoryTest::directActionsFollowFactoryLifetime()
{
    std::vector<QPointer<QAction>> guardedActions;
    {
        SelectionFactory factory;
        const auto actions = factory.actions();
        guardedActions.reserve(actions.size());
        for (QAction *action : actions) {
            guardedActions.emplace_back(action);
        }
    }

    bool allDestroyed = true;
    for (const QPointer<QAction> &action : guardedActions) {
        if (action) {
            allDestroyed = false;
            delete action.data();
        }
    }
    QVERIFY(allDestroyed);
}

void KisSelectionToolFactoryTest::collectionOwnsActions()
{
    SelectionFactory factory;
    std::vector<QPointer<QAction>> guardedActions;
    {
        QObject collection;
        const auto actions = factory.createActions(&collection);
        guardedActions.reserve(actions.size());
        for (QAction *action : actions) {
            QCOMPARE(action->parent(), &collection);
            guardedActions.emplace_back(action);
        }
    }

    for (const QPointer<QAction> &action : guardedActions) {
        QVERIFY(action.isNull());
    }
}

void KisSelectionToolFactoryTest::duplicateCreationReusesCollectionActions()
{
    SelectionFactory factory;
    QObject collection;
    const auto first = factory.createActions(&collection);
    const auto second = factory.createActions(&collection);

    QCOMPARE(second.size(), first.size());
    for (int i = 0; i < first.size(); ++i) {
        QCOMPARE(second.at(i), first.at(i));
        const PkStringList tools =
            toPkStringList(first.at(i)->property("tool_action").toStringList());
        QCOMPARE(tools, PkStringList({factory.id(), factory.id()}));
    }
}

SIMPLE_TEST_MAIN(KisSelectionToolFactoryTest)

#include "kis_selection_tool_factory_test.moc"
