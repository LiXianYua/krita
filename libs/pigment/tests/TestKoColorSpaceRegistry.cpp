/*
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "TestKoColorSpaceRegistry.h"

#include <simpletest.h>

#include <KoColorSpaceRegistry.h>
#include <KoColorModelStandardIds.h>
#include <KoColorProfile.h>

TestBaseColorSpaceRegistry::TestBaseColorSpaceRegistry()
{
}

void TestBaseColorSpaceRegistry::testLab16()
{
    const KoColorSpace* cs = KoColorSpaceRegistry::instance()->lab16();
    PK_COMPARE(cs->colorModelId().id(), LABAColorModelID.id());
    PK_COMPARE(cs->colorDepthId().id(), Integer16BitsColorDepthID.id());
    PK_VERIFY(*cs == *KoColorSpaceRegistry::instance()->colorSpace(LABAColorModelID.id(), Integer16BitsColorDepthID.id(), 0));
}

void TestBaseColorSpaceRegistry::testRgb8()
{
    const KoColorSpace* cs = KoColorSpaceRegistry::instance()->rgb8();
    PK_COMPARE(cs->colorModelId().id(), RGBAColorModelID.id());
    PK_COMPARE(cs->colorDepthId().id(), Integer8BitsColorDepthID.id());
    PK_VERIFY(*cs == *KoColorSpaceRegistry::instance()->colorSpace(RGBAColorModelID.id(), Integer8BitsColorDepthID.id(), 0));
}

void TestBaseColorSpaceRegistry::testRgb16()
{
    const KoColorSpace* cs = KoColorSpaceRegistry::instance()->rgb16();
    PK_COMPARE(cs->colorModelId().id(), RGBAColorModelID.id());
    PK_COMPARE(cs->colorDepthId().id(), Integer16BitsColorDepthID.id());
    PK_VERIFY(*cs == *KoColorSpaceRegistry::instance()->colorSpace(RGBAColorModelID.id(), Integer16BitsColorDepthID.id(), 0));
}

void TestBaseColorSpaceRegistry::testProfileByUniqueId()
{
    const KoColorSpace* cs = KoColorSpaceRegistry::instance()->rgb16();
    const KoColorProfile *profile = cs->profile();
    PK_VERIFY(profile);

    const KoColorProfile *fetchedProfile =
        KoColorSpaceRegistry::instance()->profileByUniqueId(profile->uniqueId());

    PK_COMPARE(*fetchedProfile, *profile);
}

SIMPLE_TEST_MAIN(TestBaseColorSpaceRegistry)
