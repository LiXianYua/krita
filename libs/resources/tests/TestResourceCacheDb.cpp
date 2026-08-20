/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "TestResourceCacheDb.h"
#include <simpletest.h>
#include <PkSqlDatabase.h>
#include <PkSqlQuery.h>
#include <QStandardPaths>
#include <QDir>
#include <QImage>
#include <QPainter>

#include <KisResourceCacheDb.h>
#include <KisResourceLoaderRegistry.h>

void TestResourceCacheDb::initTestCase()
{
    QDir dbLocation(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    if (dbLocation.exists()) {
        QFile(dbLocation.path() + "/" + KisResourceCacheDb::resourceCacheDbFilename).remove();
        dbLocation.rmpath(dbLocation.path());
    }
}

void TestResourceCacheDb::testCreateDatabase()
{
    bool res = KisResourceCacheDb::initialize(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    QVERIFY(res);
    QVERIFY(KisResourceCacheDb::isValid());

    PkSqlDatabase sqlDb = PkSqlDatabase::database();

    QStringList tables = QStringList() << "version_information"
                                       << "storage_types"
                                       << "resource_types"
                                       << "storages"
                                       << "tags"
                                       << "resources"
                                       << "versioned_resources"
                                       << "resource_tags";
    const PkStringList dbTables = sqlDb.tables();

    Q_FOREACH(const QString &table, tables) {
        const QByteArray tableUtf8 = table.toUtf8();
        const PkString pkTable = PkString::PkFromUtf8(tableUtf8.constData(), tableUtf8.size());
        QVERIFY2(dbTables.contains(pkTable), table.toLatin1());
    }

    res = KisResourceCacheDb::initialize(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    if (!res) {
        qWarning() << KisResourceCacheDb::lastError();
    }
    QVERIFY(res);
    QVERIFY(KisResourceCacheDb::isValid());
}

void TestResourceCacheDb::testLookupTables()
{
    PkSqlQuery query;
    bool r = query.exec("SELECT COUNT(*) FROM storage_types");
    QVERIFY(r);
    QVERIFY(!query.lastError().isValid());
    query.first();
    QCOMPARE(query.value(0).toInt(), 7);

    r = query.exec("SELECT COUNT(*) FROM resource_types");
    QVERIFY(r);
    QVERIFY(!query.lastError().isValid());
    query.first();
    QVERIFY(query.value(0).toInt() == KisResourceLoaderRegistry::instance()->resourceTypes().count());
}

void TestResourceCacheDb::testMetaData()
{
    // Test adding metadata
    QMap<QString, QVariant> m1;
    m1["string"] = QString("bla");
    m1["bool"] = QVariant::fromValue<bool>(true);
    m1["int"] = QVariant::fromValue<int>(10);
    QImage img(50, 50, QImage::Format_RGB32);
    QPainter gc(&img);
    gc.fillRect(QRect(0, 0, 50, 50), QBrush(Qt::red));
    gc.end();
    m1["image"] = QVariant::fromValue<QImage>(img);

    bool r = KisResourceCacheDb::updateMetaDataForId(m1, 1, "test");
    QVERIFY(r);

    // Test retrieving metadata
    QMap<QString, QVariant> m2 = KisResourceCacheDb::metaDataForId(1, "test");
    QVERIFY(m1 == m2);

    // Test deleting metadata
    r = KisResourceCacheDb::updateMetaDataForId(QMap<QString, QVariant>(), 1, "test");
    QMap<QString, QVariant> m3 = KisResourceCacheDb::metaDataForId(1, "test");
    QVERIFY(m3.size() == 0);
}

void TestResourceCacheDb::cleanupTestCase()
{
    PkSqlDatabase::database().close();
    QDir dbLocation(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    bool res = QFile(dbLocation.path() + "/" + KisResourceCacheDb::resourceCacheDbFilename).remove();
    Q_ASSERT(res);
    res = dbLocation.rmpath(dbLocation.path());
    Q_ASSERT(res);
}

SIMPLE_TEST_MAIN(TestResourceCacheDb)
