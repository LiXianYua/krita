/*
 * SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "psd_utils_test.h"

#include <PkAuxTypes.h>

#include <cmath>
#include <cstdlib>

#include <kis_debug.h>
#include <psd.h>
#include <psd_utils.h>

#include "cos/PkCosMemoryStream.h"

void PSDUtilsTest::test_psdwrite_quint8()
{
    {
        PkByteArray ba;
        PkCosMemoryStream buf(&ba);
        buf.open(PkStream::ReadWrite);
        const quint8 i = 3;
        PK_VERIFY(psdwrite<psd_byte_order::psdLittleEndian>(buf, i));
        PK_COMPARE(ba.size(), 1);
        PK_COMPARE(ba.constData()[0], '\3');
    }

    {
        PkByteArray ba;
        PkCosMemoryStream buf(&ba);
        buf.open(PkStream::ReadWrite);
        const quint8 i = 3;
        PK_VERIFY(psdwrite(buf, i));
        PK_COMPARE(ba.size(), 1);
        PK_COMPARE(ba.constData()[0], '\3');
    }
}

void PSDUtilsTest::test_psdwrite_quint16()
{
    {
        PkByteArray ba;
        PkCosMemoryStream buf(&ba);
        buf.open(PkStream::ReadWrite);
        const quint16 i = 0x3u;
        PK_VERIFY(psdwrite<psd_byte_order::psdLittleEndian>(buf, i));
        PK_COMPARE(ba.size(), 2);
        PK_COMPARE(ba.constData()[0], '\x03');
        PK_COMPARE(ba.constData()[1], '\x00');
    }

    {
        PkByteArray ba;
        PkCosMemoryStream buf(&ba);
        buf.open(PkStream::ReadWrite);
        const quint16 i = 0x3u;
        PK_VERIFY(psdwrite(buf, i));
        PK_COMPARE(ba.size(), 2);
        PK_COMPARE(ba.constData()[0], '\x00');
        PK_COMPARE(ba.constData()[1], '\x03');
    }
}

void PSDUtilsTest::test_psdwrite_quint32()
{
    {
        PkByteArray ba;
        PkCosMemoryStream buf(&ba);
        buf.open(PkStream::ReadWrite);
        const quint32 i = 0x64u;
        PK_VERIFY(psdwrite<psd_byte_order::psdLittleEndian>(buf, i));
        PK_COMPARE(ba.size(), 4);
        PK_COMPARE(ba.constData()[0], '\x64');
        PK_COMPARE(ba.constData()[1], '\x00');
        PK_COMPARE(ba.constData()[2], '\x00');
        PK_COMPARE(ba.constData()[3], '\x00');
    }

    {
        PkByteArray ba;
        PkCosMemoryStream buf(&ba);
        buf.open(PkStream::ReadWrite);
        const quint32 i = 0x64u;
        PK_VERIFY(psdwrite(buf, i));
        PK_COMPARE(ba.size(), 4);
        PK_COMPARE(ba.constData()[0], '\x00');
        PK_COMPARE(ba.constData()[1], '\x00');
        PK_COMPARE(ba.constData()[2], '\x00');
        PK_COMPARE(ba.constData()[3], '\x64');
    }
}

void PSDUtilsTest::test_psdwrite_qstring()
{
    PkByteArray ba;
    PkCosMemoryStream buf(&ba);
    buf.open(PkStream::ReadWrite);
    PkString s = "8BPS";
    PK_VERIFY(psdwrite(buf, s));
    PK_COMPARE(ba.size(), 4);
    PK_COMPARE(ba.constData()[0], '8');
    PK_COMPARE(ba.constData()[1], 'B');
    PK_COMPARE(ba.constData()[2], 'P');
    PK_COMPARE(ba.constData()[3], 'S');
}

void PSDUtilsTest::test_psdwrite_pascalstring()
{
    {
        PkByteArray ba;
        PkCosMemoryStream buf(&ba);
        buf.open(PkStream::ReadWrite);

        // test null string
        PkString s;
        PK_VERIFY(psdwrite_pascalstring<psd_byte_order::psdLittleEndian>(buf, s));
        PK_COMPARE(ba.size(), 2);
        PK_COMPARE(ba.constData()[0], '\0');
        PK_COMPARE(ba.constData()[1], '\0');

        buf.close();
        ba.resize(0);
        buf.open(PkStream::ReadWrite);
        buf.seek(0);

        // test even string
        s = PkString("bl");
        PK_VERIFY(psdwrite_pascalstring<psd_byte_order::psdLittleEndian>(buf, s));
        PK_COMPARE(ba.size(), 3);
        PK_COMPARE(ba.constData()[0], '\x02');
        PK_COMPARE(ba.constData()[1], 'b');
        PK_COMPARE(ba.constData()[2], 'l');

        buf.close();
        ba.resize(0);
        buf.open(PkStream::ReadWrite);
        buf.seek(0);

        // test uneven string
        s = PkString("bla");
        PK_VERIFY(psdwrite_pascalstring<psd_byte_order::psdLittleEndian>(buf, s));
        PK_COMPARE(ba.size(), 5);
        PK_COMPARE(ba.constData()[0], '\x03');
        PK_COMPARE(ba.constData()[1], 'b');
        PK_COMPARE(ba.constData()[2], 'l');
        PK_COMPARE(ba.constData()[3], 'a');
        PK_COMPARE(ba.constData()[4], '\0');
    }

    {
        PkByteArray ba;
        PkCosMemoryStream buf(&ba);
        buf.open(PkStream::ReadWrite);

        // test null string
        PkString s;
        PK_VERIFY(psdwrite_pascalstring(buf, s));
        PK_COMPARE(ba.size(), 2);
        PK_COMPARE(ba.constData()[0], '\0');
        PK_COMPARE(ba.constData()[1], '\0');

        buf.close();
        ba.resize(0);
        buf.open(PkStream::ReadWrite);
        buf.seek(0);

        // test even string
        s = PkString("bl");
        PK_VERIFY(psdwrite_pascalstring(buf, s));
        PK_COMPARE(ba.size(), 3);
        PK_COMPARE(ba.constData()[0], '\2');
        PK_COMPARE(ba.constData()[1], 'b');
        PK_COMPARE(ba.constData()[2], 'l');

        buf.close();
        ba.resize(0);
        buf.open(PkStream::ReadWrite);
        buf.seek(0);

        // test uneven string
        s = PkString("bla");
        PK_VERIFY(psdwrite_pascalstring(buf, s));
        PK_COMPARE(ba.size(), 5);
        PK_COMPARE(ba.constData()[0], '\3');
        PK_COMPARE(ba.constData()[1], 'b');
        PK_COMPARE(ba.constData()[2], 'l');
        PK_COMPARE(ba.constData()[3], 'a');
        PK_COMPARE(ba.constData()[4], '\0');
    }
}

void PSDUtilsTest::test_psdpad()
{
    PkByteArray ba;
    PkCosMemoryStream buf(&ba);
    buf.open(PkStream::ReadWrite);
    PK_VERIFY(psdpad(buf, 6));
    PK_COMPARE(ba.size(), 6);
    PK_COMPARE(ba.constData()[0], '\x00');
    PK_COMPARE(ba.constData()[1], '\x00');
    PK_COMPARE(ba.constData()[2], '\x00');
    PK_COMPARE(ba.constData()[3], '\x00');
    PK_COMPARE(ba.constData()[4], '\x00');
    PK_COMPARE(ba.constData()[5], '\x00');
}

void PSDUtilsTest::test_psdread_quint8()
{
    {
        PkByteArray ba;
        PkCosMemoryStream buf(&ba);
        buf.open(PkStream::ReadWrite);
        const quint8 s = 3;
        PK_VERIFY(psdwrite<psd_byte_order::psdLittleEndian>(buf, s));
        buf.close();
        buf.open(PkStream::ReadOnly);
        buf.seek(0);
        quint8 r;
        PK_VERIFY(psdread<psd_byte_order::psdLittleEndian>(buf, r));
        PK_COMPARE(r, s);
    }

    {
        PkByteArray ba;
        PkCosMemoryStream buf(&ba);
        buf.open(PkStream::ReadWrite);
        const quint8 s = 3;
        PK_VERIFY(psdwrite(buf, s));
        buf.close();
        buf.open(PkStream::ReadOnly);
        buf.seek(0);
        quint8 r;
        PK_VERIFY(psdread(buf, r));
        PK_COMPARE(r, s);
    }
}

void PSDUtilsTest::test_psdread_quint16()
{
    {
        PkByteArray ba;
        PkCosMemoryStream buf(&ba);
        buf.open(PkStream::ReadWrite);
        const quint16 s = 1024;
        PK_VERIFY(psdwrite<psd_byte_order::psdLittleEndian>(buf, s));
        buf.close();
        buf.open(PkStream::ReadOnly);
        buf.seek(0);
        quint16 r;
        PK_VERIFY(psdread<psd_byte_order::psdLittleEndian>(buf, r));
        PK_COMPARE(r, s);
    }

    {
        PkByteArray ba;
        PkCosMemoryStream buf(&ba);
        buf.open(PkStream::ReadWrite);
        quint16 s = 1024;
        PK_VERIFY(psdwrite(buf, s));
        buf.close();
        buf.open(PkStream::ReadOnly);
        buf.seek(0);
        quint16 r;
        PK_VERIFY(psdread(buf, r));
        PK_COMPARE(r, s);
    }
}

void PSDUtilsTest::test_psdread_quint32()
{
    {
        PkByteArray ba;
        PkCosMemoryStream buf(&ba);
        PK_VERIFY(buf.open(PkStream::ReadWrite));
        const quint32 s = 300000;
        PK_VERIFY(psdwrite<psd_byte_order::psdLittleEndian>(buf, s));
        buf.close();
        buf.open(PkStream::ReadOnly);
        buf.seek(0);
        quint32 r;
        PK_VERIFY(psdread<psd_byte_order::psdLittleEndian>(buf, r));
        PK_COMPARE(r, s);
    }

    {
        PkByteArray ba;
        PkCosMemoryStream buf(&ba);
        PK_VERIFY(buf.open(PkStream::ReadWrite));
        quint32 s = 300000;
        PK_VERIFY(psdwrite(buf, s));
        buf.close();
        buf.open(PkStream::ReadOnly);
        buf.seek(0);
        quint32 r;
        PK_VERIFY(psdread(buf, r));
        PK_COMPARE(r, s);
    }
}

void PSDUtilsTest::test_psdread_pascalstring()
{
    {
        PkByteArray ba;
        PkCosMemoryStream buf(&ba);

        PkString s;
        PkString r;

        // test null string
        buf.open(PkStream::ReadWrite);
        PK_VERIFY(psdwrite_pascalstring<psd_byte_order::psdLittleEndian>(buf, s));
        buf.close();
        buf.open(PkStream::ReadOnly);
        buf.seek(0);
        PK_VERIFY(psdread_pascalstring<psd_byte_order::psdLittleEndian>(buf, r, 2));
        PK_COMPARE(r, s);
        PK_VERIFY(buf.bytesAvailable() == 0);

        // test even string
        buf.close();
        ba.resize(0);
        r = PkString();
        buf.open(PkStream::ReadWrite);
        buf.seek(0);
        s = PkString("bl");
        PK_VERIFY(psdwrite_pascalstring<psd_byte_order::psdLittleEndian>(buf, s));
        buf.close();
        buf.open(PkStream::ReadOnly);
        buf.seek(0);
        PK_VERIFY(psdread_pascalstring<psd_byte_order::psdLittleEndian>(buf, r, 1));
        PK_COMPARE(r, s);
        PK_VERIFY(buf.bytesAvailable() == 0);

        // test uneven string
        buf.close();
        ba.resize(0);
        r = PkString();
        buf.open(PkStream::ReadWrite);
        buf.seek(0);
        s = PkString("bla");
        PK_VERIFY(psdwrite_pascalstring<psd_byte_order::psdLittleEndian>(buf, s, 2));
        buf.close();
        buf.open(PkStream::ReadOnly);
        buf.seek(0);
        PK_VERIFY(psdread_pascalstring<psd_byte_order::psdLittleEndian>(buf, r, 2));
        PK_COMPARE(r, s);
        dbgKrita << buf.bytesAvailable();
        PK_VERIFY(buf.bytesAvailable() == 0);
    }

    {
        PkByteArray ba;
        PkCosMemoryStream buf(&ba);

        PkString s;
        PkString r;

        // test null string
        buf.open(PkStream::ReadWrite);
        PK_VERIFY(psdwrite_pascalstring(buf, s));
        buf.close();
        buf.open(PkStream::ReadOnly);
        buf.seek(0);
        PK_VERIFY(psdread_pascalstring(buf, r, 2));
        PK_COMPARE(r, s);
        PK_VERIFY(buf.bytesAvailable() == 0);

        // test even string
        buf.close();
        ba.resize(0);
        r = PkString();
        buf.open(PkStream::ReadWrite);
        buf.seek(0);
        s = PkString("bl");
        PK_VERIFY(psdwrite_pascalstring(buf, s));
        buf.close();
        buf.open(PkStream::ReadOnly);
        buf.seek(0);
        PK_VERIFY(psdread_pascalstring(buf, r, 1));
        PK_COMPARE(r, s);
        PK_VERIFY(buf.bytesAvailable() == 0);

        // test uneven string
        buf.close();
        ba.resize(0);
        r = PkString();
        buf.open(PkStream::ReadWrite);
        buf.seek(0);
        s = PkString("bla");
        PK_VERIFY(psdwrite_pascalstring(buf, s, 2));
        buf.close();
        buf.open(PkStream::ReadOnly);
        buf.seek(0);
        PK_VERIFY(psdread_pascalstring(buf, r, 2));
        PK_COMPARE(r, s);
        dbgKrita << buf.bytesAvailable();
        PK_VERIFY(buf.bytesAvailable() == 0);
    }
}

void PSDUtilsTest::test_psdread_fixedpoint()
{
    {
        PkByteArray ba;
        PkCosMemoryStream buf(&ba);
        PK_VERIFY(buf.open(PkStream::ReadWrite));
        const double s = -2.7;
        psdwriteFixedPoint<psd_byte_order::psdLittleEndian>(buf, s);
        buf.close();
        buf.open(PkStream::ReadOnly);
        buf.seek(0);
        double r = psdreadFixedPoint<psd_byte_order::psdLittleEndian>(buf);
        PK_VERIFY(fabs(r - s) < 0.001);
    }

    {
        PkByteArray ba;
        PkCosMemoryStream buf(&ba);
        PK_VERIFY(buf.open(PkStream::ReadWrite));
        const double s = -2.7;
        psdwriteFixedPoint(buf, s);
        buf.close();
        buf.open(PkStream::ReadOnly);
        buf.seek(0);
        double r = psdreadFixedPoint(buf);
        PK_VERIFY(fabs(r - s) < 0.001);
    }
}

#ifdef PK_SHELL_MOC_BINDER
#include "pk_binder_psd_utils_test.inc"
#endif

PK_TEST_GUILESS_MAIN(PSDUtilsTest)
