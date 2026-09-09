/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2011 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef SVGSAVINGCONTEXT_H
#define SVGSAVINGCONTEXT_H

class KoXmlWriter;
class KoShape;
class PkStream;
class PkString;
class PkTransform;
class PkImage;

#include "kritaflake_export.h"

/// Context for saving svg files
class KRITAFLAKE_EXPORT SvgSavingContext
{
public:
    /// Creates a new svg saving context on the specified output device
    explicit SvgSavingContext(PkStream &outputDevice, bool saveInlineImages = true);
    explicit SvgSavingContext(PkStream &shapesDevice, PkStream &styleDevice, bool saveInlineImages = true);

    /// Virtual destructor
    virtual ~SvgSavingContext();

    /// Provides access to the style writer
    KoXmlWriter &styleWriter();

    /// Provides access to the shape writer
    KoXmlWriter &shapeWriter();

    /// Create a unique id from the specified base text
    PkString createUID(const PkString &base);

    /// Returns the unique id for the given shape
    PkString getID(const KoShape *obj);

    /// Returns the transformation used to transform into user space
    PkTransform userSpaceTransform() const;

    /// Returns if image should be saved inline
    bool isSavingInlineImages() const;

    /// Create a filename suitable for saving external data
    PkString createFileName(const PkString &extension);

    /// Saves given image and returns the href used
    PkString saveImage(const PkImage &image);

    void setStrippedTextMode(bool value);
    bool strippedTextMode() const;

private:
    SvgSavingContext(const SvgSavingContext &) = delete;
    SvgSavingContext &operator=(const SvgSavingContext &) = delete;

private:
    class Private;
    Private * const d;
};

#endif // SVGSAVINGCONTEXT_H
