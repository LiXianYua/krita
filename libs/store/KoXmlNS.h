/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2004 David Faure <faure@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KOXMLNS_H
#define KOXMLNS_H

#include "PkString.h"

#include "kritastore_export.h"
/**
 * Repository of XML namespaces used for ODF documents.
 *
 * Please make sure that you do not use the variables provided by this class in
 * the destructor of a static object.
 */
class KRITASTORE_EXPORT KoXmlNS
{
public:
    static const PkString office;
    static const PkString meta;
    static const PkString config;
    static const PkString text;
    static const PkString table;
    static const PkString draw;
    static const PkString presentation;
    static const PkString dr3d;
    static const PkString chart;
    static const PkString form;
    static const PkString script;
    static const PkString style;
    static const PkString number;
    static const PkString manifest;
    static const PkString anim;

    static const PkString math;
    static const PkString svg;
    static const PkString fo;
    static const PkString dc;
    static const PkString xlink;
    static const PkString VL;
    static const PkString smil;
    static const PkString xhtml;
    static const PkString xml;
    static const PkString sodipodi;
    static const PkString krita;

    static const PkString calligra;
    static const PkString officeooo;
    static const PkString ooo;

    static const char* nsURI2NS(const PkString &nsURI);

    static const PkString delta;
    static const PkString split;
    static const PkString ac;
private:
    KoXmlNS(); // don't create an instance of me :)
};

#endif /* KOXMLNS_H */
