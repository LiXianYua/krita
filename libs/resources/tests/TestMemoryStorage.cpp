/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "TestMemoryStorage.h"
#include <simpletest.h>

#include <PkMemoryStream.h>

#include <KisMemoryStorage.h>
#include <KoResource.h>

#include "DummyResource.h"
#include "ResourceTestHelper.h"

#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing installing resources"
#endif

void TestMemoryStorage::initTestCase()
{
    ResourceTestHelper::createDummyLoaderRegistry();
}

void TestMemoryStorage ::testStorage()
{
    KisMemoryStorage memoryStorage;
    KoResourceSP resource(new DummyResource("test.gbr", ResourceType::Brushes));
    memoryStorage.saveAsNewVersion(ResourceType::Brushes, resource);

    PkSharedPointer<KisResourceStorage::ResourceIterator> iter = memoryStorage.resources(ResourceType::Brushes);
    QVERIFY(iter->hasNext());
    int count = 0;
    while (iter->hasNext()) {
        iter->next();
        QVERIFY(iter->resource());
        count++;
    }
    QCOMPARE(count, 1);
}

void TestMemoryStorage ::testStorageRetrieval()
{
    KisMemoryStorage memoryStorage;
    KoResourceSP resource1(new DummyResource("test1.gbr", ResourceType::Brushes));
    memoryStorage.saveAsNewVersion(ResourceType::Brushes, resource1);
    KoResourceSP resource2(new DummyResource("test2.gbr", ResourceType::Brushes));
    memoryStorage.saveAsNewVersion(ResourceType::Brushes, resource2);

    const PkString url("brushes/test1.0000.gbr");
    KoResourceSP resource = memoryStorage.resource(url);
    QVERIFY(resource);
    QCOMPARE(resource->filename(), "test1.0000.gbr");
}


void TestMemoryStorage::testAddResource()
{
    KisMemoryStorage memoryStorage;
    KoResourceSP res1(new DummyResource("test1.gbr", ResourceType::Brushes));
    memoryStorage.saveAsNewVersion(ResourceType::Brushes, res1);

    ResourceTestHelper::testVersionedStorage(memoryStorage, ResourceType::Brushes, PkString("brushes/test1.0000.gbr"));
    ResourceTestHelper::testVersionedStorageIterator(memoryStorage, ResourceType::Brushes, PkString("brushes/test1.0000.gbr"));
}

void TestMemoryStorage::testVersionSaveWithASubfolder()
{
    KisMemoryStorage memoryStorage;
    KoResourceSP res1(new DummyResource("subfolder/test1.gbr", ResourceType::Brushes));
    memoryStorage.saveAsNewVersion(ResourceType::Brushes, res1);

    // saving a version of a resource is expected to strip all the subfolders
    // from the resource file path

    const PkString url("brushes/test1.0000.gbr");
    KoResourceSP resource = memoryStorage.resource(url);
    QVERIFY(resource);
    QCOMPARE(resource->filename(), "test1.0000.gbr");
}

void TestMemoryStorage::testImportExportWithASubfolder()
{
    KisMemoryStorage memoryStorage;
    KoResourceSP res1(new DummyResource("test1.gbr", ResourceType::Brushes));

    {
        PkMemoryStream buf;
        buf.open(PkStream::WriteOnly);
        res1->saveToDevice(&buf);
        buf.close();

        KIS_ASSERT(buf.size() > 0);

        buf.open(PkStream::ReadOnly);
        memoryStorage.importResource(PkString("brushes/subfolder/test1.gbr"), &buf);
        buf.close();
    }

    KoResourceSP resource = memoryStorage.resource(PkString("brushes/subfolder/test1.gbr"));
    QVERIFY(resource);
    QCOMPARE(resource->filename(), "subfolder/test1.gbr");
    QVERIFY(!resource->md5Sum().isEmpty());

    {
        PkMemoryStream buf;
        buf.open(PkStream::WriteOnly);
        memoryStorage.exportResource(PkString("brushes/subfolder/test1.gbr"), &buf);
        buf.close();
        QVERIFY(buf.size() > 0);
    }
}

void TestMemoryStorage::testTagIterator()
{
    KisMemoryStorage memoryStorage;
    KoResourceSP res1(new DummyResource("test1.gbr", ResourceType::Brushes));
    memoryStorage.saveAsNewVersion(ResourceType::Brushes, res1);

    {
        KisTagSP tag(new KisTag());
        tag->setName("Test Tag 1");
        tag->setUrl("test_tag_url_1");
        tag->setResourceType(ResourceType::Brushes);
        tag->setDefaultResources({res1->filename()});
        tag->setValid(true);

        memoryStorage.testingAddTag(ResourceType::Brushes, tag);
    }

    {
        KisTagSP tag(new KisTag());
        tag->setName("Test Tag 2");
        tag->setUrl("test_tag_url_2");
        tag->setResourceType(ResourceType::Brushes);
        tag->setDefaultResources({res1->filename()});
        tag->setValid(true);

        memoryStorage.testingAddTag(ResourceType::Brushes, tag);
    }

    {
        const PkStringList expectedUrls = {PkString("test_tag_url_1"), PkString("test_tag_url_2")};
        auto it = memoryStorage.tags(ResourceType::Brushes);
        int numTags = 0;
        while (it->hasNext()) {
            it->next();
            QCOMPARE(it->tag()->url(), expectedUrls[numTags]);
            numTags++;
        }
        QCOMPARE(numTags, 2);
    }

    memoryStorage.testingRemoveTag(ResourceType::Brushes, PkString("test_tag_url_1"));

    {
        const PkStringList expectedUrls = {PkString("test_tag_url_2")};
        auto it = memoryStorage.tags(ResourceType::Brushes);
        int numTags = 0;
        while (it->hasNext()) {
            it->next();
            QCOMPARE(it->tag()->url(), expectedUrls[numTags]);
            numTags++;
        }
        QCOMPARE(numTags, 1);
    }
}

SIMPLE_TEST_MAIN(TestMemoryStorage)
