/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2011 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include "SvgSavingContext.h"
#include "SvgUtil.h"

#include <KoXmlWriter.h>
#include <KoShape.h>
#include <KoShapeGroup.h>
#include <KoShapeLayer.h>

#include <QTemporaryFile>

#include <PkImage.h>
#include <PkTransform.h>
#include <PkMemoryStream.h>
#include <PkHash.h>
#include <PkFileStream.h>
#include <QFileInfo>
#include <KisMimeDatabase.h>
// [migrate] missing include for Pk/Qt type
#include <PkScopedPointer.h>

class Q_DECL_HIDDEN SvgSavingContext::Private
{
public:
    Private(PkStream *_mainDevice, PkStream *_styleDevice)
        : mainDevice(_mainDevice)
        , styleDevice(_styleDevice)
        , styleWriter(0)
        , shapeWriter(0)
        , saveInlineImages(true)
    {
        styleBuffer.open(PkStream::WriteOnly);
        styleWriter.reset(new KoXmlWriter(&styleBuffer, 1));
        styleWriter->startElement("defs");
        shapeBuffer.open(PkStream::WriteOnly);
        shapeWriter.reset(new KoXmlWriter(&shapeBuffer, 1));

        const qreal scaleToUserSpace = SvgUtil::toUserSpace(1.0);
        userSpaceMatrix.scale(scaleToUserSpace, scaleToUserSpace);
    }

    ~Private()
    {
    }

    PkStream *mainDevice;
    PkStream *styleDevice;
    PkMemoryStream styleBuffer;
    PkMemoryStream shapeBuffer;
    PkScopedPointer<KoXmlWriter> styleWriter;
    PkScopedPointer<KoXmlWriter> shapeWriter;

    PkHash<PkString, int> uniqueNames;
    PkHash<const KoShape*, PkString> shapeIds;
    PkTransform userSpaceMatrix;
    bool saveInlineImages;
    bool strippedTextMode = false;
};

SvgSavingContext::SvgSavingContext(PkStream &outputDevice, bool saveInlineImages)
    : d(new Private(&outputDevice, 0))
{
    d->saveInlineImages = saveInlineImages;
}

SvgSavingContext::SvgSavingContext(PkStream &shapesDevice, PkStream &styleDevice, bool saveInlineImages)
    : d(new Private(&shapesDevice, &styleDevice))
{
    d->saveInlineImages = saveInlineImages;
}

SvgSavingContext::~SvgSavingContext()
{
    d->styleWriter->endElement();

    if (d->styleDevice) {
        d->styleDevice->write(d->styleBuffer.data(), d->styleBuffer.size());
    } else {
        d->mainDevice->write(d->styleBuffer.data(), d->styleBuffer.size());
        d->mainDevice->write("\n", 1);
    }

    d->mainDevice->write(d->shapeBuffer.data(), d->shapeBuffer.size());

    delete d;
}

KoXmlWriter &SvgSavingContext::styleWriter()
{
    return *d->styleWriter;
}

KoXmlWriter &SvgSavingContext::shapeWriter()
{
    return *d->shapeWriter;
}

PkString SvgSavingContext::createUID(const PkString &base)
{
    PkString idBase = base.isEmpty() ? "defitem" : base;
    int counter = d->uniqueNames.value(idBase);
    PkString res;
    do {
        res = idBase + PkString::number(counter);
        counter++;
    } while (d->uniqueNames.contains(res));

    d->uniqueNames.insert(idBase, counter);
    d->uniqueNames.insert(res, 1);
    return res;
}

PkString SvgSavingContext::getID(const KoShape *obj)
{
    PkString id;
    // do we have already an id for this object ?
    if (d->shapeIds.contains(obj)) {
        // use existing id
        id = d->shapeIds[obj];
    } else {
        // initialize from object name
        id = obj->name();
        // if object name is not empty and was not used already
        // we can use it as is
        if (!id.isEmpty() && !d->uniqueNames.contains(id)) {
            // add to unique names so it does not get reused
            d->uniqueNames.insert(id, 1);
        } else {
            if (id.isEmpty()) {
                // differentiate a little between shape types
                if (dynamic_cast<const KoShapeGroup*>(obj))
                    id = "group";
                else if (dynamic_cast<const KoShapeLayer*>(obj))
                    id = "layer";
                else
                    id = "shape";
            }
            // create a completely new id based on object name
            // or a generic name
            id = createUID(id);
        }
        // record id for this shape
        d->shapeIds.insert(obj, id);
    }
    return id;
}

PkTransform SvgSavingContext::userSpaceTransform() const
{
    return d->userSpaceMatrix;
}

bool SvgSavingContext::isSavingInlineImages() const
{
    return d->saveInlineImages;
}

PkString SvgSavingContext::createFileName(const PkString &extension)
{
    PkFileStream *file = dynamic_cast<PkFileStream*>(d->mainDevice);
    if (!file)
        return PkString();

    QFileInfo fi(toQString(file->fileName()));
    PkString path = toPkString(fi.absolutePath());
    PkString dstBaseFilename = toPkString(fi.completeBaseName());

    // create a filename for the image file at the destination directory
    PkString fname = dstBaseFilename + PkString("_") + createUID("file");

    // check if file exists already
    int i = 0;
    PkString counter;
    // change filename as long as the filename already exists
    while (QFileInfo(toQString(path + fname + counter + extension)).exists()) {
        counter = PkString("_%1").arg(++i);
    }

    return fname + counter + extension;
}

PkString SvgSavingContext::saveImage(const PkImage &image)
{
    if (isSavingInlineImages()) {
        QBuffer buffer;
        buffer.open(QIODevice::WriteOnly);
        if (toQImage(image).save(&buffer, "PNG")) {
            const PkString header("data:image/x-png;base64,");
            return header + PkString(buffer.data().toBase64().constData());
        }
    } else {
        // write to a temp file first
        QTemporaryFile imgFile;
        if (toQImage(image).save(&imgFile, "PNG")) {
            PkString dstFilename = createFileName(".png");
            if (QFile::copy(toQString(imgFile.fileName()), toQString(dstFilename))) {
                return dstFilename;
            }
            else {
                QFile::remove(toQString(imgFile.fileName()));
            }
        }
    }

    return PkString();
}

void SvgSavingContext::setStrippedTextMode(bool value)
{
    d->strippedTextMode = value;
}

bool SvgSavingContext::strippedTextMode() const
{
    return d->strippedTextMode;
}
