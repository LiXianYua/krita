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

    PkList<KisHostActionSpec> actions()
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

    PkList<KisHostActionSpec> actions()
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
    const auto specs = factory.actions();
    QCOMPARE(specs.size(), 10);

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
    // 声明侧只携带未翻译源文本（const char*）；翻译发生在 createActions() 的
    // 物化边界，故这里钉住源文本，翻译后的显示文本在 collectionOwnsActions()
    // 里对同一批 spec 的物化结果断言。
    const std::vector<const char *> expectedPaintActionSourceText {
        "Increase Brush Size",
        "Decrease Brush Size",
        "Rotate brush tip clockwise",
        "Rotate brush tip clockwise (precise)",
        "Rotate brush tip counter-clockwise",
        "Rotate brush tip counter-clockwise (precise)"
    };
    for (int i = 0; i < specs.size(); ++i) {
        QCOMPARE(specs.at(i).objectName, expected.at(i));
        if (i < static_cast<int>(expectedPaintActionSourceText.size())) {
            QCOMPARE(PkString(specs.at(i).text), PkString(expectedPaintActionSourceText.at(i)));
        }
    }
}

void KisSelectionToolFactoryTest::preservesPolylineActionNames()
{
    PolylineFactory factory;
    const auto specs = factory.actions();
    QCOMPARE(specs.size(), 8);
    QCOMPARE(specs.at(6).objectName, PkString("undo_polygon_selection"));
    QCOMPARE(specs.at(7).objectName, PkString("selection_tool_mode_add"));
}

void KisSelectionToolFactoryTest::directActionsFollowFactoryLifetime()
{
    std::vector<QPointer<QAction>> guardedActions;
    {
        SelectionFactory factory;
        // actionCollection == nullptr: the materialization boundary keeps every
        // candidate parented to the factory's private actionOwner, so the whole
        // returned list must die together with the factory. No cleanup on the
        // observation side — a survivor here is a real defect, not a test artefact.
        const auto actions = factory.createActions(nullptr);
        QCOMPARE(actions.size(), 10);
        guardedActions.reserve(actions.size());
        for (QAction *action : actions) {
            guardedActions.emplace_back(action);
        }
    }

    int survivingCount = 0;
    for (const QPointer<QAction> &action : guardedActions) {
        if (!action.isNull()) {
            ++survivingCount;
        }
    }
    QCOMPARE(survivingCount, 0);
}

void KisSelectionToolFactoryTest::collectionOwnsActions()
{
    SelectionFactory factory;
    std::vector<QPointer<QAction>> guardedActions;
    {
        QObject collection;
        const auto actions = factory.createActions(&collection);
        QCOMPARE(actions.size(), 10);
        guardedActions.reserve(actions.size());
        for (QAction *action : actions) {
            QCOMPARE(action->parent(), &collection);
            guardedActions.emplace_back(action);
        }

        // 物化侧的翻译后显示文本断言：与声明侧的源文本断言合起来覆盖旧版
        // 「createHostAction() 创建时即翻译」那条覆盖。
        const std::vector<QString> expectedPaintActionText {
            i18n("Increase Brush Size"),
            i18n("Decrease Brush Size"),
            i18n("Rotate brush tip clockwise"),
            i18n("Rotate brush tip clockwise (precise)"),
            i18n("Rotate brush tip counter-clockwise"),
            i18n("Rotate brush tip counter-clockwise (precise)")
        };
        for (int i = 0; i < static_cast<int>(expectedPaintActionText.size()); ++i) {
            QCOMPARE(actions.at(i)->text(), expectedPaintActionText.at(i));
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
