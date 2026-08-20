/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisResourceModelProvider.h"

#include <PkMap.h>

#include <memory>

#include "KisResourceMetaDataModel.h"
#include "KisResourceModel.h"
#include "KisTagModel.h"
#include "KisTagResourceModel.h"

struct KisResourceModelProvider::Private
{
    PkMap<PkString, KisAllResourcesModel *> resourceModels;
    PkMap<PkString, KisAllTagsModel *> tagModels;
    PkMap<PkString, KisAllTagResourceModel *> tagResourceModels;
    std::unique_ptr<KisResourceMetaDataModel> metaDataModel;
};

namespace
{

KisResourceModelProvider &provider()
{
    static KisResourceModelProvider instance;
    return instance;
}

} // namespace

KisResourceModelProvider::KisResourceModelProvider()
    : d(new Private)
{
}

KisResourceModelProvider::~KisResourceModelProvider()
{
    for (KisAllResourcesModel *model : d->resourceModels) {
        delete model;
    }
    for (KisAllTagsModel *model : d->tagModels) {
        delete model;
    }
    for (KisAllTagResourceModel *model : d->tagResourceModels) {
        delete model;
    }
    delete d;
}

KisAllResourcesModel *KisResourceModelProvider::resourceModel(
    const PkString &resourceType)
{
    KisResourceModelProvider &instance = provider();
    if (!instance.d->resourceModels.contains(resourceType)) {
        instance.d->resourceModels.insert(resourceType,
                                          new KisAllResourcesModel(resourceType));
    }
    return instance.d->resourceModels.value(resourceType);
}

bool KisResourceModelProvider::refreshResourceModel(
    const PkString &resourceType)
{
    KisAllResourcesModel *model = resourceModel(resourceType);
    return model && model->refresh();
}

bool KisResourceModelProvider::refreshTagResourceModel(
    const PkString &resourceType)
{
    KisAllTagResourceModel *model = tagResourceModel(resourceType);
    return model && model->refresh();
}

KisAllTagsModel *KisResourceModelProvider::tagModel(
    const PkString &resourceType)
{
    KisResourceModelProvider &instance = provider();
    if (!instance.d->tagModels.contains(resourceType)) {
        instance.d->tagModels.insert(resourceType,
                                     new KisAllTagsModel(resourceType));
    }
    return instance.d->tagModels.value(resourceType);
}

KisAllTagResourceModel *KisResourceModelProvider::tagResourceModel(
    const PkString &resourceType)
{
    KisResourceModelProvider &instance = provider();
    if (!instance.d->tagResourceModels.contains(resourceType)) {
        instance.d->tagResourceModels.insert(
            resourceType,
            new KisAllTagResourceModel(resourceType));
    }
    return instance.d->tagResourceModels.value(resourceType);
}

void KisResourceModelProvider::testingResetAllModels()
{
    KisResourceModelProvider &instance = provider();
    for (KisAllTagsModel *model : instance.d->tagModels) {
        model->refresh();
    }
    for (KisAllResourcesModel *model : instance.d->resourceModels) {
        model->refresh();
    }
    for (KisAllTagResourceModel *model : instance.d->tagResourceModels) {
        model->refresh();
    }
    instance.d->metaDataModel.reset();
}

void KisResourceModelProvider::testingCloseAllQueries()
{
    KisResourceModelProvider &instance = provider();
    for (KisAllTagsModel *model : instance.d->tagModels) {
        model->closeQuery();
    }
    for (KisAllResourcesModel *model : instance.d->resourceModels) {
        model->closeQuery();
    }
    for (KisAllTagResourceModel *model : instance.d->tagResourceModels) {
        model->closeQuery();
    }
    instance.d->metaDataModel.reset();
}

KisResourceMetaDataModel *KisResourceModelProvider::resourceMetadataModel()
{
    KisResourceModelProvider &instance = provider();
    if (!instance.d->metaDataModel) {
        instance.d->metaDataModel.reset(
            new KisResourceMetaDataModel(PkString("resources")));
    }
    return instance.d->metaDataModel.get();
}
