/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include "HtmlSavingContext.h"
#include <KoXmlWriter.h>
#include <KoShape.h>
#include <PkMemoryStream.h>

struct HtmlSavingContext::Private {

    Private(PkStream *_shapeDevice)
        : shapeDevice(_shapeDevice)
        , shapeWriter(0)
    {
        shapeBufferStream.attach(&shapeBuffer);
        shapeWriter.reset(new KoXmlWriter(&shapeBufferStream, 1));
    }

    PkStream *shapeDevice;
    PkMemoryStream shapeBuffer;
    PkDeviceStream shapeBufferStream;
    PkScopedPointer<KoXmlWriter> shapeWriter;
};

HtmlSavingContext::HtmlSavingContext(PkStream &shapeDevice)
    : d(new Private(&shapeDevice))
{
}

HtmlSavingContext::~HtmlSavingContext()
{
    d->shapeDevice->write(d->shapeBuffer.data());
}

KoXmlWriter &HtmlSavingContext::shapeWriter()
{
    return *d->shapeWriter;
}
