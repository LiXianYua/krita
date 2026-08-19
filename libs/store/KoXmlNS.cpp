/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2004 David Faure <faure@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "KoXmlNS.h"

#include <string.h>

const PkString KoXmlNS::office("urn:oasis:names:tc:opendocument:xmlns:office:1.0");
const PkString KoXmlNS::meta("urn:oasis:names:tc:opendocument:xmlns:meta:1.0");
const PkString KoXmlNS::config("urn:oasis:names:tc:opendocument:xmlns:config:1.0");
const PkString KoXmlNS::text("urn:oasis:names:tc:opendocument:xmlns:text:1.0");
const PkString KoXmlNS::table("urn:oasis:names:tc:opendocument:xmlns:table:1.0");
const PkString KoXmlNS::draw("urn:oasis:names:tc:opendocument:xmlns:drawing:1.0");
const PkString KoXmlNS::presentation("urn:oasis:names:tc:opendocument:xmlns:presentation:1.0");
const PkString KoXmlNS::dr3d("urn:oasis:names:tc:opendocument:xmlns:dr3d:1.0");
const PkString KoXmlNS::chart("urn:oasis:names:tc:opendocument:xmlns:chart:1.0");
const PkString KoXmlNS::form("urn:oasis:names:tc:opendocument:xmlns:form:1.0");
const PkString KoXmlNS::script("urn:oasis:names:tc:opendocument:xmlns:script:1.0");
const PkString KoXmlNS::style("urn:oasis:names:tc:opendocument:xmlns:style:1.0");
const PkString KoXmlNS::number("urn:oasis:names:tc:opendocument:xmlns:datastyle:1.0");
const PkString KoXmlNS::manifest("urn:oasis:names:tc:opendocument:xmlns:manifest:1.0");
const PkString KoXmlNS::anim("urn:oasis:names:tc:opendocument:xmlns:animation:1.0");

const PkString KoXmlNS::math("http://www.w3.org/1998/Math/MathML");
const PkString KoXmlNS::svg("urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0");
const PkString KoXmlNS::fo("urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0");
const PkString KoXmlNS::dc("http://purl.org/dc/elements/1.1/");
const PkString KoXmlNS::xlink("http://www.w3.org/1999/xlink");
const PkString KoXmlNS::VL("http://openoffice.org/2001/versions-list");
const PkString KoXmlNS::smil("urn:oasis:names:tc:opendocument:xmlns:smil-compatible:1.0");
const PkString KoXmlNS::xhtml("http://www.w3.org/1999/xhtml");
const PkString KoXmlNS::xml("http://www.w3.org/XML/1998/namespace");
const PkString KoXmlNS::sodipodi("http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd");
const PkString KoXmlNS::krita("http://krita.org/namespaces/svg/krita");

const PkString KoXmlNS::calligra = "http://www.calligra.org/2005/";
const PkString KoXmlNS::officeooo = "http://openoffice.org/2009/office";
const PkString KoXmlNS::ooo = "http://openoffice.org/2004/office";

const PkString KoXmlNS::delta("http://www.deltaxml.com/ns/track-changes/delta-namespace");
const PkString KoXmlNS::split("http://www.deltaxml.com/ns/track-changes/split-namespace");
const PkString KoXmlNS::ac("http://www.deltaxml.com/ns/track-changes/attribute-change-namespace");

const char* KoXmlNS::nsURI2NS(const PkString &nsURI)
{
    if (nsURI == KoXmlNS::office)
        return "office";
    else if (nsURI == KoXmlNS::meta)
        return "meta";
    else if (nsURI == KoXmlNS::config)
        return "config";
    else if (nsURI == KoXmlNS::text)
        return "text";
    else if (nsURI == KoXmlNS::table)
        return "table";
    else if (nsURI == KoXmlNS::draw)
        return "draw";
    else if (nsURI == KoXmlNS::presentation)
        return "presentation";
    else if (nsURI == KoXmlNS::dr3d)
        return "dr3d";
    else if (nsURI == KoXmlNS::chart)
        return "chart";
    else if (nsURI == KoXmlNS::form)
        return "form";
    else if (nsURI == KoXmlNS::script)
        return "script";
    else if (nsURI == KoXmlNS::style)
        return "style";
    else if (nsURI == KoXmlNS::number)
        return "number";
    else if (nsURI == KoXmlNS::manifest)
        return "manifest";
    else if (nsURI == KoXmlNS::anim)
        return "anim";
    else if (nsURI == KoXmlNS::math)
        return "math";
    else if (nsURI == KoXmlNS::svg)
        return "svg";
    else if (nsURI == KoXmlNS::fo)
        return "fo";
    else if (nsURI == KoXmlNS::dc)
        return "dc";
    else if (nsURI == KoXmlNS::xlink)
        return "xlink";
    else if (nsURI == KoXmlNS::VL)
        return "VL";
    else if (nsURI == KoXmlNS::smil)
        return "smil";
    else if (nsURI == KoXmlNS::xhtml)
        return "xhtml";
    else if (nsURI == KoXmlNS::calligra)
        return "calligra";
    else if (nsURI == KoXmlNS::officeooo)
        return "officeooo";
    else if (nsURI == KoXmlNS::xml)
        return "xml";

    // Shouldn't happen.
    return "";
}
