/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2011 Thorsten Zachmann <zachmann@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KOMARKER_H
#define KOMARKER_H

#include <QMetaType>

#include "kritaflake_export.h"
#include <KoFlake.h>

#include <PkXmlDocument.h>
// [migrate] missing include for Pk/Qt type
#include <PkPen.h>

class KoShapeLoadingContext;
class KoShapeSavingContext;
class PkString;
class PkPainterPath;
class KoShape;
class PkPainter;
class KoShapeStroke;

#include <kis_shared.h>

class  KRITAFLAKE_EXPORT KoMarker : public KisShared
{
public:
    KoMarker();
    ~KoMarker();

    /**
     * Display name of the marker
     *
     * @return Display name of the marker
     */
    PkString name() const;

    KoMarker(const KoMarker &rhs);
    bool operator==(const KoMarker &other) const;

    enum MarkerCoordinateSystem {
        StrokeWidth,
        UserSpaceOnUse
    };

    void setCoordinateSystem(MarkerCoordinateSystem value);
    MarkerCoordinateSystem coordinateSystem() const;

    static MarkerCoordinateSystem coordinateSystemFromString(const PkString &value);
    static PkString coordinateSystemToString(MarkerCoordinateSystem value);

    void setReferencePoint(const PkPointF &value);
    PkPointF referencePoint() const;

    void setReferenceSize(const PkSizeF &size);
    PkSizeF referenceSize() const;

    bool hasAutoOrientation() const;
    void setAutoOrientation(bool value);

    // measured in radians!
    qreal explicitOrientation() const;

    // measured in radians!
    void setExplicitOrientation(qreal value);

    void setShapes(const PkList<KoShape*> &shapes);
    PkList<KoShape*> shapes() const;

    /**
     * @brief paintAtOrigin paints the marker at the position \p pos.
     *        Scales and rotates the marker if needed.
     */
    void paintAtPosition(PkPainter *painter, const PkPointF &pos, qreal strokeWidth, qreal nodeAngle);

    /**
     * Return maximum distance that the marker can take outside the shape itself
     */
    qreal maxInset(qreal strokeWidth) const;

    /**
     * Bounding rect of the marker in local coordinates. It is assumed that the marker
     * is painted with the reference point placed at position (0,0)
     */
    PkRectF boundingRect(qreal strokeWidth, qreal nodeAngle) const;

    /**
     * Outline of the marker in local coordinates. It is assumed that the marker
     * is painted with the reference point placed at position (0,0)
     */
    PkPainterPath outline(qreal strokeWidth, qreal nodeAngle) const;

    /**
     * Draws a preview of the marker in \p previewRect of \p painter
     */
    void drawPreview(PkPainter *painter, const PkRectF &previewRect,
                     const PkPen &pen, KoFlake::MarkerPosition position);


    void applyShapeStroke(const KoShape *shape, KoShapeStroke *stroke, const PkPointF &pos, qreal strokeWidth, qreal nodeAngle);

private:
    class Private;
    Private * const d;
};

Q_DECLARE_METATYPE(KoMarker*)

#endif /* KOMARKER_H */
