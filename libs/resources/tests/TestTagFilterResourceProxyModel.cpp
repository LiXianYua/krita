/*
 * SPDX-FileCopyrightText: 2019 boud <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "TestTagFilterResourceProxyModel.h"

#include <algorithm>

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
#include <KisTagFilterResourceProxyModel.h>
#include <KisStorageModel.h>

#include <DummyResource.h>
#include <ResourceTestHelper.h>

#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing installing resources"
#endif


void TestTagFilterResourceProxyModel::initTestCase()
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

void TestTagFilterResourceProxyModel::testWithTagModelTester()
{
    KisTagFilterResourceProxyModel model(m_resourceType);
    for (const KisResourceRecord &record : model.records()) {
        const KoResourceSP resource = model.resourceForId(record.id);
        QVERIFY(resource);
        QCOMPARE(resource->resourceId(), record.id);
    }
}


void TestTagFilterResourceProxyModel::testRowCount()
{
    PkSqlQuery q;
    QVERIFY(q.prepare("SELECT count(*)\n"
                      "FROM   resources\n"
                      ",      resource_types\n"
                      "WHERE  resources.resource_type_id = resource_types.id\n"
                      "AND    resource_types.name = :resource_type"));
    q.bindValue(":resource_type", m_resourceType);
    QVERIFY(q.exec());
    q.first();
    int rowCount = q.value(0).toInt();
    QVERIFY(rowCount == 3);
    KisTagFilterResourceProxyModel proxyModel(m_resourceType);
    QCOMPARE(static_cast<int>(proxyModel.records().size()), rowCount);
}

void TestTagFilterResourceProxyModel::testData()
{
    KisTagFilterResourceProxyModel proxyModel(m_resourceType);

    QStringList names = QStringList() << "test0.kpp"
                                      << "test1.kpp"
                                      << "test2.kpp";
    for (const KisResourceRecord &record : proxyModel.records()) {
        QVERIFY(names.contains(ResourceTestHelper::toQString(record.name)));
    }
}


void TestTagFilterResourceProxyModel::testResource()
{
    KisTagFilterResourceProxyModel proxyModel(m_resourceType);
    const auto records = proxyModel.records();
    QVERIFY(!records.empty());
    KoResourceSP resource = proxyModel.resourceForId(records[0].id);
    QVERIFY(resource);
}

void TestTagFilterResourceProxyModel::testFilterByTag()
{
    KisResourceModel resourceModel(ResourceType::PaintOpPresets);
    KisTagModel tagModel(ResourceType::PaintOpPresets);
    KisTagFilterResourceProxyModel proxyModel(m_resourceType);

    KoResourceSP resource = resourceModel.resourcesForName("test2.kpp").first();
    QVERIFY(resource);

    const auto tags = tagModel.tags();
    QVERIFY(tags.size() > 2);
    KisTagSP tag = tags[2];
    QVERIFY(tag);

    proxyModel.setTagFilter(tag);
    int rowCount = static_cast<int>(proxyModel.records().size());

    proxyModel.tagResources(tag, PkVector<int>() << resource->resourceId());
    QCOMPARE(static_cast<int>(proxyModel.records().size()), rowCount + 1);

    proxyModel.untagResources(tag, PkVector<int>() << resource->resourceId());
    QCOMPARE(static_cast<int>(proxyModel.records().size()), rowCount);
}

void TestTagFilterResourceProxyModel::testFilterByResource()
{
    KisResourceModel resourceModel(ResourceType::PaintOpPresets);
    KisTagModel tagModel(ResourceType::PaintOpPresets);

    KisTagFilterResourceProxyModel proxyModel(m_resourceType);

    KoResourceSP resource = resourceModel.resourcesForName("test2.kpp").first();

    QVERIFY(resource);

    tagModel.addTag("testtag1", false, PkVector<KoResourceSP>() << resource);
    tagModel.addTag("testtag2", false, PkVector<KoResourceSP>() << resource);

    int rowCount = static_cast<int>(proxyModel.records().size());

    proxyModel.setResourceFilter(resource);
    proxyModel.setFilterInCurrentTag(false);

    QCOMPARE(static_cast<int>(proxyModel.records().size()), 2);

    proxyModel.setResourceFilter(0);
    QCOMPARE(static_cast<int>(proxyModel.records().size()), rowCount);

}

void TestTagFilterResourceProxyModel::testFilterByString()
{
    KisResourceModel resourceModel(ResourceType::PaintOpPresets);
    KisTagModel tagModel(ResourceType::PaintOpPresets);

    KisTagFilterResourceProxyModel proxyModel(m_resourceType);
    proxyModel.setSearchText("test2");
    QCOMPARE(static_cast<int>(proxyModel.records().size()), 1);

    KoResourceSP resource = resourceModel.resourcesForName("test2.kpp").first();
    QVERIFY(resource);

    const auto tags = tagModel.tags();
    QVERIFY(tags.size() > 2);
    KisTagSP tag = tags[2];
    QVERIFY(tag);

    proxyModel.tagResources(tag, PkVector<int>() << resource->resourceId());
    proxyModel.setTagFilter(tag);
    proxyModel.setFilterInCurrentTag(true);

    QCOMPARE(static_cast<int>(proxyModel.records().size()), 1);
}

void TestTagFilterResourceProxyModel::testFilterByStorage()
{
    KisResourceModel resourceModel(ResourceType::PaintOpPresets);
    KisTagModel tagModel(ResourceType::PaintOpPresets);

    KisTagFilterResourceProxyModel proxyModel(m_resourceType);

    proxyModel.setFilterInCurrentTag(false);
    proxyModel.setStorageFilter(true, 1);
    proxyModel.setSearchText("");
    proxyModel.setMetaDataFilter(PkMap<PkString, PkVariant>());
    proxyModel.setResourceFilter(0);

    QCOMPARE(static_cast<int>(proxyModel.records().size()), 3);

}


void TestTagFilterResourceProxyModel::testDataWhenSwitchingBetweenTagAllAllUntagged()
{
    KisTagFilterResourceProxyModel proxyModel(m_resourceType);
    KisResourceModel resourceModel(m_resourceType);

    KoResourceSP resource = resourceModel.resourcesForName("test2.kpp").first();
    const auto records = proxyModel.records();
    const auto found = std::find_if(records.begin(), records.end(), [id = resource->resourceId()](const KisResourceRecord &record) {
        return record.id == id;
    });

    QVERIFY(found != records.end());

    QString name = ResourceTestHelper::toQString(found->name);
    QCOMPARE(name, "test2.kpp");

    QVERIFY(!found->thumbnail.isNull());

    proxyModel.setSearchText("test2");
    const auto filteredRecords = proxyModel.records();
    QVERIFY(std::any_of(filteredRecords.begin(), filteredRecords.end(), [id = resource->resourceId()](const KisResourceRecord &record) {
        return record.id == id;
    }));
}

void TestTagFilterResourceProxyModel::testResourceForIndex()
{
    KisTagModel tagModel(ResourceType::PaintOpPresets);
    KisTagFilterResourceProxyModel proxyModel(m_resourceType);
    KisResourceModel resourceModel(m_resourceType);

    KoResourceSP resource = resourceModel.resourcesForName("test2.kpp").first();
    QVERIFY(resource);

    resource = proxyModel.resourceForId(resource->resourceId());
    QVERIFY(resource);


    KisTagResourceModel tagResourceModel(ResourceType::PaintOpPresets);
    tagResourceModel.setResourcesFilter(PkVector<KoResourceSP>() << resource);
    for (const KisTagResourceRecord &relation : tagResourceModel.relations()) {
        tagResourceModel.untagResources(relation.tag, PkVector<int>() << resource->resourceId());
    }

    const auto tags = tagModel.tags();
    QVERIFY(tags.size() > 3);
    KisTagSP tag = tags[3];
    QVERIFY(tag);

    proxyModel.setTagFilter(tag);
    int rowCount = static_cast<int>(proxyModel.records().size());

    QCOMPARE(rowCount, 0);

    proxyModel.tagResources(tag, PkVector<int>() << resource->resourceId());

    QCOMPARE(static_cast<int>(proxyModel.records().size()), 1);

    const auto taggedRecords = proxyModel.records();
    QVERIFY(!taggedRecords.empty());
    KoResourceSP resource2 = proxyModel.resourceForId(taggedRecords[0].id);

    QVERIFY(resource2);

}
void TestTagFilterResourceProxyModel::cleanupTestCase()
{
    ResourceTestHelper::rmTestDb();
    ResourceTestHelper::cleanDstLocation(m_dstLocation);
}

#include <kistest.h>
KISTEST_MAIN(TestTagFilterResourceProxyModel)
