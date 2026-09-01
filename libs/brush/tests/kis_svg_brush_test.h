/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_SVG_BRUSH_TEST_H
#define KIS_SVG_BRUSH_TEST_H

#include <simpletest.h>

class KisSvgBrushTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testUtf8NameAndSvgRoundTrip();
};

#endif
