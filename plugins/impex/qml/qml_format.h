/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef QML_FORMAT_H
#define QML_FORMAT_H

#include <PkRect.h>
#include <PkString.h>

#include <vector>

class PkStream;

struct QmlLayerRecord
{
    PkString id;
    PkRect bounds;
    PkString source;
    double opacity {1.0};
};

bool writeQmlDocument(PkStream *device,
                      int width,
                      int height,
                      const std::vector<QmlLayerRecord> &layers);

#endif
