/*
 * SPDX-FileCopyrightText: 2020 boud <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "TestTagResourceModel.h"

#include <simpletest.h>
#include <QStandardPaths>
#include <QDir>
#include <QVersionNumber>
#include <QDirIterator>
#include <PkSqlQuery.h>

#include <PkConfigGroup.h>
#include <PkSharedConfig.h>

#include <KisResourceCacheDb.h>
#include <KisResourceLocator.h>
#include <KisResourceModel.h>
#include <KisTagModel.h>
#include <KisTagResourceModel.h>
#include <KisResourceTypes.h>
#include <KisResourceModelProvider.h>

#include <DummyResource.h>
#include <ResourceTestHelper.h>

#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing installing resources"
#endif


void TestTagResourceModel::initTestCase()
{
    ResourceTestHelper::initTestDb();
    ResourceTestHelper::createDummyLoaderRegistry();

    m_srcLocation = QString(FILES_DATA_DIR);
    QVERIFY2(QDir(m_srcLocation).exists(), m_srcLocation.toUtf8());

    m_dstLocation = ResourceTestHelper::filesDestDir();
    ResourceTestHelper::cleanDstLocation(m_dstLocation);

    PkConfigGroup cfg(PkSharedConfig::openConfig(), PkString());
    cfg.writeEntry(KisResourceLocator::resourceLocationKey,
                   ResourceTestHelper::toPkString(m_dstLocation));

    m_locator = KisResourceLocator::instance();

    if (!KisResourceCacheDb::initialize(ResourceTestHelper::toPkString(
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)))) {
        qDebug() << "Could not initialize KisResourceCacheDb on" << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    QVERIFY(KisResourceCacheDb::isValid());

    KisResourceLocator::LocatorError r =
        m_locator->initialize(ResourceTestHelper::toPkString(m_srcLocation));
    for (const PkString &message : m_locator->errorMessages()) {
        qDebug() << ResourceTestHelper::toQString(message);
    }

    QVERIFY(r == KisResourceLocator::LocatorError::Ok);
    QVERIFY(QDir(m_dstLocation).exists());
}

void TestTagResourceModel::testWithTagModelTester()
{
    KisAllTagResourceModel *model =
        KisResourceModelProvider::tagResourceModel(ResourceType::PaintOpPresets);
    QVERIFY(model);
    for (const KisTagResourceRecord &relation : model->relations()) {
        QVERIFY(relation.tag);
        QVERIFY(relation.tag->valid());
        QCOMPARE(relation.tag->id(), relation.tagId);
        QCOMPARE(relation.resource.id, relation.resourceId);
    }
}


void TestTagResourceModel::testRowCount()
{
    PkSqlQuery q;
    QVERIFY(q.prepare("SELECT count(*)\n"
                      "FROM   resource_tags\n"
                      "WHERE  resource_tags.active = 1\n"));
    QVERIFY(q.exec());
    q.first();
    int rowCount = q.value(0).toInt();
    QCOMPARE(rowCount, 2);

    KisAllTagResourceModel *tagResourceModel =
        KisResourceModelProvider::tagResourceModel(ResourceType::PaintOpPresets);
    QVERIFY(tagResourceModel);
    QCOMPARE(static_cast<int>(tagResourceModel->relations().size()), rowCount);
}

void TestTagResourceModel::testData()
{
     KisAllTagResourceModel *tagResourceModel =
         KisResourceModelProvider::tagResourceModel(ResourceType::PaintOpPresets);
     QVERIFY(tagResourceModel);
     const auto relations = tagResourceModel->relations();
     QVERIFY(!relations.empty());
     const KisTagResourceRecord &relation = relations[0];

     QCOMPARE(relation.resource.id, 3);
     QCOMPARE(relation.resource.storageId, 1);
     QCOMPARE(ResourceTestHelper::toQString(relation.resource.name), QString("test0.kpp"));
     QCOMPARE(ResourceTestHelper::toQString(relation.resource.filename), QString("test0.kpp"));
     QCOMPARE(ResourceTestHelper::toQString(relation.resource.tooltip), QString("test0.kpp"));
     QVERIFY(!relation.resource.thumbnail.isNull());
     QCOMPARE(ResourceTestHelper::toQString(relation.resource.location), QString());
     QCOMPARE(ResourceTestHelper::toQString(relation.resource.resourceType),
              ResourceTestHelper::toQString(ResourceType::PaintOpPresets));
     QCOMPARE(static_cast<int>(relation.resource.tags.size()), 1);
     QCOMPARE(ResourceTestHelper::toQString(relation.resource.tags[0]), QString("* Favorites"));
     QVERIFY(!relation.resource.dirty);
     QCOMPARE(static_cast<int>(relation.resource.metaData.size()), 1);
     QVERIFY(relation.resource.resourceActive);
     QVERIFY(relation.resource.storageActive);

     QCOMPARE(relation.tagId, 1);
     QCOMPARE(relation.resourceId, 3);

     KisTagSP tag = relation.tag;
     QVERIFY(tag);
     QVERIFY(tag->valid());
     QCOMPARE(ResourceTestHelper::toQString(tag->name()), QString("* Favorites"));
     QCOMPARE(tag->id(), 1);

     KisResourceModel resourceModel(ResourceType::PaintOpPresets);
     KoResourceSP resource = resourceModel.resourceForId(relation.resourceId);
     QVERIFY(resource);
     QVERIFY(resource->valid());
     QCOMPARE(ResourceTestHelper::toQString(resource->name()), QString("test0.kpp"));
     QCOMPARE(resource->resourceId(), 3);

     QVERIFY(relation.tagActive);
     QVERIFY(relation.resourceActive);
     QVERIFY(relation.resourceStorageActive);
     QCOMPARE(ResourceTestHelper::toQString(relation.resource.name), QString("test0.kpp"));
     QCOMPARE(ResourceTestHelper::toQString(relation.tagName), QString("* Favorites"));
}

void TestTagResourceModel::testTagResource()
{
    KisResourceModel resourceModel(ResourceType::PaintOpPresets);
    KoResourceSP resource = resourceModel.resourcesForName("test2.kpp").first();
    Q_ASSERT(resource);

    KisTagModel tagModel(ResourceType::PaintOpPresets);
    const auto tags = tagModel.tags();
    QVERIFY(tags.size() > 2);
    KisTagSP tag = tags[2];
    Q_ASSERT(tag);

    KisAllTagResourceModel *tagResourceModel =
        KisResourceModelProvider::tagResourceModel(ResourceType::PaintOpPresets);
    QVERIFY(tagResourceModel);
    if (tagResourceModel->isResourceTagged(tag, resource->resourceId())) {
        tagResourceModel->untagResources(tag, { resource->resourceId() });
    }

    int rowCount = static_cast<int>(tagResourceModel->relations().size());

    QVERIFY(tagResourceModel->tagResources(tag, { resource->resourceId() }));

    QCOMPARE(static_cast<int>(tagResourceModel->relations().size()), rowCount + 1);
}

void TestTagResourceModel::testUntagResource()
{
    KisResourceModel resourceModel(ResourceType::PaintOpPresets);
    KoResourceSP resource = resourceModel.resourcesForName("test1.kpp").first();
    QVERIFY(resource);

    KisTagModel tagModel(ResourceType::PaintOpPresets);
    const auto tags = tagModel.tags();
    QVERIFY(tags.size() > 2);
    KisTagSP tag = tags[2];
    QVERIFY(tag);

    KisAllTagResourceModel *tagResourceModel =
        KisResourceModelProvider::tagResourceModel(ResourceType::PaintOpPresets);
    QVERIFY(tagResourceModel);

    if (!tagResourceModel->isResourceTagged(tag, resource->resourceId())) {
        tagResourceModel->tagResources(tag, { resource->resourceId() });
    }

    int rowCount = static_cast<int>(tagResourceModel->relations().size());
    tagResourceModel->untagResources(tag, { resource->resourceId() });

    QCOMPARE(static_cast<int>(tagResourceModel->relations().size()), rowCount - 1);
}

void TestTagResourceModel::testIsResourceTagged()
{
    KisResourceModel resourceModel(ResourceType::PaintOpPresets);
    KoResourceSP resource = resourceModel.resourcesForName("test2.kpp").first();
    Q_ASSERT(resource);

    KisTagModel tagModel(ResourceType::PaintOpPresets);
    const auto tags = tagModel.tags();
    QVERIFY(tags.size() > 2);
    KisTagSP tag = tags[2];
    Q_ASSERT(tag);

    KisAllTagResourceModel *tagResourceModel =
        KisResourceModelProvider::tagResourceModel(ResourceType::PaintOpPresets);
    QVERIFY(tagResourceModel);

    QVERIFY(tagResourceModel->tagResources(tag, { resource->resourceId() }));
    QCOMPARE(tagResourceModel->isResourceTagged(tag, resource->resourceId()), true);

    resource = resourceModel.resourcesForName("test1.kpp").first();
    QVERIFY(resource);

    tag = tags[2];
    QVERIFY(tag);

    tagResourceModel->untagResources(tag, { resource->resourceId() });
    QCOMPARE(tagResourceModel->isResourceTagged(tag, resource->resourceId()), false);
}

void TestTagResourceModel::testFilterTagResource()
{
    KisResourceModel resourceModel(ResourceType::PaintOpPresets);
    KoResourceSP resource = resourceModel.resourcesForName("test2.kpp").first();
    Q_ASSERT(resource);

    KisTagModel tagModel(ResourceType::PaintOpPresets);
    const auto tags = tagModel.tags();
    QVERIFY(tags.size() > 2);
    KisTagSP tag = tags[2];
    Q_ASSERT(tag);

    KisTagResourceModel tagResourceModel(ResourceType::PaintOpPresets);
    QCOMPARE(static_cast<int>(tagResourceModel.relations().size()), 2);

    PkVector<int> tagIds;
    tagIds << tag->id();
    tagResourceModel.setTagsFilter(tagIds);

    PkVector<int> resourceIds;
    resourceIds << resource->resourceId();
    tagResourceModel.setResourcesFilter(resourceIds);

    QCOMPARE(static_cast<int>(tagResourceModel.relations().size()), 1);
}


void TestTagResourceModel::testBeginEndInsert()
{
    KisTagResourceModel tagResourceModel(ResourceType::PaintOpPresets);
    KisResourceModel resourceModel(ResourceType::PaintOpPresets);
    KisTagModel tagModel(ResourceType::PaintOpPresets);

    PkVector<KoResourceSP> resources;
    PkVector<int> resourceIds;
    for (const KisResourceRecord &record : resourceModel.records()) {
        if (resources.size() < 3) {
            resources << resourceModel.resourceForId(record.id);
            resourceIds << record.id;
        }
    }
    QCOMPARE(static_cast<int>(resources.size()), 3);

    KisTagSP other1 = tagModel.addTag("Tag1", true, {});
    KisTagSP other2 = tagModel.addTag("Tag2", true, {});
    KisTagSP special = tagModel.addTag("TagSpecial", true, {});
    QVERIFY(other1 && other2 && special);

    QVERIFY(tagResourceModel.tagResources(other1, {resourceIds[0]}));
    QVERIFY(tagResourceModel.tagResources(other2, {resourceIds[1]}));
    QVERIFY(tagResourceModel.tagResources(special, {resourceIds[2], resourceIds[1], resourceIds[0]}));
    QVERIFY(tagResourceModel.tagResources(other2, {resourceIds[2]}));
    QVERIFY(tagResourceModel.tagResources(other1, {resourceIds[1]}));

    const int taggedCount = static_cast<int>(tagResourceModel.relations().size());
    for (int resourceId : resourceIds) {
        QVERIFY(tagResourceModel.isResourceTagged(special, resourceId));
    }

    QVERIFY(tagResourceModel.untagResources(special, {resourceIds[2], resourceIds[1], resourceIds[0]}));
    QCOMPARE(static_cast<int>(tagResourceModel.relations().size()), taggedCount - 3);
    for (int resourceId : resourceIds) {
        QVERIFY(!tagResourceModel.isResourceTagged(special, resourceId));
    }

    const int afterFirstRemoval = static_cast<int>(tagResourceModel.relations().size());
    QVERIFY(tagResourceModel.untagResources(special, {resourceIds[2], resourceIds[1], resourceIds[0]}));
    QCOMPARE(static_cast<int>(tagResourceModel.relations().size()), afterFirstRemoval);

    QVERIFY(tagResourceModel.tagResources(special, {resourceIds[2], resourceIds[1], resourceIds[0]}));
    QCOMPARE(static_cast<int>(tagResourceModel.relations().size()), afterFirstRemoval + 3);

    QVERIFY(tagResourceModel.untagResources(special, {resourceIds[2], resourceIds[0]}));
    QCOMPARE(static_cast<int>(tagResourceModel.relations().size()), afterFirstRemoval + 1);
    QVERIFY(tagResourceModel.isResourceTagged(special, resourceIds[1]));
    QVERIFY(!tagResourceModel.isResourceTagged(special, resourceIds[0]));
    QVERIFY(!tagResourceModel.isResourceTagged(special, resourceIds[2]));

    const int beforeExtraTags = static_cast<int>(tagResourceModel.relations().size());
    KisTagSP tagExtra1 = tagModel.addTag("tagExtra1", true, resources);
    QVERIFY(tagExtra1);
    QCOMPARE(static_cast<int>(tagResourceModel.relations().size()), beforeExtraTags + 3);

    KisTagSP tagExtra2 = tagModel.addTag("tagExtra2", true, {});
    QVERIFY(tagExtra2);
    QVERIFY(tagResourceModel.tagResources(tagExtra2, {resourceIds[2], resourceIds[1], resourceIds[0]}));
    QCOMPARE(static_cast<int>(tagResourceModel.relations().size()), beforeExtraTags + 6);
}

void TestTagResourceModel::cleanupTestCase()
{
    ResourceTestHelper::rmTestDb();
    ResourceTestHelper::cleanDstLocation(m_dstLocation);
}

#include <kistest.h>
KISTEST_MAIN(TestTagResourceModel)
