/*
 *  SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <PkXmlCompat.h>

#include "KoColorTransformation.h"

KoColorTransformation::~KoColorTransformation()
{
}

PkList<PkString> KoColorTransformation::parameters() const
{
    return PkList<PkString>();
}

int KoColorTransformation::parameterId(const PkString& name) const
{
    Q_UNUSED(name);
    qFatal("No parameter for this transformation");
    return -1;
}

void KoColorTransformation::setParameter(int id, const PkVariant& parameter)
{
    Q_UNUSED(id);
    Q_UNUSED(parameter);
    qFatal("No parameter for this transformation");
}

void KoColorTransformation::setParameters(const PkHash<PkString, PkVariant> & parameters)
{
    for (PkHash<PkString, PkVariant>::const_iterator it = parameters.begin(); it != parameters.end(); ++it) {
        setParameter( parameterId(it.key()), it.value());
    }

}
