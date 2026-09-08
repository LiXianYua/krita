/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOVECTORPATTERNBACKGROUND_H
#define KOVECTORPATTERNBACKGROUND_H

#include <KoShapeBackground.h>
#include <KoFlakeCoordinateSystem.h>
#include <PkSharedDataPointer.h>

class KoShape;
class PkPointF;
class PkRectF;
class PkTransform;


class KoVectorPatternBackground : public KoShapeBackground
{
public:
    KoVectorPatternBackground();
    ~KoVectorPatternBackground() override;

    bool compareTo(const KoShapeBackground *other) const override;

    void setReferenceCoordinates(KoFlake::CoordinateSystem value);
    KoFlake::CoordinateSystem referenceCoordinates() const;

    /**
     * In ViewBox just use the same mode as for referenceCoordinates
     */
    void setContentCoordinates(KoFlake::CoordinateSystem value);
    KoFlake::CoordinateSystem contentCoordinates() const;

    void setReferenceRect(const PkRectF &value);
    PkRectF referenceRect() const;

    void setPatternTransform(const PkTransform &value);
    PkTransform patternTransform() const;

    void setShapes(const PkList<KoShape*> value);
    PkList<KoShape*> shapes() const;

    void paint(PkPainter &painter, const PkPainterPath &fillPath) const override;
    bool hasTransparency() const override;
private:
    class Private;
    PkSharedDataPointer<Private> d;
};

#endif // KOVECTORPATTERNBACKGROUND_H
