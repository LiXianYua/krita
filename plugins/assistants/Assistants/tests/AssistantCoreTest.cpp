/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_painting_assistant.h"
#include "kis_painting_assistant_collection.h"
#include "assistant_tool.h"
#include "ConcentricEllipseAssistant.h"
#include "ConcentricEllipseAssistantGeometry.h"
#include "CurvilinearPerspectiveAssistant.h"
#include "Ellipse.h"
#include "EllipseAssistant.h"
#include "FisheyePointAssistant.h"
#include "InfiniteRulerAssistant.h"
#include "ParallelRulerAssistant.h"
#include "PerspectiveAssistant.h"
#include "PerspectiveBasedAssistantHelper.h"
#include "PerspectiveEllipseAssistant.h"
#include "Ruler.h"
#include "RulerAssistant.h"
#include "SplineAssistant.h"
#include "TwoPointAssistant.h"
#include "VanishingPointAssistant.h"

#include <KoStore.h>

#include <PkAuxTypes.h>
#include <PkColor.h>
#include <PkList.h>
#include <PkMap.h>
#include <PkPoint.h>
#include <PkString.h>
#include <PkTransform.h>
#include <PkXmlDocument.h>
#include <PkXmlElement.h>
#include <PkXmlStreamReader.h>
#include <PkXmlStreamWriter.h>

#include <type_traits>
#include <cmath>
#include <thread>
#include <vector>

namespace
{
using HandleMap = PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP>;
using HandleIdMap = PkMap<KisPaintingAssistantHandleSP, int>;
using IdHandleMap = PkMap<int, KisPaintingAssistantHandleSP>;

using CloneSignature = KisPaintingAssistantSP (KisPaintingAssistant::*)(HandleMap &) const;
using AdjustSignature = PkPointF (KisPaintingAssistant::*)(const PkPointF &, const PkPointF &, bool, qreal);
using TransformSignature = void (KisPaintingAssistant::*)(const PkTransform &);
using SaveSignature = PkByteArray (KisPaintingAssistant::*)(HandleIdMap &);
using LoadSignature = void (KisPaintingAssistant::*)(KoStore *, IdHandleMap &, PkString);
using SaveListSignature = void (KisPaintingAssistant::*)(PkXmlDocument &, PkXmlElement &, int);

static_assert(std::is_same_v<decltype(&KisPaintingAssistant::clone), CloneSignature>);
static_assert(std::is_same_v<decltype(&KisPaintingAssistant::adjustPosition), AdjustSignature>);
static_assert(std::is_same_v<decltype(&KisPaintingAssistant::transform), TransformSignature>);
static_assert(std::is_same_v<decltype(&KisPaintingAssistant::saveXml), SaveSignature>);
static_assert(std::is_same_v<decltype(&KisPaintingAssistant::loadXml), LoadSignature>);
static_assert(std::is_same_v<decltype(&KisPaintingAssistant::saveXmlList), SaveListSignature>);
static_assert(std::is_same_v<decltype(&KisPaintingAssistantFactory::id), PkString (KisPaintingAssistantFactory::*)() const>);
static_assert(std::is_same_v<decltype(&KisPaintingAssistantCollection::assistants), PkList<KisPaintingAssistantSP> (KisPaintingAssistantCollection::*)() const>);

bool closeEnough(double lhs, double rhs, double epsilon = 1e-7)
{
    return std::abs(lhs - rhs) <= epsilon;
}

bool finitePoint(const PkPointF &point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

bool closePoint(const PkPointF &actual, const PkPointF &expected, double epsilon = 1e-6)
{
    return closeEnough(actual.x(), expected.x(), epsilon) &&
           closeEnough(actual.y(), expected.y(), epsilon);
}

template<typename Assistant>
void addNormalHandles(Assistant &assistant, std::initializer_list<PkPointF> points)
{
    for (const PkPointF &point : points) {
        assistant.addHandle(KisPaintingAssistantHandleSP(new KisPaintingAssistantHandle(point)), HandleType::NORMAL);
    }
}

PkList<PkPointF> serializedSideHandlePoints(const PkByteArray &bytes)
{
    PkList<PkPointF> points;
    PkXmlStreamReader reader(PkString::PkFromUtf8(bytes.constData(), bytes.size()));
    while (!reader.atEnd()) {
        if (reader.readNext() != PkXmlStreamReader::StartElement ||
            reader.name() != "sidehandle") {
            continue;
        }
        points << PkPointF(reader.attributes().value("x").toDouble(),
                           reader.attributes().value("y").toDouble());
    }
    return reader.hasError() ? PkList<PkPointF>() : points;
}

int registryPreservesAllIdsAndIsIdempotent()
{
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back(registerAssistantFactories);
    }
    for (std::thread &thread : threads) thread.join();
    registerAssistantFactories();

    if (assistantToolPluginId() != "AssistantTool") return 1;
    const PkList<PkString> ids {
        "ruler", "ellipse", "spline", "perspective", "vanishing point",
        "infinite ruler", "parallel ruler", "concentric ellipse", "fisheye-point",
        "two point", "perspective ellipse", "curvilinear-perspective"
    };
    KisPaintingAssistantFactoryRegistry *registry = KisPaintingAssistantFactoryRegistry::instance();
    if (registry->count() != ids.size()) return 2;
    for (const PkString &id : ids) {
        KisPaintingAssistantFactory *factory = registry->get(id);
        if (!factory || factory->id() != id || !factory->createPaintingAssistant()) return 3;
    }
    return 0;
}

int coreStateCloneHandlesAndCollectionRemainLive()
{
    RulerAssistant original;
    addNormalHandles(original, {{0.0, 0.0}, {10.0, 0.0}});
    original.setSnappingActive(false);
    original.setUseCustomColor(true);
    original.setAssistantCustomColor(PkColor(12, 34, 56, 78));
    original.setLocked(true);

    HandleMap handleMap;
    KisPaintingAssistantSP clone = original.clone(handleMap);
    if (!clone || clone->id() != "ruler" || clone->handles().size() != 2) return 10;
    if (clone->handles()[0] == original.handles()[0] || clone->handles()[1] == original.handles()[1]) return 11;
    if (PkPointF(*clone->handles()[0]) != PkPointF(0.0, 0.0) ||
        PkPointF(*clone->handles()[1]) != PkPointF(10.0, 0.0)) return 12;
    if (clone->isSnappingActive() || !clone->isLocked() ||
        clone->effectiveAssistantColor() != PkColor(12, 34, 56, 78)) return 13;

    PkList<KisPaintingAssistantSP> ordered {KisPaintingAssistantSP(new RulerAssistant), clone};
    KisPaintingAssistantCollection collection(ordered);
    if (collection.assistants().size() != 2 || collection.assistants()[0] != ordered[0] ||
        collection.assistants()[1] != ordered[1]) return 14;
    collection.setFirstAssistant(clone);
    collection.endStroke();
    if (collection.firstAssistant()) return 15;
    return 0;
}

int sharedHandleMergeRewiresEveryAssistant()
{
    KisPaintingAssistantSP first(new RulerAssistant);
    KisPaintingAssistantSP second(new RulerAssistant);
    KisPaintingAssistantSP third(new RulerAssistant);
    KisPaintingAssistantHandleSP firstOnly(new KisPaintingAssistantHandle(PkPointF(-10, 0)));
    KisPaintingAssistantHandleSP shared(new KisPaintingAssistantHandle(PkPointF(0, 0)));
    KisPaintingAssistantHandleSP secondOnly(new KisPaintingAssistantHandle(PkPointF(10, 0)));
    KisPaintingAssistantHandleSP thirdOnly(new KisPaintingAssistantHandle(PkPointF(20, 0)));
    first->addHandle(firstOnly, HandleType::NORMAL);
    first->addHandle(shared, HandleType::NORMAL);
    second->addHandle(shared, HandleType::NORMAL);
    second->addHandle(secondOnly, HandleType::NORMAL);
    third->addHandle(thirdOnly, HandleType::NORMAL);
    third->addHandle(shared, HandleType::NORMAL);

    KisPaintingAssistantHandleSP replacement(new KisPaintingAssistantHandle(PkPointF(1, 2)));
    replacement->setType(HandleType::CORNER);
    replacement->mergeWith(shared);

    if (first->handles().size() != 2 || first->handles()[0] != firstOnly ||
        first->handles()[1] != replacement) return 110;
    if (second->handles().size() != 2 || second->handles()[0] != replacement ||
        second->handles()[1] != secondOnly) return 111;
    if (third->handles().size() != 2 || third->handles()[0] != thirdOnly ||
        third->handles()[1] != replacement) return 112;
    if (shared->chiefAssistant() != nullptr || replacement->chiefAssistant() != first.data()) return 113;

    first.clear();
    if (replacement->chiefAssistant() != second.data()) return 114;
    second.clear();
    if (replacement->chiefAssistant() != third.data()) return 115;
    third.clear();
    if (replacement->chiefAssistant() != nullptr) return 116;
    return 0;
}

int renderIndependentHandleLifecycleRemainsLive()
{
    PerspectiveAssistant perspective;
    addNormalHandles(perspective, {{0, 0}, {10, 0}, {10, 10}, {0, 10}});
    if (perspective.sideHandles().size() != 4 || !perspective.topLeft() ||
        !perspective.topRight() || !perspective.bottomLeft() || !perspective.bottomRight()) return 120;
    if (!closePoint(*perspective.rightMiddle(), {10, 5}) ||
        !closePoint(*perspective.leftMiddle(), {0, 5}) ||
        !closePoint(*perspective.bottomMiddle(), {5, 10}) ||
        !closePoint(*perspective.topMiddle(), {5, 0})) return 121;

    *perspective.handles()[1] += PkPointF(10, 0);
    if (!closePoint(*perspective.rightMiddle(), {15, 5}) ||
        !closePoint(*perspective.topMiddle(), {10, 0})) return 122;

    perspective.handles()[2]->setX(30);
    perspective.handles()[2]->setY(20);
    HandleIdMap perspectiveHandleIds;
    const PkByteArray savedPerspectiveXml = perspective.saveXml(perspectiveHandleIds);
    const PkList<PkPointF> savedPerspectiveSides = serializedSideHandlePoints(savedPerspectiveXml);
    if (savedPerspectiveSides.size() != 4 ||
        !closePoint(savedPerspectiveSides[0], {25, 10}) ||
        !closePoint(savedPerspectiveSides[2], {15, 15}) ||
        !closePoint(savedPerspectiveSides[3], {10, 0})) return 126;
    if (perspective.topRight() != perspective.handles()[1] ||
        perspective.bottomRight() != perspective.handles()[2] ||
        !closePoint(*perspective.rightMiddle(), {25, 10}) ||
        !closePoint(*perspective.bottomMiddle(), {15, 15})) return 127;

    PerspectiveAssistant sharedPerspective;
    KisPaintingAssistantHandleSP sharedCorner = perspective.handles()[1];
    sharedPerspective.addHandle(new KisPaintingAssistantHandle(0, 0), HandleType::NORMAL);
    sharedPerspective.addHandle(sharedCorner, HandleType::NORMAL);
    sharedPerspective.addHandle(new KisPaintingAssistantHandle(10, 10), HandleType::NORMAL);
    sharedPerspective.addHandle(new KisPaintingAssistantHandle(0, 10), HandleType::NORMAL);
    *sharedCorner += PkPointF(10, 0);
    if (!closePoint(*perspective.topMiddle(), {15, 0}) ||
        !closePoint(*sharedPerspective.topMiddle(), {15, 0}) ||
        !closePoint(*sharedPerspective.rightMiddle(), {20, 5})) return 128;

    VanishingPointAssistant freshVanishing;
    addNormalHandles(freshVanishing, {{10, 20}});
    if (freshVanishing.sideHandles().size() != 4) return 123;

    const char legacyText[] =
        "<assistant type=\"vanishing point\" active=\"1\">"
        "<handles><handle id=\"0\" x=\"10\" y=\"20\"/></handles>"
        "</assistant>";
    KoStore legacyStore(PkByteArray(legacyText, static_cast<int>(sizeof(legacyText) - 1)));
    IdHandleMap loadedHandles;
    VanishingPointAssistant loadedVanishing;
    loadedVanishing.loadXml(&legacyStore, loadedHandles, "legacy-vanishing.assistant");
    if (loadedVanishing.handles().size() != 1 || loadedVanishing.sideHandles().size() != 4) return 124;
    const PkList<PkPointF> expected {{-60, 20}, {-130, 20}, {80, 20}, {150, 20}};
    for (int i = 0; i < expected.size(); ++i) {
        if (!closePoint(*loadedVanishing.sideHandles()[i], expected[i])) return 125;
    }

    VanishingPointAssistant customVanishing;
    addNormalHandles(customVanishing, {{3, 4}});
    const PkList<PkPointF> customSides {{-11, 7}, {-23, 8}, {17, 9}, {29, 10}};
    for (int i = 0; i < customSides.size(); ++i) {
        *customVanishing.sideHandles()[i] = customSides[i];
    }
    HandleIdMap customIds;
    KoStore customStore(customVanishing.saveXml(customIds));
    IdHandleMap customLoadedHandles;
    VanishingPointAssistant customLoaded;
    customLoaded.loadXml(&customStore, customLoadedHandles, "custom-vanishing.assistant");
    if (customLoaded.sideHandles().size() != customSides.size()) return 129;
    for (int i = 0; i < customSides.size(); ++i) {
        if (!closePoint(*customLoaded.sideHandles()[i], customSides[i])) return 130;
    }
    return 0;
}

class ExposedPerspectiveEllipseAssistant : public PerspectiveEllipseAssistant
{
public:
    PkRect modelBoundingRect() const { return boundingRect(); }
};

int representativeGeometryRemainsLive()
{
    Ruler ruler;
    ruler.setPoint1({0.0, 0.0});
    ruler.setPoint2({10.0, 10.0});
    if (ruler.project({3.0, 4.0}) != PkPointF(3.5, 3.5)) return 20;

    Ellipse ellipse({-10.0, 0.0}, {10.0, 0.0}, {0.0, 5.0});
    const PkPointF ellipseProjection = ellipse.project({0.0, 8.0});
    if (!closeEnough(ellipseProjection.x(), 0.0) || !closeEnough(ellipseProjection.y(), 5.0)) return 21;

    const PkList<PkPointF> concentricHandles {{-10, 0}, {10, 0}, {0, 5}};
    PkPointF concentricEnd(0, 15);
    ConcentricEllipseAssistantGeometry::adjustLine(concentricHandles, concentricEnd, {0, 10});
    if (!closePoint(concentricEnd, {0, 10}, 1e-4)) return 22;
    ConcentricEllipseAssistant concentric;
    addNormalHandles(concentric, {{-10, 0}, {10, 0}, {0, 5}});
    if (!closePoint(concentric.adjustPosition({0, 15}, {0, 10}, true, 0.0), {0, 10}, 1e-4)) return 23;

    PkList<KisPaintingAssistantHandleSP> perspectiveHandles;
    for (const PkPointF &point : PkList<PkPointF>{{-4, 4}, {4, 4}, {8, 8}, {-8, 8}}) {
        perspectiveHandles << KisPaintingAssistantHandleSP(new KisPaintingAssistantHandle(point));
    }
    PkPolygonF polygon;
    if (!PerspectiveBasedAssistantHelper::getTetragon(perspectiveHandles, true, polygon)) return 23;
    PerspectiveBasedAssistantHelper::CacheData cache;
    PerspectiveBasedAssistantHelper::updateCacheData(cache, polygon);
    if (!closeEnough(PerspectiveBasedAssistantHelper::distanceInGrid(cache, {0, 3}), 3.0 / 8.0)) return 24;

    RulerAssistant rulerAssistant;
    addNormalHandles(rulerAssistant, {{0, 0}, {10, 0}});
    if (rulerAssistant.adjustPosition({3, 4}, {0, 0}, true, 0.0) != PkPointF(3, 0)) return 25;

    EllipseAssistant ellipseAssistant;
    addNormalHandles(ellipseAssistant, {{-10, 0}, {10, 0}, {0, 5}});
    if (!closePoint(ellipseAssistant.adjustPosition({0, 8}, {0, 5}, true, 0.0), {0, 5})) return 26;

    SplineAssistant spline;
    addNormalHandles(spline, {{0, 0}, {10, 0}, {10.0 / 3.0, 0}, {20.0 / 3.0, 0}});
    const PkPointF splineProjection = spline.adjustPosition({5, 6}, {1, 1}, true, 0.0);
    if (!closePoint(splineProjection, {5, 0}, 1e-4)) return 27;

    VanishingPointAssistant vanishing;
    addNormalHandles(vanishing, {{0, 0}});
    if (!closePoint(vanishing.adjustPosition({5, 2}, {10, 0}, true, 0.0), {5, 0})) return 28;

    TwoPointAssistant twoPoint;
    addNormalHandles(twoPoint, {{-10, 0}, {10, 0}, {0, 5}});
    const PkPointF twoPointFirst = twoPoint.adjustPosition({8, 2}, {0, 5}, true, 0.0);
    if (!closePoint(twoPointFirst, {7.6, 1.2})) return 29;
    if (!closePoint(twoPoint.adjustPosition({1, 2}, {0, 5}, false, 0.0), {2, 4})) return 30;
    twoPoint.endStroke();
    if (!closePoint(twoPoint.adjustPosition({1, 2}, {0, 5}, false, 0.0), {0, 2})) return 31;

    FisheyePointAssistant fisheye;
    addNormalHandles(fisheye, {{-10, 0}, {10, 0}, {0, 5}});
    if (!closePoint(fisheye.adjustPosition({0, 8}, {0, 5}, true, 0.0), {0, 5})) return 32;

    CurvilinearPerspectiveAssistant curvilinear;
    addNormalHandles(curvilinear, {{-10, 0}, {10, 0}});
    if (!closePoint(curvilinear.adjustPosition({25, -7.5}, {0, 5}, true, 0.0),
                    {12.5, -7.5})) return 33;

    PerspectiveAssistant perspective;
    addNormalHandles(perspective, {{-10, -10}, {10, -10}, {10, 10}, {-10, 10}});
    if (!closePoint(perspective.adjustPosition({3, 4}, {0, 0}, true, 0.0), {0, 4})) return 34;
    perspective.endStroke();
    *perspective.handles()[0] = PkPointF(0, -10);
    *perspective.handles()[1] = PkPointF(20, -10);
    *perspective.handles()[2] = PkPointF(20, 10);
    *perspective.handles()[3] = PkPointF(0, 10);
    if (!closePoint(perspective.adjustPosition({13, 4}, {10, 0}, true, 0.0), {10, 4})) return 35;

    ExposedPerspectiveEllipseAssistant perspectiveEllipse;
    addNormalHandles(perspectiveEllipse, {{-10, -10}, {10, -10}, {10, 10}, {-10, 10}});
    if (!closePoint(perspectiveEllipse.adjustPosition({0, 15}, {0, 0}, true, 0.0), {0, 10})) return 36;
    if (perspectiveEllipse.modelBoundingRect().isEmpty() ||
        !closeEnough(perspectiveEllipse.distance({0, 3}), 1.0)) return 37;
    *perspectiveEllipse.handles()[1] = PkPointF(20, -10);
    *perspectiveEllipse.handles()[2] = PkPointF(20, 10);
    if (!closePoint(perspectiveEllipse.adjustPosition({5, 15}, {5, 0}, true, 0.0), {5, 10})) return 38;

    ExposedPerspectiveEllipseAssistant trapezoidEllipse;
    addNormalHandles(trapezoidEllipse, {{-4, 4}, {4, 4}, {8, 8}, {-8, 8}});
    if (!closeEnough(trapezoidEllipse.distance({0, 3}), 3.0 / 8.0)) return 45;

    ExposedPerspectiveEllipseAssistant invalidEllipse;
    addNormalHandles(invalidEllipse, {{0, 0}, {1, 0}, {2, 0}, {3, 0}});
    const PkPointF invalidInput(9, 7);
    if (!invalidEllipse.modelBoundingRect().isEmpty() ||
        invalidEllipse.adjustPosition(invalidInput, {0, 0}, true, 0.0) != invalidInput ||
        !closeEnough(invalidEllipse.distance({0, 3}), 1.0)) return 46;

    InfiniteRulerAssistant infinite;
    addNormalHandles(infinite, {{0, 0}, {10, 0}});
    if (!closePoint(infinite.adjustPosition({3, 4}, {0, 0}, true, 0.0), {3, 0})) return 39;
    if (!closePoint(infinite.adjustPosition({1, 1}, {0, 0}, true, 2.0), {0, 0})) return 40;

    ParallelRulerAssistant parallel;
    parallel.setLocal(true);
    addNormalHandles(parallel, {{0, 0}, {10, 0}, {0, -2}, {10, 2}});
    if (finitePoint(parallel.adjustPosition({3, 4}, {0, 1}, true, 0.0))) return 41;
    if (!closePoint(parallel.adjustPosition({3, 1}, {0, 1}, true, 0.0), {3, 1})) return 42;
    if (!closePoint(parallel.adjustPosition({3, 4}, {0, 1}, true, 0.0), {3, 1})) return 43;
    parallel.endStroke();
    if (finitePoint(parallel.adjustPosition({3, 4}, {0, 1}, true, 0.0))) return 44;
    return 0;
}

int xmlPersistencePreservesNamesDefaultsAndFailures()
{
    RulerAssistant ruler;
    addNormalHandles(ruler, {{1.25, 2.5}, {3.75, 4.5}});
    ruler.setSnappingActive(false);
    ruler.setLocked(true);
    ruler.setUseCustomColor(true);
    ruler.setAssistantCustomColor(PkColor(12, 34, 56, 78));
    ruler.setSubdivisions(7);
    ruler.setMinorSubdivisions(3);
    ruler.enableFixedLength(true);
    ruler.setFixedLength(42.5);
    ruler.setFixedLengthUnit("cm");

    HandleIdMap handleIds;
    const PkByteArray bytes = ruler.saveXml(handleIds);
    if (bytes.isEmpty()) return 40;
    const PkString text = PkString::PkFromUtf8(bytes.constData(), bytes.size());
    PkXmlStreamReader reader(text);
    bool sawAssistant = false;
    int handleCount = 0;
    while (!reader.atEnd()) {
        if (reader.readNext() != PkXmlStreamReader::StartElement) continue;
        if (reader.name() == "assistant") {
            sawAssistant = true;
            if (reader.attributes().value("type") != "ruler" ||
                reader.attributes().value("active") != "0" ||
                reader.attributes().value("locked") != "1") return 41;
        } else if (reader.name() == "handle") {
            ++handleCount;
        }
    }
    if (reader.hasError() || !sawAssistant || handleCount != 2) return 42;

    KoStore store(bytes);
    IdHandleMap loadedHandles;
    RulerAssistant loaded;
    loaded.loadXml(&store, loadedHandles, "ruler.assistant");
    if (loaded.handles().size() != 2 || loaded.isSnappingActive() || !loaded.isLocked()) return 43;
    if (!loaded.useCustomColor() || loaded.assistantCustomColor() != PkColor(12, 34, 56, 78)) return 44;
    if (loaded.subdivisions() != 7 || loaded.minorSubdivisions() != 3 ||
        !loaded.hasFixedLength() || !closeEnough(loaded.fixedLength(), 42.5) ||
        loaded.fixedLengthUnit() != "cm") return 45;

    const char malformedText[] = "<assistant><handle id=\"0\" x=\"1\"";
    KoStore malformed(PkByteArray(malformedText, static_cast<int>(sizeof(malformedText) - 1)));
    IdHandleMap malformedHandles;
    RulerAssistant rejected;
    rejected.loadXml(&malformed, malformedHandles, "broken.assistant");
    if (!rejected.handles().isEmpty()) return 46;

    PkXmlDocument document;
    PkXmlElement root = document.createElement("assistants");
    document.appendChild(root);
    ruler.saveXmlList(document, root, 5);
    const PkXmlElement element = root.firstChildElement("assistant");
    if (element.isNull() || element.attribute("type") != "ruler" ||
        element.attribute("filename") != "ruler5.assistant") return 47;
    return 0;
}
} // namespace

int main()
{
    if (const int result = registryPreservesAllIdsAndIsIdempotent()) return result;
    if (const int result = coreStateCloneHandlesAndCollectionRemainLive()) return result;
    if (const int result = sharedHandleMergeRewiresEveryAssistant()) return result;
    if (const int result = renderIndependentHandleLifecycleRemainsLive()) return result;
    if (const int result = representativeGeometryRemainsLive()) return result;
    return xmlPersistencePreservesNamesDefaultsAndFailures();
}
