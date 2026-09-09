/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "HtmlSavingContext.h"
#include <KoXmlWriter.h>
#include <KoShape.h>
#include <PkMemoryStream.h>

struct HtmlSavingContext::Private {

    Private(PkStream *_shapeDevice)
        : shapeDevice(_shapeDevice)
        , shapeWriter(0)
    {
        shapeBuffer.open(PkStream::WriteOnly);
        shapeWriter.reset(new KoXmlWriter(&shapeBuffer, 1));
    }

    PkStream *shapeDevice;
    PkMemoryStream shapeBuffer;
    PkScopedPointer<KoXmlWriter> shapeWriter;
};

HtmlSavingContext::HtmlSavingContext(PkStream &shapeDevice)
    : d(new Private(&shapeDevice))
{
}

HtmlSavingContext::~HtmlSavingContext()
{
    d->shapeDevice->write(d->shapeBuffer.data(), d->shapeBuffer.size());
}

KoXmlWriter &HtmlSavingContext::shapeWriter()
{
    return *d->shapeWriter;
}
