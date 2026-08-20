/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISTAGLOADER_H
#define KISTAGLOADER_H

#include <PkString.h>
#include <PkStringList.h>
#include <PkMap.h>
#include <PkSharedPointer.h>
#include <PkScopedPointer.h>
#include <PkStream.h>
#include <PkDebug.h>

#include "kritaresources_export.h"

class KisTag;
typedef PkSharedPointer<KisTag> KisTagSP;


/**
 * @brief The KisTag loads a tag from a .tag file.
 * A .tag file is a .desktop file. The following fields
 * are important:
 *
 * name: the name of the tag, which can be translated
 * comment: a tooltip for the tag, which can be translated
 * url: the untranslated name of the tag
 *
 */
class KRITARESOURCES_EXPORT KisTag
{
public:
    KisTag();
    virtual ~KisTag();
    KisTag(const KisTag &rhs);
    KisTag &operator=(const KisTag &rhs);
    KisTagSP clone() const;

    static PkString currentLocale();

    bool valid() const;

    int id() const;
    bool active() const;

    PkString filename();
    void setFilename(const PkString &fileName);

    /// The unique identifier for the tag. Since tag urls are compared COLLATE NOCASE, tag urls must be ASCII only.
    PkString url() const;
    void setUrl(const PkString &url);

    /// The translated names of the tag
    PkString name(bool translated = true) const;
    void setName(const PkString &name);
    PkMap<PkString, PkString> names() const;
    void setNames(const PkMap<PkString, PkString> &names);

    /// The translated tooltips for the tag
    PkString comment(bool translated = true) const;
    void setComment(const PkString comment);
    PkMap<PkString, PkString> comments() const;
    void setComments(const PkMap<PkString, PkString> &comments);

    PkString resourceType() const;
    void setResourceType(const PkString &resourceType);

    PkStringList defaultResources() const;
    void setDefaultResources(const PkStringList &defaultResources);

    bool load(PkStream &io);
    bool save(PkStream &io);

private:

    friend class KisTagModel;
    friend class KisAllTagsModel;
    friend class KisAllTagResourceModel;
    friend class KisAllResourcesModel;
    friend class KisResourceModel;
    friend class KisTagChooserWidget;
    friend class TestTagModel;
    friend class KisResourceLocator;
    friend class BundleTagIterator;
    friend class AbrTagIterator;
    friend class TestMemoryStorage;
    friend class TestResourceLocator;
    friend class TestStorageWrapper;

    void setId(int id);
    void setActive(bool active);
    void setValid(bool valid);

    static const PkString s_group;
    static const PkString s_type;
    static const PkString s_tag;
    static const PkString s_name;
    static const PkString s_resourceType;
    static const PkString s_url;
    static const PkString s_comment;
    static const PkString s_defaultResources;
    static const PkString s_desktop;

    class Private;
    PkScopedPointer<Private> d;
};


inline PkDebug operator<<(PkDebug dbg, const KisTagSP tag)
{
    if (tag) {
        dbg.space() << "[TAG] Name" << tag->name()
                    << "Url" << tag->url()
                    << "Comment" << tag->comment()
                    << "Default resources" << tag->defaultResources().join(", ");
    } else {
        dbg.space() << "[TAG] NULL";
    }

    return dbg.space();
}

#endif // KISTAGLOADER_H
