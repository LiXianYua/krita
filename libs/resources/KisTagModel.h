/*
 * SPDX-FileCopyrightText: 2018 boud <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2020 Agata Cacko <cacko.azh@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISALLTAGSMODEL_H
#define KISALLTAGSMODEL_H

#include <PkObject.h>
#include <PkSharedPointer.h>
#include <PkString.h>
#include <PkVector.h>

#include <KisTag.h>
#include <KoResource.h>

#include "kritaresources_export.h"

class KRITARESOURCES_EXPORT KisAbstractTagModel
{
public:
    virtual ~KisAbstractTagModel() = default;

    virtual PkVector<KisTagSP> tags() const = 0;
    virtual KisTagSP tagForUrl(const PkString &url) const = 0;
    virtual KisTagSP addTag(const PkString &tagName,
                            bool allowOverwrite,
                            PkVector<KoResourceSP> taggedResources) = 0;
    virtual bool addTag(const KisTagSP &tag,
                        bool allowOverwrite,
                        PkVector<KoResourceSP> taggedResources = {}) = 0;
    virtual bool setTagActive(const KisTagSP &tag) = 0;
    virtual bool setTagInactive(const KisTagSP &tag) = 0;
    virtual bool renameTag(const KisTagSP &tag,
                           const PkString &newName,
                           bool allowOverwrite) = 0;
    virtual bool changeTagActive(const KisTagSP &tag, bool active) = 0;
};

/** Shared cached snapshot of every tag for one resource type. */
class KRITARESOURCES_EXPORT KisAllTagsModel final
    : public PkObject
    , public KisAbstractTagModel
{
public:
    enum Ids {
        All = -2,
        AllUntagged = -1,
    };

    ~KisAllTagsModel() override;

    PkVector<KisTagSP> tags() const override;
    KisTagSP tagForUrl(const PkString &tagUrl) const override;
    KisTagSP addTag(const PkString &tagName,
                    bool allowOverwrite,
                    PkVector<KoResourceSP> taggedResources) override;
    bool addTag(const KisTagSP &tag,
                bool allowOverwrite,
                PkVector<KoResourceSP> taggedResources = {}) override;
    bool setTagActive(const KisTagSP &tag) override;
    bool setTagInactive(const KisTagSP &tag) override;
    bool renameTag(const KisTagSP &tag,
                   const PkString &newName,
                   bool allowOverwrite) override;
    bool changeTagActive(const KisTagSP &tag, bool active) override;

    static PkString urlAll() { return PkString("All"); }
    static PkString urlAllUntagged() { return PkString("All untagged"); }

private:
    friend class KisResourceModelProvider;
    friend class KisTagModel;

    explicit KisAllTagsModel(const PkString &resourceType,
                             PkObject *parent = nullptr);

    KisTagSP specialTag(Ids id) const;
    void untagAllResources(const KisTagSP &tag);
    void storageChanged(const PkString &location);
    bool refresh();
    void closeQuery();

    struct Private;
    Private *const d;
};

/** Ordinary active/storage filtering over the shared tag snapshot. */
class KRITARESOURCES_EXPORT KisTagModel : public KisAbstractTagModel
{
public:
    enum TagFilter {
        ShowInactiveTags = 0,
        ShowActiveTags,
        ShowAllTags
    };

    enum StorageFilter {
        ShowInactiveStorages = 0,
        ShowActiveStorages,
        ShowAllStorages
    };

    explicit KisTagModel(const PkString &type);
    ~KisTagModel() override;

    KisTagModel(const KisTagModel &) = delete;
    KisTagModel &operator=(const KisTagModel &) = delete;

    void setTagFilter(TagFilter filter);
    void setStorageFilter(StorageFilter filter);

    PkVector<KisTagSP> tags() const override;
    KisTagSP tagForUrl(const PkString &url) const override;
    KisTagSP addTag(const PkString &tagName,
                    bool allowOverwrite,
                    PkVector<KoResourceSP> taggedResources) override;
    bool addTag(const KisTagSP &tag,
                bool allowOverwrite,
                PkVector<KoResourceSP> taggedResources = {}) override;
    bool setTagInactive(const KisTagSP &tag) override;
    bool setTagActive(const KisTagSP &tag) override;
    bool renameTag(const KisTagSP &tag,
                   const PkString &newName,
                   bool allowOverwrite) override;
    bool changeTagActive(const KisTagSP &tag, bool active) override;

private:
    bool accepts(const KisTagSP &tag) const;

    struct Private;
    Private *const d;
};

using KisAllTagsModelSP = PkSharedPointer<KisAllTagsModel>;

#endif // KISALLTAGSMODEL_H
