/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KOGRADIENTBACKGROUND_H
#define KOGRADIENTBACKGROUND_H

#include "KoShapeBackground.h"
#include "kritaflake_export.h"

#include <PkTransform.h>
#include <QSharedDataPointer>

class PkGradient;

/// A gradient shape background
class KRITAFLAKE_EXPORT KoGradientBackground : public KoShapeBackground
{
public:
    /**
     * Creates new gradient background from given gradient.
     * The background takes ownership of the given gradient.
     */
    explicit KoGradientBackground(PkGradient *gradient, const PkTransform &matrix = PkTransform());

    /**
     * Create new gradient background from the given gradient.
     * A clone of the given gradient is used.
     */
    explicit KoGradientBackground(const PkGradient &gradient, const PkTransform &matrix = PkTransform());

    /// Destroys the background
    ~KoGradientBackground() override;

    // Work around MSVC inability to generate copy ops with QSharedDataPointer.
    KoGradientBackground(const KoGradientBackground &);
    KoGradientBackground &operator=(const KoGradientBackground &);

    bool compareTo(const KoShapeBackground *other) const override;

    /// Sets the transform matrix
    void setTransform(const PkTransform &matrix);

    /// Returns the transform matrix
    PkTransform transform() const;

    /**
     * Sets a new gradient.
     * A clone of the given gradient is used.
     */
    void setGradient(const PkGradient &gradient);

    /// Returns the gradient
    const PkGradient *gradient() const;

    /// reimplemented from KoShapeBackground
    void paint(QPainter &painter, const PkPainterPath &fillPath) const override;
private:
    class Private;
    QSharedDataPointer<Private> d;
};

#endif // KOGRADIENTBACKGROUND_H
