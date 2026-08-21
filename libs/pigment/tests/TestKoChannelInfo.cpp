/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "TestKoChannelInfo.h"

#include <simpletest.h>

#include <PkXmlElement.h>

#include "KoColorModelStandardIds.h"

#include "KoColor.h"
#include "KoChannelInfo.h"
#include "DebugPigment.h"

void TestKoChannelInfo::testDisplayPositionToChannelIndex()
{
    PkList<KoChannelInfo*> channels;
    channels << new KoChannelInfo("Blue" , 0, 2, KoChannelInfo::COLOR, KoChannelInfo::UINT8, 1, PkColor(0, 0, 255))
             << new KoChannelInfo("Green", 1, 1, KoChannelInfo::COLOR, KoChannelInfo::UINT8, 1, PkColor(0, 255, 0))
             << new KoChannelInfo("Red"  , 2, 0, KoChannelInfo::COLOR, KoChannelInfo::UINT8, 1, PkColor(255, 0, 0))
             << new KoChannelInfo("Alpha", 3, 3, KoChannelInfo::ALPHA, KoChannelInfo::UINT8);

    PK_COMPARE(KoChannelInfo::displayPositionToChannelIndex(0, channels), 2);
    PK_COMPARE(KoChannelInfo::displayPositionToChannelIndex(1, channels), 1);
    PK_COMPARE(KoChannelInfo::displayPositionToChannelIndex(2, channels), 0);
    PK_COMPARE(KoChannelInfo::displayPositionToChannelIndex(3, channels), 3);
}

void TestKoChannelInfo::testdisplayOrderSorted()
{
    PkList<KoChannelInfo*> channels;
    channels << new KoChannelInfo("Blue" , 0, 2, KoChannelInfo::COLOR, KoChannelInfo::UINT8, 1, PkColor(0, 0, 255))
             << new KoChannelInfo("Green", 1, 1, KoChannelInfo::COLOR, KoChannelInfo::UINT8, 1, PkColor(0, 255, 0))
             << new KoChannelInfo("Red"  , 2, 0, KoChannelInfo::COLOR, KoChannelInfo::UINT8, 1, PkColor(255, 0, 0))
             << new KoChannelInfo("Alpha", 3, 3, KoChannelInfo::ALPHA, KoChannelInfo::UINT8);

    PkList<KoChannelInfo*> sortedChannels = KoChannelInfo::displayOrderSorted(channels);
    PK_COMPARE(sortedChannels[0]->displayPosition(), 0);
    PK_COMPARE(sortedChannels[1]->displayPosition(), 1);
    PK_COMPARE(sortedChannels[2]->displayPosition(), 2);
    PK_COMPARE(sortedChannels[3]->displayPosition(), 3);
}

PK_TEST_GUILESS_MAIN(TestKoChannelInfo)
