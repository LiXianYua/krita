/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2007-2008 Thorsten Zachmann <zachmann@kde.org>
 * SPDX-FileCopyrightText: 2009 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoDrag.h"
#include <PkFlakeBridge.h>

#include <QApplication>
#include <PkMemoryStream.h>
#include <PkByteArray.h>
#include <QClipboard>
#include <QMimeData>
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
    KoDragPrivate() : mimeData(0) { }
    ~KoDragPrivate() { delete mimeData; }
    QMimeData *mimeData;
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
    QLatin1String mimeType("image/svg+xml");

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
    if (d->mimeData == 0) {
        d->mimeData = new QMimeData();
    }
    d->mimeData->setData(toQString(mimeType), QByteArray(data.data(), int(data.size())));
}

void KoDrag::addToClipboard()
{
    if (d->mimeData) {
        QApplication::clipboard()->setMimeData(d->mimeData);
        d->mimeData = 0;
    }
}

QMimeData * KoDrag::mimeData()
{
    QMimeData *mimeData = d->mimeData;
    d->mimeData = 0;
    return mimeData;
}
