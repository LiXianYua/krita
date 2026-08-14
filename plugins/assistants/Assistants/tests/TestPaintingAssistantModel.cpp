/*
 *  SPDX-FileCopyrightText: 2026 Krita contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtTest>

#include <kis_painting_assistant.h>
#include <kis_painting_assistant_collection.h>

namespace
{

class TestAssistant final : public KisPaintingAssistant
{
public:
    TestAssistant()
        : KisPaintingAssistant(QStringLiteral("model-test"), QStringLiteral("Model Test"))
    {
    }

    KisPaintingAssistantSP clone(
        QMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const override
    {
        return KisPaintingAssistantSP(new TestAssistant(*this, handleMap));
    }

    QPointF adjustPosition(const QPointF &point, const QPointF &, bool, qreal) override
    {
        return point;
    }

    void adjustLine(QPointF &, QPointF &) override
    {
    }

    QPointF getDefaultEditorPosition() const override
    {
        return QPointF();
    }

    int numHandles() const override
    {
        return 1;
    }

    void endStroke() override
    {
        ++endStrokeCount;
        KisPaintingAssistant::endStroke();
    }

    int endStrokeCount = 0;

protected:
    TestAssistant(
        const TestAssistant &rhs,
        QMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap)
        : KisPaintingAssistant(rhs, handleMap)
    {
    }

    void drawCache(QPainter &, const KisCoordinatesConverter *, bool) override
    {
    }
};

} // namespace

class TestPaintingAssistantModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void preservesModelStateWithoutUiLibrary();
    void collectionEndsStrokeForEveryAssistant();
};

void TestPaintingAssistantModel::preservesModelStateWithoutUiLibrary()
{
    TestAssistant assistant;

    QCOMPARE(assistant.id(), QStringLiteral("model-test"));
    QCOMPARE(assistant.name(), QStringLiteral("Model Test"));

    assistant.setSnappingActive(false);
    assistant.setLocal(true);
    assistant.setLocked(true);
    assistant.setUseCustomColor(true);
    assistant.setAssistantCustomColor(QColor(12, 34, 56, 78));

    QVERIFY(!assistant.isSnappingActive());
    QVERIFY(assistant.isLocal());
    QVERIFY(assistant.isLocked());
    QCOMPARE(assistant.effectiveAssistantColor(), QColor(12, 34, 56, 78));

    KisPaintingAssistantHandleSP handle(new KisPaintingAssistantHandle(QPointF(2.0, 3.0)));
    assistant.addHandle(handle, HandleType::NORMAL);
    QCOMPARE(assistant.handles().size(), 1);

    QTransform transform;
    transform.translate(5.0, -2.0);
    assistant.transform(transform);
    QCOMPARE(QPointF(*assistant.handles().constFirst()), QPointF(7.0, 1.0));
}

void TestPaintingAssistantModel::collectionEndsStrokeForEveryAssistant()
{
    KisPaintingAssistantSP first(new TestAssistant);
    KisPaintingAssistantSP second(new TestAssistant);
    KisPaintingAssistantCollection collection({first, second});

    collection.setFirstAssistant(first);
    QCOMPARE(collection.firstAssistant(), first);

    collection.endStroke();

    QVERIFY(!collection.firstAssistant());
    QCOMPARE(static_cast<TestAssistant *>(first.data())->endStrokeCount, 1);
    QCOMPARE(static_cast<TestAssistant *>(second.data())->endStrokeCount, 1);
}

QTEST_GUILESS_MAIN(TestPaintingAssistantModel)

#include "TestPaintingAssistantModel.moc"
