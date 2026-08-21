/*
 *  SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "TestConvolutionOpImpl.h"

#include <simpletest.h>

#include "../KoColorSpaceAbstract.h"
#include "../KoColorSpaceTraits.h"
#include "../DebugPigment.h"

void TestConvolutionOpImpl::testConvolutionOpImpl()
{
    KoConvolutionOpImpl<KoBgrU16Traits> op;
    quint8** colors = new quint8*[3];
    colors[0] = new quint8[KoBgrU16Traits::pixelSize];
    ((quint16*)colors[0])[0] = 100;
    ((quint16*)colors[0])[1] = 200;
    ((quint16*)colors[0])[2] = 300;
    ((quint16*)colors[0])[3] = 0xFFFF;
    colors[1] = new quint8[KoBgrU16Traits::pixelSize];
    ((quint16*)colors[1])[0] = 50;
    ((quint16*)colors[1])[1] = 150;
    ((quint16*)colors[1])[2] = 0;
    ((quint16*)colors[1])[3] = 0xFFFF;
    colors[2] = new quint8[KoBgrU16Traits::pixelSize];
    ((quint16*)colors[2])[0] = 100;
    ((quint16*)colors[2])[1] = 300;
    ((quint16*)colors[2])[2] = 50;
    ((quint16*)colors[2])[3] = 0xFFFF;
    quint8* dst = new quint8[KoBgrU16Traits::pixelSize];
    quint16* dst16 = (quint16*)dst;
    {
        memcpy(dst16, colors[0], KoBgrU16Traits::pixelSize);
        PK_VERIFY2(dst16[0] == 100, PkString("%1 100").arg(dst16[0]).PkToUtf8());
        PK_VERIFY2(dst16[1] == 200, PkString("%1 200").arg(dst16[1]).PkToUtf8());
        PK_VERIFY2(dst16[2] == 300, PkString("%1 300").arg(dst16[2]).PkToUtf8());
        PK_VERIFY2(dst16[3] == 0xFFFF, PkString("%1 0xFFFF").arg(dst16[3]).PkToUtf8());
        memcpy(dst16, colors[1], KoBgrU16Traits::pixelSize);
        PK_VERIFY2(dst16[0] == 50, PkString("%1 50").arg(dst16[0]).PkToUtf8());
        PK_VERIFY2(dst16[1] == 150, PkString("%1 150").arg(dst16[1]).PkToUtf8());
        PK_VERIFY2(dst16[2] == 0, PkString("%1 0").arg(dst16[2]).PkToUtf8());
        PK_VERIFY2(dst16[3] == 0xFFFF, PkString("%1 0xFFFF").arg(dst16[3]).PkToUtf8());
        memcpy(dst16, colors[2], KoBgrU16Traits::pixelSize);
        PK_VERIFY2(dst16[0] == 100, PkString("%1 100").arg(dst16[0]).PkToUtf8());
        PK_VERIFY2(dst16[1] == 300, PkString("%1 300").arg(dst16[1]).PkToUtf8());
        PK_VERIFY2(dst16[2] == 50, PkString("%1 50").arg(dst16[2]).PkToUtf8());
        PK_VERIFY2(dst16[3] == 0xFFFF, PkString("%1 0xFFFF").arg(dst16[3]).PkToUtf8());
    }

    memset(dst16, 0, KoBgrU16Traits::pixelSize);

    // Tests for Case A)

    {
        qreal kernelValues[] = { 1, 1, 1};
        op.convolveColors(colors, kernelValues, dst, 1, 0, 3, PkBitArray());
        dbgPigment << dst16[0] << " " << dst16[1] << " " << dst16[2] << " " << PkBitArray().isEmpty();
        PK_VERIFY2(dst16[0] == 250, PkString("%1 250").arg(dst16[0]).PkToUtf8());
        PK_VERIFY2(dst16[1] == 650, PkString("%1 650").arg(dst16[1]).PkToUtf8());
        PK_VERIFY2(dst16[2] == 350, PkString("%1 350").arg(dst16[2]).PkToUtf8());
        PK_VERIFY2(dst16[3] == 0xFFFF, PkString("%1 0xFFFF").arg(dst16[3]).PkToUtf8());
        op.convolveColors(colors, kernelValues, dst, 3, 0, 3, PkBitArray());
        PK_VERIFY2(dst16[0] == 83, PkString("%1 83").arg(dst16[0]).PkToUtf8());
        PK_VERIFY2(dst16[1] == 216, PkString("%1 216").arg(dst16[1]).PkToUtf8());
        PK_VERIFY2(dst16[2] == 116, PkString("%1 116").arg(dst16[2]).PkToUtf8());
        PK_VERIFY2(dst16[3] == 0xFFFF, PkString("%1 0xFFFF").arg(dst16[3]).PkToUtf8());
    }
    {
        qreal kernelValues[] = { -1, 1, -1};
        op.convolveColors(colors, kernelValues, dst, 1, 0, 3, PkBitArray());
        PK_VERIFY2(dst16[0] == 0, PkString("%1 0").arg(dst16[0]).PkToUtf8());
        PK_VERIFY2(dst16[1] == 0, PkString("%1 0").arg(dst16[1]).PkToUtf8());
        PK_VERIFY2(dst16[2] == 0, PkString("%1 0").arg(dst16[2]).PkToUtf8());
        PK_VERIFY2(dst16[3] == 0, PkString("%1 0").arg(dst16[3]).PkToUtf8());
    }
    {
        qreal kernelValues[] = { 1, 2, 1};
        op.convolveColors(colors, kernelValues, dst, 1, 0, 3, PkBitArray());
        PK_VERIFY2(dst16[0] == 300, PkString("%1 300").arg(dst16[0]).PkToUtf8());
        PK_VERIFY2(dst16[1] == 800, PkString("%1 800").arg(dst16[1]).PkToUtf8());
        PK_VERIFY2(dst16[2] == 350, PkString("%1 350").arg(dst16[2]).PkToUtf8());
        PK_VERIFY2(dst16[3] == 0xFFFF, PkString("%1 0xFFFF").arg(dst16[3]).PkToUtf8());
    }
    {
        qreal kernelValues[] = { 1, -1, 1};
        op.convolveColors(colors, kernelValues, dst, 1, 0, 3, PkBitArray());
        PK_VERIFY2(dst16[0] == 150, PkString("%1 150").arg(dst16[0]).PkToUtf8());
        PK_VERIFY2(dst16[1] == 350, PkString("%1 350").arg(dst16[1]).PkToUtf8());
        PK_VERIFY2(dst16[2] == 350, PkString("%1 350").arg(dst16[2]).PkToUtf8());
        PK_VERIFY2(dst16[3] == 0xFFFF, PkString("%1 0xFFFF").arg(dst16[3]).PkToUtf8());
    }
    {
        qreal kernelValues[] = { 1, -1, 1};
        op.convolveColors(colors, kernelValues, dst, 1, 100, 3, PkBitArray());
        PK_VERIFY2(dst16[0] == 250, PkString("%1 250").arg(dst16[0]).PkToUtf8());
        PK_VERIFY2(dst16[1] == 450, PkString("%1 450").arg(dst16[1]).PkToUtf8());
        PK_VERIFY2(dst16[2] == 450, PkString("%1 450").arg(dst16[2]).PkToUtf8());
        PK_VERIFY2(dst16[3] == 0xFFFF, PkString("%1 0xFFFF").arg(dst16[3]).PkToUtf8());
        op.convolveColors(colors, kernelValues, dst, 1, -100, 3, PkBitArray());
        PK_VERIFY2(dst16[0] == 50, PkString("%1 50").arg(dst16[0]).PkToUtf8());
        PK_VERIFY2(dst16[1] == 250, PkString("%1 250").arg(dst16[1]).PkToUtf8());
        PK_VERIFY2(dst16[2] == 250, PkString("%1 250").arg(dst16[2]).PkToUtf8());
        PK_VERIFY2(dst16[3] == 0xFF9B, PkString("%1 0xFF9B").arg(dst16[3]).PkToUtf8());
    }
}

void TestConvolutionOpImpl::testOneSemiTransparent()
{
    KoConvolutionOpImpl<KoBgrU16Traits> op;
    quint8** colors = new quint8*[3];
    colors[0] = new quint8[KoBgrU16Traits::pixelSize];
    ((quint16*)colors[0])[0] = 100;
    ((quint16*)colors[0])[1] = 200;
    ((quint16*)colors[0])[2] = 300;

    ((quint16*)colors[0])[3] = 0x00FF;

    colors[1] = new quint8[KoBgrU16Traits::pixelSize];
    ((quint16*)colors[1])[0] = 50;
    ((quint16*)colors[1])[1] = 150;
    ((quint16*)colors[1])[2] = 0;
    ((quint16*)colors[1])[3] = 0xFFFF;
    colors[2] = new quint8[KoBgrU16Traits::pixelSize];
    ((quint16*)colors[2])[0] = 100;
    ((quint16*)colors[2])[1] = 300;
    ((quint16*)colors[2])[2] = 50;
    ((quint16*)colors[2])[3] = 0xFFFF;
    quint8* dst = new quint8[KoBgrU16Traits::pixelSize];
    quint16* dst16 = (quint16*)dst;
    memset(dst16, 0, KoBgrU16Traits::pixelSize);

    {
        // Tests for Case A)

        qreal kernelValues[] = { 1, 1, 1};
        op.convolveColors(colors, kernelValues, dst, 1, 0, 3, PkBitArray());
        PK_VERIFY2(dst16[0] == 250, PkString("%1 250").arg(dst16[0]).PkToUtf8());
        PK_VERIFY2(dst16[1] == 650, PkString("%1 650").arg(dst16[1]).PkToUtf8());
        PK_VERIFY2(dst16[2] == 350, PkString("%1 350").arg(dst16[2]).PkToUtf8());

        PK_VERIFY2(dst16[3] == 0xFFFF, PkString("%1 0xFFFF").arg(dst16[3]).PkToUtf8());

        op.convolveColors(colors, kernelValues, dst, 3, 0, 3, PkBitArray());
        PK_VERIFY2(dst16[0] == 83, PkString("%1 83").arg(dst16[0]).PkToUtf8());
        PK_VERIFY2(dst16[1] == 216, PkString("%1 216").arg(dst16[1]).PkToUtf8());
        PK_VERIFY2(dst16[2] == 116, PkString("%1 116").arg(dst16[2]).PkToUtf8());

        PK_VERIFY2(dst16[3] == 0xAAFF, PkString("%1 0xAAFF").arg(dst16[3]).PkToUtf8());
    }
}

void TestConvolutionOpImpl::testOneFullyTransparent()
{
    KoConvolutionOpImpl<KoBgrU16Traits> op;
    quint8** colors = new quint8*[3];
    colors[0] = new quint8[KoBgrU16Traits::pixelSize];
    ((quint16*)colors[0])[0] = 100;
    ((quint16*)colors[0])[1] = 200;
    ((quint16*)colors[0])[2] = 300;

    ((quint16*)colors[0])[3] = 0x0000;

    colors[1] = new quint8[KoBgrU16Traits::pixelSize];
    ((quint16*)colors[1])[0] = 50;
    ((quint16*)colors[1])[1] = 150;
    ((quint16*)colors[1])[2] = 0;
    ((quint16*)colors[1])[3] = 0xFFFF;
    colors[2] = new quint8[KoBgrU16Traits::pixelSize];
    ((quint16*)colors[2])[0] = 100;
    ((quint16*)colors[2])[1] = 300;
    ((quint16*)colors[2])[2] = 50;
    ((quint16*)colors[2])[3] = 0xFFFF;
    quint8* dst = new quint8[KoBgrU16Traits::pixelSize];
    quint16* dst16 = (quint16*)dst;
    memset(dst16, 0, KoBgrU16Traits::pixelSize);

    {
        qreal kernelValues[] = { 1, 1, 1};

        // Test for Case C)
        op.convolveColors(colors, kernelValues, dst, 1, 0, 3, PkBitArray());
        PK_VERIFY2(dst16[0] == 225, PkString("%1 225").arg(dst16[0]).PkToUtf8());
        PK_VERIFY2(dst16[1] == 675, PkString("%1 675").arg(dst16[1]).PkToUtf8());
        PK_VERIFY2(dst16[2] ==  75, PkString("%1  75").arg(dst16[2]).PkToUtf8());
        PK_VERIFY2(dst16[3] == 0xFFFF, PkString("%1 0xFFFF").arg(dst16[3]).PkToUtf8());

        // Test for Case B)
        op.convolveColors(colors, kernelValues, dst, 3, 0, 3, PkBitArray());
        PK_VERIFY2(dst16[0] == 75, PkString("%1 75").arg(dst16[0]).PkToUtf8());
        PK_VERIFY2(dst16[1] == 225, PkString("%1 225").arg(dst16[1]).PkToUtf8());
        PK_VERIFY2(dst16[2] == 25, PkString("%1 25").arg(dst16[2]).PkToUtf8());
        PK_VERIFY2(dst16[3] == 0xAAAA, PkString("%1 0xAAAA").arg(dst16[3]).PkToUtf8());

        // Test for Case C)
        op.convolveColors(colors, kernelValues, dst, 15, 0, 3, PkBitArray());
        PK_VERIFY2(dst16[0] == 15, PkString("%1 15").arg(dst16[0]).PkToUtf8());
        PK_VERIFY2(dst16[1] == 45, PkString("%1 45").arg(dst16[1]).PkToUtf8());
        PK_VERIFY2(dst16[2] ==  5, PkString("%1  5").arg(dst16[2]).PkToUtf8());
        PK_VERIFY2(dst16[3] == 0x2222, PkString("%1 0x2222").arg(dst16[3]).PkToUtf8());
    }
}


PK_TEST_GUILESS_MAIN(TestConvolutionOpImpl)
