/*
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <PkXmlCompat.h>

#include "KoColorSpaceFactory.h"

#include "DebugPigment.h"

#include <PkMutex.h>
#include <PkList.h>
#include <PkHash.h>
#include <PkString.h>

#include "KoColorProfile.h"
#include "KoColorSpace.h"
#include "KoColorSpaceRegistry.h"

#include "kis_assert.h"

struct KoColorSpaceFactory::Private {
    PkList<KoColorProfile*> colorprofiles;
    PkHash<PkString, KoColorSpace*> availableColorspaces;
    PkMutex mutex;
#ifndef NDEBUG
    PkHash<KoColorSpace*, PkString> stackInformation;
#endif
};

KoColorSpaceFactory::KoColorSpaceFactory() : d(new Private)
{
}

KoColorSpaceFactory::~KoColorSpaceFactory()
{
#ifndef NDEBUG
    // Check that all color spaces have been released
    int count = 0;
    count += d->availableColorspaces.size();

    for (PkHash<KoColorSpace*, PkString>::const_iterator it = d->stackInformation.constBegin();
        it != d->stackInformation.constEnd(); ++it)
    {
        errorPigment << "*******************************************";
        errorPigment << it.key()->id() << " still in used, and grabbed in: ";
        errorPigment << it.value();
    }
#endif
    for (KoColorProfile* profile : d->colorprofiles) {
        KoColorSpaceRegistry::instance()->removeProfile(profile);
        delete profile;
    }
    delete d;
}

const KoColorProfile *KoColorSpaceFactory::colorProfile(const PkByteArray &rawData, KoColorSpaceFactory::ProfileRegistrationInterface *registrationInterface) const
{
    KoColorProfile* colorProfile = createColorProfile(rawData);
    if (colorProfile && colorProfile->valid()) {
        if (const KoColorProfile* existingProfile = registrationInterface->profileByName(colorProfile->name())) {
            delete colorProfile;
            return existingProfile;
        }
        registrationInterface->registerNewProfile(colorProfile);
        d->colorprofiles.append(colorProfile);
    }
    return colorProfile;
}

const KoColorSpace *KoColorSpaceFactory::grabColorSpace(const KoColorProfile * profile)
{
    PkMutexLocker l(&d->mutex);
    Q_ASSERT(profile);
    auto it = d->availableColorspaces.find(profile->name());
    KoColorSpace* cs;

    if (it == d->availableColorspaces.end()) {
        cs = createColorSpace(profile);
        KIS_ASSERT_X(cs != nullptr, "KoColorSpaceFactory::grabColorSpace", "createColorSpace returned nullptr.");
        if (cs) {
            d->availableColorspaces[profile->name()] = cs;
        }
    }
    else {
        cs = it.value();
    }

    return cs;
}

