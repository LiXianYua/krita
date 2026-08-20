/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "TestFolderStorage.h"
#include <simpletest.h>

#include <PkConfigGroup.h>
#include <PkSharedConfig.h>

#include <KritaVersionWrapper.h>

#include <KisFolderStorage.h>
#include <KoResource.h>
#include <KisResourceCacheDb.h>

#include <KisResourceLocator.h>

#include "DummyResource.h"
#include "ResourceTestHelper.h"

#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing installing resources"
#endif


void TestFolderStorage::initTestCase()
{
    ResourceTestHelper::initTestDb();

    m_srcLocation = QString(FILES_DATA_DIR);
    QVERIFY2(QDir(m_srcLocation).exists(), m_srcLocation.toUtf8());

    m_dstLocation = ResourceTestHelper::filesDestDir();
    ResourceTestHelper::cleanDstLocation(m_dstLocation);

    PkConfigGroup cfg(PkSharedConfig::openConfig(), PkString());
    cfg.writeEntry(KisResourceLocator::resourceLocationKey,
                   ResourceTestHelper::toPkString(m_dstLocation));

    m_locator = KisResourceLocator::instance();

    ResourceTestHelper::createDummyLoaderRegistry();

    KisResourceCacheDb::initialize(ResourceTestHelper::toPkString(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)));
    m_locator->initialize(ResourceTestHelper::toPkString(m_srcLocation));

    if (!m_locator->errorMessages().isEmpty()) {
        qDebug() << ResourceTestHelper::toQString(m_locator->errorMessages().join("\n"));
    }
}

void TestFolderStorage ::testStorage()
{
    KisFolderStorage folderStorage(ResourceTestHelper::toPkString(m_dstLocation));
    PkSharedPointer<KisResourceStorage::ResourceIterator> iter = folderStorage.resources(ResourceType::Brushes);
    QVERIFY(iter->hasNext());
    int count = 0;
    while (iter->hasNext()) {
        iter->next();
        qDebug() << iter->url() << iter->type() << iter->lastModified();
        count++;
    }
    QVERIFY(count == 1);
}

void TestFolderStorage::testTagIterator()
{
    KisFolderStorage folderStorage(ResourceTestHelper::toPkString(m_dstLocation));
    PkSharedPointer<KisResourceStorage::TagIterator> iter = folderStorage.tags(ResourceType::PaintOpPresets);
    QVERIFY(iter->hasNext());
    int count = 0;
    while (iter->hasNext()) {
        iter->next();
        qDebug() << iter->tag();
        count++;
    }
    QVERIFY(count == 1);
}

void TestFolderStorage::testAddResource()
{
    KoResourceSP resource(new DummyResource("anewresource.kpp", ResourceType::PaintOpPresets));
    resource->setValid(true);
    resource->setVersion(0);

    KisFolderStorage folderStorage(ResourceTestHelper::toPkString(m_dstLocation));
    bool r = folderStorage.saveAsNewVersion(ResourceType::PaintOpPresets, resource);
    QVERIFY(r);

    ResourceTestHelper::testVersionedStorage(folderStorage,
                                             ResourceType::PaintOpPresets,
                                             PkString("paintoppresets/anewresource.0000.kpp"),
                                             m_dstLocation);
    ResourceTestHelper::testVersionedStorageIterator(folderStorage,
                                                     ResourceType::PaintOpPresets,
                                                     PkString("paintoppresets/anewresource.0000.kpp"));

}

void TestFolderStorage::testResourceFilePath()
{
    KoResourceSP resource(new DummyResource("anewresource.kpp", ResourceType::PaintOpPresets));
    resource->setValid(true);
    resource->setVersion(0);

    KisFolderStorage folderStorage(ResourceTestHelper::toPkString(m_dstLocation));
    bool r = folderStorage.saveAsNewVersion(ResourceType::PaintOpPresets, resource);
    QVERIFY(r);

    QCOMPARE(ResourceTestHelper::toQString(
                 folderStorage.resourceFilePath(PkString("paintoppresets/anewresource.0000.kpp"))),
             QFileInfo(m_dstLocation + "/" + "paintoppresets/anewresource.0000.kpp").absoluteFilePath());
}

void TestFolderStorage::testResourceCaseSensitivity()
{
    KoResourceSP resource(new DummyResource("resourcecasetest.kpp", ResourceType::PaintOpPresets));
    resource->setValid(true);
    resource->setVersion(0);

    KisFolderStorage folderStorage(ResourceTestHelper::toPkString(m_dstLocation));
    bool r = folderStorage.addResource(ResourceType::PaintOpPresets, resource);
    QVERIFY(r);

    KoResourceSP res1 = folderStorage.resource(PkString("paintoppresets/resourcecasetest.kpp"));
    QVERIFY(res1);
    QVERIFY(res1->filename() == PkString("resourcecasetest.kpp"));

    /**
     * In the folder storage we expect all resources to have
     * filesystem-dependent name resolution. That is, if we request a resource
     * with wrong casing, it should still fetch the resource. But the filename
     * field of the resource must contain the correct casing
     */

#if defined Q_OS_WIN || defined Q_OS_MACOS
    KoResourceSP res2 = folderStorage.resource(PkString("paintoppresets/ReSoUrCeCaSeTeSt.kpp"));
    QVERIFY(res2);
    QVERIFY(res2->filename() == PkString("resourcecasetest.kpp"));
#endif
}

void TestFolderStorage::cleanupTestCase()
{
    ResourceTestHelper::rmTestDb();
    ResourceTestHelper::cleanDstLocation(m_dstLocation);
}

SIMPLE_TEST_MAIN(TestFolderStorage)
