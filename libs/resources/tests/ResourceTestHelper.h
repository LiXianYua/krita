/*
 * SPDX-FileCopyrightText: 2018 boud <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef RESOURCETESTHELPER_H
#define RESOURCETESTHELPER_H

#include <QImageReader>
#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDirIterator>

#include <string>

#include <KisMimeDatabase.h>
#include <KisResourceLoaderRegistry.h>

#include <KisResourceLocator.h>
#include <KisResourceCacheDb.h>
#include "KisResourceTypes.h"
#include <DummyResource.h>
#include <KisStoragePlugin.h>
#include <simpletest.h>
#include "kis_debug.h"
#include <KisSqlQueryLoader.h>
#include <KisDatabaseTransactionLock.h>
#include <KisResourceModelProvider.h>
#include <PkSqlDatabase.h>

#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing installing resources"
#endif

namespace ResourceTestHelper {

inline PkString toPkString(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return PkString::PkFromUtf8(utf8.constData(), utf8.size());
}

inline QString toQString(const PkString &value)
{
    const std::string utf8 = value.PkToUtf8();
    return QString::fromUtf8(utf8.data(), static_cast<int>(utf8.size()));
}

inline PkStringList toPkStringList(const QStringList &values)
{
    PkStringList result;
    for (const QString &value : values) {
        result.append(toPkString(value));
    }
    return result;
}

// 让本测试进程的 kritarc 落在进程私有的临时目录里，不碰用户的
// ~/.config/kritarc。PkConfigStore 是进程级单例，析构时调 sync()
// （pk/config/PkConfigStore.cpp）；sync() 自己没有待写变更时会早退，但只要
// 本进程写过配置，那份 journal 就会在退出时合并进 kritarc。不隔离的话，
// 测试写下的 ResourceDirectory=<测试 dst 目录> 就落进用户的全局配置，
// 静默改道同机后续所有进程的资源解析。
//
// PkConfigStore::genericConfigPath() 已经支持 XDG_CONFIG_HOME
// （pk/config/PkConfigStore.cpp），pk/config 自己的测试也是这么隔离的
// （pk/config/tests/test_config_group.cpp）。
//
// 必须早于本进程第一次构造 PkConfigStore 才有效，所以调用点必须是
// initTestCase() 的第一条语句。
inline void isolateUserConfig()
{
    const QString configRoot = QDir::tempPath() + "/krita-resource-tests/"
        + QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    QDir().mkpath(configRoot);
    // 上一轮跑剩的文件清掉，保证每次从空配置开始。
    QFile::remove(configRoot + "/kritarc");
    qputenv("XDG_CONFIG_HOME", configRoot.toUtf8());
}

const QString &filesDestDir() {
    static const QString s_path = QDir::cleanPath(
            QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/testdest") + '/';
    return s_path;
}

void rmTestDb() {
    QDir dbLocation(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    QFile(dbLocation.path() + "/" + toQString(KisResourceCacheDb::resourceCacheDbFilename)).remove();
    dbLocation.rmpath(dbLocation.path());
}


class KisDummyResourceLoader : public KisResourceLoaderBase {
public:
    KisDummyResourceLoader(const PkString &id,
                           const PkString &folder,
                           const PkString &name,
                           const PkStringList &mimetypes)
        : KisResourceLoaderBase(id, folder, name, mimetypes)
    {
    }

    KoResourceSP create(const PkString &name) override
    {
        PkSharedPointer<DummyResource> resource = PkSharedPointer<DummyResource>::create(
            name, resourceType());
        return resource;
    }
};

void createDummyLoaderRegistry() {

    KisResourceLoaderRegistry *reg = KisResourceLoaderRegistry::instance();
    reg->registerLoader(new KisDummyResourceLoader(
        ResourceType::PaintOpPresets, ResourceType::PaintOpPresets, PkString("Brush presets"),
        PkStringList{PkString("application/x-krita-paintoppreset")}));
    reg->registerLoader(new KisDummyResourceLoader(
        ResourceSubType::GbrBrushes, ResourceType::Brushes, PkString("Brush tips"),
        PkStringList{PkString("image/x-gimp-brush")}));
    reg->registerLoader(new KisDummyResourceLoader(
        ResourceSubType::GihBrushes, ResourceType::Brushes, PkString("Brush tips"),
        PkStringList{PkString("image/x-gimp-brush-animated")}));
    reg->registerLoader(new KisDummyResourceLoader(
        ResourceSubType::SvgBrushes, ResourceType::Brushes, PkString("Brush tips"),
        PkStringList{PkString("image/svg+xml")}));
    reg->registerLoader(new KisDummyResourceLoader(
        ResourceSubType::PngBrushes, ResourceType::Brushes, PkString("Brush tips"),
        PkStringList{PkString("image/png")}));
    reg->registerLoader(new KisDummyResourceLoader(
        ResourceSubType::SegmentedGradients, ResourceType::Gradients, PkString("Gradients"),
        PkStringList{PkString("application/x-gimp-gradient")}));
    reg->registerLoader(new KisDummyResourceLoader(
        ResourceSubType::StopGradients, ResourceType::Gradients, PkString("Gradients"),
        PkStringList{PkString("image/svg+xml")}));

    const PkStringList paletteMimes = {
        KisMimeDatabase::mimeTypeForSuffix("kpl"),
        KisMimeDatabase::mimeTypeForSuffix("gpl"),
        KisMimeDatabase::mimeTypeForSuffix("pal"),
        KisMimeDatabase::mimeTypeForSuffix("act"),
        KisMimeDatabase::mimeTypeForSuffix("aco"),
        KisMimeDatabase::mimeTypeForSuffix("css"),
        KisMimeDatabase::mimeTypeForSuffix("colors"),
        KisMimeDatabase::mimeTypeForSuffix("xml"),
        KisMimeDatabase::mimeTypeForSuffix("sbz")
    };
    reg->registerLoader(new KisDummyResourceLoader(
        ResourceType::Palettes, ResourceType::Palettes, PkString("Palettes"), paletteMimes));

    QList<QByteArray> src = QImageReader::supportedMimeTypes();
    PkStringList allImageMimes;
    Q_FOREACH(const QByteArray ba, src) {
        allImageMimes.append(PkString::PkFromUtf8(ba.constData(), ba.size()));
    }
    allImageMimes.append(KisMimeDatabase::mimeTypeForSuffix("pat"));

    reg->registerLoader(new KisDummyResourceLoader(
        ResourceType::Patterns, ResourceType::Patterns, PkString("Patterns"), allImageMimes));
    reg->registerLoader(new KisDummyResourceLoader(
        ResourceType::Workspaces, ResourceType::Workspaces, PkString("Workspaces"),
        PkStringList{PkString("application/x-krita-workspace")}));
    reg->registerLoader(new KisDummyResourceLoader(
        ResourceType::Symbols, ResourceType::Symbols, PkString("SVG symbol libraries"),
        PkStringList{PkString("image/svg+xml")}));
    reg->registerLoader(new KisDummyResourceLoader(
        ResourceType::WindowLayouts, ResourceType::WindowLayouts, PkString("Window layouts"),
        PkStringList{PkString("application/x-krita-windowlayout")}));
    reg->registerLoader(new KisDummyResourceLoader(
        ResourceType::Sessions, ResourceType::Sessions, PkString("Sessions"),
        PkStringList{PkString("application/x-krita-session")}));
    reg->registerLoader(new KisDummyResourceLoader(
        ResourceType::GamutMasks, ResourceType::GamutMasks, PkString("Gamut masks"),
        PkStringList{PkString("application/x-krita-gamutmask")}));

}

bool cleanDstLocation(const QString &dstLocation)
{
    if (QDir(dstLocation).exists()) {
        {
            QDirIterator iter(dstLocation, QStringList() << "*", QDir::Files, QDirIterator::Subdirectories);
            while (iter.hasNext()) {
                iter.next();
                QFile f(iter.filePath());
                f.remove();
                //qDebug() << (r ? "Removed" : "Failed to remove") << iter.filePath();
            }
        }
        {
            QDirIterator iter(dstLocation, QStringList() << "*", QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (iter.hasNext()) {
                iter.next();
                QDir(iter.filePath()).rmdir(iter.filePath());
                //qDebug() << (r ? "Removed" : "Failed to remove") << iter.filePath();
            }
        }

        return QDir().rmpath(dstLocation);
    }
    return true;
}

void initTestDb()
{
    rmTestDb();
    cleanDstLocation(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
}

void overrideResourceVersion(KoResourceSP resource, int version)
{
    resource->setVersion(version);
}

void testVersionedStorage(KisStoragePlugin &storage,
                          const PkString &resourceType,
                          const PkString &resourceUrl,
                          const QString &optionalFolderCheck = QString())
{
    const QFileInfo fileInfo(toQString(resourceUrl));

    auto verifyFileExists = [optionalFolderCheck, resourceType] (KoResourceSP res) {
        if (optionalFolderCheck.isEmpty()) return;

        const QString filePath = optionalFolderCheck + "/" + toQString(resourceType) + "/" +
            toQString(res->filename());

        if (!QFileInfo(filePath).exists()) {
            qWarning() << "Couldn't find a file in the resource storage:";
            qWarning() << "    " << ppVar(toQString(res->filename()));
            qWarning() << "    " << ppVar(optionalFolderCheck);
            qWarning() << "    " << ppVar(filePath);
        }

        QVERIFY(QFileInfo(filePath).exists());
    };

    KoResourceSP res1 = storage.resource(resourceUrl);
    QCOMPARE(toQString(res1->filename()), fileInfo.fileName()); // filenames are not URLs
    QCOMPARE(res1->version(), -1); // storages don't work with versions
    QCOMPARE(res1->valid(), true);

    const QString originalSomething = res1.dynamicCast<DummyResource>()->something();

    KoResourceSP res2 = storage.resource(resourceUrl);
    QCOMPARE(toQString(res2->filename()), fileInfo.fileName());
    QCOMPARE(res2->version(), -1); // storages don't work with versions
    QCOMPARE(res2->valid(), true);

    QVERIFY(res1 != res2);

    res2.dynamicCast<DummyResource>()->setSomething("It's changed");
    QCOMPARE(res1.dynamicCast<DummyResource>()->something(), originalSomething);
    QCOMPARE(res2.dynamicCast<DummyResource>()->something(), "It's changed");

    KoResourceSP res3 = storage.resource(resourceUrl);
    QCOMPARE(toQString(res3->filename()), fileInfo.fileName());
    QCOMPARE(res3->version(), -1); // storages don't work with versions
    QCOMPARE(res3->valid(), true);
    QCOMPARE(res3.dynamicCast<DummyResource>()->something(), originalSomething);

    const QString versionedName = fileInfo.baseName() + ".0001." + fileInfo.suffix();

    storage.saveAsNewVersion(resourceType, res2);
    QCOMPARE(toQString(res2->filename()), versionedName);
    QCOMPARE(res2->version(), -1); // storages don't work with versions
    QCOMPARE(res2->valid(), true);
    verifyFileExists(res2);

    KoResourceSP res4 = storage.resource(resourceType + PkString("/") + toPkString(versionedName));
    QCOMPARE(toQString(res4->filename()), versionedName);
    QCOMPARE(res4->version(), -1); // storages don't work with versions
    QCOMPARE(res4->valid(), true);
    QCOMPARE(res4.dynamicCast<DummyResource>()->something(), "It's changed");
    verifyFileExists(res4);

    overrideResourceVersion(res4, 10000);
    storage.saveAsNewVersion(resourceType, res4);
    QCOMPARE(toQString(res4->filename()), fileInfo.baseName() + ".10000." + fileInfo.suffix());
    verifyFileExists(res4);

    overrideResourceVersion(res4, -1);
    const QString versionedName2 = fileInfo.baseName() + ".10001." + fileInfo.suffix();

    storage.saveAsNewVersion(resourceType, res4);
    QCOMPARE(toQString(res4->filename()), versionedName2);
    QCOMPARE(res4->version(), -1); // storages don't work with versions
    QCOMPARE(res4->valid(), true);
    verifyFileExists(res4);
}

void testVersionedStorageIterator(KisStoragePlugin &storage,
                                  const PkString &resourceType,
                                  const PkString &resourceUrl)
{
    const QString basename = QFileInfo(toQString(resourceUrl)).baseName();

    PkSharedPointer<KisResourceStorage::ResourceIterator> iter =
        storage.resources(resourceType);
    QVERIFY(iter->hasNext());
    int count = 0;
    int numVersions = 0;
    while (iter->hasNext()) {
        iter->next();

        //qDebug() << iter->url() << ppVar(iter->guessedVersion()) << ppVar(iter->lastModified());

        if (iter->url().contains(toPkString(basename))) {

            // because of versioning, the URL should have been changed
            QVERIFY(iter->url() != resourceUrl);

            //qDebug() << iter->url() << ppVar(iter->guessedVersion()) << ppVar(iter->lastModified());

            count++;

            auto verIt = iter->versions();
            while (verIt->hasNext()) {
                verIt->next();

                qDebug() << toQString(verIt->url()) << ppVar(verIt->guessedVersion());
                numVersions++;
                QVERIFY(verIt->url().contains(toPkString(basename)));
            }
        }

        KoResourceSP res = iter->resource();
        QVERIFY(res);
    }

    QCOMPARE(count, 1);
    QCOMPARE(numVersions, 4);
};

bool recreateDatabaseForATest(KisResourceLocator *locator, const QString &srcLocation, const QString &dstLocation)
{
    auto listDbResources = [](const QString &dbResourceType) {
        KisSqlQueryLoader loader("inline://list_all_db_tables",
                                 "SELECT name FROM sqlite_master WHERE sql IS NOT NULL and name != \"sqlite_sequence\" "
                                 "and type = :db_resource_type",
                                 KisSqlQueryLoader::single_statement_mode);
        loader.query().bindValue(":db_resource_type", toPkString(dbResourceType));
        loader.exec();

        QVector<QString> dbResources;
        while (loader.query().next()) {
            dbResources.append(toQString(loader.query().value(0).toString()));
        }
        return dbResources;
    };

    auto dropDbResource = [](const QString &dbResourceType, const QString &dbResourceName) {
        KisSqlQueryLoader loader(toPkString("inline://drop_db_resource_" + dbResourceType),
                                 toPkString(QString("DROP %1 %2").arg(dbResourceType.toUpper(), dbResourceName)));
        loader.exec();

        QVector<QString> dbResources;
        while (loader.query().next()) {
            dbResources.append(toQString(loader.query().value(0).toString()));
        }
        return dbResources;
    };

    if (PkSqlDatabase::database(PkSqlDatabase::defaultConnection, false).isOpen()) {
        try {
            KisResourceModelProvider::testingCloseAllQueries();

            // foreign keys should be disabled outside the transaction's scope!
            KisResourceCacheDb::setForeignKeysStateImpl(false);

            KisDatabaseTransactionLock transactionLock(PkSqlDatabase::database());

            Q_FOREACH (const QString &dbResourceType, QStringList({"table", "index", "trigger", "view"})) {
                auto resources = listDbResources(dbResourceType);
                Q_FOREACH (const auto &resource, resources) {
                    // qDebug() << "dropping" << ppVar(dbResourceType) << ppVar(resource);
                    dropDbResource(dbResourceType, resource);
                }
            }

            // defuse the lock and save the results
            transactionLock.commit();

            KisResourceCacheDb::setForeignKeysStateImpl(true);

        } catch (const KisSqlQueryLoader::SQLException &e) {
            qWarning().noquote() << "ERROR: failed to execute query:" << toQString(e.message);
            qWarning().noquote() << "       file:" << toQString(e.filePath);
            qWarning().noquote() << "       statement:" << e.statementIndex;
            qWarning().noquote() << "       error:" << toQString(e.sqlError.text());

            return false;
        }
    }

    ResourceTestHelper::cleanDstLocation(dstLocation);

    // Reinitialize the database from scratch
    KisResourceCacheDb::initialize(
        toPkString(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)));

    KisResourceLocator::LocatorError r = locator->initialize(toPkString(srcLocation));
    KisResourceModelProvider::testingResetAllModels();

    if (!locator->errorMessages().isEmpty()) {
        for (const PkString &message : locator->errorMessages()) {
            qDebug() << toQString(message);
        }
    }
    if (r != KisResourceLocator::LocatorError::Ok) {
        return false;
    }

    return true;
}

}

#endif // RESOURCETESTHELPER_H
