/*
 * SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "psd_header_test.h"

#include <simpletest.h>
#include <PkFileStream.h>
#include <psd_header.h>
#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing the importing of files in krita"
#endif

void PSDHeaderTest::testCreation()
{
    PSDHeader header;
    KIS_ASSERT(!header.valid());
}

void PSDHeaderTest::testLoading()
{
    PkString filename = PkString(FILES_DATA_DIR) + "/sources/2.psd";
    PkFileStream f(filename);
    KIS_ASSERT(f.open(PkStream::ReadOnly));
    PSDHeader header;
    header.read(f);

    PK_VERIFY(header.signature == PkString("8BPS"));
    PK_VERIFY(header.version == 1);
    PK_VERIFY(header.nChannels == 3);
    PK_VERIFY(header.width == 100 );
    PK_VERIFY(header.height == 100);
    PK_VERIFY(header.channelDepth == 8);
    PK_VERIFY(header.colormode == RGB);

}

void PSDHeaderTest::testRoundTripping()
{
    PkString filename = "test.psd";
    PkFileStream f(filename);
    KIS_ASSERT(f.open(PkStream::ReadWrite));
    PSDHeader header;
    KIS_ASSERT(!header.valid());
    header.signature = "8BPS";
    header.version = 1;
    header.nChannels = 3;
    header.width = 1000;
    header.height = 1000;
    header.channelDepth = 8;
    header.colormode = RGB;
    KIS_ASSERT(header.valid());
    bool retval = header.write(f);
    KIS_ASSERT(retval); (void)retval;

    f.close();
    KIS_ASSERT(f.open(PkStream::ReadOnly));
    PSDHeader header2;
    retval = header2.read(f);
    KIS_ASSERT(retval);

    PK_COMPARE(header.signature, header2.signature);
    PK_VERIFY(header.version == header2.version);
    PK_VERIFY(header.nChannels == header2.nChannels);
    PK_VERIFY(header.width == header2.width);
    PK_VERIFY(header.height == header2.height);
    PK_VERIFY(header.channelDepth == header2.channelDepth);
    PK_VERIFY(header.colormode == header2.colormode);
}



#ifdef PK_SHELL_MOC_BINDER
#include "pk_binder_psd_header_test.inc"
#endif

SIMPLE_TEST_MAIN(PSDHeaderTest)
