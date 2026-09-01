/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_QIMAGE_PYRAMID_TEST_H
#define KIS_QIMAGE_PYRAMID_TEST_H

#include <simpletest.h>

class KisQImagePyramidTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testSmoothRotationUsesTransparentBorder();
};

#endif
