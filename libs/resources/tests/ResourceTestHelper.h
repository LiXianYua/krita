/*
 * SPDX-FileCopyrightText: 2018 boud <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef RESOURCETESTHELPER_H
#define RESOURCETESTHELPER_H

#include <QImageReader>
#include <QDir>
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
    KisDummyResourceLoader(const PkString &id, const PkString &folder, const QString &name, const QStringList &mimetypes)
        : KisResourceLoaderBase(id,
                                folder,
                                toPkString(name),
                                toPkStringList(mimetypes))
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
    reg->registerLoader(new KisDummyResourceLoader(ResourceType::PaintOpPresets, ResourceType::PaintOpPresets, QStringLiteral("Brush presets"), QStringList() << "application/x-krita-paintoppreset"));
    reg->registerLoader(new KisDummyResourceLoader(ResourceSubType::GbrBrushes, ResourceType::Brushes, QStringLiteral("Brush tips"), QStringList() << "image/x-gimp-brush"));
    reg->registerLoader(new KisDummyResourceLoader(ResourceSubType::GihBrushes, ResourceType::Brushes, QStringLiteral("Brush tips"), QStringList() << "image/x-gimp-brush-animated"));
    reg->registerLoader(new KisDummyResourceLoader(ResourceSubType::SvgBrushes, ResourceType::Brushes, QStringLiteral("Brush tips"), QStringList() << "image/svg+xml"));
    reg->registerLoader(new KisDummyResourceLoader(ResourceSubType::PngBrushes, ResourceType::Brushes, QStringLiteral("Brush tips"), QStringList() << "image/png"));
    reg->registerLoader(new KisDummyResourceLoader(ResourceSubType::SegmentedGradients, ResourceType::Gradients, QStringLiteral("Gradients"), QStringList() << "application/x-gimp-gradient"));
    reg->registerLoader(new KisDummyResourceLoader(ResourceSubType::StopGradients, ResourceType::Gradients, QStringLiteral("Gradients"), QStringList() << "image/svg+xml"));
    reg->registerLoader(new KisDummyResourceLoader(ResourceType::Palettes, ResourceType::Palettes, QStringLiteral("Palettes"),
                                        QStringList() << toQString(KisMimeDatabase::mimeTypeForSuffix("kpl"))
                                        << toQString(KisMimeDatabase::mimeTypeForSuffix("gpl"))
                                        << toQString(KisMimeDatabase::mimeTypeForSuffix("pal"))
                                        << toQString(KisMimeDatabase::mimeTypeForSuffix("act"))
                                        << toQString(KisMimeDatabase::mimeTypeForSuffix("aco"))
                                        << toQString(KisMimeDatabase::mimeTypeForSuffix("css"))
                                        << toQString(KisMimeDatabase::mimeTypeForSuffix("colors"))
                                        << toQString(KisMimeDatabase::mimeTypeForSuffix("xml"))
                                        << toQString(KisMimeDatabase::mimeTypeForSuffix("sbz"))));

    QList<QByteArray> src = QImageReader::supportedMimeTypes();
    QStringList allImageMimes;
    Q_FOREACH(const QByteArray ba, src) {
        allImageMimes << QString::fromUtf8(ba);
    }
    allImageMimes << toQString(KisMimeDatabase::mimeTypeForSuffix("pat"));

    reg->registerLoader(new KisDummyResourceLoader(ResourceType::Patterns, ResourceType::Patterns, QStringLiteral("Patterns"), allImageMimes));
    reg->registerLoader(new KisDummyResourceLoader(ResourceType::Workspaces, ResourceType::Workspaces, QStringLiteral("Workspaces"), QStringList() << "application/x-krita-workspace"));
    reg->registerLoader(new KisDummyResourceLoader(ResourceType::Symbols, ResourceType::Symbols, QStringLiteral("SVG symbol libraries"), QStringList() << "image/svg+xml"));
    reg->registerLoader(new KisDummyResourceLoader(ResourceType::WindowLayouts, ResourceType::WindowLayouts, QStringLiteral("Window layouts"), QStringList() << "application/x-krita-windowlayout"));
    reg->registerLoader(new KisDummyResourceLoader(ResourceType::Sessions, ResourceType::Sessions, QStringLiteral("Sessions"), QStringList() << "application/x-krita-session"));
    reg->registerLoader(new KisDummyResourceLoader(ResourceType::GamutMasks, ResourceType::GamutMasks, QStringLiteral("Gamut masks"), QStringList() << "application/x-krita-gamutmask"));

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

void testVersionedStorage(KisStoragePlugin &storage, const QString &resourceType, const QString &resourceUrl, const QString &optionalFolderCheck = QString())
{
    const QFileInfo fileInfo(resourceUrl);

    auto verifyFileExists = [optionalFolderCheck, resourceType] (KoResourceSP res) {
        if (optionalFolderCheck.isEmpty()) return;

        const QString filePath = optionalFolderCheck + "/" + resourceType + "/" +
            toQString(res->filename());

        if (!QFileInfo(filePath).exists()) {
            qWarning() << "Couldn't find a file in the resource storage:";
            qWarning() << "    " << ppVar(toQString(res->filename()));
            qWarning() << "    " << ppVar(optionalFolderCheck);
            qWarning() << "    " << ppVar(filePath);
        }

        QVERIFY(QFileInfo(filePath).exists());
    };

    KoResourceSP res1 = storage.resource(toPkString(resourceUrl));
    QCOMPARE(toQString(res1->filename()), fileInfo.fileName()); // filenames are not URLs
    QCOMPARE(res1->version(), -1); // storages don't work with versions
    QCOMPARE(res1->valid(), true);

    const QString originalSomething = res1.dynamicCast<DummyResource>()->something();

    KoResourceSP res2 = storage.resource(toPkString(resourceUrl));
    QCOMPARE(toQString(res2->filename()), fileInfo.fileName());
    QCOMPARE(res2->version(), -1); // storages don't work with versions
    QCOMPARE(res2->valid(), true);

    QVERIFY(res1 != res2);

    res2.dynamicCast<DummyResource>()->setSomething("It's changed");
    QCOMPARE(res1.dynamicCast<DummyResource>()->something(), originalSomething);
    QCOMPARE(res2.dynamicCast<DummyResource>()->something(), "It's changed");

    KoResourceSP res3 = storage.resource(toPkString(resourceUrl));
    QCOMPARE(toQString(res3->filename()), fileInfo.fileName());
    QCOMPARE(res3->version(), -1); // storages don't work with versions
    QCOMPARE(res3->valid(), true);
    QCOMPARE(res3.dynamicCast<DummyResource>()->something(), originalSomething);

    const QString versionedName = fileInfo.baseName() + ".0001." + fileInfo.suffix();

    storage.saveAsNewVersion(toPkString(resourceType), res2);
    QCOMPARE(toQString(res2->filename()), versionedName);
    QCOMPARE(res2->version(), -1); // storages don't work with versions
    QCOMPARE(res2->valid(), true);
    verifyFileExists(res2);

    KoResourceSP res4 = storage.resource(toPkString(resourceType + "/" + versionedName));
    QCOMPARE(toQString(res4->filename()), versionedName);
    QCOMPARE(res4->version(), -1); // storages don't work with versions
    QCOMPARE(res4->valid(), true);
    QCOMPARE(res4.dynamicCast<DummyResource>()->something(), "It's changed");
    verifyFileExists(res4);

    overrideResourceVersion(res4, 10000);
    storage.saveAsNewVersion(toPkString(resourceType), res4);
    QCOMPARE(toQString(res4->filename()), fileInfo.baseName() + ".10000." + fileInfo.suffix());
    verifyFileExists(res4);

    overrideResourceVersion(res4, -1);
    const QString versionedName2 = fileInfo.baseName() + ".10001." + fileInfo.suffix();

    storage.saveAsNewVersion(toPkString(resourceType), res4);
    QCOMPARE(toQString(res4->filename()), versionedName2);
    QCOMPARE(res4->version(), -1); // storages don't work with versions
    QCOMPARE(res4->valid(), true);
    verifyFileExists(res4);
}

void testVersionedStorageIterator(KisStoragePlugin &storage, const QString &resourceType, const QString &resourceUrl)
{
    const QString basename = QFileInfo(resourceUrl).baseName();

    PkSharedPointer<KisResourceStorage::ResourceIterator> iter =
        storage.resources(toPkString(resourceType));
    QVERIFY(iter->hasNext());
    int count = 0;
    int numVersions = 0;
    while (iter->hasNext()) {
        iter->next();

        //qDebug() << iter->url() << ppVar(iter->guessedVersion()) << ppVar(iter->lastModified());

        if (iter->url().contains(toPkString(basename))) {

            // because of versioning, the URL should have been changed
            QVERIFY(iter->url() != toPkString(resourceUrl));

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
