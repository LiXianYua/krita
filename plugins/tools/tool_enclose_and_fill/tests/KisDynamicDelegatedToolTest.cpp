/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <simpletest.h>

#include <PkConnection.h>
#include <PkObject.h>
#include <PkPointer.h>

#include <QCursor>

#include "kis_paint_device.h"
#include "tool/kis_tool.h"
#include "subtools/KisDynamicDelegatedTool.h"

namespace
{
class DynamicBase : public KisTool
{
public:
    DynamicBase(KoCanvasBase *canvas, const QCursor &cursor)
        : KisTool(canvas, cursor)
    {
    }

    void paint(PkPainter &, const KoViewConverter &) override {}
    void mousePressEvent(KoPointerEvent *) override {}
    void mouseMoveEvent(KoPointerEvent *) override {}
    void mouseReleaseEvent(KoPointerEvent *) override {}
    void pkKeyPressEvent(PkToolKeyEvent *event) override
    {
        ++keyPressCount;
        event->accept();
    }
    void pkKeyReleaseEvent(PkToolKeyEvent *event) override
    {
        ++keyReleaseCount;
        event->ignore();
    }
    void canvasResourceChanged(int key, const PkVariant &value) override
    {
        resourceKey = key;
        resourceValue = value;
        ++resourceChangeCount;
    }
    void useCursor(const QCursor &cursor) { cursorChanged(cursor); }
    virtual void requestUpdateOutline(const PkPointF &, const KoPointerEvent *) {}
    int getOutlinePath() const { return 0; }

    int keyPressCount = 0;
    int keyReleaseCount = 0;
    int resourceChangeCount = 0;
    int resourceKey = 0;
    PkVariant resourceValue;
};

using DynamicTool = KisDynamicDelegatedTool<DynamicBase>;
using DelegateTool = DynamicTool::DelegateType;
}

class KisDynamicDelegatedToolTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void forwardsAllNotificationsAcrossReplacement();
    void forwardsKeyAndResourceEventsAcrossReplacement();
    void disconnectsWhenSenderOrReceiverDies();
};

void KisDynamicDelegatedToolTest::forwardsAllNotificationsAcrossReplacement()
{
    DynamicTool tool(nullptr);
    PkObject observer;
    PkString activationId;
    QCursor cursor;
    bool hasSelection = false;
    PkString statusText;
    int activationCount = 0;
    int cursorCount = 0;
    int selectionCount = 0;
    int statusCount = 0;

    PkObject::connect(&tool, &KoToolBase::activateTool,
                      &observer, [&](const PkString &value) {
                          activationId = value;
                          ++activationCount;
                      });
    PkObject::connect(&tool, &KoToolBase::cursorChanged,
                      &observer, [&](const QCursor &value) {
                          cursor = value;
                          ++cursorCount;
                      });
    PkObject::connect(&tool, &KoToolBase::selectionChanged,
                      &observer, [&](bool value) {
                          hasSelection = value;
                          ++selectionCount;
                      });
    PkObject::connect(&tool, &KoToolBase::statusTextChanged,
                      &observer, [&](const PkString &value) {
                          statusText = value;
                          ++statusCount;
                      });

    auto *first = new DelegateTool(nullptr, QCursor());
    PkPointer<DelegateTool> firstGuard(first);
    tool.setDelegateTool(first);
    first->activateTool("first-tool");
    first->cursorChanged(QCursor(Qt::CrossCursor));
    first->selectionChanged(true);
    first->statusTextChanged("first-status");

    QCOMPARE(activationId, PkString("first-tool"));
    QCOMPARE(cursor.shape(), Qt::CrossCursor);
    QVERIFY(hasSelection);
    QCOMPARE(statusText, PkString("first-status"));
    QCOMPARE(activationCount, 1);
    QCOMPARE(cursorCount, 1);
    QCOMPARE(selectionCount, 1);
    QCOMPARE(statusCount, 1);

    auto *second = new DelegateTool(nullptr, QCursor());
    PkPointer<DelegateTool> secondGuard(second);
    tool.setDelegateTool(second);
    QVERIFY(firstGuard.isNull());
    second->activateTool("second-tool");
    second->cursorChanged(QCursor(Qt::WaitCursor));
    second->selectionChanged(false);
    second->statusTextChanged("second-status");

    QCOMPARE(activationId, PkString("second-tool"));
    QCOMPARE(cursor.shape(), Qt::WaitCursor);
    QVERIFY(!hasSelection);
    QCOMPARE(statusText, PkString("second-status"));
    QCOMPARE(activationCount, 2);
    QCOMPARE(cursorCount, 2);
    QCOMPARE(selectionCount, 2);
    QCOMPARE(statusCount, 2);

    tool.setDelegateTool(nullptr);
    QVERIFY(secondGuard.isNull());
}

void KisDynamicDelegatedToolTest::forwardsKeyAndResourceEventsAcrossReplacement()
{
    DynamicTool tool(nullptr);

    auto *first = new DelegateTool(nullptr, QCursor());
    tool.setDelegateTool(first);

    PkToolKeyEvent pressEvent(Pk::Key_A, Pk::ControlModifier, false);
    tool.pkKeyPressEvent(&pressEvent);
    QVERIFY(pressEvent.isAccepted());
    QCOMPARE(first->keyPressCount, 1);

    PkToolKeyEvent releaseEvent(Pk::Key_A, Pk::ControlModifier, true);
    tool.pkKeyReleaseEvent(&releaseEvent);
    QVERIFY(!releaseEvent.isAccepted());
    QCOMPARE(first->keyReleaseCount, 1);

    tool.canvasResourceChanged(17, PkVariant(PkString("first-resource")));
    QCOMPARE(first->resourceChangeCount, 1);
    QCOMPARE(first->resourceKey, 17);
    QCOMPARE(first->resourceValue.toString(), PkString("first-resource"));

    auto *second = new DelegateTool(nullptr, QCursor());
    tool.setDelegateTool(second);

    PkToolKeyEvent replacementPress(Pk::Key_Return, Pk::ShiftModifier, false);
    tool.pkKeyPressEvent(&replacementPress);
    QVERIFY(replacementPress.isAccepted());
    QCOMPARE(second->keyPressCount, 1);

    tool.canvasResourceChanged(23, PkVariant(PkString("second-resource")));
    QCOMPARE(second->resourceChangeCount, 1);
    QCOMPARE(second->resourceKey, 23);
    QCOMPARE(second->resourceValue.toString(), PkString("second-resource"));
}

void KisDynamicDelegatedToolTest::disconnectsWhenSenderOrReceiverDies()
{
    auto *sender = new DelegateTool(nullptr, QCursor());
    auto *receiver = new DynamicTool(nullptr);
    PkConnection senderLifetime =
        PkObject::connect(sender, &KoToolBase::statusTextChanged,
                          receiver, &KoToolBase::statusTextChanged);
    QVERIFY(senderLifetime.isValid());
    delete sender;
    QVERIFY(!senderLifetime.isValid());

    sender = new DelegateTool(nullptr, QCursor());
    PkConnection receiverLifetime =
        PkObject::connect(sender, &KoToolBase::statusTextChanged,
                          receiver, &KoToolBase::statusTextChanged);
    QVERIFY(receiverLifetime.isValid());
    delete receiver;
    QVERIFY(!receiverLifetime.isValid());

    sender->statusTextChanged("ignored-after-receiver-destruction");
    delete sender;
}

SIMPLE_TEST_MAIN(KisDynamicDelegatedToolTest)

#include "KisDynamicDelegatedToolTest.moc"
