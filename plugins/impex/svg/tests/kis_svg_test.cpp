/*
 * SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_svg_test.h"


#include <simpletest.h>
#include <QCoreApplication>

#include <testui.h>

#include "filestest.h"

#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing the importing of files in krita"
#endif


const PkString SvgMimetype = "image/svg+xml";


void KisSvgTest::testFiles()
{
    TestUtil::testFiles(PkString(FILES_DATA_DIR) + "/sources", PkStringList(), PkString(), 30, 50);
}



void KisSvgTest::testImportFromWriteonly()
{
    TestUtil::testImportFromWriteonly(SvgMimetype);
}



void KisSvgTest::testImportIncorrectFormat()
{
    TestUtil::testImportIncorrectFormat(SvgMimetype);
}


KISTEST_MAIN(KisSvgTest)
