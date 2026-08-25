/* This file is part of the KDE project
   SPDX-FileCopyrightText: 1998, 1999, 2000 Torben Weis <weis@kde.org>
   SPDX-FileCopyrightText: 2004 David Faure <faure@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "KoDocumentInfo.h"

#include "KoXmlNS.h"
#include <KoResourcePaths.h>
#include <PkDateTime.h>
#include <PkXmlDocument.h>
#include <PkFileStream.h>

#include <PkConfigGroup.h>
#include <PkSharedConfig.h>

#include <filesystem>
#include <string>


KoDocumentInfo::KoDocumentInfo(PkObject *parent) : PkObject(parent)
{
    m_aboutTags << "title" << "description" << "subject" << "abstract"
    << "keyword" << "initial-creator" << "editing-cycles" << "editing-time"
    << "date" << "creation-date" << "language" << "license";

    m_authorTags << "creator" << "creator-first-name" << "creator-last-name" << "initial" << "author-title" << "position" << "company";
    m_contactTags << "email" << "telephone" << "telephone-work" << "fax" << "country" << "postal-code" << "city" << "street";
    setAboutInfo("editing-cycles", "0");
    setAboutInfo("time-elapsed", "0");
    setAboutInfo("initial-creator", PkString("Unknown"));
    setAboutInfo("creation-date", PkString(PkDateTime::currentDateTime()
                 .toString(PkDateTime::DateFormat::ISODate).c_str()));
}

KoDocumentInfo::KoDocumentInfo(const KoDocumentInfo &rhs, PkObject *parent)
    : PkObject(parent),
      m_aboutTags(rhs.m_aboutTags),
      m_authorTags(rhs.m_authorTags),
      m_contact(rhs.m_contact),
      m_authorInfo(rhs.m_authorInfo),
      m_authorInfoOverride(rhs.m_authorInfoOverride),
      m_aboutInfo(rhs.m_aboutInfo),
      m_generator(rhs.m_generator)
{
}

KoDocumentInfo::~KoDocumentInfo()
{
}

bool KoDocumentInfo::load(const PkXmlDocument &doc)
{
    m_authorInfo.clear();

    if (!loadAboutInfo(doc.documentElement()))
        return false;

    if (!loadAuthorInfo(doc.documentElement()))
        return false;

    return true;
}


PkXmlDocument KoDocumentInfo::save(PkXmlDocument &doc, bool autosaving, bool documentModified)
{
    updateParametersAndBumpNumCycles(autosaving, documentModified);

    PkXmlElement s = saveAboutInfo(doc);
    if (!s.isNull())
        doc.documentElement().appendChild(s);

    s = saveAuthorInfo(doc);
    if (!s.isNull())
        doc.documentElement().appendChild(s);


    if (doc.documentElement().isNull())
        return PkXmlDocument();

    return doc;
}

void KoDocumentInfo::setAuthorInfo(const PkString &info, const PkString &data)
{
    if (!m_authorTags.contains(info) && !m_contactTags.contains(info) && !info.contains("contact-mode-")) {
        return;
    }

    m_authorInfoOverride.insert(info, data);
}

void KoDocumentInfo::setActiveAuthorInfo(const PkString &info, const PkString &data)
{
    if (!m_authorTags.contains(info) && !m_contactTags.contains(info) && !info.contains("contact-mode-")) {
        return;
    }
    if (m_contactTags.contains(info)) {
        m_contact.insert(data, info);
    } else {
        m_authorInfo.insert(info, data);
    }
    Q_EMIT infoUpdated(info, data);
}

PkString KoDocumentInfo::authorInfo(const PkString &info) const
{
    if (!m_authorTags.contains(info)  && !m_contactTags.contains(info) && !info.contains("contact-mode-"))
        return PkString();

    return m_authorInfo[ info ];
}

PkStringList KoDocumentInfo::authorContactInfo() const
{
    return m_contact.keys();
}

void KoDocumentInfo::setAboutInfo(const PkString &info, const PkString &data)
{
    if (!m_aboutTags.contains(info))
        return;

    m_aboutInfo.insert(info, data);
    Q_EMIT infoUpdated(info, data);
}

PkString KoDocumentInfo::aboutInfo(const PkString &info) const
{
    if (!m_aboutTags.contains(info)) {
        return PkString();
    }

    return m_aboutInfo[info];
}


bool KoDocumentInfo::loadAuthorInfo(const PkXmlElement &root)
{
    m_contact.clear();

    PkXmlElement e = root.firstChildElement("author");
    if(e.isNull()) {
        return false;
    }

    for (e = e.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
        PkString field = e.tagName();
        PkString value = e.text();

        if (field == "full-name") {
            setActiveAuthorInfo("creator", value.trimmed());
        } else if (field == "contact") {
            m_contact.insert(value, e.attribute("type"));
        } else {
            setActiveAuthorInfo(field, value.trimmed());
        }
    }

    return true;
}

PkXmlElement KoDocumentInfo::saveAuthorInfo(PkXmlDocument &doc)
{
    PkXmlElement e = doc.createElement("author");
    PkXmlElement t;

    for (const PkString &tag : m_authorTags) {
        if (tag == "creator")
            t = doc.createElement("full-name");
        else
            t = doc.createElement(tag);

        e.appendChild(t);
        t.appendChild(doc.createTextNode(authorInfo(tag)));
    }
    for (int i=0; i<m_contact.keys().size(); i++) {
        t = doc.createElement("contact");
        e.appendChild(t);
        PkString key = m_contact.keys().at(i);
        t.setAttribute("type", m_contact[key]);
        t.appendChild(doc.createTextNode(key));
    }

    return e;
}


bool KoDocumentInfo::loadAboutInfo(const PkXmlElement &root)
{
    PkXmlElement e = root.firstChildElement("about");
    if(e.isNull()) {
        return false;
    }

    for (e = e.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
        setAboutInfo(e.tagName(), e.text());
    }

    return true;
}

PkXmlElement KoDocumentInfo::saveAboutInfo(PkXmlDocument &doc)
{
    PkXmlElement e = doc.createElement("about");
    PkXmlElement t;

    for (const PkString &tag : m_aboutTags) {
        if (tag == "abstract") {
            t = doc.createElement("abstract");
            e.appendChild(t);
            t.appendChild(doc.createCDATASection(aboutInfo(tag)));
        } else {
            t = doc.createElement(tag);
            e.appendChild(t);
            t.appendChild(doc.createTextNode(aboutInfo(tag)));
        }
    }

    return e;
}

void KoDocumentInfo::updateParametersAndBumpNumCycles(bool autosaving, bool documentModified)
{
    if (autosaving) {
        return;
    }

    setAboutInfo("editing-cycles", PkString(std::to_string(aboutInfo("editing-cycles").toInt() + 1).c_str()));
    setAboutInfo("date", PkString(PkDateTime::currentDateTime().toString(PkDateTime::DateFormat::ISODate).c_str()));

    updateParameters(documentModified);
}

void KoDocumentInfo::updateParameters(bool documentModified)
{
    if (!documentModified) {
        return;
    }

    PkSharedConfig *config = PkSharedConfig::openConfig();
    PkConfigGroup appAuthorGroup(config, "Author");
    PkString profile = appAuthorGroup.readEntry("active-profile", "");

    PkString authorInfo = KoResourcePaths::getAppDataLocation() + "/authorinfo/";
    const std::filesystem::path authorInfoDir(authorInfo.PkToUtf8());
    const std::string profileFile = profile.PkToUtf8() + ".authorinfo";

    //Anon case
    setActiveAuthorInfo("creator", PkString());
    setActiveAuthorInfo("initial", "");
    setActiveAuthorInfo("author-title", "");
    setActiveAuthorInfo("position", "");
    setActiveAuthorInfo("company", "");
    std::error_code ec;
    if (std::filesystem::exists(authorInfoDir / profileFile, ec)) {
        PkFileStream file(PkString((authorInfoDir / profileFile).c_str()));
        if (file.open(PkStream::ReadOnly)) {
            PkXmlDocument doc;
            doc.setContent(&file);
            file.close();
            PkXmlElement root = doc.firstChildElement();

            PkXmlElement el = root.firstChildElement("nickname");
            if (!el.isNull()) {
                setActiveAuthorInfo("creator", el.text());
            }
            el = root.firstChildElement("givenname");
            if (!el.isNull()) {
                setActiveAuthorInfo("creator-first-name", el.text());
            }
            el = root.firstChildElement("middlename");
            if (!el.isNull()) {
                setActiveAuthorInfo("initial", el.text());
            }
            el = root.firstChildElement("familyname");
            if (!el.isNull()) {
               setActiveAuthorInfo("creator-last-name", el.text());
            }
            el = root.firstChildElement("title");
            if (!el.isNull()) {
                setActiveAuthorInfo("author-title", el.text());
            }
            el = root.firstChildElement("position");
            if (!el.isNull()) {
                setActiveAuthorInfo("position", el.text());
            }
            el = root.firstChildElement("company");
            if (!el.isNull()) {
                setActiveAuthorInfo("company", el.text());
            }

            m_contact.clear();
            el = root.firstChildElement("contact");
            while (!el.isNull()) {
                m_contact.insert(el.text(), el.attribute("type"));
                el = el.nextSiblingElement("contact");
            }
        }
    }

    //allow author info set programmatically to override info from author profile
    for (const PkString &tag : m_authorTags) {
        if (m_authorInfoOverride.contains(tag)) {
            setActiveAuthorInfo(tag, m_authorInfoOverride.value(tag));
        }
    }
}

void KoDocumentInfo::resetMetaData()
{
    setAboutInfo("editing-cycles", PkString(std::to_string(0).c_str()));
    setAboutInfo("initial-creator", authorInfo("creator"));
    setAboutInfo("creation-date", PkString(PkDateTime::currentDateTime().toString(PkDateTime::DateFormat::ISODate).c_str()));
    setAboutInfo("editing-time", PkString(std::to_string(0).c_str()));
}

PkString KoDocumentInfo::originalGenerator() const
{
    return m_generator;
}

void KoDocumentInfo::setOriginalGenerator(const PkString &generator)
{
    m_generator = generator;
}
