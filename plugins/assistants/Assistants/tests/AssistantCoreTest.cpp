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

template<typename Assistant>
void addNormalHandles(Assistant &assistant, std::initializer_list<PkPointF> points)
{
    for (const PkPointF &point : points) {
        assistant.addHandle(KisPaintingAssistantHandleSP(new KisPaintingAssistantHandle(point)), HandleType::NORMAL);
    }
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

int representativeGeometryRemainsLive()
{
    Ruler ruler;
    ruler.setPoint1({0.0, 0.0});
    ruler.setPoint2({10.0, 10.0});
    if (ruler.project({3.0, 4.0}) != PkPointF(3.5, 3.5)) return 20;

    Ellipse ellipse({-10.0, 0.0}, {10.0, 0.0}, {0.0, 5.0});
    const PkPointF ellipseProjection = ellipse.project({0.0, 8.0});
    if (!closeEnough(ellipseProjection.x(), 0.0) || !closeEnough(ellipseProjection.y(), 5.0)) return 21;

    const PkList<PkPointF> concentricHandles {{0, 100}, {100, 0}, {200, 200}};
    PkPointF concentricEnd(100, 5);
    ConcentricEllipseAssistantGeometry::adjustLine(concentricHandles, concentricEnd, {0, 100});
    if (!finitePoint(concentricEnd)) return 22;

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
    if (!finitePoint(ellipseAssistant.adjustPosition({0, 8}, {0, 5}, true, 0.0))) return 26;

    SplineAssistant spline;
    addNormalHandles(spline, {{0, 0}, {10, 0}, {0, 10}, {10, 10}});
    if (!finitePoint(spline.adjustPosition({5, 6}, {0, 0}, true, 0.0))) return 27;

    VanishingPointAssistant vanishing;
    addNormalHandles(vanishing, {{0, 0}});
    if (!finitePoint(vanishing.adjustPosition({5, 2}, {10, 0}, true, 0.0))) return 28;

    TwoPointAssistant twoPoint;
    addNormalHandles(twoPoint, {{-10, 0}, {10, 0}, {0, 5}});
    if (!finitePoint(twoPoint.adjustPosition({1, 2}, {0, 5}, true, 0.0))) return 29;

    FisheyePointAssistant fisheye;
    addNormalHandles(fisheye, {{-10, 0}, {10, 0}, {0, 5}});
    if (!finitePoint(fisheye.adjustPosition({1, 2}, {0, 5}, true, 0.0))) return 30;

    CurvilinearPerspectiveAssistant curvilinear;
    addNormalHandles(curvilinear, {{-10, 0}, {10, 0}});
    if (!finitePoint(curvilinear.adjustPosition({1, 2}, {0, 5}, true, 0.0))) return 31;

    PerspectiveAssistant perspective;
    addNormalHandles(perspective, {{-4, 4}, {4, 4}, {8, 8}, {-8, 8}});
    if (!finitePoint(perspective.adjustPosition({0, 6}, {0, 4}, true, 0.0))) return 32;

    PerspectiveEllipseAssistant perspectiveEllipse;
    addNormalHandles(perspectiveEllipse, {{-4, 4}, {4, 4}, {8, 8}, {-8, 8}});
    if (!finitePoint(perspectiveEllipse.adjustPosition({0, 6}, {0, 4}, true, 0.0))) return 33;

    InfiniteRulerAssistant infinite;
    addNormalHandles(infinite, {{0, 0}, {10, 0}});
    if (!finitePoint(infinite.adjustPosition({3, 4}, {0, 0}, true, 0.0))) return 34;

    ParallelRulerAssistant parallel;
    addNormalHandles(parallel, {{0, 0}, {10, 0}});
    if (!finitePoint(parallel.adjustPosition({3, 4}, {0, 1}, true, 0.0))) return 35;
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
    if (const int result = representativeGeometryRemainsLive()) return result;
    return xmlPersistencePreservesNamesDefaultsAndFailures();
}
