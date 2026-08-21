/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <PkXmlCompat.h>

#include "KoColorConversionTransformationFactory.h"

#include <PkString.h>

#include "KoColorProfile.h"
#include "KoColorSpace.h"
#include "DebugPigment.h"
#include "KoColorSpaceRegistry.h"

struct KoColorConversionTransformationFactory::Private {
    PkString srcModelId;
    PkString srcDepthId;
    PkString dstModelId;
    PkString dstDepthId;
    PkString srcProfile;
    PkString dstProfile;
};

KoColorConversionTransformationFactory::KoColorConversionTransformationFactory(const PkString &_srcModelId, const PkString &_srcDepthId, const PkString &_srcProfile, const PkString &_dstModelId, const PkString &_dstDepthId, const PkString &_dstProfile) : d(new Private)
{
    d->srcModelId = _srcModelId;
    d->srcDepthId = _srcDepthId;
    d->dstModelId = _dstModelId;
    d->dstDepthId = _dstDepthId;
    d->srcProfile = KoColorSpaceRegistry::instance()->profileAlias(_srcProfile);
    d->dstProfile = KoColorSpaceRegistry::instance()->profileAlias(_dstProfile);
}

KoColorConversionTransformationFactory::~KoColorConversionTransformationFactory()
{
    delete d;
}

bool KoColorConversionTransformationFactory::canBeSource(const KoColorSpace* srcCS) const
{
    return ((srcCS->colorModelId().id() == d->srcModelId)
            && (srcCS->colorDepthId().id() == d->srcDepthId)
            && (d->srcProfile == "" || srcCS->profile()->name() == d->srcProfile));
}

bool KoColorConversionTransformationFactory::canBeDestination(const KoColorSpace* dstCS) const
{
    dbgPigment << dstCS->colorModelId().id() << " " << d->dstModelId << " " << dstCS->colorDepthId().id() << " " <<  d->dstDepthId << " " << d->dstProfile << " " << (dstCS->profile() ? dstCS->profile()->name() : "noprofile")  << " " << d->dstProfile;
    return ((dstCS->colorModelId().id() == d->dstModelId)
            && (dstCS->colorDepthId().id() == d->dstDepthId)
            && (d->dstProfile == "" || dstCS->profile()->name() == d->dstProfile));
}

PkString KoColorConversionTransformationFactory::srcColorModelId() const
{
    return d->srcModelId;
}
PkString KoColorConversionTransformationFactory::srcColorDepthId() const
{
    return d->srcDepthId;
}

PkString KoColorConversionTransformationFactory::srcProfile() const
{
    return d->srcProfile;
}

PkString KoColorConversionTransformationFactory::dstColorModelId() const
{
    return d->dstModelId;
}
PkString KoColorConversionTransformationFactory::dstColorDepthId() const
{
    return d->dstDepthId;
}

PkString KoColorConversionTransformationFactory::dstProfile() const
{
    return d->dstProfile;
}

