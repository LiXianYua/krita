/*
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <simpletest.h>
#include "TestKoColorSpaceMaths.h"
#include "KoIntegerMaths.h"
#include "KoColorSpaceMaths.h"


void TestKoColorSpaceMaths::testColorSpaceMathsTraits()
{
    PK_COMPARE(KoColorSpaceMathsTraits<quint8>::channelValueType, KoChannelInfo::UINT8);
    PK_COMPARE(KoColorSpaceMathsTraits<quint16>::channelValueType, KoChannelInfo::UINT16);
    PK_COMPARE(KoColorSpaceMathsTraits<qint16>::channelValueType, KoChannelInfo::INT16);
    PK_COMPARE(KoColorSpaceMathsTraits<quint32>::channelValueType, KoChannelInfo::UINT32);
    PK_COMPARE(KoColorSpaceMathsTraits<float>::channelValueType, KoChannelInfo::FLOAT32);
#ifdef HAVE_OPENEXR
    PK_COMPARE(KoColorSpaceMathsTraits<half>::channelValueType, KoChannelInfo::FLOAT16);
#endif
}

void TestKoColorSpaceMaths::testScaleToA()
{
    for (int i = 0; i < 256; ++i) {
        quint16 opacity = KoColorSpaceMaths<quint8, quint16 >::scaleToA(i);
        quint8 opacity8 = UINT16_TO_UINT8(opacity);
        PK_VERIFY(opacity8 == i);
    }
}

PK_TEST_GUILESS_MAIN(TestKoColorSpaceMaths)
