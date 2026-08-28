/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <kis_painting_assistant.h>
#include <kis_painting_assistant_collection.h>

namespace
{

class TestAssistant final : public KisPaintingAssistant
{
public:
    TestAssistant()
        : KisPaintingAssistant(PkString("model-test"), PkString("Model Test"))
    {
    }

    KisPaintingAssistantSP clone(
        PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const override
    {
        return KisPaintingAssistantSP(new TestAssistant(*this, handleMap));
    }

    PkPointF adjustPosition(const PkPointF &point, const PkPointF &, bool, qreal) override
    {
        return point;
    }

    void adjustLine(PkPointF &, PkPointF &) override {}
    PkPointF getDefaultEditorPosition() const override { return PkPointF(); }
    int numHandles() const override { return 1; }

    void endStroke() override
    {
        ++endStrokeCount;
        KisPaintingAssistant::endStroke();
    }

    int endStrokeCount = 0;

protected:
    TestAssistant(
        const TestAssistant &rhs,
        PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap)
        : KisPaintingAssistant(rhs, handleMap)
    {
    }
};

int preservesModelStateWithoutUiLibrary()
{
    TestAssistant assistant;
    if (assistant.id() != "model-test" || assistant.name() != "Model Test") return 1;

    assistant.setSnappingActive(false);
    assistant.setLocal(true);
    assistant.setLocked(true);
    assistant.setUseCustomColor(true);
    assistant.setAssistantCustomColor(PkColor(12, 34, 56, 78));

    if (assistant.isSnappingActive() || !assistant.isLocal() || !assistant.isLocked()) return 2;
    if (assistant.effectiveAssistantColor() != PkColor(12, 34, 56, 78)) return 3;

    KisPaintingAssistantHandleSP handle(new KisPaintingAssistantHandle(PkPointF(2.0, 3.0)));
    assistant.addHandle(handle, HandleType::NORMAL);
    if (assistant.handles().size() != 1) return 4;

    PkTransform transform;
    transform.translate(5.0, -2.0);
    assistant.transform(transform);
    if (PkPointF(*assistant.handles().first()) != PkPointF(7.0, 1.0)) return 5;
    return 0;
}

int collectionEndsStrokeForEveryAssistant()
{
    KisPaintingAssistantSP first(new TestAssistant);
    KisPaintingAssistantSP second(new TestAssistant);
    KisPaintingAssistantCollection collection({first, second});

    collection.setFirstAssistant(first);
    if (collection.firstAssistant() != first) return 10;

    collection.endStroke();
    if (collection.firstAssistant()) return 11;
    if (static_cast<TestAssistant *>(first.data())->endStrokeCount != 1) return 12;
    if (static_cast<TestAssistant *>(second.data())->endStrokeCount != 1) return 13;
    return 0;
}

}

int main()
{
    if (const int result = preservesModelStateWithoutUiLibrary()) return result;
    return collectionEndsStrokeForEveryAssistant();
}
