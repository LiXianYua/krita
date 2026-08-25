/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_serializable_configuration.h"


KisSerializableConfiguration::KisSerializableConfiguration()
{
}

KisSerializableConfiguration::KisSerializableConfiguration(const KisSerializableConfiguration &)
    : KisShared()
{
}

bool KisSerializableConfiguration::fromXML(const PkString &s, bool)
{
    PkXmlDocument doc;
    bool rv = bool(doc.setContent(s));
    if (rv) {
        PkXmlElement e = doc.documentElement();
        fromXML(e);
    }
    return rv;
}

PkString KisSerializableConfiguration::toXML() const
{
    PkXmlDocument doc = PkXmlDocument("params");
    PkXmlElement root = doc.createElement("params");
    doc.appendChild(root);
    toXML(doc, root);
    return doc.toString();
}

KisSerializableConfigurationFactory::~KisSerializableConfigurationFactory()
{
}
