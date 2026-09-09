/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KoSvgPaste.h"

#include <SvgParser.h>
#include <KoDocumentResourceManager.h>
#include <FlakeDebug.h>
#include <PkRect.h>
#include <KoMarker.h>

class KoSvgPaste::Private
{
public:
    Private(const PkByteArray &data, bool hasData)
        : svgData(data)
        , hasSvgData(hasData)
    {
    }

    PkByteArray svgData;
    bool hasSvgData = false;
};

KoSvgPaste::KoSvgPaste(const PkByteArray &svgData, bool hasSvgData)
    : d(new Private(svgData, hasSvgData))
{
}

KoSvgPaste::~KoSvgPaste()
{
    delete(d);
}

bool KoSvgPaste::hasShapes() const
{
    return d->hasSvgData;
}

PkList<KoShape*> KoSvgPaste::fetchShapes(const PkRectF viewportInPx, double resolutionPPI, PkSizeF *fragmentSize)
{
    PkList<KoShape*> shapes;

    if (!d->hasSvgData || d->svgData.isEmpty()) {
        return shapes;
    }

    return fetchShapesFromData(d->svgData, viewportInPx, resolutionPPI, fragmentSize);

}

PkList<KoShape*> KoSvgPaste::fetchShapesFromData(const PkByteArray &data, const PkRectF viewportInPx, double resolutionPPI, PkSizeF *fragmentSize)
{
    PkList<KoShape*> shapes;

    if (data.isEmpty()) {
        return shapes;
    }



    PkString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    PkXmlDocument doc = SvgParser::createDocumentFromSvg(data, &errorMsg, &errorLine, &errorColumn);
    if (doc.isNull()) {
        qWarning() << "Failed to process an SVG file at"
                   << errorLine << ":" << errorColumn << "->" << errorMsg;
        return shapes;
    }

    KoDocumentResourceManager resourceManager;
    SvgParser parser(&resourceManager);
    parser.setResolution(viewportInPx, resolutionPPI);

    shapes = parser.parseSvg(doc.documentElement(), fragmentSize);

    return shapes;
}
