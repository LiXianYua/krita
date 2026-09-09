/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef HTMLSAVINGCONTEXT_H
#define HTMLSAVINGCONTEXT_H

#include <PkScopedPointer.h>

class KoXmlWriter;
class KoShape;
class PkStream;
class PkString;
class PkTransform;
class PkImage;

/**
 * @brief The HtmlSavingContext class provides context for saving a flake-based document
 * to html.
 */
class HtmlSavingContext
{
public:
    HtmlSavingContext(PkStream &shapeDevice);
    virtual ~HtmlSavingContext();
    /// Provides access to the shape writer
    KoXmlWriter &shapeWriter();
private:
    HtmlSavingContext(const HtmlSavingContext &) = delete;
    HtmlSavingContext &operator=(const HtmlSavingContext &) = delete;
private:
    struct Private;
    const PkScopedPointer<Private> d;
};

#endif // HTMLSAVINGCONTEXT_H
