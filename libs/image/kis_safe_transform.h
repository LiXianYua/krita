/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_SAFE_TRANSFORM_H
#define __KIS_SAFE_TRANSFORM_H

#include <pk/pointer/PkScopedPointer.h>

#include "kritaimage_export.h"

class PkTransform;
class PkRect;
class PkRectF;
class PkPolygonF;


class KRITAIMAGE_EXPORT KisSafeTransform
{
public:
    KisSafeTransform(const PkTransform &transform,
                     const PkRect &bounds,
                     const PkRect &srcInterestRect);

    ~KisSafeTransform();

    PkPolygonF srcClipPolygon() const;
    PkPolygonF dstClipPolygon() const;

    PkPolygonF mapForward(const PkPolygonF &p);
    PkPolygonF mapBackward(const PkPolygonF &p);

    PkRectF mapRectForward(const PkRectF &rc);
    PkRectF mapRectBackward(const PkRectF &rc);

    PkRect mapRectForward(const PkRect &rc);
    PkRect mapRectBackward(const PkRect &rc);

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_SAFE_TRANSFORM_H */
