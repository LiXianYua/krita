/*
 * SPDX-FileCopyrightText: 2019 boud <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "TestStorageFilterProxyModel.h"

#include <algorithm>

#include <simpletest.h>
#include <QStandardPaths>
#include <QDir>
#include <QVersionNumber>
#include <QDirIterator>

#include <PkConfigGroup.h>
#include <PkSharedConfig.h>

#include <KisResourceCacheDb.h>
#include <KisResourceLocator.h>
#include <KisResourceModel.h>

#include <DummyResource.h>
#include <ResourceTestHelper.h>

#include <KisStorageFilterProxyModel.h>


#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing installing resources"
#endif


void TestStorageFilterProxyModel::initTestCase()
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

void TestStorageFilterProxyModel::testWithTagModelTester()
{
    KisStorageFilterProxyModel model;
    QCOMPARE(model.storages().size(), KisStorageModel::instance()->storages().size());
    for (const KisStorageRecord &record : model.storages()) {
        QVERIFY(model.storageForId(record.id));
    }
}


void TestStorageFilterProxyModel::testFilterByName()
{
    QScopedPointer<KisStorageFilterProxyModel> proxyModel(new KisStorageFilterProxyModel());

    const auto allStorages = proxyModel->storages();
    QVERIFY(!allStorages.empty());
    const auto selected = std::find_if(allStorages.begin(), allStorages.end(),
                                       [](const KisStorageRecord &record) {
                                           return !record.location.isEmpty();
                                       });
    QVERIFY2(selected != allStorages.end(),
             "Filename filtering requires a storage with a non-empty location");
    const PkString fileName = selected->location;
    QVERIFY(!fileName.isEmpty());

    proxyModel->setFilter(KisStorageFilterProxyModel::ByFileName,
                          PkVariant(fileName));
    const auto filteredStorages = proxyModel->storages();
    QVERIFY(!filteredStorages.empty());
    QVERIFY(filteredStorages.size() < allStorages.size());
    for (const KisStorageRecord &record : filteredStorages) {
        QVERIFY(record.location.contains(fileName));
    }

}

void TestStorageFilterProxyModel::testFilterByType()
{
    QScopedPointer<KisStorageFilterProxyModel> proxyModel(new KisStorageFilterProxyModel());
    PkStringList storageTypes;
    storageTypes << KisResourceStorage::storageTypeToUntranslatedString(KisResourceStorage::StorageType::Bundle)
                 << KisResourceStorage::storageTypeToUntranslatedString(KisResourceStorage::StorageType::Folder);
    proxyModel->setFilter(KisStorageFilterProxyModel::ByStorageType,
                          PkVariant(storageTypes));
    for (const KisStorageRecord &record : proxyModel->storages()) {
        QVERIFY(storageTypes.contains(record.storageType));
    }

}

void TestStorageFilterProxyModel::testFilterByActive()
{
    QScopedPointer<KisStorageFilterProxyModel> proxyModel(new KisStorageFilterProxyModel());
    proxyModel->setFilter(KisStorageFilterProxyModel::ByActive, PkVariant(true));
    for (const KisStorageRecord &record : proxyModel->storages()) {
        QVERIFY(record.active);
    }
}


void TestStorageFilterProxyModel::cleanupTestCase()
{
    ResourceTestHelper::rmTestDb();
    ResourceTestHelper::cleanDstLocation(m_dstLocation);
}


SIMPLE_TEST_MAIN(TestStorageFilterProxyModel)
