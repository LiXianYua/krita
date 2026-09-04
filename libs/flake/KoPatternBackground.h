/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KOPATTERNBACKGROUND_H
#define KOPATTERNBACKGROUND_H

#include "KoShapeBackground.h"
#include "kritaflake_export.h"

#include <QSharedDataPointer>

class KoPatternBackgroundPrivate;

class PkTransform;
class PkImage;
class PkPointF;
class PkRectF;

/// A pattern shape background
class KRITAFLAKE_EXPORT KoPatternBackground : public KoShapeBackground
{
public:
    /// Pattern rendering style
    enum PatternRepeat {
        Original,
        Tiled,
        Stretched
    };
    /// Pattern reference point
    enum ReferencePoint {
        TopLeft,
        Top,
        TopRight,
        Left,
        Center,
        Right,
        BottomLeft,
        Bottom,
        BottomRight
    };

    explicit KoPatternBackground();

    ~KoPatternBackground() override;

    // Work around MSVC inability to generate copy ops with QSharedDataPointer.
    KoPatternBackground(const KoPatternBackground &);
    KoPatternBackground& operator=(const KoPatternBackground &);

    bool compareTo(const KoShapeBackground *other) const override;

    /// Sets the transform matrix
    void setTransform(const PkTransform &matrix);

    /// Returns the transform matrix
    PkTransform transform() const;

    /// Sets a new pattern
    void setPattern(const PkImage &pattern);

    /// Returns the pattern
    PkImage pattern() const;

    /// Sets the pattern repeatgfl
    void setRepeat(PatternRepeat repeat);

    /// Returns the pattern repeat
    PatternRepeat repeat() const;

    /// Returns the pattern reference point identifier
    ReferencePoint referencePoint() const;

    /// Sets the pattern reference point
    void setReferencePoint(ReferencePoint referencePoint);

    /// Returns reference point offset in percent of the pattern display size
    PkPointF referencePointOffset() const;

    /// Sets the reference point offset in percent of the pattern display size
    void setReferencePointOffset(const PkPointF &offset);

    /// Returns tile repeat offset in percent of the pattern display size
    PkPointF tileRepeatOffset() const;

    /// Sets the tile repeat offset in percent of the pattern display size
    void setTileRepeatOffset(const PkPointF &offset);

    /// Returns the pattern display size
    PkSizeF patternDisplaySize() const;

    /// Sets pattern display size
    void setPatternDisplaySize(const PkSizeF &size);

    /// Returns the original image size
    PkSizeF patternOriginalSize() const;

    /// reimplemented from KoShapeBackground
    void paint(QPainter &painter, const PkPainterPath &fillPath) const override;

    /// Returns the bounding rect of the pattern image based on the given fill size
    PkRectF patternRectFromFillSize(const PkSizeF &size);
private:
    class Private;
    QSharedDataPointer<Private> d;
};

#endif // KOPATTERNBACKGROUND_H
