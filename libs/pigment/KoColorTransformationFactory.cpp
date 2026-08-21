/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <PkXmlCompat.h>

#include "KoColorTransformationFactory.h"

struct Q_DECL_HIDDEN KoColorTransformationFactory::Private {
    PkString id;
};

KoColorTransformationFactory::KoColorTransformationFactory(const PkString &id)
    : d(new Private)
{
    d->id = id;
}

KoColorTransformationFactory::~KoColorTransformationFactory()
{
    delete d;
}

PkString KoColorTransformationFactory::id() const
{
    return d->id;
}
