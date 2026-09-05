/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2002-2005, 2007 Rob Buis <buis@kde.org>
 * SPDX-FileCopyrightText: 2002-2004 Nicolas Goutte <nicolasg@snafu.de>
 * SPDX-FileCopyrightText: 2005-2006 Tim Beaulen <tbscope@gmail.com>
 * SPDX-FileCopyrightText: 2005-2009 Jan Hambrecht <jaham@gmx.net>
 * SPDX-FileCopyrightText: 2005, 2007 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2006-2007 Inge Wallin <inge@lysator.liu.se>
 * SPDX-FileCopyrightText: 2007-2008, 2010 Thorsten Zachmann <zachmann@kde.org>

 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include "SvgParser.h"

#include <cmath>

#include <FlakeDebug.h>

#include <PkColor.h>
#include <QDir>
#include <QPainter>
#include <PkPainterPath.h>
#include <QRandomGenerator>

#include <KoShape.h>
#include <KoShapeRegistry.h>
#include <KoShapeFactoryBase.h>
#include <KoShapeGroup.h>
#include <KoPathShape.h>
#include <KoDocumentResourceManager.h>
#include <KoPathShapeLoader.h>
#include <commands/KoShapeGroupCommand.h>
#include <commands/KoShapeUngroupCommand.h>
#include <KoColorBackground.h>
#include <KoGradientBackground.h>
#include <KoMeshGradientBackground.h>
#include <KoPatternBackground.h>
#include <KoClipPath.h>
#include <KoClipMask.h>
#include <KoXmlNS.h>

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include <QXmlSimpleReader>
#include <QXmlInputSource>
#endif

#include "SvgMeshGradient.h"
#include "SvgMeshPatch.h"
#include "SvgUtil.h"
#include "SvgShape.h"
#include "SvgGraphicContext.h"
#include "SvgGradientHelper.h"
#include "SvgClipPathHelper.h"
#include "parsers/SvgTransformParser.h"
#include "kis_pointer_utils.h"
#include <KoVectorPatternBackground.h>
#include <KoMarker.h>

#include <text/KoSvgTextShape.h>
#include <text/KoSvgTextLoader.h>

#include "kis_dom_utils.h"

#include "kis_algebra_2d.h"
#include "kis_debug.h"
#include "kis_global.h"
#include <QXmlStreamReader>
#include <algorithm>

#include <PkGradient.h>
#include <KisPortingUtils.h>
#include <klocalizedstring.h>

// ---- Pk<->Qt 边界小工具（本文件内部用，不跨 TU） ----
static inline PkStringList toStringList(const PkStringList &list)
{
    PkStringList out;
    for (const PkString &s : list) out.append(toQString(s));
    return out;
}

static inline PkGradientStops toPkGradientStops(const PkGradientStops &stops)
{
    PkGradientStops out;
    for (const PkGradientStop &s : stops) {
        out.append(PkGradientStop{s.first, toPkColor(s.second)});
    }
    return out;
}

static inline PkGradientStops toQGradientStops(const PkGradientStops &stops)
{
    PkGradientStops out;
    for (const PkGradientStop &s : stops) {
        out.append(PkGradientStop(s.offset, toQColor(s.color)));
    }
    return out;
}

static inline PkGradient toPkGradient(const PkGradient &g)
{
    PkGradient out;
    switch (g.type()) {
    case PkGradient::LinearGradient: {
        const QLinearGradient &lg = static_cast<const QLinearGradient&>(g);
        out = PkGradient::linear(toPkPointF(lg.start()), toPkPointF(lg.finalStop()));
        break;
    }
    case PkGradient::RadialGradient: {
        const QRadialGradient &rg = static_cast<const QRadialGradient&>(g);
        out = PkGradient::radial(toPkPointF(rg.center()), rg.radius(), toPkPointF(rg.focalPoint()));
        break;
    }
    case PkGradient::ConicalGradient: {
        const QConicalGradient &cg = static_cast<const QConicalGradient&>(g);
        out = PkGradient::conical(toPkPointF(cg.center()), cg.angle());
        break;
    }
    default:
        break;
    }
    out.setSpread(static_cast<PkGradientEnums::Spread>(g.spread()));
    out.setCoordinateMode(static_cast<PkGradientEnums::CoordinateMode>(g.coordinateMode()));
    out.setStops(toPkGradientStops(g.stops()));
    return out;
}

struct SvgParser::DeferredUseStore {
    struct El {
        El(const PkXmlElement* ue, const PkString& key) :
            m_useElement(ue), m_key(key) {
        }
        const PkXmlElement* m_useElement;
        PkString m_key;
    };
    DeferredUseStore(SvgParser* p) :
        m_parse(p) {
    }

    void add(const PkXmlElement* useE, const PkString& key) {
        m_uses.push_back(El(useE, key));
    }
    bool empty() const {
        return m_uses.empty();
    }

    void checkPendingUse(const PkXmlElement &b, PkList<KoShape*>& shapes) {
        KoShape* shape = 0;
        const PkString id = b.attribute("id");

        if (id.isEmpty())
            return;

        // debugFlake << "Checking id: " << id;
        auto i = std::partition(m_uses.begin(), m_uses.end(),
                                [&](const El& e) -> bool {return e.m_key != id;});

        while (i != m_uses.end()) {
            const El& el = m_uses.back();
            if (m_parse->m_context.hasDefinition(toPkString(el.m_key))) {
                // debugFlake << "Found pending use for id: " << el.m_key;
                shape = m_parse->resolveUse(*(el.m_useElement), el.m_key);
                if (shape) {
                    shapes.append(shape);
                }
            }
            m_uses.pop_back();
        }
    }

    ~DeferredUseStore() {
        while (!m_uses.empty()) {
            const El& el = m_uses.back();
            debugFlake << "WARNING: could not find path in <use xlink:href=\"#xxxxx\" expression. Losing data here. Key:"
                       << el.m_key;
            m_uses.pop_back();
        }
    }
    SvgParser* m_parse;
    std::vector<El> m_uses;
};


SvgParser::SvgParser(KoDocumentResourceManager *documentResourceManager)
    : m_context(documentResourceManager)
    , m_documentResourceManager(documentResourceManager)
{
}

SvgParser::~SvgParser()
{
    for (auto it = m_symbols.begin(); it != m_symbols.end(); ++it) {
        delete it.value();
    }
    qDeleteAll(m_defsShapes);
}

/*
 * Qt 5.15 deprecated this way of setting the document content, however,
 * they forgot to address reading text nodes with only white spaces, which
 * results in bugs for SVG text parsing.
 *
 * See bug 513085
 *
 */
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
PkXmlDocument createDocumentFromXmlInputSource(QXmlInputSource *source, PkString *errorMsg, int *errorLine, int *errorColumn) {
    PkXmlDocument doc;
    QXmlSimpleReader simpleReader;
    simpleReader.setFeature("http://qt-project.org/xml/features/report-whitespace-only-CharData", true);
    simpleReader.setFeature("http://xml.org/sax/features/namespaces", false);
    simpleReader.setFeature("http://xml.org/sax/features/namespace-prefixes", true);
    if (!doc.setContent(source, &simpleReader, errorMsg, errorLine, errorColumn)) {
        return {};
    }
    return doc;
}
#endif

PkXmlDocument SvgParser::createDocumentFromSvg(PkStream *device, PkString *errorMsg, int *errorLine, int *errorColumn)
{
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QXmlInputSource source(device);
    return createDocumentFromXmlInputSource(&source, errorMsg, errorLine, errorColumn);
#else
    return createDocumentFromSvg(QXmlStreamReader(device), errorMsg, errorLine, errorColumn);
#endif
}

PkXmlDocument SvgParser::createDocumentFromSvg(const PkByteArray &data, PkString *errorMsg, int *errorLine, int *errorColumn)
{
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QXmlInputSource source;
    source.setData(data);
    return createDocumentFromXmlInputSource(&source, errorMsg, errorLine, errorColumn);
#else
    return createDocumentFromSvg(QXmlStreamReader(data), errorMsg, errorLine, errorColumn);
#endif
}

PkXmlDocument SvgParser::createDocumentFromSvg(const PkString &data, PkString *errorMsg, int *errorLine, int *errorColumn)
{
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QXmlInputSource source;
    source.setData(data);
    return createDocumentFromXmlInputSource(&source, errorMsg, errorLine, errorColumn);
#else
    return createDocumentFromSvg(QXmlStreamReader(data), errorMsg, errorLine, errorColumn);
#endif
}

PkXmlDocument SvgParser::createDocumentFromSvg(QXmlStreamReader reader, PkString *errorMsg, int *errorLine, int *errorColumn)
{
    PkXmlDocument doc;


    reader.setNamespaceProcessing(false);
#if (QT_VERSION < QT_VERSION_CHECK(6, 5, 0))
    if (!doc.setContent(&reader, false, errorMsg, errorLine, errorColumn)) {
        return {};
    }
#else
    PkXmlDocument::ParseResult result = doc.setContent(&reader, PkXmlDocument::ParseOption::PreserveSpacingOnlyNodes);
    if (!result) {
        if (errorMsg && errorLine && errorColumn) {
            *errorMsg = result.errorMessage;
            *errorLine = result.errorLine;
            *errorColumn = result.errorColumn;
        }
        return {};
    }
#endif
    return doc;
}

void SvgParser::setXmlBaseDir(const PkString &baseDir)
{
    m_context.setInitialXmlBaseDir(toPkString(baseDir));

    setFileFetcher(
        [this](const PkString &name) {
            PkStringList possibleNames;
            possibleNames << name;
            possibleNames << QDir::cleanPath(QDir(toQString(m_context.xmlBaseDir())).absoluteFilePath(toQString(name)));
            for (PkString fileName : possibleNames) {
                PkFileStream file(fileName);
                if (file.open(PkStream::ReadOnly)) {
                    return file.readAll();
                }
            }
            return PkByteArray();
        });
}

void SvgParser::setResolution(const PkRectF boundsInPixels, qreal pixelsPerInch)
{
    KIS_ASSERT(!m_context.currentGC());
    m_context.pushGraphicsContext();
    m_context.currentGC()->isResolutionFrame = true;
    m_context.currentGC()->pixelsPerInch = pixelsPerInch;

    const qreal scale = 72.0 / pixelsPerInch;
    const PkTransform t = PkTransform::fromScale(scale, scale);
    m_context.currentGC()->currentBoundingBox = toPkRectF(boundsInPixels);
    m_context.currentGC()->matrix = toPkTransform(toQTransform(t));
}

void SvgParser::setDefaultKraTextVersion(int version)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_context.currentGC());
    m_context.currentGC()->textProperties.setProperty(KoSvgTextProperties::KraTextVersionId, version);
}

void SvgParser::setFillStrokeInheritByDefault(const bool enable)
{
    m_inheritStrokeFillByDefault = enable;
}

void SvgParser::setResolveTextPropertiesForTopLevel(const bool enable)
{
    m_resolveTextPropertiesForTopLevel = enable;
}

PkList<KoShape*> SvgParser::shapes() const
{
    return m_shapes;
}

PkVector<KoSvgSymbol *> SvgParser::takeSymbols()
{
    PkVector<KoSvgSymbol*> symbols = m_symbols.values().toVector();
    m_symbols.clear();
    return symbols;
}

// Helper functions
// ---------------------------------------------------------------------------------------

SvgGradientHelper* SvgParser::findGradient(const PkString &id)
{
    SvgGradientHelper *result = 0;

    // check if gradient was already parsed, and return it
    if (m_gradients.contains(id)) {
        result = &m_gradients[ id ];
    }

    // check if gradient was stored for later parsing
    if (!result && m_context.hasDefinition(toPkString(id))) {
        const PkXmlElement e = toQDomElement(m_context.definition(toPkString(id)));
        if (e.tagName().contains("Gradient")) {
            result = parseGradient(e);
        } else if (e.tagName() == "meshgradient") {
            result = parseMeshGradient(e);
        }
    }

    return result;
}

PkSharedPointer<KoVectorPatternBackground> SvgParser::findPattern(const PkString &id, const KoShape *shape)
{
    PkSharedPointer<KoVectorPatternBackground> result;

    // check if gradient was stored for later parsing
    if (m_context.hasDefinition(toPkString(id))) {
        const PkXmlElement e = toQDomElement(m_context.definition(toPkString(id)));
        if (e.tagName() == "pattern") {
            result = parsePattern(e, shape);
        }
    }

    return result;
}

SvgClipPathHelper* SvgParser::findClipPath(const PkString &id)
{
    return m_clipPaths.contains(id) ? &m_clipPaths[id] : 0;
}

// Parsing functions
// ---------------------------------------------------------------------------------------

qreal SvgParser::parseUnit(const PkString &unit, bool horiz, bool vert, const PkRectF &bbox)
{
    return SvgUtil::parseUnit(m_context.currentGC(), m_context.resolvedProperties(), toPkString(unit), horiz, vert, toPkRectF(bbox));
}

qreal SvgParser::parseUnitX(const PkString &unit)
{
    return SvgUtil::parseUnitX(m_context.currentGC(), m_context.resolvedProperties(), toPkString(unit));
}

qreal SvgParser::parseUnitY(const PkString &unit)
{
    return SvgUtil::parseUnitY(m_context.currentGC(), m_context.resolvedProperties(), toPkString(unit));
}

qreal SvgParser::parseUnitXY(const PkString &unit)
{
    return SvgUtil::parseUnitXY(m_context.currentGC(), m_context.resolvedProperties(), toPkString(unit));
}

qreal SvgParser::parseAngular(const PkString &unit)
{
    return SvgUtil::parseUnitAngular(m_context.currentGC(), toPkString(unit));
}


SvgGradientHelper* SvgParser::parseGradient(const PkXmlElement &e)
{
    // IMPROVEMENTS:
    // - Store the parsed colorstops in some sort of a cache so they don't need to be parsed again.
    // - A gradient inherits attributes it does not have from the referencing gradient.
    // - Gradients with no color stops have no fill or stroke.
    // - Gradients with one color stop have a solid color.

    SvgGraphicsContext *gc = m_context.currentGC();
    if (!gc) return 0;

    SvgGradientHelper gradHelper;

    PkString gradientId = e.attribute("id");
    if (gradientId.isEmpty()) return 0;

    // check if we have this gradient already parsed
    // copy existing gradient if it exists
    if (m_gradients.contains(gradientId)) {
        return &m_gradients[gradientId];
    }

    if (e.hasAttribute("xlink:href")) {
        // strip the '#' symbol
        PkString href = e.attribute("xlink:href").mid(1);

        if (!href.isEmpty()) {
            // copy the referenced gradient if found
            SvgGradientHelper *pGrad = findGradient(href);
            if (pGrad) {
                gradHelper = *pGrad;
            }
        }
    }

    const PkGradientStops defaultStops = gradHelper.gradient()->stops();

    if (e.attribute("gradientUnits") == "userSpaceOnUse") {
        gradHelper.setGradientUnits(KoFlake::UserSpaceOnUse);
    }

    m_context.pushGraphicsContext(toPkXmlElement(e));
    uploadStyleToContext(e);

    if (e.tagName() == "linearGradient") {
        QLinearGradient *g = new QLinearGradient();
        if (gradHelper.gradientUnits() == KoFlake::ObjectBoundingBox) {
            g->setCoordinateMode(PkGradientEnums::ObjectBoundingMode);
            g->setStart(PkPointF(SvgUtil::fromPercentage(toPkString(e.attribute("x1", "0%"))),
                                SvgUtil::fromPercentage(toPkString(e.attribute("y1", "0%")))));
            g->setFinalStop(PkPointF(SvgUtil::fromPercentage(toPkString(e.attribute("x2", "100%"))),
                                    SvgUtil::fromPercentage(toPkString(e.attribute("y2", "0%")))));
        } else {
            g->setStart(PkPointF(parseUnitX(e.attribute("x1")),
                                parseUnitY(e.attribute("y1"))));
            g->setFinalStop(PkPointF(parseUnitX(e.attribute("x2")),
                                    parseUnitY(e.attribute("y2"))));
        }
        gradHelper.setGradient(g);

    } else if (e.tagName() == "radialGradient") {
        QRadialGradient *g = new QRadialGradient();
        if (gradHelper.gradientUnits() == KoFlake::ObjectBoundingBox) {
            g->setCoordinateMode(PkGradientEnums::ObjectBoundingMode);
            g->setCenter(PkPointF(SvgUtil::fromPercentage(toPkString(e.attribute("cx", "50%"))),
                                 SvgUtil::fromPercentage(toPkString(e.attribute("cy", "50%")))));
            g->setRadius(SvgUtil::fromPercentage(toPkString(e.attribute("r", "50%"))));
            g->setFocalPoint(PkPointF(SvgUtil::fromPercentage(toPkString(e.attribute("fx", "50%"))),
                                     SvgUtil::fromPercentage(toPkString(e.attribute("fy", "50%")))));
        } else {
            g->setCenter(PkPointF(parseUnitX(e.attribute("cx")),
                                 parseUnitY(e.attribute("cy"))));
            g->setFocalPoint(PkPointF(parseUnitX(e.attribute("fx")),
                                     parseUnitY(e.attribute("fy"))));
            g->setRadius(parseUnitXY(e.attribute("r")));
        }
        gradHelper.setGradient(g);
    } else {
        debugFlake << "WARNING: Failed to parse gradient with tag" << e.tagName();
    }

    // handle spread method
    PkGradient::Spread spreadMethod = PkGradientEnums::PadSpread;
    PkString spreadMethodStr = e.attribute("spreadMethod");
    if (!spreadMethodStr.isEmpty()) {
        if (spreadMethodStr == "reflect") {
            spreadMethod = PkGradientEnums::ReflectSpread;
        } else if (spreadMethodStr == "repeat") {
            spreadMethod = PkGradientEnums::RepeatSpread;
        }
    }

    gradHelper.setSpreadMode(spreadMethod);

    // Parse the color stops.
    {
        PkGradient pkGradient = toPkGradient(*gradHelper.gradient());
        m_context.styleParser().parseColorStops(&pkGradient, toPkXmlElement(e), gc, toPkGradientStops(defaultStops));
        gradHelper.gradient()->setStops(toQGradientStops(pkGradient.stops()));
    }

    if (e.hasAttribute("gradientTransform")) {
        SvgTransformParser p(toPkString(e.attribute("gradientTransform")));
        if (p.isValid()) {
            gradHelper.setTransform(toQTransform(p.transform()));
        }
    }

    m_context.popGraphicsContext();

    m_gradients.insert(gradientId, gradHelper);

    return &m_gradients[gradientId];
}

SvgGradientHelper* SvgParser::parseMeshGradient(const PkXmlElement &e)
{
    SvgGradientHelper gradHelper;
    PkString gradientId = e.attribute("id");
    PkScopedPointer<SvgMeshGradient> g(new SvgMeshGradient);

    // check if we have this gradient already parsed
    // copy existing gradient if it exists
    if (m_gradients.contains(gradientId)) {
        return &m_gradients[gradientId];
    }

    if (e.hasAttribute("xlink:href")) {
        // strip the '#' symbol
        PkString href = e.attribute("xlink:href").mid(1);

        if (!href.isEmpty()) {
            // copy the referenced gradient if found
            SvgGradientHelper *pGrad = findGradient(href);
            if (pGrad) {
                gradHelper = *pGrad;
            }
        }
    }

    if (e.attribute("gradientUnits") == "userSpaceOnUse") {
        gradHelper.setGradientUnits(KoFlake::UserSpaceOnUse);
    }

    if (e.hasAttribute("transform")) {
        SvgTransformParser p(toPkString(e.attribute("transform")));
        if (p.isValid()) {
            gradHelper.setTransform(toQTransform(p.transform()));
        }
    }

    PkString type = e.attribute("type");
    g->setType(SvgMeshGradient::BILINEAR);
    if (!type.isEmpty() && type == "bicubic") {
        g->setType(SvgMeshGradient::BICUBIC);
    }

    int irow = 0, icols;
    for (int i = 0; i < e.childNodes().size(); ++i) {
        PkXmlNode node = e.childNodes().at(i);

        if (node.nodeName() == "meshrow") {

            SvgMeshStop startingNode;
            if (irow == 0) {
                startingNode.point = PkPointF(
                            parseUnitX(e.attribute("x")),
                            parseUnitY(e.attribute(("y"))));
                startingNode.color = PkColor();
            }

            icols = 0;
            g->getMeshArray()->newRow();
            for (int j = 0; j < node.childNodes().size() ; ++j) {
                PkXmlNode meshpatchNode = node.childNodes().at(j);

                if (meshpatchNode.nodeName() == "meshpatch") {
                    if (irow > 0) {
                        // Starting point for this would be the bottom (right) corner of the above patch
                        startingNode = g->getMeshArray()->getStop(SvgMeshPatch::Bottom, irow - 1, icols);
                    } else if (icols != 0) {
                        // Starting point for this would be the right (top) corner of the previous patch
                        startingNode = g->getMeshArray()->getStop(SvgMeshPatch::Right, irow, icols - 1);
                    }

                    PkList<std::pair<PkString, PkColor>> rawStops = parseMeshPatch(meshpatchNode);
                    // TODO handle the false result
                    PkList<PkPair<PkString, PkColor>> pkRawStops;
                    for (const auto &rs : rawStops) {
                        pkRawStops.append(PkPair<PkString, PkColor>(toPkString(rs.first), toPkColor(rs.second)));
                    }
                    if (!g->getMeshArray()->addPatch(pkRawStops, startingNode.point)) {
                        debugFlake << "WARNING: Failed to create meshpatch";
                    }
                    icols++;
                }
            }
            irow++;
        }
    }
    gradHelper.setMeshGradient(g.data());
    m_gradients.insert(gradientId, gradHelper);

    return &m_gradients[gradientId];
}

#define forEachElement( elem, parent ) \
    for ( PkXmlNode _node = parent.firstChild(); !_node.isNull(); _node = _node.nextSibling() ) \
    if ( ( elem = _node.toElement() ).isNull() ) {} else

PkList<std::pair<PkString, PkColor>> SvgParser::parseMeshPatch(const PkXmlNode& meshpatchNode)
{
    // path and its associated color
    PkList<std::pair<PkString, PkColor>> rawstops;

    SvgGraphicsContext *gc = m_context.currentGC();
    if (!gc) return rawstops;

    PkXmlElement e = meshpatchNode.toElement();

    PkXmlElement stop;

    forEachElement(stop, e) {
        qreal X = 0;    // dummy value, don't care, just to ensure the function won't blow up (also to avoid a Coverity issue)
        PkColor color = toQColor(m_context.styleParser().parseColorStop(toPkXmlElement(stop), gc, X).second);

        PkString pathStr = stop.attribute("path");

        rawstops.append({pathStr, color});
    }

    return rawstops;
}

inline PkPointF bakeShapeOffset(const PkTransform &patternTransform, const PkPointF &shapeOffset)
{
    PkTransform result =
            patternTransform *
            PkTransform::fromTranslate(-shapeOffset.x(), -shapeOffset.y()) *
            patternTransform.inverted();
    KIS_ASSERT_RECOVER_NOOP(result.type() <= PkTransform::TxTranslate);

    return PkPointF(result.dx(), result.dy());
}

PkSharedPointer<KoVectorPatternBackground> SvgParser::parsePattern(const PkXmlElement &e, const KoShape *shape)
{
    /**
     * Unlike the gradient parsing function, this method is called every time we
     * *reference* the pattern, not when we define it. Therefore we can already
     * use the coordinate system of the destination.
     */

    PkSharedPointer<KoVectorPatternBackground> pattHelper;

    SvgGraphicsContext *gc = m_context.currentGC();
    if (!gc) return pattHelper;

    const PkString patternId = e.attribute("id");
    if (patternId.isEmpty()) return pattHelper;

    pattHelper = PkSharedPointer<KoVectorPatternBackground>(new KoVectorPatternBackground);

    if (e.hasAttribute("xlink:href")) {
        // strip the '#' symbol
        PkString href = e.attribute("xlink:href").mid(1);

        if (!href.isEmpty() &&href != patternId) {
            // copy the referenced pattern if found
            PkSharedPointer<KoVectorPatternBackground> pPatt = findPattern(href, shape);
            if (pPatt) {
                pattHelper = pPatt;
            }
        }
    }

    pattHelper->setReferenceCoordinates(
                KoFlake::coordinatesFromString(e.attribute("patternUnits"),
                                               pattHelper->referenceCoordinates()));

    pattHelper->setContentCoordinates(
                KoFlake::coordinatesFromString(e.attribute("patternContentUnits"),
                                               pattHelper->contentCoordinates()));

    if (e.hasAttribute("patternTransform")) {
        SvgTransformParser p(toPkString(e.attribute("patternTransform")));
        if (p.isValid()) {
            pattHelper->setPatternTransform(toQTransform(p.transform()));
        }
    }

    if (pattHelper->referenceCoordinates() == KoFlake::ObjectBoundingBox) {
        PkRectF referenceRect(
            SvgUtil::fromPercentage(toPkString(e.attribute("x", "0%"))),
            SvgUtil::fromPercentage(toPkString(e.attribute("y", "0%"))),
            SvgUtil::fromPercentage(toPkString(e.attribute("width", "0%"))), // 0% is according to SVG 1.1, don't ask me why!
            SvgUtil::fromPercentage(toPkString(e.attribute("height", "0%")))); // 0% is according to SVG 1.1, don't ask me why!

        pattHelper->setReferenceRect(referenceRect);
    } else {
        PkRectF referenceRect(
            parseUnitX(e.attribute("x", "0")),
            parseUnitY(e.attribute("y", "0")),
            parseUnitX(e.attribute("width", "0")), // 0 is according to SVG 1.1, don't ask me why!
            parseUnitY(e.attribute("height", "0"))); // 0 is according to SVG 1.1, don't ask me why!

        pattHelper->setReferenceRect(referenceRect);
    }

    /**
     * In Krita shapes X,Y coordinates are baked into the shape global transform, but
     * the pattern should be painted in "user" coordinates. Therefore, we should handle
     * this offset separately.
     *
     * TODO: Please also note that this offset is different from extraShapeOffset(),
     * because A.inverted() * B != A * B.inverted(). I'm not sure which variant is
     * correct (DK)
     */

   const PkTransform dstShapeTransform = shape->absoluteTransformation();
   const PkTransform shapeOffsetTransform = dstShapeTransform * toQTransform(gc->matrix).inverted();
   KIS_SAFE_ASSERT_RECOVER_NOOP(shapeOffsetTransform.type() <= PkTransform::TxTranslate);
   const PkPointF extraShapeOffset(shapeOffsetTransform.dx(), shapeOffsetTransform.dy());

   m_context.pushGraphicsContext(toPkXmlElement(e));
   gc = m_context.currentGC();
   gc->workaroundClearInheritedFillProperties(); // HACK!

   // start building shape tree from scratch
   gc->matrix = toPkTransform(toQTransform(PkTransform()));

   const PkRectF boundingRect = shape->outline().boundingRect()/*.translated(extraShapeOffset)*/;
   const PkTransform relativeToShape(boundingRect.width(), 0, 0, boundingRect.height(),
                                    boundingRect.x(), boundingRect.y());



   // WARNING1: OBB and ViewBox transformations are *baked* into the pattern shapes!
   //          although we expect the pattern be reusable, but it is not so!
   // WARNING2: the pattern shapes are stored in *User* coordinate system, although
   //           the "official" content system might be either OBB or User. It means that
   //           this baked transform should be stripped before writing the shapes back
   //           into SVG
   if (e.hasAttribute("viewBox")) {
        gc->currentBoundingBox = toPkRectF(
            pattHelper->referenceCoordinates() == KoFlake::ObjectBoundingBox ?
            relativeToShape.mapRect(pattHelper->referenceRect()) :
            pattHelper->referenceRect());

        applyViewBoxTransform(e);
        pattHelper->setContentCoordinates(pattHelper->referenceCoordinates());

    } else if (pattHelper->contentCoordinates() == KoFlake::ObjectBoundingBox) {
        gc->matrix = toPkTransform(toQTransform(relativeToShape)) * gc->matrix;
    }

    // We do *not* apply patternTransform here! Here we only bake the untransformed
    // version of the shape. The transformed one will be done in the very end while rendering.

    PkList<KoShape*> patternShapes = parseContainer(e);

    if (pattHelper->contentCoordinates() == KoFlake::UserSpaceOnUse) {
        // In Krita we normalize the shapes, bake this transform into the pattern shapes

        const PkPointF offset = bakeShapeOffset(pattHelper->patternTransform(), extraShapeOffset);

        Q_FOREACH (KoShape *shape, patternShapes) {
            shape->applyAbsoluteTransformation(PkTransform::fromTranslate(offset.x(), offset.y()));
        }
    }

    if (pattHelper->referenceCoordinates() == KoFlake::UserSpaceOnUse) {
        // In Krita we normalize the shapes, bake this transform into reference rect
        // NOTE: this is possible *only* when pattern transform is not perspective
        //       (which is always true for SVG)

        const PkPointF offset = bakeShapeOffset(pattHelper->patternTransform(), extraShapeOffset);

        PkRectF ref = pattHelper->referenceRect();
        ref.translate(offset);
        pattHelper->setReferenceRect(ref);
    }

    m_context.popGraphicsContext();
    gc = m_context.currentGC();

    if (!patternShapes.isEmpty()) {
        pattHelper->setShapes(patternShapes);
    }

    return pattHelper;
}

bool SvgParser::parseMarker(const PkXmlElement &e)
{
    const PkString id = e.attribute("id");
    if (id.isEmpty()) return false;

    std::unique_ptr<KoMarker> marker(new KoMarker());
    marker->setCoordinateSystem(
        KoMarker::coordinateSystemFromString(e.attribute("markerUnits", "strokeWidth")));

    marker->setReferencePoint(PkPointF(parseUnitX(e.attribute("refX")),
                                      parseUnitY(e.attribute("refY"))));

    marker->setReferenceSize(PkSizeF(parseUnitX(e.attribute("markerWidth", "3")),
                                     parseUnitY(e.attribute("markerHeight", "3"))));

    const PkString orientation = e.attribute("orient", "0");

    if (orientation == "auto") {
        marker->setAutoOrientation(true);
    } else {
        marker->setExplicitOrientation(parseAngular(orientation));
    }

    // ensure that the clip path is loaded in local coordinates system
    m_context.pushGraphicsContext(toPkXmlElement(e), false);
    m_context.currentGC()->matrix = toPkTransform(toQTransform(PkTransform()));
    m_context.currentGC()->currentBoundingBox = toPkRectF(PkRectF(PkPointF(0, 0), marker->referenceSize()));

    KoShape *markerShape = parseGroup(e);

    m_context.popGraphicsContext();

    if (!markerShape) return false;

    marker->setShapes({markerShape});

    m_markers.insert(id, QExplicitlySharedDataPointer<KoMarker>(marker.release()));

    return true;
}

bool SvgParser::parseSymbol(const PkXmlElement &e)
{
    const PkString id = e.attribute("id");

    if (id.isEmpty()) return false;

    std::unique_ptr<KoSvgSymbol> svgSymbol(new KoSvgSymbol());

    // ensure that the clip path is loaded in local coordinates system
    m_context.pushGraphicsContext(toPkXmlElement(e), false);
    m_context.currentGC()->matrix = toPkTransform(toQTransform(PkTransform()));
    m_context.currentGC()->currentBoundingBox = toPkRectF(PkRectF(0.0, 0.0, 1.0, 1.0));

    PkString title = e.firstChildElement("title").toElement().text();

    std::unique_ptr<KoShape> symbolShape(parseGroup(e));

    m_context.popGraphicsContext();

    if (!symbolShape) return false;

    svgSymbol->shape = symbolShape.release();
    svgSymbol->title = title;
    svgSymbol->id = id;
    if (title.isEmpty()) svgSymbol->title = id;

    if (svgSymbol->shape->boundingRect() == PkRectF(0.0, 0.0, 0.0, 0.0)) {
        debugFlake << "Symbol" << id << "seems to be empty, discarding";
        return false;
    }

    // TODO: out default set of symbols had duplicated ids! We should
    //       make sure they are unique!
    if (m_symbols.contains(id)) {
        delete m_symbols[id];
        m_symbols.remove(id);
    }

    m_symbols.insert(id, svgSymbol.release());

    return true;
}

void SvgParser::parseMetadataApplyToShape(const PkXmlElement &e, KoShape *shape)
{
    const PkString titleTag = "title";
    const PkString descriptionTag = "desc";
    PkXmlElement title = e.firstChildElement(titleTag);
    if (!title.isNull()) {
        PkXmlText text = getTheOnlyTextChild(title);
        if (!text.data().isEmpty()) {
            shape->setAdditionalAttribute(titleTag, text.data());
        }
    }
    PkXmlElement description = e.firstChildElement(descriptionTag);
    if (!description.isNull()) {
        PkXmlText text = getTheOnlyTextChild(description);
        if (!text.data().isEmpty()) {
            shape->setAdditionalAttribute(descriptionTag, text.data());
        }
    }
}

bool SvgParser::parseClipPath(const PkXmlElement &e)
{
    SvgClipPathHelper clipPath;

    const PkString id = e.attribute("id");
    if (id.isEmpty()) return false;

    clipPath.setClipPathUnits(
                KoFlake::coordinatesFromString(e.attribute("clipPathUnits"), KoFlake::UserSpaceOnUse));

    // ensure that the clip path is loaded in local coordinates system
    m_context.pushGraphicsContext(toPkXmlElement(e));
    m_context.currentGC()->matrix = toPkTransform(toQTransform(PkTransform()));
    m_context.currentGC()->workaroundClearInheritedFillProperties(); // HACK!

    KoShape *clipShape = parseGroup(e);

    m_context.popGraphicsContext();

    if (!clipShape) return false;

    clipPath.setShapes({clipShape});
    m_clipPaths.insert(id, clipPath);

    return true;
}

bool SvgParser::parseClipMask(const PkXmlElement &e)
{
    PkSharedPointer<KoClipMask> clipMask(new KoClipMask);

    const PkString id = e.attribute("id");
    if (id.isEmpty()) return false;

    clipMask->setCoordinates(KoFlake::coordinatesFromString(e.attribute("maskUnits"), KoFlake::ObjectBoundingBox));
    clipMask->setContentCoordinates(KoFlake::coordinatesFromString(e.attribute("maskContentUnits"), KoFlake::UserSpaceOnUse));

    PkRectF maskRect;

    if (clipMask->coordinates() == KoFlake::ObjectBoundingBox) {
        maskRect.setRect(
            SvgUtil::fromPercentage(toPkString(e.attribute("x", "-10%"))),
            SvgUtil::fromPercentage(toPkString(e.attribute("y", "-10%"))),
            SvgUtil::fromPercentage(toPkString(e.attribute("width", "120%"))),
            SvgUtil::fromPercentage(toPkString(e.attribute("height", "120%"))));
    } else {
        maskRect.setRect(
            parseUnitX(e.attribute("x", "-10%")), // yes, percents are insane in this case,
            parseUnitY(e.attribute("y", "-10%")), // but this is what SVG 1.1 tells us...
            parseUnitX(e.attribute("width", "120%")),
            parseUnitY(e.attribute("height", "120%")));
    }

    clipMask->setMaskRect(maskRect);


    // ensure that the clip mask is loaded in local coordinates system
    m_context.pushGraphicsContext(toPkXmlElement(e));
    m_context.currentGC()->matrix = toPkTransform(toQTransform(PkTransform()));
    m_context.currentGC()->workaroundClearInheritedFillProperties(); // HACK!

    KoShape *clipShape = parseGroup(e);

    m_context.popGraphicsContext();

    if (!clipShape) return false;
    clipMask->setShapes({clipShape});

    m_clipMasks.insert(id, clipMask);
    return true;
}

void SvgParser::uploadStyleToContext(const PkXmlElement &e)
{
    SvgStyles styles = m_context.styleParser().collectStyles(toPkXmlElement(e));
    m_context.styleParser().parseFont(styles);
    m_context.styleParser().parseStyle(styles, m_inheritStrokeFillByDefault);
}

void SvgParser::applyCurrentStyle(KoShape *shape, const PkPointF &shapeToOriginalUserCoordinates)
{
    if (!shape) return;

    applyCurrentBasicStyle(shape);

    if (KoPathShape *pathShape = dynamic_cast<KoPathShape*>(shape)) {
        applyMarkers(pathShape);
    }

    applyClipping(shape, shapeToOriginalUserCoordinates);
    applyMaskClipping(shape, shapeToOriginalUserCoordinates);

}

void SvgParser::applyCurrentBasicStyle(KoShape *shape)
{
    if (!shape) return;

    SvgGraphicsContext *gc = m_context.currentGC();
    KIS_ASSERT(gc);

    if (!dynamic_cast<KoShapeGroup*>(shape)) {
        applyFillStyle(shape);
        applyStrokeStyle(shape);
    }

    if (!gc->display || !gc->visible) {
        /**
         * WARNING: here is a small inconsistency with the standard:
         *          in the standard, 'display' is not inherited, but in
         *          flake it is!
         *
         * NOTE: though the standard says: "A value of 'display:none' indicates
         *       that the given element and ***its children*** shall not be
         *       rendered directly". Therefore, using setVisible(false) is fully
         *       legitimate here (DK 29.11.16).
         */
        shape->setVisible(false);
    }
    shape->setTransparency(1.0 - gc->opacity);

    applyPaintOrder(shape);
}


void SvgParser::applyStyle(KoShape *obj, const PkXmlElement &e, const PkPointF &shapeToOriginalUserCoordinates)
{
    applyStyle(obj, m_context.styleParser().collectStyles(toPkXmlElement(e)), shapeToOriginalUserCoordinates);
}

void SvgParser::applyStyle(KoShape *obj, const SvgStyles &styles, const PkPointF &shapeToOriginalUserCoordinates)
{
    SvgGraphicsContext *gc = m_context.currentGC();
    if (!gc)
        return;

    m_context.styleParser().parseStyle(styles, m_inheritStrokeFillByDefault);

    if (!obj)
        return;

    if (!dynamic_cast<KoShapeGroup*>(obj)) {
        applyFillStyle(obj);
        applyStrokeStyle(obj);
    }

    if (KoPathShape *pathShape = dynamic_cast<KoPathShape*>(obj)) {
        applyMarkers(pathShape);
    }

    applyClipping(obj, shapeToOriginalUserCoordinates);
    applyMaskClipping(obj, shapeToOriginalUserCoordinates);

    if (!gc->display || !gc->visible) {
        obj->setVisible(false);
    }
    obj->setTransparency(1.0 - gc->opacity);
    applyPaintOrder(obj);
}

PkGradient* prepareGradientForShape(const SvgGradientHelper *gradient,
                                   const KoShape *shape,
                                   const SvgGraphicsContext *gc,
                                   PkTransform *transform)
{
    PkGradient *resultGradient = 0;
    KIS_ASSERT(transform);

    if (gradient->gradientUnits() == KoFlake::ObjectBoundingBox) {
        resultGradient = KoFlake::cloneGradient(gradient->gradient());
        *transform = gradient->transform();
    } else {
        if (gradient->gradient()->type() == PkGradient::LinearGradient) {
            /**
             * Create a converted gradient that looks the same, but linked to the
             * bounding rect of the shape, so it would be transformed with the shape
             */

            const PkRectF boundingRect = shape->outline().boundingRect();
            const PkTransform relativeToShape(boundingRect.width(), 0, 0, boundingRect.height(),
                                             boundingRect.x(), boundingRect.y());

            const PkTransform relativeToUser =
                    relativeToShape * shape->transformation() * toQTransform(gc->matrix).inverted();

            const PkTransform userToRelative = relativeToUser.inverted();

            const QLinearGradient *o = static_cast<const QLinearGradient*>(gradient->gradient());
            QLinearGradient *g = new QLinearGradient();
            g->setStart(userToRelative.map(o->start()));
            g->setFinalStop(userToRelative.map(o->finalStop()));
            g->setCoordinateMode(PkGradientEnums::ObjectBoundingMode);
            g->setStops(o->stops());
            g->setSpread(o->spread());

            resultGradient = g;
            *transform = relativeToUser * gradient->transform() * userToRelative;

        } else if (gradient->gradient()->type() == PkGradient::RadialGradient) {
            // For radial and conical gradients such conversion is not possible

            resultGradient = KoFlake::cloneGradient(gradient->gradient());
            *transform = gradient->transform() * toQTransform(gc->matrix) * shape->transformation().inverted();

            const PkRectF outlineRect = shape->outlineRect();
            if (outlineRect.isEmpty()) return resultGradient;

            /**
             * If shape outline rect is valid, convert the gradient into OBB mode by
             * doing some magic conversions: we compensate non-uniform size of the shape
             * by applying an additional pre-transform
             */

            QRadialGradient *rgradient = static_cast<QRadialGradient*>(resultGradient);

            const qreal maxDimension = KisAlgebra2D::maxDimension(outlineRect);
            const PkRectF uniformSize(outlineRect.topLeft(), PkSizeF(maxDimension, maxDimension));

            const PkTransform uniformizeTransform =
                    PkTransform::fromTranslate(-outlineRect.x(), -outlineRect.y()) *
                    PkTransform::fromScale(maxDimension / shape->outlineRect().width(),
                                          maxDimension / shape->outlineRect().height()) *
                    PkTransform::fromTranslate(outlineRect.x(), outlineRect.y());

            const PkPointF centerLocal = transform->map(rgradient->center());
            const PkPointF focalLocal = transform->map(rgradient->focalPoint());

            const PkPointF centerOBB = toQPointF(KisAlgebra2D::absoluteToRelative(toPkPointF(centerLocal), toPkRectF(uniformSize)));
            const PkPointF focalOBB = toQPointF(KisAlgebra2D::absoluteToRelative(toPkPointF(focalLocal), toPkRectF(uniformSize)));

            rgradient->setCenter(centerOBB);
            rgradient->setFocalPoint(focalOBB);

            const qreal centerRadiusOBB = KisAlgebra2D::absoluteToRelative(rgradient->centerRadius(), toPkRectF(uniformSize));
            const qreal focalRadiusOBB = KisAlgebra2D::absoluteToRelative(rgradient->focalRadius(), toPkRectF(uniformSize));

            rgradient->setCenterRadius(centerRadiusOBB);
            rgradient->setFocalRadius(focalRadiusOBB);

            rgradient->setCoordinateMode(PkGradientEnums::ObjectBoundingMode);

            // Warning: should it really be pre-multiplication?
            *transform = uniformizeTransform * gradient->transform();
        }
    }

    // TODO: all gradients in Krita are rendered in a premultiplied-alpha
    //       mode, which is against SVG standard. We need to fix that. Though
    //       it requires deepeer changes, than just mere setting of the
    //       PkGradient's interpolation mode on loading.
    // resultGradient->setInterpolationMode(PkGradient::ComponentInterpolation);

    return resultGradient;
}

SvgMeshGradient* prepareMeshGradientForShape(SvgGradientHelper *gradient,
                                             const KoShape *shape,
                                             const SvgGraphicsContext *gc) {

    SvgMeshGradient *resultGradient = nullptr;

    if (gradient->gradientUnits() == KoFlake::ObjectBoundingBox) {

        resultGradient = new SvgMeshGradient(*gradient->meshgradient());

        const PkRectF boundingRect = shape->outline().boundingRect();
        const PkTransform relativeToShape(boundingRect.width(), 0, 0, boundingRect.height(),
                                         boundingRect.x(), boundingRect.y());

        // NOTE: we apply translation right away, because caching hasn't been implemented for rendering, yet.
        // So, transform is called multiple times on the mesh and that's not nice
        resultGradient->setTransform(toPkTransform(gradient->transform() toQTransform(* relativeToShape)));
    } else {
        // NOTE: Krita's shapes use their own coordinate system. Where origin is at the top left
        // of the SHAPE. All the mesh patches will be rendered in the global 'user' coordinate system
        // where the origin is at the top left of the LAYER/DOCUMENT.

        // Get the user coordinates of the shape
        const PkTransform shapeglobal = shape->absoluteTransformation() * toQTransform(gc->matrix).inverted();

        // Get the translation offset to shift the origin from "Shape" to "User"
        const PkTransform translationOffset = PkTransform::fromTranslate(-shapeglobal.dx(), -shapeglobal.dy());

        resultGradient = new SvgMeshGradient(*gradient->meshgradient());

        // NOTE: we apply translation right away, because caching hasn't been implemented for rendering, yet.
        // So, transform is called multiple times on the mesh and that's not nice
        resultGradient->setTransform(toPkTransform(gradient->transform() toQTransform(* translationOffset)));
    }

    return resultGradient;
}

void SvgParser::applyFillStyle(KoShape *shape)
{
    SvgGraphicsContext *gc = m_context.currentGC();
    if (! gc)
        return;

    if (gc->fillType == SvgGraphicsContext::None) {
        shape->setBackground(PkSharedPointer<KoShapeBackground>(0));
    } else if (gc->fillType == SvgGraphicsContext::Solid) {
        shape->setBackground(PkSharedPointer<KoColorBackground>(new KoColorBackground(toQColor(gc->fillColor))));
    } else if (gc->fillType == SvgGraphicsContext::Complex) {
        // try to find referenced gradient
        SvgGradientHelper *gradient = findGradient(toQString(gc->fillId));
        if (gradient) {
            PkTransform transform;

            if (gradient->isMeshGradient()) {
                PkSharedPointer<KoMeshGradientBackground> bg;

                PkScopedPointer<SvgMeshGradient> result(prepareMeshGradientForShape(gradient, shape, gc));

                bg = PkSharedPointer<KoMeshGradientBackground>(new KoMeshGradientBackground(result.data(), transform));
                shape->setBackground(bg);
            } else if (gradient->gradient()) {
                PkGradient *result = prepareGradientForShape(gradient, shape, gc, &transform);
                if (result) {
                    PkSharedPointer<KoGradientBackground> bg;
                    bg = PkSharedPointer<KoGradientBackground>(new KoGradientBackground(result));
                    bg->setTransform(transform);
                    shape->setBackground(bg);
                }
            }
        } else {
            PkSharedPointer<KoVectorPatternBackground> pattern =
                findPattern(toQString(gc->fillId), shape);

            if (pattern) {
                shape->setBackground(pattern);
            } else {
                // no referenced fill found, use fallback color
                shape->setBackground(PkSharedPointer<KoColorBackground>(new KoColorBackground(toQColor(gc->fillColor))));
            }
        }
    } else if (gc->fillType == SvgGraphicsContext::Inherit) {
        shape->setInheritBackground(true);
    }

    KoPathShape *path = dynamic_cast<KoPathShape*>(shape);
    if (path)
        path->setFillRule(gc->fillRule);
}

void applyDashes(const KoShapeStrokeSP srcStroke, KoShapeStrokeSP dstStroke)
{
    const double lineWidth = srcStroke->lineWidth();
    PkVector<qreal> dashes = srcStroke->lineDashes();

    // apply line width to dashes and dash offset
    if (dashes.count() && lineWidth > 0.0) {
        const double dashOffset = srcStroke->dashOffset();
        PkVector<qreal> dashes = srcStroke->lineDashes();

        for (int i = 0; i < dashes.count(); ++i) {
            dashes[i] /= lineWidth;
        }

        dstStroke->setLineStyle(Qt::CustomDashLine, dashes);
        dstStroke->setDashOffset(dashOffset / lineWidth);
    } else {
        dstStroke->setLineStyle(Qt::SolidLine, PkVector<qreal>());
    }
}

void SvgParser::applyStrokeStyle(KoShape *shape)
{
    SvgGraphicsContext *gc = m_context.currentGC();
    if (! gc)
        return;

    if (gc->strokeType == SvgGraphicsContext::None) {
        KoShapeStrokeSP stroke(new KoShapeStroke());
        stroke->setLineWidth(0.0);
        const PkColor color = Qt::transparent;
        stroke->setColor(color);
        shape->setStroke(stroke);
    } else if (gc->strokeType == SvgGraphicsContext::Solid) {
        KoShapeStrokeSP stroke(new KoShapeStroke(*gc->stroke));
        applyDashes(gc->stroke, stroke);
        shape->setStroke(stroke);
    } else if (gc->strokeType == SvgGraphicsContext::Complex) {
        // try to find referenced gradient
        SvgGradientHelper *gradient = findGradient(toQString(gc->strokeId));
        if (gradient) {
            PkTransform transform;
            PkGradient *result = prepareGradientForShape(gradient, shape, gc, &transform);
            if (result) {
                QBrush brush = *result;
                delete result;
                brush.setTransform(toQTransform(transform));

                KoShapeStrokeSP stroke(new KoShapeStroke(*gc->stroke));
                stroke->setLineBrush(brush);
                applyDashes(gc->stroke, stroke);
                shape->setStroke(stroke);
            }
        } else {
            // no referenced stroke found, use fallback color
            KoShapeStrokeSP stroke(new KoShapeStroke(*gc->stroke));
            applyDashes(gc->stroke, stroke);
            shape->setStroke(stroke);
        }
    } else if (gc->strokeType == SvgGraphicsContext::Inherit) {
        shape->setInheritStroke(true);
    }
}

void SvgParser::applyMarkers(KoPathShape *shape)
{
    SvgGraphicsContext *gc = m_context.currentGC();
    if (!gc)
        return;

    if (!gc->markerStartId.isEmpty() && m_markers.contains(toQString(gc->markerStartId))) {
        shape->setMarker(m_markers[toQString(gc->markerStartId)].data(), KoFlake::StartMarker);
    }

    if (!gc->markerMidId.isEmpty() && m_markers.contains(toQString(gc->markerMidId))) {
        shape->setMarker(m_markers[toQString(gc->markerMidId)].data(), KoFlake::MidMarker);
    }

    if (!gc->markerEndId.isEmpty() && m_markers.contains(toQString(gc->markerEndId))) {
        shape->setMarker(m_markers[toQString(gc->markerEndId)].data(), KoFlake::EndMarker);
    }

    shape->setAutoFillMarkers(gc->autoFillMarkers);
}

void SvgParser::applyPaintOrder(KoShape *shape)
{
    SvgGraphicsContext *gc = m_context.currentGC();
    if (!gc)
        return;

    if (!gc->paintOrder.isEmpty() && gc->paintOrder != "inherit") {
        PkStringList paintOrder;
        for (const PkString &po : gc->paintOrder.split(u' ')) {
            paintOrder.append(toQString(po));
        }
        PkVector<KoShape::PaintOrder> order;
        Q_FOREACH(const PkString p, paintOrder) {
            if (p == "fill") {
                order.append(KoShape::Fill);
            } else if (p == "stroke") {
                order.append(KoShape::Stroke);
            } else if (p == "markers") {
                order.append(KoShape::Markers);
            }
        }
        if (paintOrder.size() == 1 && order.isEmpty()) { // Normal
            order = KoShape::defaultPaintOrder();
        }
        if (order.size() == 1) {
            if (order.first() == KoShape::Fill) {
                shape->setPaintOrder(KoShape::Fill, KoShape::Stroke);
            } else if (order.first() == KoShape::Stroke) {
                shape->setPaintOrder(KoShape::Stroke, KoShape::Fill);
            } else if (order.first() == KoShape::Markers) {
                shape->setPaintOrder(KoShape::Markers, KoShape::Fill);
            }
        } else if (order.size() > 1) {
            shape->setPaintOrder(order.at(0), order.at(1));
        }
    }
}

void SvgParser::applyClipping(KoShape *shape, const PkPointF &shapeToOriginalUserCoordinates)
{
    SvgGraphicsContext *gc = m_context.currentGC();
    if (! gc)
        return;

    if (gc->clipPathId.isEmpty())
        return;

    SvgClipPathHelper *clipPath = findClipPath(toQString(gc->clipPathId));
    if (!clipPath || clipPath->isEmpty())
        return;

    PkList<KoShape*> shapes;

    Q_FOREACH (KoShape *item, clipPath->shapes()) {
        KoShape *clonedShape = item->cloneShape();
        KIS_ASSERT_RECOVER(clonedShape) { continue; }

        shapes.append(clonedShape);
    }

    if (!shapeToOriginalUserCoordinates.isNull()) {
        const PkTransform t =
            PkTransform::fromTranslate(shapeToOriginalUserCoordinates.x(),
                                      shapeToOriginalUserCoordinates.y());

        Q_FOREACH(KoShape *s, shapes) {
            s->applyAbsoluteTransformation(t);
        }
    }

    KoClipPath *clipPathObject = new KoClipPath(shapes,
                                                clipPath->clipPathUnits() == KoFlake::ObjectBoundingBox ?
                                                KoFlake::ObjectBoundingBox : KoFlake::UserSpaceOnUse);
    shape->setClipPath(clipPathObject);
}

void SvgParser::applyMaskClipping(KoShape *shape, const PkPointF &shapeToOriginalUserCoordinates)
{
    SvgGraphicsContext *gc = m_context.currentGC();
    if (!gc)
        return;

    if (gc->clipMaskId.isEmpty())
        return;


    PkSharedPointer<KoClipMask> originalClipMask = m_clipMasks.value(toQString(gc->clipMaskId));
    if (!originalClipMask || originalClipMask->isEmpty()) return;

    KoClipMask *clipMask = originalClipMask->clone();

    clipMask->setExtraShapeOffset(shapeToOriginalUserCoordinates);

    shape->setClipMask(clipMask);
}

KoShape* SvgParser::parseUse(const PkXmlElement &e, DeferredUseStore* deferredUseStore)
{
    PkString href = e.attribute("xlink:href");
    if (href.isEmpty())
        return 0;

    PkString key = href.mid(1);
    const bool gotDef = m_context.hasDefinition(toPkString(key));
    if (gotDef) {
        return resolveUse(e, key);
    } else if (deferredUseStore) {
        deferredUseStore->add(&e, key);
        return 0;
    }
    debugFlake << "WARNING: Did not find reference for svg 'use' element. Skipping. Id: "
             << key;
    return 0;
}

KoShape* SvgParser::resolveUse(const PkXmlElement &e, const PkString& key)
{
    KoShape *result = 0;

    SvgGraphicsContext *gc = m_context.pushGraphicsContext(toPkXmlElement(e));

    // TODO: parse 'width' and 'height' as well
    gc->matrix.translate(parseUnitX(e.attribute("x", "0")), parseUnitY(e.attribute("y", "0")));

    const PkXmlElement referencedElement = toQDomElement(m_context.definition(toPkString(key)));
    result = parseGroup(e, referencedElement, false);

    m_context.popGraphicsContext();
    return result;
}

void SvgParser::addToGroup(PkList<KoShape*> shapes, KoShapeContainer *group)
{
    m_shapes += shapes;

    if (!group || shapes.isEmpty())
        return;

    // not normalized
    KoShapeGroupCommand cmd(group, toPkList(shapes), false);
    cmd.redo();
}

PkList<KoShape*> SvgParser::parseSvg(const PkXmlElement &e, PkSizeF *fragmentSize)
{
    // check if we are the root svg element
    const bool isRootSvg = m_context.isRootContext();

    // parse 'transform' field if preset
    SvgGraphicsContext *gc = m_context.pushGraphicsContext(toPkXmlElement(e));

    applyStyle(0, e, PkPointF());

    const PkString w = e.attribute("width");
    const PkString h = e.attribute("height");

    qreal width = w.isEmpty() ? 666.0 : parseUnitX(w);
    qreal height = h.isEmpty() ? 555.0 : parseUnitY(h);

    if (w.isEmpty() || h.isEmpty()) {
        PkRectF viewRect;
        PkTransform viewTransform_unused;
        PkRectF fakeBoundingRect(0.0, 0.0, 1.0, 1.0);

        PkRectF pkViewRect = toPkRectF(viewRect);
        PkTransform pkViewTransform_unused = toPkTransform(toQTransform(viewTransform_unused));
        if (SvgUtil::parseViewBox(toPkXmlElement(e), toPkRectF(fakeBoundingRect),
                                  &pkViewRect, &pkViewTransform_unused)) {
            viewRect = toQRectF(pkViewRect);

            PkSizeF estimatedSize = viewRect.size();

            if (estimatedSize.isValid()) {

                if (!w.isEmpty()) {
                    estimatedSize = PkSizeF(width, width * estimatedSize.height() / estimatedSize.width());
                } else if (!h.isEmpty()) {
                    estimatedSize = PkSizeF(height * estimatedSize.width() / estimatedSize.height(), height);
                }

                width = estimatedSize.width();
                height = estimatedSize.height();
            }
        }
    }

    PkSizeF svgFragmentSize(PkSizeF(width, height));

    if (fragmentSize) {
        *fragmentSize = svgFragmentSize;
    }

    gc->currentBoundingBox = toPkRectF(PkRectF(PkPointF(0, 0), svgFragmentSize));

    if (!isRootSvg) {
        // x and y attribute has no meaning for outermost svg elements
        const qreal x = parseUnit(e.attribute("x", "0"));
        const qreal y = parseUnit(e.attribute("y", "0"));

        PkTransform move = toPkTransform(PkTransform::fromTranslate(x, y));
        gc->matrix = move * gc->matrix;
    }

    /**
     * In internal SVG coordinate systems pixels are linked to absolute
     * values with a fixed ratio.
     *
     * See CSS specification:
     * https://www.w3.org/TR/css-values-3/#absolute-lengths
     */
    gc->pixelsPerInch = 96.0;

    applyViewBoxTransform(e);

    PkList<KoShape*> shapes;

    // First find the metadata
    for (PkXmlNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
        PkXmlElement b = n.toElement();
        if (b.isNull())
            continue;

        if (b.tagName() == "title") {
            m_documentTitle = b.text().trimmed();
        }
        else if (b.tagName() == "desc") {
            m_documentDescription = b.text().trimmed();
        }
        else if (b.tagName() == "metadata") {
            // TODO: parse the metadata
        }
    }


    // SVG 1.1: skip the rendering of the element if it has null viewBox; however an inverted viewbox is just peachy
    // and as mother makes them -- if mother is inkscape.
    if (gc->currentBoundingBox.normalized().isValid()) {
        shapes = parseContainer(e);
    }

    m_context.popGraphicsContext();

    return shapes;
}

void SvgParser::applyViewBoxTransform(const PkXmlElement &element)
{
    SvgGraphicsContext *gc = m_context.currentGC();

    PkRectF viewRect = toQRectF(gc->currentBoundingBox);
    PkTransform viewTransform;
    PkRectF pkViewRect = toPkRectF(viewRect);
    PkTransform pkViewTransform = toPkTransform(toQTransform(viewTransform));

    if (SvgUtil::parseViewBox(toPkXmlElement(element), gc->currentBoundingBox,
                              &pkViewRect, &pkViewTransform)) {

        gc->matrix = pkViewTransform * gc->matrix;
        gc->currentBoundingBox = pkViewRect;
    }
}

PkStringList SvgParser::warnings() const
{
    PkStringList warnings;

    Q_FOREACH (const KoID &id, m_warnings) {
        warnings << toQString(id.name());
    }

    return warnings;
}

PkList<QExplicitlySharedDataPointer<KoMarker> > SvgParser::knownMarkers() const
{
    return m_markers.values();
}

PkString SvgParser::documentTitle() const
{
    return m_documentTitle;
}

PkString SvgParser::documentDescription() const
{
    return m_documentDescription;
}

void SvgParser::setFileFetcher(SvgParser::FileFetcherFunc func)
{
    m_context.setFileFetcher(
        [func](const PkString &url) {
            return toPkByteArray(func(toQString(url)));
        });
}

inline PkPointF extraShapeOffset(const KoShape *shape, const PkTransform coordinateSystemOnLoading)
{
    const PkTransform shapeToOriginalUserCoordinates =
        shape->absoluteTransformation().inverted() *
        coordinateSystemOnLoading;

    KIS_SAFE_ASSERT_RECOVER_NOOP(shapeToOriginalUserCoordinates.type() <= PkTransform::TxTranslate);
    return PkPointF(shapeToOriginalUserCoordinates.dx(), shapeToOriginalUserCoordinates.dy());
}

KoShape* SvgParser::parseGroup(const PkXmlElement &b, const PkXmlElement &overrideChildrenFrom, bool createContext)
{
    if (createContext) {
        m_context.pushGraphicsContext(toPkXmlElement(b));
    }

    KoShapeGroup *group = new KoShapeGroup();
    group->setZIndex(m_context.nextZIndex());

    // groups should also have their own coordinate system!
    group->applyAbsoluteTransformation(toQTransform(m_context.currentGC()->matrix));
    const PkPointF extraOffset = extraShapeOffset(group, toQTransform(m_context.currentGC()->matrix));

    uploadStyleToContext(b);

    PkList<KoShape*> childShapes;

    if (!overrideChildrenFrom.isNull()) {
        // we upload styles from both: <use> and <defs>
        uploadStyleToContext(overrideChildrenFrom);
        if (overrideChildrenFrom.tagName() == "symbol") {
            childShapes = {parseGroup(overrideChildrenFrom)};
        } else {
            childShapes = parseSingleElement(overrideChildrenFrom, 0);
        }
    } else {
        childShapes = parseContainer(b);
    }

    // handle id
    applyId(b.attribute("id"), group);

    if (b.hasAttribute(KoSvgTextShape_TEXTCONTOURGROUP)) {
        Q_FOREACH(KoShape *shape, childShapes) {
            if (shape->shapeId() == KoSvgTextShape_SHAPEID) {
                shape->setTransformation(group->transformation());

                if (createContext) {
                    m_context.popGraphicsContext();
                }
                return shape;
            }
        }
    }
    addToGroup(childShapes, group);

    applyCurrentStyle(group, extraOffset); // apply style to this group after size is set

    parseMetadataApplyToShape(b, group);

    if (createContext) {
        m_context.popGraphicsContext();
    }

    return group;
}

PkXmlText SvgParser::getTheOnlyTextChild(const PkXmlElement &e)
{
    PkXmlNode firstChild = e.firstChild();
    return !firstChild.isNull() && firstChild == e.lastChild() && firstChild.isText() ?
                firstChild.toText() : PkXmlText();
}

bool SvgParser::shapeInDefs(const KoShape *shape)
{
    for (auto defs = m_defsShapes.begin(); defs != m_defsShapes.end(); defs++) {
        KoShape *dShape = *defs;
        if (!dShape) continue;
        if (dShape->hasCommonParent(shape)) return true;
    }
    return false;
}

KoShape* SvgParser::getTextPath(const PkXmlElement &e, bool hideShapesFromDefs) {
    if (e.hasAttribute("path")) {
        PkXmlElement p = e.ownerDocument().createElement("path");
        p.setAttribute("d", e.attribute("path"));
        KoShape *s = createPath(p);
        if (hideShapesFromDefs) {
            s->setTransparency(1.0);
        }
        return s;
    } else {
        PkString pathId;
        if (e.hasAttribute("href")) {
            pathId = e.attribute("href").remove(0, 1);
        } else if (e.hasAttribute("xlink:href")) {
            pathId = e.attribute("xlink:href").remove(0, 1);
        }
        if (!pathId.isNull()) {
            KoShape *s = m_context.shapeById(toPkString(pathId));
            if (s) {
                KoShape *cloned = s->cloneShape();
                const PkTransform absTf = s->absoluteTransformation();
                cloned->setTransformation(absTf * m_shapeParentTransform.value(s).inverted());
                if(cloned && shapeInDefs(s) && hideShapesFromDefs) {
                    cloned->setTransparency(1.0);
                }
                return cloned;
            }
        }
    }
    return nullptr;
}

void SvgParser::parseTextChildren(const PkXmlElement &e, KoSvgTextLoader &textLoader, bool hideShapesFromDefs) {
    PkXmlText t = getTheOnlyTextChild(e);
    if (!t.isNull()) {
        textLoader.loadSvgText(t, m_context);
    } else {
        textLoader.enterNodeSubtree();
        for (PkXmlNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
            PkXmlElement b = n.toElement();
            if (b.tagName() == "title" || b.tagName() == "desc") continue; /// TODO: we should skip dublin core metadata too...
            textLoader.nextNode();
            if (b.isNull()) {
                textLoader.loadSvgText(n.toText(), m_context);
                KoShape *styleDummy = new KoPathShape();
                applyCurrentBasicStyle(styleDummy);
                textLoader.setStyleInfo(styleDummy);
            } else {
                m_context.pushGraphicsContext(toPkXmlElement(b));
                uploadStyleToContext(b);
                textLoader.loadSvg(b, m_context);
                if (b.hasChildNodes()) {
                    parseTextChildren(b, textLoader, hideShapesFromDefs);
                }
                textLoader.setTextPathOnCurrentNode(getTextPath(b, hideShapesFromDefs));
                m_context.popGraphicsContext();
            }
        }
        textLoader.leaveNodeSubtree();
    }
    KoShape *styleDummy = new KoPathShape();
    applyCurrentBasicStyle(styleDummy);
    textLoader.setStyleInfo(styleDummy);
}

KoShape *SvgParser::parseTextElement(const PkXmlElement &e, KoSvgTextShape *mergeIntoShape)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(e.tagName() == "text" || e.tagName() == "tspan" || e.tagName() == "textPath", 0);
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(m_isInsideTextSubtree || e.tagName() == "text", 0);
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(e.tagName() == "text" || !mergeIntoShape, 0);

    KoSvgTextShape *rootTextShape  = 0;

    bool hideShapesFromDefs = true;
    if (mergeIntoShape) {
        rootTextShape = mergeIntoShape;
        hideShapesFromDefs = false;
    } else {
        KoShapeFactoryBase *factory = KoShapeRegistry::instance()->value("KoSvgTextShapeID");
        rootTextShape = dynamic_cast<KoSvgTextShape*>(factory->createDefaultShape(m_documentResourceManager));
    }
    KoSvgTextLoader textLoader(rootTextShape);

    if (rootTextShape) {
        m_isInsideTextSubtree = true;
    }

    m_context.pushGraphicsContext(toPkXmlElement(e));
    uploadStyleToContext(e);

    if (rootTextShape) {
        if (!m_context.currentGC()->shapeInsideValue.isEmpty()) {
            PkList<KoShape*> shapesInside = createListOfShapesFromCSS(e, toQString(m_context.currentGC()->shapeInsideValue), m_context, hideShapesFromDefs);
            rootTextShape->setShapesInside(shapesInside);
        }

        if (!m_context.currentGC()->shapeSubtractValue.isEmpty()) {
            PkList<KoShape*> shapesSubtract = createListOfShapesFromCSS(e, toQString(m_context.currentGC()->shapeSubtractValue), m_context, hideShapesFromDefs);
            rootTextShape->setShapesSubtract(shapesSubtract);
        }
    }

    if (e.hasAttribute("krita:textVersion")) {
        m_context.currentGC()->textProperties.setProperty(KoSvgTextProperties::KraTextVersionId, e.attribute("krita:textVersion", "1").toInt());

        if (m_isInsideTextSubtree) {
            debugFlake << "WARNING: \"krita:textVersion\" attribute appeared in non-root text shape";
        }
    }

    parseMetadataApplyToShape(e, rootTextShape);

    if (!mergeIntoShape) {
        rootTextShape->setZIndex(m_context.nextZIndex());
    }

    if (m_context.currentGC()->textProperties.hasProperty(KoSvgTextProperties::KraTextVersionId) &&
        m_context.currentGC()->textProperties.property(KoSvgTextProperties::KraTextVersionId).toInt() < 2) {

        static const KoID warning("warn_text_version_1",
                                  toPkString(i18nc("warning while loading SVG text",
                                        "The document has vector text created "
                                        "in Krita 4.x. When you save the document, "
                                        "the text object will be converted into "
                                        "Krita 5 format that will no longer be "
                                        "compatible with Krita 4.x")));

        if (!m_warnings.contains(warning)) {
            m_warnings << warning;
        }
    }

    textLoader.loadSvg(e, m_context, m_resolveTextPropertiesForTopLevel);

    // 1) apply transformation only in case we are not overriding the shape!
    // 2) the transformation should be applied *before* the shape is added to the group!
    if (!mergeIntoShape) {
        // groups should also have their own coordinate system!
        rootTextShape->applyAbsoluteTransformation(toQTransform(m_context.currentGC()->matrix));
        const PkPointF extraOffset = extraShapeOffset(rootTextShape, toQTransform(m_context.currentGC()->matrix));

        // handle id
        applyId(e.attribute("id"), rootTextShape);
        applyCurrentStyle(rootTextShape, extraOffset); // apply style to this group after size is set
    } else {
        m_context.currentGC()->matrix = toPkTransform(mergeIntoShape->absoluteTransformation());
        applyCurrentBasicStyle(rootTextShape);
    }

    PkXmlText onlyTextChild = getTheOnlyTextChild(e);
    if (!onlyTextChild.isNull()) {
        textLoader.loadSvgText(onlyTextChild, m_context);

    } else {
        parseTextChildren(e, textLoader, hideShapesFromDefs);
    }

    m_context.popGraphicsContext();

    m_isInsideTextSubtree = false;

    //rootTextShape->debugParsing();


    return rootTextShape;
}

PkList<KoShape*> SvgParser::parseContainer(const PkXmlElement &e)
{
    PkList<KoShape*> shapes;

    // are we parsing a switch container
    bool isSwitch = e.tagName() == "switch";

    DeferredUseStore deferredUseStore(this);

    for (PkXmlNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
        PkXmlElement b = n.toElement();
        if (b.isNull()) {
            continue;
        }

        if (isSwitch) {
            // if we are parsing a switch check the requiredFeatures, requiredExtensions
            // and systemLanguage attributes
            // TODO: evaluate feature list
            if (b.hasAttribute("requiredFeatures")) {
                continue;
            }
            if (b.hasAttribute("requiredExtensions")) {
                // we do not support any extensions
                continue;
            }
            if (b.hasAttribute("systemLanguage")) {
                // not implemented yet
            }
        }

        PkList<KoShape*> currentShapes = parseSingleElement(b, &deferredUseStore);
        shapes.append(currentShapes);

        // if we are parsing a switch, stop after the first supported element
        if (isSwitch && !currentShapes.isEmpty())
            break;
    }
    return shapes;
}

void SvgParser::parseDefsElement(const PkXmlElement &e)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(e.tagName() == "defs");
    parseSingleElement(e);
}

PkList<KoShape*> SvgParser::parseSingleElement(const PkXmlElement &b, DeferredUseStore* deferredUseStore)
{
    PkList<KoShape*> shapes;

    // save definition for later instantiation with 'use'
    m_context.addDefinition(toPkXmlElement(b));
    if (deferredUseStore) {
        deferredUseStore->checkPendingUse(b, shapes);
    }

    if (b.tagName() == "svg") {
        shapes += parseSvg(b);
    } else if (b.tagName() == "g" || b.tagName() == "a") {
        // treat svg link <a> as group so we don't miss its child elements
        shapes += parseGroup(b);
    } else if (b.tagName() == "symbol") {
        parseSymbol(b);
    } else if (b.tagName() == "switch") {
        m_context.pushGraphicsContext(toPkXmlElement(b));
        shapes += parseContainer(b);
        m_context.popGraphicsContext();
    } else if (b.tagName() == "defs") {
        if (b.childNodes().count() > 0) {
            /**
             * WARNING: 'defs' are basically 'display:none' style, therefore they should not play
             *          any role in shapes outline calculation. But setVisible(false) shapes do!
             *          Should be fixed in the future!
             */
            KoShape *defsShape = parseGroup(b);
            defsShape->setVisible(false);
            m_defsShapes << defsShape; // TODO: where to delete the shape!?

        }
    } else if (b.tagName() == "linearGradient" || b.tagName() == "radialGradient") {
    } else if (b.tagName() == "pattern") {
    } else if (b.tagName() == "filter") {
        // not supported!
    } else if (b.tagName() == "clipPath") {
        parseClipPath(b);
    } else if (b.tagName() == "mask") {
        parseClipMask(b);
    } else if (b.tagName() == "marker") {
        parseMarker(b);
    } else if (b.tagName() == "style") {
        m_context.addStyleSheet(toPkXmlElement(b));
    } else if (b.tagName() == "text" || b.tagName() == "tspan" || b.tagName() == "textPath") {
        shapes += parseTextElement(b);
    } else if (b.tagName() == "rect" || b.tagName() == "ellipse" || b.tagName() == "circle" || b.tagName() == "line" || b.tagName() == "polyline"
               || b.tagName() == "polygon" || b.tagName() == "path" || b.tagName() == "image") {
        KoShape *shape = createObjectDirect(b);

        if (shape) {
            if (!shape->outlineRect().isNull() || !shape->boundingRect().isNull()) {
                shapes.append(shape);
            } else {
                debugFlake << "WARNING: shape is totally empty!" << shape->shapeId() << ppVar(shape->outlineRect());
                debugFlake << "    " << shape->shapeId() << ppVar(shape->outline());
                {
                    PkString string;
                    PkTextStream stream(&string);
                    KisPortingUtils::setUtf8OnStream(stream);
                    stream << b;
                    debugFlake << "    " << string;
                }
                delete shape;
            }
        }
    } else if (b.tagName() == "use") {
        KoShape* s = parseUse(b, deferredUseStore);
        if (s) {
            shapes += s;
        }
    } else if (b.tagName() == "color-profile") {
        m_context.parseProfile(toPkXmlElement(b));
    } else {
        // this is an unknown element, so try to load it anyway
        // there might be a shape that handles that element
        KoShape *shape = createObject(b);
        if (shape) {
            shapes.append(shape);
        }
    }

    return shapes;
}

// Creating functions
// ---------------------------------------------------------------------------------------

KoShape * SvgParser::createPath(const PkXmlElement &element)
{
    KoShape *obj = 0;
    if (element.tagName() == "line") {
        KoPathShape *path = static_cast<KoPathShape*>(createShape(KoPathShapeId));
        if (path) {
            double x1 = element.attribute("x1").isEmpty() ? 0.0 : parseUnitX(element.attribute("x1"));
            double y1 = element.attribute("y1").isEmpty() ? 0.0 : parseUnitY(element.attribute("y1"));
            double x2 = element.attribute("x2").isEmpty() ? 0.0 : parseUnitX(element.attribute("x2"));
            double y2 = element.attribute("y2").isEmpty() ? 0.0 : parseUnitY(element.attribute("y2"));
            path->clear();
            path->moveTo(PkPointF(x1, y1));
            path->lineTo(PkPointF(x2, y2));
            path->normalize();
            obj = path;
        }
    } else if (element.tagName() == "polyline" || element.tagName() == "polygon") {
        KoPathShape *path = static_cast<KoPathShape*>(createShape(KoPathShapeId));
        if (path) {
            path->clear();

            bool bFirst = true;
            PkStringList pointList = toStringList(SvgUtil::simplifyList(toPkString(element.attribute("points"))));
            for (PkStringList::Iterator it = pointList.begin(); it != pointList.end(); ++it) {
                PkPointF point;
                point.setX(SvgUtil::fromUserSpace(KisDomUtils::toDouble(toPkString(*it))));
                ++it;
                if (it == pointList.end())
                    break;
                point.setY(SvgUtil::fromUserSpace(KisDomUtils::toDouble(toPkString(*it))));
                if (bFirst) {
                    path->moveTo(point);
                    bFirst = false;
                } else
                    path->lineTo(point);
            }
            if (element.tagName() == "polygon")
                path->close();

            path->setPosition(path->normalize());

            obj = path;
        }
    } else if (element.tagName() == "path") {
        KoPathShape *path = static_cast<KoPathShape*>(createShape(KoPathShapeId));
        if (path) {
            path->clear();

            KoPathShapeLoader loader(path);
            loader.parseSvg(element.attribute("d"), true);
            path->setPosition(path->normalize());

            PkPointF newPosition = PkPointF(SvgUtil::fromUserSpace(path->position().x()),
                                          SvgUtil::fromUserSpace(path->position().y()));
            PkSizeF newSize = PkSizeF(SvgUtil::fromUserSpace(path->size().width()),
                                    SvgUtil::fromUserSpace(path->size().height()));

            path->setSize(newSize);
            path->setPosition(newPosition);

            if (element.hasAttribute("sodipodi:nodetypes")) {
                path->loadNodeTypes(element.attribute("sodipodi:nodetypes"));
            }
            obj = path;
        }
    }

    return obj;
}

KoShape * SvgParser::createObjectDirect(const PkXmlElement &b)
{
    m_context.pushGraphicsContext(toPkXmlElement(b));
    uploadStyleToContext(b);

    KoShape *obj = createShapeFromElement(b, m_context);
    if (obj) {
        obj->applyAbsoluteTransformation(toQTransform(m_context.currentGC()->matrix));
        const PkPointF extraOffset = extraShapeOffset(obj, toQTransform(m_context.currentGC()->matrix));

        applyCurrentStyle(obj, extraOffset);

        // handle id
        applyId(b.attribute("id"), obj);
        obj->setZIndex(m_context.nextZIndex());
        parseMetadataApplyToShape(b, obj);
    }

    m_context.popGraphicsContext();

    if (obj) {
        m_shapeParentTransform.insert(obj, toQTransform(m_context.currentGC()->matrix));
    }
    return obj;
}

KoShape * SvgParser::createObject(const PkXmlElement &b, const SvgStyles &style)
{
    m_context.pushGraphicsContext(toPkXmlElement(b));

    KoShape *obj = createShapeFromElement(b, m_context);
    if (obj) {
        obj->applyAbsoluteTransformation(toQTransform(m_context.currentGC()->matrix));
        const PkPointF extraOffset = extraShapeOffset(obj, toQTransform(m_context.currentGC()->matrix));

        SvgStyles objStyle = style.isEmpty() ? m_context.styleParser().collectStyles(toPkXmlElement(b)) : style;
        m_context.styleParser().parseFont(objStyle);
        applyStyle(obj, objStyle, extraOffset);

        // handle id
        applyId(b.attribute("id"), obj);
        obj->setZIndex(m_context.nextZIndex());
        parseMetadataApplyToShape(b, obj);
    }

    m_context.popGraphicsContext();

    if (obj) {
        m_shapeParentTransform.insert(obj, toQTransform(m_context.currentGC()->matrix));
    }

    return obj;
}

KoShape * SvgParser::createShapeFromElement(const PkXmlElement &element, SvgLoadingContext &context)
{
    KoShape *object = 0;


    const PkString tagName = toQString(SvgUtil::mapExtendedShapeTag(toPkString(element.tagName()), toPkXmlElement(element)));
    PkList<KoShapeFactoryBase*> factories = KoShapeRegistry::instance()->factoriesForElement(toQString(KoXmlNS::svg), tagName);

    foreach (KoShapeFactoryBase *f, factories) {
        KoShape *shape = f->createDefaultShape(m_documentResourceManager);
        if (!shape)
            continue;

        SvgShape *svgShape = dynamic_cast<SvgShape*>(shape);
        if (!svgShape) {
            delete shape;
            continue;
        }

        // reset transformation that might come from the default shape
        shape->setTransformation(PkTransform());

        // reset border
        KoShapeStrokeModelSP oldStroke = shape->stroke();
        shape->setStroke(KoShapeStrokeModelSP());

        // reset fill
        shape->setBackground(PkSharedPointer<KoShapeBackground>(0));

        if (!svgShape->loadSvg(toPkXmlElement(element), context)) {
            delete shape;
            continue;
        }

        object = shape;
        break;
    }

    if (!object) {
        object = createPath(element);
    }

    return object;
}

KoShape *SvgParser::createShapeFromCSS(const PkXmlElement e, const PkString value, SvgLoadingContext &context, bool hideShapesFromDefs)
{
    if (value.isEmpty()) {
        return 0;
    }
    unsigned int start = value.indexOf('(') + 1;
    unsigned int end = value.indexOf(')', start);

    PkString val = value.mid(start, end - start);
    PkString fillRule;
    if (val.startsWith("evenodd,")) {
        start += PkString("evenodd,").size();
        fillRule = "evenodd";
    } else if (val.startsWith("nonzero,")) {
        start += PkString("nonzero,").size();
        fillRule = "nonzero";
    }
    val = value.mid(start, end - start);

    PkXmlElement el;
    if (value.startsWith("url(")) {
        start = value.indexOf('#') + 1;
        KoShape *s = m_context.shapeById(toPkString(value.mid(start, end - start)));
        if (s) {
            const PkTransform absTf = s->absoluteTransformation();
            KoShape *cloned = s->cloneShape();
            cloned->setTransformation(absTf * m_shapeParentTransform.value(s).inverted());
            // When we have a parent, the shape is inside the defs, but when not,
            // it's in the group we're in the currently parsing.

            if (cloned && shapeInDefs(s) && hideShapesFromDefs) {
                cloned->setTransparency(1.0);
            }
            return cloned;
        }
    } else if (value.startsWith("circle(")) {
        el = e.ownerDocument().createElement("circle");
        PkStringList params = val.split(" ");
        el.setAttribute("r", SvgUtil::parseUnitXY(context.currentGC(), context.resolvedProperties(), toPkString(params.first())));
        if (params.contains("at")) {
            // 1 == "at"
            el.setAttribute("cx", SvgUtil::parseUnitX(context.currentGC(), context.resolvedProperties(), toPkString(params.at(2))));
            el.setAttribute("cy", SvgUtil::parseUnitY(context.currentGC(), context.resolvedProperties(), toPkString(params.at(3))));
        }
    } else if (value.startsWith("ellipse(")) {
        el = e.ownerDocument().createElement("ellipse");
        PkStringList params = val.split(" ");
        el.setAttribute("rx", SvgUtil::parseUnitX(context.currentGC(), context.resolvedProperties(), toPkString(params.at(0))));
        el.setAttribute("ry", SvgUtil::parseUnitY(context.currentGC(), context.resolvedProperties(), toPkString(params.at(1))));
        if (params.contains("at")) {
            // 2 == "at"
            el.setAttribute("cx", SvgUtil::parseUnitX(context.currentGC(), context.resolvedProperties(), toPkString(params.at(3))));
            el.setAttribute("cy", SvgUtil::parseUnitY(context.currentGC(), context.resolvedProperties(), toPkString(params.at(4))));
        }
    } else if (value.startsWith("polygon(")) {
        el = e.ownerDocument().createElement("polygon");
        PkStringList points;
        Q_FOREACH(PkString point, toStringList(SvgUtil::simplifyList(toPkString(val)))) {
            bool xVal = points.size() % 2;
            if (xVal) {
                points.append(PkString::number(SvgUtil::parseUnitX(context.currentGC(), context.resolvedProperties(), toPkString(point))));
            } else {
                points.append(PkString::number(SvgUtil::parseUnitY(context.currentGC(), context.resolvedProperties(), toPkString(point))));
            }
        }
        el.setAttribute("points", points.join(" "));
    } else if (value.startsWith("path(")) {
        el = e.ownerDocument().createElement("path");
        // SVG path data is inside a string.
        start += 1;
        end -= 1;
        el.setAttribute("d", value.mid(start, end - start));
    }

    el.setAttribute("fill-rule", fillRule);
    KoShape *shape = createShapeFromElement(el, context);
    if (shape) shape->setTransparency(1.0);
    return shape;
}

PkList<KoShape *> SvgParser::createListOfShapesFromCSS(const PkXmlElement e, const PkString value, SvgLoadingContext &context, bool hideShapesFromDefs)
{
    PkList<KoShape*> shapeList;
    if (value == "auto" || value == "none") {
        return shapeList;
    }
    PkStringList params = value.split(")");
    Q_FOREACH(const PkString param, params) {
        KoShape *s = createShapeFromCSS(e, param.trimmed()+")", context, hideShapesFromDefs);
        if (s) {
            shapeList.append(s);
        }
    }
    return shapeList;
}

KoShape *SvgParser::createShape(const PkString &shapeID)
{
    KoShapeFactoryBase *factory = KoShapeRegistry::instance()->get(toPkString(shapeID));
    if (!factory) {
        debugFlake << "Could not find factory for shape id" << shapeID;
        return 0;
    }

    KoShape *shape = factory->createDefaultShape(m_documentResourceManager);
    if (!shape) {
        debugFlake << "Could not create Default shape for shape id" << shapeID;
        return 0;
    }
    if (shape->shapeId().isEmpty()) {
        shape->setShapeId(factory->id());
    }

    // reset transformation that might come from the default shape
    shape->setTransformation(PkTransform());

    // reset border
    // ??? KoShapeStrokeModelSP oldStroke = shape->stroke();
    shape->setStroke(KoShapeStrokeModelSP());

    // reset fill
    shape->setBackground(PkSharedPointer<KoShapeBackground>(0));

    return shape;
}

void SvgParser::applyId(const PkString &id, KoShape *shape)
{
    if (id.isEmpty())
        return;

    KoShape *existingShape = m_context.shapeById(toPkString(id));
    if (existingShape) {
        debugFlake << "SVG contains nodes with duplicated id:" << id;
        // Generate a random name and just don't register the shape.
        // We don't use the name as a unique identifier so we don't need to
        // worry about the extremely rare case of name collision.
        const PkString suffix = PkString::number(QRandomGenerator::system()->bounded(0x10000000, 0x7FFFFFFF), 16);
        const PkString newName = id + '_' + suffix;
        shape->setName(newName);
    } else {
        shape->setName(id);
        m_context.registerShape(toPkString(id), shape);
    }
}
