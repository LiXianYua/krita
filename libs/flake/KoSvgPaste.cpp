/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include "KoSvgPaste.h"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>

#include <SvgParser.h>
#include <KoDocumentResourceManager.h>
#include <FlakeDebug.h>
#include <PkRect.h>
#include <KoMarker.h>

class KoSvgPaste::Private
{
public:
    Private()
        : mimeData(QApplication::clipboard()->mimeData())
        {

        }

    const QMimeData *mimeData;
};

KoSvgPaste::KoSvgPaste()
    : d(new Private)
{
}

KoSvgPaste::~KoSvgPaste()
{
    delete(d);
}

bool KoSvgPaste::hasShapes()
{
    bool hasSvg = false;
    if (d->mimeData) {
        Q_FOREACH(const PkString &format, d->mimeData->formats()) {
            if (format.toLower().contains("svg")) {
                hasSvg = true;
                break;
            }
        }
    }

    return hasSvg;
}

PkList<KoShape*> KoSvgPaste::fetchShapes(const PkRectF viewportInPx, qreal resolutionPPI, PkSizeF *fragmentSize)
{
    PkList<KoShape*> shapes;

    if (!d->mimeData) return shapes;

    PkByteArray data;

    Q_FOREACH(const PkString &format, d->mimeData->formats()) {
        if (format.toLower().contains("svg")) {
            data = d->mimeData->data(format);
            break;
        }
    }

    if (data.isEmpty()) {
        return shapes;
    }

    return fetchShapesFromData(data, viewportInPx, resolutionPPI, fragmentSize);

}

PkList<KoShape*> KoSvgPaste::fetchShapesFromData(const PkByteArray &data, const PkRectF viewportInPx, qreal resolutionPPI, PkSizeF *fragmentSize)
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
