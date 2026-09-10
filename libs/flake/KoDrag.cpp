/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2007-2008 Thorsten Zachmann <zachmann@kde.org>
 * SPDX-FileCopyrightText: 2009 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoDrag.h"
#include <PkFlakeBridge.h>

#include <PkMemoryStream.h>
#include <PkByteArray.h>
#include <PkString.h>

#include <FlakeDebug.h>

#include <KoStore.h>
#include <KoXmlWriter.h>
#include "KoShapeSavingContext.h"

#include <KoShapeContainer.h>
#include <KoShape.h>

#include <PkRect.h>
#include <SvgWriter.h>


class KoDragPrivate {
public:
    PkClipboardData payload;
};

KoDrag::KoDrag()
    : d(new KoDragPrivate())
{
}

KoDrag::~KoDrag()
{
    delete d;
}

bool KoDrag::setSvg(const PkList<KoShape *> originalShapes)
{
    PkRectF boundingRect;
    PkList<KoShape*> shapes;

    Q_FOREACH (KoShape *shape, originalShapes) {
        boundingRect |= shape->boundingRect();

        KoShape *clonedShape = shape->cloneShapeAndBakeAbsoluteTransform();
        shapes.append(clonedShape);
    }

    std::sort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);

    PkMemoryStream buffer;
    const PkString mimeType("image/svg+xml");

    buffer.open(PkStream::WriteOnly);

    const PkSizeF pageSize(boundingRect.right(), boundingRect.bottom());
    SvgWriter writer(shapes);
    writer.save(buffer, pageSize);

    buffer.close();

    qDeleteAll(shapes);

    setData(PkString(mimeType), PkByteArray(buffer.data(), static_cast<int>(buffer.size())));
    return true;
}

void KoDrag::setData(const PkString &mimeType, const PkByteArray &data)
{
    // The payload models exactly the formats the clipboard contract carries.
    // A mime type outside that set has no slot here and is not representable.
    if (mimeType == PkString("image/svg+xml")) {
        d->payload.hasSvg = true;
        d->payload.svg = data;
    } else if (mimeType == PkString("text/html")) {
        d->payload.hasHtml = true;
        d->payload.html = PkString::fromUtf8(data);
    } else if (mimeType == PkString("text/plain")) {
        d->payload.hasText = true;
        d->payload.text = PkString::fromUtf8(data);
    }
}

PkClipboardData KoDrag::takeClipboardData()
{
    PkClipboardData payload = d->payload;
    d->payload = PkClipboardData();
    return payload;
}
