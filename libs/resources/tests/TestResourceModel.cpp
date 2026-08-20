/*
 * SPDX-FileCopyrightText: 2018 boud <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "TestResourceModel.h"

#include <simpletest.h>
#include <QStandardPaths>
#include <QDir>
#include <QVersionNumber>
#include <QDirIterator>
#include <PkSqlQuery.h>
#include <QTemporaryFile>

#include <PkConfigGroup.h>
#include <PkSharedConfig.h>

#include <KisResourceCacheDb.h>
#include <KisResourceLocator.h>
#include <KisResourceModel.h>

#include <DummyResource.h>
#include <ResourceTestHelper.h>

#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing installing resources"
#endif


void TestResourceModel::initTestCase()
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
        qWarning() << "Could not initialize KisResourceCacheDb on" << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    QVERIFY(KisResourceCacheDb::isValid());

    KisResourceLocator::LocatorError r =
        m_locator->initialize(ResourceTestHelper::toPkString(m_srcLocation));
    if (!m_locator->errorMessages().isEmpty()) {
        for (const PkString &message : m_locator->errorMessages()) {
            qDebug() << ResourceTestHelper::toQString(message);
        }
    }

    QVERIFY(r == KisResourceLocator::LocatorError::Ok);
    QVERIFY(QDir(m_dstLocation).exists());
}

void TestResourceModel::testWithTagModelTester()
{
    KisResourceModel model(m_resourceType);
    const auto records = model.records();
    for (const KisResourceRecord &record : records) {
        const KoResourceSP resource = model.resourceForId(record.id);
        QVERIFY(resource);
        QCOMPARE(resource->resourceId(), record.id);
    }
}


void TestResourceModel::testRowCount()
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
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);
    QCOMPARE(static_cast<int>(resourceModel.records().size()), rowCount);
}

void TestResourceModel::testData()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    QStringList resourceNames;

    for (const KisResourceRecord &record : resourceModel.records()) {
        resourceNames << ResourceTestHelper::toQString(record.name);
    }

    QVERIFY(resourceNames.contains("test0.kpp"));
    QVERIFY(resourceNames.contains("test1.kpp"));
    QVERIFY(resourceNames.contains("test2.kpp"));
}


void TestResourceModel::testResourceForIndex()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    const auto records = resourceModel.records();
    QVERIFY(!records.empty());
    KoResourceSP resource = resourceModel.resourceForId(records[0].id);
    QVERIFY(resource);
    QVERIFY(resource->resourceId() > -1);
}

void TestResourceModel::testIndexFromResource()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    const auto records = resourceModel.records();
    QVERIFY(records.size() > 1);
    KoResourceSP resource = resourceModel.resourceForId(records[1].id);
    QVERIFY(resource);
    QCOMPARE(resource->resourceId(), records[1].id);
}

void TestResourceModel::testSetInactiveByIndex()
{
    KisResourceModel resourceModel(m_resourceType);
    const auto initialRecords = resourceModel.records();
    int resourceCount = static_cast<int>(initialRecords.size());
    QVERIFY(!initialRecords.empty());
    KoResourceSP resource = resourceModel.resourceForId(initialRecords[0].id);
    bool r = resourceModel.setResourceInactive(resource);
    QVERIFY(r);
    QCOMPARE(resourceCount - 1, static_cast<int>(resourceModel.records().size()));
    QVERIFY(!resourceModel.resourceForId(resource->resourceId()));
    QVERIFY(resourceModel.resourcesForName(resource->name()).isEmpty());
    // verify that all mapped resources are still reachable by id
    for (const KisResourceRecord &record : resourceModel.records()) {
        KoResourceSP resource2 = resourceModel.resourceForId(record.id);
        QVERIFY(resource2);
        QVERIFY(resourceModel.resourceForId(resource2->resourceId()));
    }
}

void TestResourceModel::testImportResourceFile()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    QTemporaryFile f(QDir::tempPath() + "/testresourcemodel-testimportresourcefile-XXXXXX.kpp");
    KIS_ASSERT(f.open());
    f.write("0");
    f.close();

    int resourceCount = static_cast<int>(resourceModel.records().size());
    bool r = bool(resourceModel.importResourceFile(
        ResourceTestHelper::toPkString(f.fileName()), false));
    QVERIFY(r);
    QCOMPARE(static_cast<int>(resourceModel.records().size()), resourceCount + 1);
}

void TestResourceModel::testAddResource()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    int resourceCount = static_cast<int>(resourceModel.records().size());
    KoResourceSP resource(new DummyResource("dummy.kpp"));
    resource->setValid(true);
    bool r = resourceModel.addResource(resource);
    QVERIFY(r);
    QCOMPARE(resourceCount + 1, static_cast<int>(resourceModel.records().size()));
}

void TestResourceModel::testAddTemporaryResource()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    int startResourceCount = static_cast<int>(resourceModel.records().size());
    KoResourceSP resource(new DummyResource("temporaryResource.kpp"));
    resource->setValid(true);
    bool r = resourceModel.addResource(resource, "memory");
    QVERIFY(r);
    QCOMPARE(startResourceCount + 1, static_cast<int>(resourceModel.records().size()));
}

void TestResourceModel::testAddDuplicatedResource()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    const int startResourceCount = static_cast<int>(resourceModel.records().size());

    KoResourceSP resource(new DummyResource("duplicated_resource.kpp"));
    resource->setValid(true);
    bool r = resourceModel.addResource(resource); // first add to the folder storage

    QVERIFY(r);
    QCOMPARE(static_cast<int>(resourceModel.records().size()), startResourceCount + 1);

    // Matching MD5, name and filename resources should be hidden -- BUG:445367
    // the copy of this resource has been added in testAddResource()
    resource.reset(new DummyResource("duplicated_resource.kpp"));
    resource->setValid(true);
    r = resourceModel.addResource(resource, "memory"); // then add to the temporary storage

    QVERIFY(r);
    QCOMPARE(static_cast<int>(resourceModel.records().size()), startResourceCount + 1);
}

void TestResourceModel::testResourceForId()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    const auto records = resourceModel.records();
    QVERIFY(!records.empty());
    KoResourceSP resource = resourceModel.resourceForId(records[0].id);
    QVERIFY(!resource.isNull());
    KoResourceSP resource2 = resourceModel.resourceForId(resource->resourceId());
    QVERIFY(!resource2.isNull());
    QVERIFY(resource == resource2);
}

void TestResourceModel::testResourceForName()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    const auto records = resourceModel.records();
    QVERIFY(!records.empty());
    KoResourceSP resource = resourceModel.resourceForId(records[0].id);
    QVERIFY(!resource.isNull());
    KoResourceSP resource2 = resourceModel.resourcesForName(resource->name()).first();
    QVERIFY(!resource2.isNull());
    QVERIFY(resource == resource2);
}

void TestResourceModel::testResourceForFileName()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    const auto records = resourceModel.records();
    QVERIFY(!records.empty());
    KoResourceSP resource = resourceModel.resourceForId(records[0].id);
    QVERIFY(!resource.isNull());
    KoResourceSP resource2 = resourceModel.resourcesForFilename(resource->filename()).first();
    QVERIFY(!resource2.isNull());
    QVERIFY(resource == resource2);
}

void TestResourceModel::testResourceForMD5()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    const auto records = resourceModel.records();
    QVERIFY(!records.empty());
    KoResourceSP resource = resourceModel.resourceForId(records[0].id);
    QVERIFY(!resource.isNull());
    const auto v = resourceModel.resourcesForMD5(resource->md5Sum());
    KoResourceSP resource2 = v.first();
    QVERIFY(!resource2.isNull());
    QCOMPARE(ResourceTestHelper::toQString(resource->md5Sum()),
             ResourceTestHelper::toQString(resource2->md5Sum()));
}

void TestResourceModel::testRenameResource()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    const auto records = resourceModel.records();
    QVERIFY(records.size() > 1);
    KoResourceSP resource = resourceModel.resourceForId(records[1].id);
    QVERIFY(!resource.isNull());
    const PkString name = resource->name();
    bool r = resourceModel.renameResource(resource, "A New Name");
    QVERIFY(r);
    PkSqlQuery q;
    if (!q.prepare("SELECT name\n"
                   "FROM   resources\n"
                   "WHERE  id = :resource_id\n")) {
        qWarning() << "Could not prepare testRenameResource Query"
                   << ResourceTestHelper::toQString(q.lastError().text());
    }

    q.bindValue(":resource_id", resource->resourceId());

    if (!q.exec()) {
        qWarning() << "Could not execute testRenameResource Query"
                   << ResourceTestHelper::toQString(q.lastError().text());
    }

    q.first();
    QString newName = ResourceTestHelper::toQString(q.value(0).toString());
    QVERIFY(ResourceTestHelper::toQString(name) != newName);
    QCOMPARE("A New Name", newName);
}

void TestResourceModel::testUpdateResource()
{
    int resourceId;
    {
        KisResourceModel resourceModel(m_resourceType);
        resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

        const auto records = resourceModel.records();
        QVERIFY(!records.empty());
        KoResourceSP resource = resourceModel.resourceForId(records[0].id);
        QVERIFY(resource);
        resource.dynamicCast<DummyResource>()->setSomething("It's changed");
        resourceId = resource->resourceId();
        bool r = resourceModel.updateResource(resource);
        QVERIFY(r);
    }

    {
        // Check the resource itself
        KisResourceLocator::instance()->purge("", {});
        KoResourceSP resource = KisResourceLocator::instance()->resourceForId(resourceId);

        QVERIFY(resource);
        QCOMPARE(resource.dynamicCast<DummyResource>()->something(), "It's changed");
        QVERIFY(resource->resourceId() == resourceId);

        // Check the versions in the database
        PkSqlQuery q;
        QVERIFY(q.prepare("SELECT count(*)\n"
                          "FROM   versioned_resources\n"
                          "WHERE  resource_id = :resource_id\n"));
        q.bindValue(":resource_id", resourceId);
        QVERIFY(q.exec());
        q.first();
        int rowCount = q.value(0).toInt();
        QCOMPARE(rowCount, 2);
    }
}

void TestResourceModel::testTwoExistingResourceModels()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    KisResourceModel resourceModelCopy(m_resourceType);
    resourceModelCopy.setResourceFilter(KisResourceModel::ShowAllResources);


    int resourceCount = static_cast<int>(resourceModel.records().size());
    KoResourceSP resource(new DummyResource("dummy_1.kpp"));
    resource->setValid(true);


    bool r = resourceModel.addResource(resource);
    QVERIFY(r);


    // it only works if you uncomment this line
    // but it should work without it
    // resourceModelCopy.invalidate();

    QCOMPARE(static_cast<int>(resourceModel.records().size()),
             static_cast<int>(resourceModelCopy.records().size()));
    QCOMPARE(resourceCount + 1, static_cast<int>(resourceModel.records().size()));
    QCOMPARE(resourceCount + 1, static_cast<int>(resourceModelCopy.records().size()));
}


void TestResourceModel::cleanupTestCase()
{
    ResourceTestHelper::rmTestDb();
    ResourceTestHelper::cleanDstLocation(m_dstLocation);
}


SIMPLE_TEST_MAIN(TestResourceModel)
