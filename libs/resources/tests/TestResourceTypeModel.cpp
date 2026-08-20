/*
 * SPDX-FileCopyrightText: 2018 boud <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "TestResourceTypeModel.h"

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
#include <KisResourceLoaderRegistry.h>
#include <KisResourceTypeModel.h>

#include <DummyResource.h>
#include <ResourceTestHelper.h>


#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing installing resources"
#endif


void TestResourceTypeModel::initTestCase()
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

void TestResourceTypeModel::testWithTagModelTester()
{
    KisResourceTypeModel model;
    for (const KisResourceTypeRecord &record : model.resourceTypes()) {
        QVERIFY(record.id >= 0);
        QVERIFY(!record.resourceType.isEmpty());
        QVERIFY(!record.displayName.isEmpty());
    }
}


void TestResourceTypeModel::testRowCount()
{
    PkSqlQuery q;
    QVERIFY(q.prepare("SELECT count(*)\n"
                      "FROM   resource_types"));
    QVERIFY(q.exec());
    q.first();
    int rowCount = q.value(0).toInt();
    QCOMPARE(rowCount, KisResourceLoaderRegistry::instance()->resourceTypes().count());

    KisResourceTypeModel typeModel;
    QCOMPARE(static_cast<int>(typeModel.resourceTypes().size()), rowCount);
}

void TestResourceTypeModel::testData()
{
    KisResourceTypeModel typeModel;
    const auto records = typeModel.resourceTypes();
    const auto brushes = std::find_if(records.begin(), records.end(), [](const KisResourceTypeRecord &record) {
        return record.resourceType == ResourceType::Brushes;
    });
    QVERIFY(brushes != records.end());
    QCOMPARE(ResourceTestHelper::toQString(brushes->displayName),
             ResourceTestHelper::toQString(
                 ResourceName::resourceTypeToName(ResourceType::Brushes)));
}


void TestResourceTypeModel::cleanupTestCase()
{
    ResourceTestHelper::rmTestDb();
    ResourceTestHelper::cleanDstLocation(m_dstLocation);
}




SIMPLE_TEST_MAIN(TestResourceTypeModel)
