/*
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "TestKoColorSpaceSanity.h"

#include <simpletest.h>
#include <KoColorSpaceRegistry.h>
#include <KoChannelInfo.h>

#include <kistest.h>

void TestKoColorSpaceSanity::testChannelsInfo()
{
    Q_FOREACH (const KoColorSpace* colorSpace, KoColorSpaceRegistry::instance()->allColorSpaces(KoColorSpaceRegistry::AllColorSpaces, KoColorSpaceRegistry::OnlyDefaultProfile))
    {

        PK_COMPARE(colorSpace->channelCount(), quint32(colorSpace->channels().size()));
        PkList<int> displayPositions;
        quint32 colorChannels = 0;
        quint32 size = 0;
        Q_FOREACH (KoChannelInfo* info, colorSpace->channels())
        {
            if(info->channelType() == KoChannelInfo::COLOR ) {
                ++colorChannels;
            }
            // Check poses
            qint32 pos = info->pos();
            PK_VERIFY(pos + info->size() <= (qint32)colorSpace->pixelSize());
            Q_FOREACH (KoChannelInfo* info2, colorSpace->channels())
            {
                if( info != info2 )
                {
                    PK_VERIFY( pos >= (info2->pos() + info2->size()) || pos + info->size() <= info2->pos());
                }
            }

            // Check displayPosition
            quint32 displayPosition = info->displayPosition();
            PK_VERIFY(displayPosition < colorSpace->channelCount());
            PK_VERIFY(displayPositions.indexOf(displayPosition) == -1);
            displayPositions.push_back(displayPosition);

            size += info->size();
        }
        PK_COMPARE(size, colorSpace->pixelSize());
        PK_COMPARE(colorSpace->colorChannelCount(), colorChannels);
    }
}




void TestKoColorSpaceSanity::testIterator()
{

}

KISTEST_MAIN(TestKoColorSpaceSanity)
