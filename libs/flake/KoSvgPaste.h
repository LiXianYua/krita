/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOSVGPASTE_H
#define KOSVGPASTE_H

#include "kritaflake_export.h"
#include <PkByteArray.h>
#include <PkList.h>

class KoShape;
class PkRectF;
class PkSizeF;

class KRITAFLAKE_EXPORT KoSvgPaste
{
public:
    KoSvgPaste(const PkByteArray &svgData, bool hasSvgData);
    virtual ~KoSvgPaste();


    bool hasShapes() const;
    PkList<KoShape*> fetchShapes(PkRectF viewportInPx, double resolutionPPI, PkSizeF *fragmentSize = nullptr);
    static PkList<KoShape*> fetchShapesFromData(const PkByteArray &data, PkRectF viewportInPx, double resolutionPPI, PkSizeF *fragmentSize = nullptr);

private:
    class Private;
    Private *const d;

    KoSvgPaste(const KoSvgPaste &) = delete;
    KoSvgPaste &operator=(const KoSvgPaste &) = delete;
};

#endif // KOSVGPASTE_H
