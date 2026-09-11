/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "TestResourceStorage.h"

#include <simpletest.h>

#include <QImage>
#include <QPainter>
#include <QUuid>
#include <QBuffer>

#include <PkFileStream.h>
#include <PkMemoryStream.h>

#include <PkConfigGroup.h>
#include <PkSharedConfig.h>

#include <KoMD5Generator.h>
#include <KoPattern.h>

#include "DummyResource.h"
#include "ResourceTestHelper.h"
#include "KisResourceStorage.h"
#include "KisResourceLocator.h"
#include "KisResourceTypes.h"

#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing installing resources"
#endif


void TestResourceStorage::initTestCase()
{
    m_dstLocation = ResourceTestHelper::filesDestDir();
    ResourceTestHelper::cleanDstLocation(m_dstLocation);
    QDir().mkpath(m_dstLocation);
    PkConfigGroup cfg(PkSharedConfig::openConfig(), PkString());
    cfg.writeEntry(KisResourceLocator::resourceLocationKey,
                   ResourceTestHelper::toPkString(m_dstLocation));
    m_locator = KisResourceLocator::instance();
    ResourceTestHelper::createDummyLoaderRegistry();
}

void TestResourceStorage ::testStorage()
{
    {
        KisResourceStorage storage(ResourceTestHelper::toPkString(QString(FILES_DATA_DIR)));
        QVERIFY(storage.type() == KisResourceStorage::StorageType::Folder);
        QVERIFY(storage.valid());
    }

    {
        KisResourceStorage storage(ResourceTestHelper::toPkString(QString(FILES_DATA_DIR) + "/bundles/test1.bundle"));
        QVERIFY(storage.type() == KisResourceStorage::StorageType::Bundle);
        QVERIFY(storage.valid());
    }

    {
        KisResourceStorage storage(ResourceTestHelper::toPkString(QString(FILES_DATA_DIR) + "/bundles/test2.bundle"));
        QVERIFY(storage.type() == KisResourceStorage::StorageType::Bundle);
        QVERIFY(storage.valid());
    }

    {
        KisResourceStorage storage(ResourceTestHelper::toPkString(QUuid().toString()));
        QVERIFY(storage.type() == KisResourceStorage::StorageType::Memory);
        QVERIFY(storage.valid());
    }

    {
        KisResourceStorage storage("Just a random storage");
        QVERIFY(storage.type() == KisResourceStorage::StorageType::Memory);
        QVERIFY(storage.valid());
    }

    {
        KisResourceStorage storage("");
        QVERIFY(storage.type() == KisResourceStorage::StorageType::Unknown);
        QVERIFY(!storage.valid());
    }
}

void TestResourceStorage::testImportExportResource()
{
    QImage img(256, 256, QImage::Format_ARGB32);
    QPainter gc(&img);
    gc.fillRect(0, 0, 256, 256, Qt::red);
    img.save("testpattern.png");

    QByteArray ba;

    {
        QFile f("testpattern.png");
        KIS_ASSERT(f.open(QFile::ReadOnly));
        ba = f.readAll();
        f.close();
    }

    const PkString md5 = KoMD5Generator::generateHash(PkByteArray(ba.constData(), ba.size()));

    {
        QDir().mkpath(m_dstLocation + "/" + ResourceTestHelper::toQString(ResourceType::Patterns));
        KisResourceStorage folderStorage(ResourceTestHelper::toPkString(m_dstLocation));

        PkFileStream f("testpattern.png");
        KIS_ASSERT(f.open(PkStream::ReadOnly));
        bool r = folderStorage.importResource("patterns/testpattern.png", &f);
        QVERIFY(r);
        QCOMPARE(md5, folderStorage.resourceMd5("patterns/testpattern.png"));

        PkMemoryStream buffer;
        buffer.open(PkStream::WriteOnly);
        r = folderStorage.exportResource("patterns/testpattern.png", &buffer);
        QVERIFY(r);
        buffer.close();
        QCOMPARE(KoMD5Generator::generateHash(PkByteArray(buffer.data(), int(buffer.size()))), md5);
    }

    {
        PkFileStream f("testpattern.png");
        KIS_ASSERT(f.open(PkStream::ReadOnly));
        KisResourceStorage memoryStorage("memory");
        bool r = memoryStorage.importResource("patterns/testpattern.png", &f);
        QVERIFY(r);
        QCOMPARE(md5, memoryStorage.resourceMd5("patterns/testpattern.png"));

        PkMemoryStream buffer;
        buffer.open(PkStream::WriteOnly);
        r = memoryStorage.exportResource("patterns/testpattern.png", &buffer);
        QVERIFY(r);
        buffer.close();
        QCOMPARE(KoMD5Generator::generateHash(PkByteArray(buffer.data(), int(buffer.size()))), md5);
    }

    {
        PkFileStream f("testpattern.png");
        KIS_ASSERT(f.open(PkStream::ReadOnly));
        KisResourceStorage bundleStorage(ResourceTestHelper::toPkString(QString(FILES_DATA_DIR) + "/bundles/test1.bundle"));
        bool r = bundleStorage.importResource("patterns/testpattern.png", &f);
        QVERIFY(!r);

        PkMemoryStream buffer;
        buffer.open(PkStream::WriteOnly);
        r = bundleStorage.exportResource("patterns/testpattern.png", &buffer);
        QVERIFY(r);
    }
}

void TestResourceStorage::testAddResource()
{
    QDir().mkpath(m_dstLocation + "/" + ResourceTestHelper::toQString(ResourceType::Patterns));
    KisResourceStorage folderStorage(ResourceTestHelper::toPkString(m_dstLocation));

    PkImage img(256, 256, PkImage::Format_ARGB32);
    img.fill(Pk::red);

    KoResourceSP res(new KoPattern(img, "testpattern2", "testpattern2.png"));
    Q_ASSERT(res->resourceType().first == ResourceType::Patterns);
    bool r =  folderStorage.addResource(res);
    QVERIFY(r);
}

void TestResourceStorage::testStorageVersioningHelperCounting()
{
    // create the resource
    QDir().mkpath(m_dstLocation + "/" + ResourceTestHelper::toQString(ResourceType::Patterns));

    PkImage img(256, 256, PkImage::Format_ARGB32);
    img.fill(Pk::red);
    KoResourceSP res(new KoPattern(img, "testpattern", "testpattern.png"));
    Q_ASSERT(res->resourceType().first == ResourceType::Patterns);

    // function that returns false for everything
    auto noResourcesExisting = [] (PkString a) {Q_UNUSED(a); return false;};
    PkString resNewFilename = KisStorageVersioningHelper::chooseUniqueName(res, 0, noResourcesExisting);
    //QCOMPARE(resNewFilename, "testpattern.png");
    QCOMPARE(resNewFilename, "testpattern.0000.png");

    // function that returns true for the same resource but false for everything else
    auto onlyFirstVersionExists = [] (PkString a) {
        return a == "testpattern.png" || a == "testpattern.0000.png";
    };
    resNewFilename = KisStorageVersioningHelper::chooseUniqueName(res, 0, onlyFirstVersionExists);
    QCOMPARE(resNewFilename, "testpattern.0001.png");

    // function that returns true for first 10 versions of the resource but false for everything else
    auto firstTenVersionExists = [] (PkString a) {
        if (a == "testpattern.png") return true;
        if (a == "testpattern.0000.png") return true;
        if (a == "testpattern.0001.png") return true;
        if (a == "testpattern.0002.png") return true;
        if (a == "testpattern.0003.png") return true;
        if (a == "testpattern.0004.png") return true;
        if (a == "testpattern.0005.png") return true;
        if (a == "testpattern.0006.png") return true;
        if (a == "testpattern.0007.png") return true;
        if (a == "testpattern.0008.png") return true;
        if (a == "testpattern.0009.png") return true;
        if (a == "testpattern.0010.png") return true;
        return false;
    };
    resNewFilename = KisStorageVersioningHelper::chooseUniqueName(res, 0, firstTenVersionExists);
    QCOMPARE(resNewFilename, "testpattern.0011.png");

}

void TestResourceStorage::cleanupTestCase()
{
    ResourceTestHelper::cleanDstLocation(m_dstLocation);
}

SIMPLE_TEST_MAIN(TestResourceStorage)
