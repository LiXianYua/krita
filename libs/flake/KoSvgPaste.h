/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOSVGPASTE_H
#define KOSVGPASTE_H

#include "kritaflake_export.h"
#include <PkList.h>

class KoShape;
class PkRectF;
class PkSizeF;
class PkByteArray;

class KRITAFLAKE_EXPORT KoSvgPaste
{
public:
    KoSvgPaste();
    virtual ~KoSvgPaste();


    bool hasShapes();
    PkList<KoShape*> fetchShapes(PkRectF viewportInPx, qreal resolutionPPI, PkSizeF *fragmentSize = nullptr);
    static PkList<KoShape*> fetchShapesFromData(const PkByteArray &data, PkRectF viewportInPx, qreal resolutionPPI, PkSizeF *fragmentSize = nullptr);

private:
    class Private;
    Private *const d;

    Q_DISABLE_COPY(KoSvgPaste);
};

#endif // KOSVGPASTE_H
