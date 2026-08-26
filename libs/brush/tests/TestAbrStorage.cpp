/*
 * SPDX-FileCopyrightText: 2018 boud <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2019 Agata Cacko <cacko.azh@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TestAbrStorage.h"


#include <simpletest.h>

#include <KoConfig.h>

#include <KisAbrStorage.h>
#include <KisBundleStorage.h>
#include <KoResource.h>
#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing the importing of files in krita"
#endif

void TestAbrStorage::initTestCase()
{
    // nothing to do
}

void TestAbrStorage::testResourceIterator()
{
    PkString filename = "brushes_by_mar_ka_d338ela.abr";

    KisAbrStorage storage(PkString(FILES_DATA_DIR) + "/" + filename);

    PkSharedPointer<KisResourceStorage::ResourceIterator> iter(storage.resources(ResourceType::Brushes));
    QVERIFY(iter->hasNext());
    int count = 0;
    while (iter->hasNext()) {
        iter->next();
        KoResourceSP res(iter->resource());
        QVERIFY(res);
        count++;
    }
    QVERIFY(count > 0);

    PkSharedPointer<KisResourceStorage::ResourceIterator> iter2(storage.resources(ResourceType::LayerStyles));
    QVERIFY(!iter2->hasNext());
    count = 0;
    while (iter2->hasNext()) {
        iter2->next();
        KoResourceSP res(iter2->resource());
        QVERIFY(res);
        count++;
    }
    QVERIFY(count == 0);

}

void TestAbrStorage::testTagIterator()
{
    PkString filename = "brushes_by_mar_ka_d338ela.abr";
    KisAbrStorage storage(PkString(FILES_DATA_DIR) + "/" + filename);

    PkSharedPointer<KisResourceStorage::TagIterator> iter = storage.tags(ResourceType::Brushes);
    int count = 0;
    while (iter->hasNext()) {
        iter->next();
        count++;
    }
    QVERIFY(count == 1);
}

void TestAbrStorage::testResourceItem()
{
    PkString name = "brushes_by_mar_ka_d338ela";
    PkString filename = name + ".abr";
    PkString resourceName = name + "_2"; // "1" seem to be invalid or something; it isn't not loaded in any case.
    KisAbrStorage storage(PkString(FILES_DATA_DIR) + "/" + filename);

    KisResourceStorage::ResourceItem item = storage.resourceItem(resourceName);
    QVERIFY(!item.url.isEmpty());
}

void TestAbrStorage::testResource()
{
    PkString name = "brushes_by_mar_ka_d338ela";
    PkString filename = name + ".abr";
    PkString resourceName = name + "_2"; // "1" seem to be invalid or something; it isn't not loaded in any case.
    KisAbrStorage storage(PkString(FILES_DATA_DIR) + "/" + filename);
    KoResourceSP res = storage.resource(resourceName);
    QVERIFY(res);
    QVERIFY(res->filename() == resourceName);
}


SIMPLE_TEST_MAIN(TestAbrStorage)

