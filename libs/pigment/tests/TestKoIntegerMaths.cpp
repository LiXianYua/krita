/*
 *  SPDX-FileCopyrightText: 2005 Adrian Page <adrian@pagenet.plus.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TestKoIntegerMaths.h"
#include "KoIntegerMaths.h"

#include <simpletest.h>

void TestKoIntegerMaths::UINT8Tests()
{
    PK_COMPARE((int)UINT8_MULT(0, 255), 0);
    PK_COMPARE((int)UINT8_MULT(255, 255), 255);

    PK_COMPARE((int)UINT8_MULT(128, 255), 128);
    PK_COMPARE((int)UINT8_MULT(255, 128), 128);

    PK_COMPARE((int)UINT8_MULT(1, 255), 1);
    PK_COMPARE((int)UINT8_MULT(1, 127), 0);
    PK_COMPARE((int)UINT8_MULT(64, 128), 32);

    PK_COMPARE((int)UINT8_DIVIDE(255, 255), 255);
    PK_COMPARE((int)UINT8_DIVIDE(64, 128), 128);
    PK_COMPARE((int)UINT8_DIVIDE(1, 64), 4);
    PK_COMPARE((int)UINT8_DIVIDE(0, 1), 0);

    for (int i = 0; i < 256; i++) {
        PK_COMPARE((int)UINT8_BLEND(255, 0, i), i );
    }
    for (int i = 0; i < 256; i++) {
        PK_COMPARE((int)UINT8_BLEND(0, 255, i), int( 255 - i) );
    }
    for (int i = 0; i < 256; i++) {
        PK_VERIFY( qAbs(int(UINT8_BLEND(0, i, 128)) - int(i*(255 - 128) / 255.0 + 0.5)) <= 1 );
    }
    PK_COMPARE((int)UINT8_BLEND(255, 128, 128), 192);
    PK_COMPARE((int)UINT8_BLEND(128, 64, 255), 128);
}

void TestKoIntegerMaths::UINT16Tests()
{
    PK_COMPARE((int)UINT16_MULT(0, 65535), 0);
    PK_COMPARE((int)UINT16_MULT(65535, 65535), 65535);

    PK_COMPARE((int)UINT16_MULT(32768, 65535), 32768);
    PK_COMPARE((int)UINT16_MULT(65535, 32768), 32768);

    PK_COMPARE((int)UINT16_MULT(1, 65535), 1);
    PK_COMPARE((int)UINT16_MULT(1, 32767), 0);
    PK_COMPARE((int)UINT16_MULT(16384, 32768), 8192);

    PK_COMPARE((int)UINT16_DIVIDE(65535, 65535), 65535);
    PK_COMPARE((int)UINT16_DIVIDE(16384, 32768), 32768);
    PK_COMPARE((int)UINT16_DIVIDE(1, 16384), 4);
    PK_COMPARE((int)UINT16_DIVIDE(0, 1), 0);

    PK_COMPARE((int)UINT16_BLEND(65535, 0, 0), 0);
    // All these tests gave off-by one errors that apparently aren't
    // errors.
    // So -- are we officially expecting 32767 instead of 32768 and
    // 49151 instead of 49152 here?
    PK_COMPARE((int)UINT16_BLEND(65535, 0, 32768), 32767);
    PK_COMPARE((int)UINT16_BLEND(65535, 32768, 32768), 49151);
    PK_COMPARE((int)UINT16_BLEND(32768, 16384, 65535), 32767);
}

void TestKoIntegerMaths::conversionTests()
{
    PK_COMPARE((int)UINT8_TO_UINT16(255), 65535);
    PK_COMPARE((int)UINT8_TO_UINT16(254), 65278); // Erm, is this right?
    PK_COMPARE((int)UINT8_TO_UINT16(0), 0);
    PK_COMPARE((int)UINT8_TO_UINT16(128), 128 * 257);

    PK_COMPARE((int)UINT16_TO_UINT8(65535), 255);
    PK_COMPARE((int)UINT16_TO_UINT8(65280), 254);
    PK_COMPARE((int)UINT16_TO_UINT8(0), 0);
    PK_COMPARE((int)UINT16_TO_UINT8(128 * 257), 128);
}

PK_TEST_GUILESS_MAIN(TestKoIntegerMaths)
