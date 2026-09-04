/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2002-2003, 2005 Rob Buis <buis@kde.org>
 * SPDX-FileCopyrightText: 2005-2006 Tim Beaulen <tbscope@gmail.com>
 * SPDX-FileCopyrightText: 2005, 2007-2009 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef SVGPARSER_H
#define SVGPARSER_H

#include <PkMap.h>
#include <PkSize.h>
#include <PkRect.h>
#include <PkSharedPointer.h>
#include <QExplicitlySharedDataPointer>

#include "kritaflake_export.h"
#include "SvgGradientHelper.h"
#include "SvgClipPathHelper.h"
#include "SvgLoadingContext.h"
#include "SvgStyleParser.h"
#include "KoClipMask.h"
#include <resources/KoSvgSymbolCollectionResource.h>
#include <KoID.h>

class KoShape;
class KoShapeGroup;
class KoShapeContainer;
class KoDocumentResourceManager;
class KoVectorPatternBackground;
class KoMarker;
class KoPathShape;
class KoSvgTextShape;
class KoSvgTextLoader;
class PkXmlText;

class KRITAFLAKE_EXPORT SvgParser
{
    struct DeferredUseStore;

public:
    explicit SvgParser(KoDocumentResourceManager *documentResourceManager);
    virtual ~SvgParser();

    static PkXmlDocument createDocumentFromSvg(PkStream *device, PkString *errorMsg = 0, int *errorLine = 0, int *errorColumn = 0);
    static PkXmlDocument createDocumentFromSvg(const PkByteArray &data, PkString *errorMsg = 0, int *errorLine = 0, int *errorColumn = 0);
    static PkXmlDocument createDocumentFromSvg(const PkString &data, PkString *errorMsg = 0, int *errorLine = 0, int *errorColumn = 0);
    static PkXmlDocument createDocumentFromSvg(QXmlStreamReader reader, PkString *errorMsg = 0, int *errorLine = 0, int *errorColumn = 0);

    /// Parses a svg fragment, returning the list of top level child shapes
    PkList<KoShape*> parseSvg(const PkXmlElement &e, PkSizeF * fragmentSize = 0);

    /// Sets the initial xml base directory (the directory form where the file is read)
    void setXmlBaseDir(const PkString &baseDir);

    void setResolution(const PkRectF boundsInPixels, qreal pixelsPerInch);
    void setDefaultKraTextVersion(int version);

    // Set whether to always consider shapes without fill or stroke explicitly set as inherited.
    // By default, this is off, and the parser retrieves the fill from the graphicsContext.
    void setFillStrokeInheritByDefault(const bool enable);

    // Whether to try and resolve the text properties for toplevel shapes.
    // By default this is on, as there might be doc-wide css that needs to
    // be resolved. However, for the style properties resource we turn this
    // off.
    void setResolveTextPropertiesForTopLevel(const bool enable);

    /// Returns the list of all shapes of the svg document
    PkList<KoShape*> shapes() const;

    /// Takes the collection of symbols contained in the svg document. The parser will
    /// no longer know about the symbols.
    PkVector<KoSvgSymbol*> takeSymbols();

    PkString documentTitle() const;
    PkString documentDescription() const;

    
    typedef std::function<PkByteArray(const PkString&)> FileFetcherFunc;
    void setFileFetcher(FileFetcherFunc func);

    PkList<QExplicitlySharedDataPointer<KoMarker>> knownMarkers() const;

    void parseDefsElement(const PkXmlElement &e);
    KoShape* parseTextElement(const PkXmlElement &e, KoSvgTextShape *mergeIntoShape = 0);

    PkStringList warnings() const;

protected:

    /// Parses a group-like element element, saving all its topmost properties
    KoShape* parseGroup(const PkXmlElement &e, const PkXmlElement &overrideChildrenFrom = PkXmlElement(), bool createContext = true);

    /// Get the path for the gives textPath element.
    KoShape* getTextPath(const PkXmlElement &e, bool hideShapesFromDefs = true);

    /// parse children of a <text /> element into the root shape.
    void parseTextChildren(const PkXmlElement &e, KoSvgTextLoader &textLoader, bool hideShapesFromDefs = true);
    
    /// Parses a container element, returning a list of child shapes
    PkList<KoShape*> parseContainer(const PkXmlElement &);

    /// XXX
    PkList<KoShape*> parseSingleElement(const PkXmlElement &b, DeferredUseStore* deferredUseStore = 0);

    /// Parses a use element, returning a list of child shapes
    KoShape* parseUse(const PkXmlElement &, DeferredUseStore* deferredUseStore);

    KoShape* resolveUse(const PkXmlElement &e, const PkString& key);

    /// Parses a gradient element
    SvgGradientHelper *parseGradient(const PkXmlElement &);

    /// Parses mesh gradient element
    SvgGradientHelper* parseMeshGradient(const PkXmlElement&);
    
    /// Parses a single meshpatch and returns the pointer
    PkList<std::pair<PkString, PkColor>> parseMeshPatch(const PkXmlNode& meshpatch);

    /// Parses a pattern element
    PkSharedPointer<KoVectorPatternBackground> parsePattern(const PkXmlElement &e, const KoShape *__shape);

    /// Parses a filter element
    bool parseFilter(const PkXmlElement &, const PkXmlElement &referencedBy = PkXmlElement());

    /// Parses a clip path element
    bool parseClipPath(const PkXmlElement &);
    bool parseClipMask(const PkXmlElement &e);

    bool parseMarker(const PkXmlElement &e);

    bool parseSymbol(const PkXmlElement &e);

    /// This parses the SVG native title and desc elements and adds them into additional attributes.
    void parseMetadataApplyToShape(const PkXmlElement &e, KoShape *shape);

    /// parses a length attribute
    qreal parseUnit(const PkString &, bool horiz = false, bool vert = false, const PkRectF &bbox = PkRectF());

    /// parses a length attribute in x-direction
    qreal parseUnitX(const PkString &unit);

    /// parses a length attribute in y-direction
    qreal parseUnitY(const PkString &unit);

    /// parses a length attribute in xy-direction
    qreal parseUnitXY(const PkString &unit);

    /// parses a angular attribute values, result in radians
    qreal parseAngular(const PkString &unit);

    KoShape *createObjectDirect(const PkXmlElement &b);

    /// Creates an object from the given xml element
    KoShape * createObject(const PkXmlElement &, const SvgStyles &style = SvgStyles());

    /// Create path object from the given xml element
    KoShape * createPath(const PkXmlElement &);

    /// find gradient with given id in gradient map
    SvgGradientHelper* findGradient(const PkString &id);

    /// find pattern with given id in pattern map
    PkSharedPointer<KoVectorPatternBackground> findPattern(const PkString &id, const KoShape *shape);

    /// find clip path with given id in clip path map
    SvgClipPathHelper* findClipPath(const PkString &id);

    /// Adds list of shapes to the given group shape
    void addToGroup(PkList<KoShape*> shapes, KoShapeContainer *group);

    /// creates a shape from the given shape id
    KoShape * createShape(const PkString &shapeID);

    /// Creates shape from specified svg element
    KoShape * createShapeFromElement(const PkXmlElement &element, SvgLoadingContext &context);

    /// Creates a shape from a CSS shapes definition.
    KoShape * createShapeFromCSS(const PkXmlElement e, const PkString value, SvgLoadingContext &context, bool hideShapesFromDefs = true);

    /// Create a list of shapes from a CSS shapes definition with potentially multiple shapes.
    PkList<KoShape*> createListOfShapesFromCSS(const PkXmlElement e, const PkString value, SvgLoadingContext &context, bool hideShapesFromDefs = true);

    /// Builds the document from the given shapes list
    void buildDocument(PkList<KoShape*> shapes);

    void uploadStyleToContext(const PkXmlElement &e);
    void applyCurrentStyle(KoShape *shape, const PkPointF &shapeToOriginalUserCoordinates);
    void applyCurrentBasicStyle(KoShape *shape);

    /// Applies styles to the given shape
    void applyStyle(KoShape *, const PkXmlElement &, const PkPointF &shapeToOriginalUserCoordinates);

    /// Applies styles to the given shape
    void applyStyle(KoShape *, const SvgStyles &, const PkPointF &shapeToOriginalUserCoordinates);

    /// Applies the current fill style to the object
    void applyFillStyle(KoShape * shape);

    /// Applies the current stroke style to the object
    void applyStrokeStyle(KoShape * shape);

    /// Applies the current clip path to the object
    void applyClipping(KoShape *shape, const PkPointF &shapeToOriginalUserCoordinates);
    void applyMaskClipping(KoShape *shape, const PkPointF &shapeToOriginalUserCoordinates);
    void applyMarkers(KoPathShape *shape);

    void applyPaintOrder(KoShape *shape);

    /// Applies id to specified shape
    void applyId(const PkString &id, KoShape *shape);

    /// Applies viewBox transformation to the current graphical context
    /// NOTE: after applying the function currentBoundingBox can become null!
    void applyViewBoxTransform(const PkXmlElement &element);

    PkXmlText getTheOnlyTextChild(const PkXmlElement &e);

    /// Check whether the shapes are in the defs of the SVG document.
    bool shapeInDefs(const KoShape *shape);

private:
    SvgLoadingContext m_context;
    PkMap<PkString, SvgGradientHelper> m_gradients;
    PkMap<PkString, SvgClipPathHelper> m_clipPaths;
    PkMap<PkString, PkSharedPointer<KoClipMask>> m_clipMasks;
    PkMap<PkString, QExplicitlySharedDataPointer<KoMarker>> m_markers;
    KoDocumentResourceManager *m_documentResourceManager;
    PkList<KoShape*> m_shapes;
    PkMap<PkString, KoSvgSymbol*> m_symbols;
    PkList<KoShape*> m_defsShapes;
    bool m_isInsideTextSubtree = false;
    PkString m_documentTitle;
    PkString m_documentDescription;
    PkVector<KoID> m_warnings;
    PkMap<KoShape *, PkTransform> m_shapeParentTransform;
    bool m_inheritStrokeFillByDefault = false;
    bool m_resolveTextPropertiesForTopLevel = true;
};

#endif
