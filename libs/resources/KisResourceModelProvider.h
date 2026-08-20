/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISRESOURCEMODELPROVIDER_H
#define KISRESOURCEMODELPROVIDER_H

#include <PkString.h>

#include "kritaresources_export.h"

class KisAllResourcesModel;
class KisAllTagsModel;
class KisAllTagResourceModel;
class KisResourceMetaDataModel;

/** Owns one shared all-data model per resource type. */
class KRITARESOURCES_EXPORT KisResourceModelProvider
{
public:
    KisResourceModelProvider();
    ~KisResourceModelProvider();

    static KisAllResourcesModel *resourceModel(const PkString &resourceType);
    static KisAllTagsModel *tagModel(const PkString &resourceType);
    static KisAllTagResourceModel *tagResourceModel(const PkString &resourceType);

    static void testingResetAllModels();
    static void testingCloseAllQueries();
    static KisResourceMetaDataModel *resourceMetadataModel();

private:
    friend class KisAllTagsModel;
    friend class KisAllTagResourceModel;

    static bool refreshResourceModel(const PkString &resourceType);

    KisResourceModelProvider(const KisResourceModelProvider &) = delete;
    KisResourceModelProvider &operator=(const KisResourceModelProvider &) = delete;

    struct Private;
    Private *const d;
};

#endif // KISRESOURCEMODELPROVIDER_H
