/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KOCOLORBACKGROUND_H
#define KOCOLORBACKGROUND_H

#include "KoShapeBackground.h"
#include "kritaflake_export.h"
#include <Qt>
#include <PkSharedDataPointer.h>
#include <PkBrush.h>

class KoColorBackgroundPrivate;
class PkColor;
class PkBrush;

/// A simple solid color shape background
class KRITAFLAKE_EXPORT KoColorBackground : public KoShapeBackground
{
public:
    KoColorBackground();

    /// Creates background from given color and style
    explicit KoColorBackground(const PkColor &color, Pk::BrushStyle style = Pk::SolidPattern);

    ~KoColorBackground() override;

    // Work around MSVC inability to generate copy ops with PkSharedDataPointer.
    KoColorBackground(const KoColorBackground &);
    KoColorBackground &operator=(const KoColorBackground &);

    bool compareTo(const KoShapeBackground *other) const override;

    /// Returns the background color
    PkColor color() const;

    /// Sets the background color
    void setColor(const PkColor &color);

    /// Returns the background style
    Pk::BrushStyle style() const;

    PkBrush brush() const;

    // reimplemented from KoShapeBackground
    void paint(PkPainter &painter, const PkPainterPath &fillPath) const override;

private:
    class Private;
    PkSharedDataPointer<Private> d;
};

#endif // KOCOLORBACKGROUND_H
