/*
 * SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "psd_colormode_block_test.h"

#include <simpletest.h>
#include <PkFileStream.h>
#include <psd.h>
#include <psd_header.h>
#include <psd_colormode_block.h>
#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing the importing of files in krita"
#endif

void PSDColorModeBlockTest::testCreation()
{
    PSDColorModeBlock colorModeBlock1(Indexed);
    KIS_ASSERT(!colorModeBlock1.valid());

    PSDColorModeBlock colorModeBlock2(DuoTone);
    KIS_ASSERT(!colorModeBlock2.valid());

    PSDColorModeBlock colorModeBlock3(RGB);
    KIS_ASSERT(colorModeBlock3.valid());
}

void PSDColorModeBlockTest::testLoadingRGB()
{
    PkString filename = PkString(FILES_DATA_DIR) + "/sources/2.psd";
    PkFileStream f(filename);
    KIS_ASSERT(f.open(PkStream::ReadOnly));
    PSDHeader header;
    header.read(f);

    PK_VERIFY(header.colormode == RGB);

    PSDColorModeBlock colorModeBlock(header.colormode);
    bool retval = colorModeBlock.read(f);
    KIS_ASSERT(retval); (void)retval;
    KIS_ASSERT(colorModeBlock.valid());

}

void PSDColorModeBlockTest::testLoadingIndexed()
{
    PkString filename = PkString(FILES_DATA_DIR) + "/sources/100x100indexed.psd";
    PkFileStream f(filename);
    KIS_ASSERT(f.open(PkStream::ReadOnly));
    PSDHeader header;
    header.read(f);

    PK_VERIFY(header.colormode == Indexed);

    PSDColorModeBlock colorModeBlock(header.colormode);
    bool retval = colorModeBlock.read(f);
    KIS_ASSERT(retval); (void)retval;
    KIS_ASSERT(colorModeBlock.valid());

}


#ifdef PK_SHELL_MOC_BINDER
#include "pk_binder_psd_colormode_block_test.inc"
#endif

SIMPLE_TEST_MAIN(PSDColorModeBlockTest)
