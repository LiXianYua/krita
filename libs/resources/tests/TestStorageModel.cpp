/*
 * SPDX-FileCopyrightText: 2018 boud <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "TestStorageModel.h"

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
#include <KisStorageModel.h>

#include <DummyResource.h>
#include <ResourceTestHelper.h>


#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing installing resources"
#endif


void TestStorageModel::initTestCase()
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

void TestStorageModel::testWithTagModelTester()
{
    KisStorageModel model;
    for (const KisStorageRecord &record : model.storages()) {
        QVERIFY(record.id >= 0);
        QVERIFY(model.storageForId(record.id));
    }
}


void TestStorageModel::testRowCount()
{
    PkSqlQuery q;
    QVERIFY(q.prepare("SELECT count(*)\n"
                      "FROM   storages"));
    QVERIFY(q.exec());
    q.first();
    int rowCount = q.value(0).toInt();

    KisStorageModel storageModel;
    QCOMPARE(static_cast<int>(storageModel.storages().size()), rowCount);
}

void TestStorageModel::testSetActive()
{
    KisStorageModel storageModel;

    const auto initialStorages = storageModel.storages();
    for (const KisStorageRecord &initial : initialStorages) {
        QVERIFY(storageModel.setStorageActive(initial.id, true));
        const auto activeStorages = storageModel.storages();
        const auto active = std::find_if(activeStorages.begin(), activeStorages.end(),
                                         [id = initial.id](const KisStorageRecord &record) {
                                             return record.id == id;
                                         });
        QVERIFY(active != activeStorages.end());
        QVERIFY(active->active);

        QVERIFY(storageModel.setStorageActive(initial.id, false));
        const auto inactiveStorages = storageModel.storages();
        const auto inactive = std::find_if(inactiveStorages.begin(), inactiveStorages.end(),
                                           [id = initial.id](const KisStorageRecord &record) {
                                               return record.id == id;
                                           });
        QVERIFY(inactive != inactiveStorages.end());
        QVERIFY(!inactive->active);

        QVERIFY(storageModel.setStorageActive(initial.id, initial.active));
        const auto restoredStorages = storageModel.storages();
        const auto restored = std::find_if(restoredStorages.begin(), restoredStorages.end(),
                                           [id = initial.id](const KisStorageRecord &record) {
                                               return record.id == id;
                                           });
        QVERIFY(restored != restoredStorages.end());
        QCOMPARE(restored->active, initial.active);
    }
}

void TestStorageModel::testSetDefaultStorageActive()
{
    KisStorageModel storageModel;
    const auto initialStorages = storageModel.storages();
    const auto initial = std::find_if(initialStorages.begin(), initialStorages.end(),
                                      [](const KisStorageRecord &record) {
                                          return record.location.isEmpty();
                                      });
    QVERIFY2(initial != initialStorages.end(),
             "The default storage must be represented by an empty location");

    const int storageId = initial->id;
    const bool initialActive = initial->active;
    QVERIFY(storageModel.setStorageActive(storageId, !initialActive));

    const auto changedStorages = storageModel.storages();
    const auto changed = std::find_if(changedStorages.begin(), changedStorages.end(),
                                      [storageId](const KisStorageRecord &record) {
                                          return record.id == storageId;
                                      });
    QVERIFY(changed != changedStorages.end());
    QCOMPARE(changed->active, !initialActive);

    QVERIFY(storageModel.setStorageActive(storageId, initialActive));
    const auto restoredStorages = storageModel.storages();
    const auto restored = std::find_if(restoredStorages.begin(), restoredStorages.end(),
                                       [storageId](const KisStorageRecord &record) {
                                           return record.id == storageId;
                                       });
    QVERIFY(restored != restoredStorages.end());
    QCOMPARE(restored->active, initialActive);
}


void TestStorageModel::cleanupTestCase()
{
    ResourceTestHelper::rmTestDb();
    ResourceTestHelper::cleanDstLocation(m_dstLocation);
}

void TestStorageModel::testMetaData()
{
    KisStorageModel storageModel;
    int rowCount = static_cast<int>(storageModel.storages().size());

    KisResourceStorageSP storage {new KisResourceStorage("My Named Memory Storage")};
    KisResourceLocator::instance()->addStorage("My Named Memory Storage", storage);
    storage->setMetaData(KisResourceStorage::s_meta_name, "My Named Memory Storage");

    QVERIFY(storage->valid());
    const auto storages = storageModel.storages();
    QVERIFY(static_cast<int>(storages.size()) > rowCount);

    const auto found = std::find_if(storages.begin(), storages.end(),
                                    [&storageModel, &storage](const KisStorageRecord &record) {
                                        return storageModel.storageForId(record.id) == storage;
                                    });
    QVERIFY(found != storages.end());
    QCOMPARE(ResourceTestHelper::toQString(found->displayName), QString("My Named Memory Storage"));
    QVERIFY(found->metaData.contains(KisResourceStorage::s_meta_name));
    QCOMPARE(ResourceTestHelper::toQString(found->metaData.value(KisResourceStorage::s_meta_name).toString()),
             QString("My Named Memory Storage"));
}

void TestStorageModel::testImportStorage()
{

}




SIMPLE_TEST_MAIN(TestStorageModel)
