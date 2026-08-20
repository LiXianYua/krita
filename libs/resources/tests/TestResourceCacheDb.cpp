/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "TestResourceCacheDb.h"
#include <simpletest.h>
#include <PkSqlDatabase.h>
#include <PkSqlQuery.h>
#include <PkImage.h>
#include <PkMap.h>
#include <PkVariant.h>
#include <QStandardPaths>
#include <QDir>

#include <KisResourceCacheDb.h>
#include <KisResourceLoaderRegistry.h>
#include <KisResourceThumbnailCodec.h>

namespace
{

PkString toPkString(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return PkString::PkFromUtf8(utf8.constData(), utf8.size());
}

QString toQString(const PkString &value)
{
    const std::string utf8 = value.PkToUtf8();
    return QString::fromUtf8(utf8.data(), static_cast<int>(utf8.size()));
}

}

void TestResourceCacheDb::initTestCase()
{
    QDir dbLocation(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    if (dbLocation.exists()) {
        QFile(dbLocation.path() + "/" + toQString(KisResourceCacheDb::resourceCacheDbFilename)).remove();
        dbLocation.rmpath(dbLocation.path());
    }
}

void TestResourceCacheDb::testCreateDatabase()
{
    const PkString dbPath = toPkString(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    bool res = KisResourceCacheDb::initialize(dbPath);
    QVERIFY(res);
    QVERIFY(KisResourceCacheDb::isValid());

    PkSqlDatabase sqlDb = PkSqlDatabase::database();

    const PkStringList tables = {
        PkString("version_information"),
        PkString("storage_types"),
        PkString("resource_types"),
        PkString("storages"),
        PkString("tags"),
        PkString("resources"),
        PkString("versioned_resources"),
        PkString("resource_tags")
    };
    const PkStringList dbTables = sqlDb.tables();

    for (const PkString &table : tables) {
        const std::string tableUtf8 = table.PkToUtf8();
        QVERIFY2(dbTables.contains(table), tableUtf8.c_str());
    }

    res = KisResourceCacheDb::initialize(dbPath);
    if (!res) {
        qWarning() << toQString(KisResourceCacheDb::lastError());
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
    PkMap<PkString, PkVariant> m1;
    m1.insert(PkString("string"), PkVariant(PkString("bla")));
    m1.insert(PkString("bool"), PkVariant(true));
    m1.insert(PkString("int"), PkVariant(10));

    PkImage image(50, 50, PkImage::Format_RGB32);
    image.fill(0xffff0000U);
    const PkByteArray encodedImage = KisResourceThumbnailCodec::encodePng(image);
    QVERIFY(!encodedImage.isEmpty());
    m1.insert(PkString("image"), PkVariant(encodedImage));

    bool r = KisResourceCacheDb::updateMetaDataForId(m1, 1, PkString("test"));
    QVERIFY(r);

    // Test retrieving metadata
    const PkMap<PkString, PkVariant> m2 =
        KisResourceCacheDb::metaDataForId(1, PkString("test"));
    QVERIFY(m1 == m2);

    const PkImage decodedImage = KisResourceThumbnailCodec::decodePng(
        m2.value(PkString("image")).toByteArray());
    QCOMPARE(decodedImage.width(), image.width());
    QCOMPARE(decodedImage.height(), image.height());
    QCOMPARE(decodedImage.pixel(0, 0), image.pixel(0, 0));
    QCOMPARE(decodedImage.pixel(49, 49), image.pixel(49, 49));

    // Test deleting metadata
    r = KisResourceCacheDb::updateMetaDataForId(
        PkMap<PkString, PkVariant>(), 1, PkString("test"));
    const PkMap<PkString, PkVariant> m3 =
        KisResourceCacheDb::metaDataForId(1, PkString("test"));
    QVERIFY(m3.size() == 0);
}

void TestResourceCacheDb::cleanupTestCase()
{
    PkSqlDatabase::database().close();
    QDir dbLocation(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    bool res = QFile(dbLocation.path() + "/" +
                     toQString(KisResourceCacheDb::resourceCacheDbFilename)).remove();
    Q_ASSERT(res);
    res = dbLocation.rmpath(dbLocation.path());
    Q_ASSERT(res);
}

SIMPLE_TEST_MAIN(TestResourceCacheDb)
