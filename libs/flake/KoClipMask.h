/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOCLIPMASK_H
#define KOCLIPMASK_H

#include "kritaflake_export.h"

#include <KoFlakeCoordinateSystem.h>
#include <PkList.h>
#include <PkSharedDataPointer.h>

class KoShape;
class PkRectF;
class PkTransform;
class PkPointF;
class PkPainter;


class KRITAFLAKE_EXPORT KoClipMask
{
public:
    KoClipMask();
    ~KoClipMask();

    // Work around MSVC inability to generate copy ops with PkSharedDataPointer.
    KoClipMask(const KoClipMask &);
    KoClipMask &operator=(const KoClipMask &);

    KoClipMask *clone() const;

    KoFlake::CoordinateSystem coordinates() const;
    void setCoordinates(KoFlake::CoordinateSystem value);

    KoFlake::CoordinateSystem contentCoordinates() const;
    void setContentCoordinates(KoFlake::CoordinateSystem value);

    PkRectF maskRect() const;
    void setMaskRect(const PkRectF &value);

    PkList<KoShape *> shapes() const;
    void setShapes(const PkList<KoShape *> &value);

    bool isEmpty() const;

    void setExtraShapeOffset(const PkPointF &value);

    void drawMask(PkPainter *painter, KoShape *shape);

private:
    struct Private;
    PkSharedDataPointer<Private> m_d;
};

#endif // KOCLIPMASK_H
