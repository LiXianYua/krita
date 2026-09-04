/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef HTMLWRITER_H
#define HTMLWRITER_H

#include <PkList.h>
#include <PkSize.h>
// [migrate] missing include for Pk/Qt type
#include <PkStringList.h>

class KoShapeLayer;
class KoShapeGroup;
class KoShape;
class KoPathShape;
class PkStream;
class PkString;
class HtmlSavingContext;

// Implements writing shapes to HTML
class HtmlWriter
{
public:
    HtmlWriter(const PkList<KoShape*> &toplevelShapes);
    virtual ~HtmlWriter();

    bool save(PkStream &outputDevice);

    PkStringList errors() const;
    PkStringList warnings() const;

private:

    void saveShapes(const PkList<KoShape*> shapes, HtmlSavingContext &savingContext);

    PkList<KoShape*> m_toplevelShapes;
    PkStringList m_errors;
    PkStringList m_warnings;
};

#endif // HTMLWRITER_H
