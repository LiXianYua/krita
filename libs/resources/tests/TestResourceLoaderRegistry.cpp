/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "TestResourceLoaderRegistry.h"
#include <simpletest.h>
#include <PkMemoryStream.h>

#include <KisResourceLoaderRegistry.h>
#include <KisResourceLoader.h>
#include <KoResource.h>
#include <KisGlobalResourcesInterface.h>

#include "DummyResource.h"
void TestResourceLoaderRegistry::testRegistry()
{
    KisResourceLoaderRegistry *reg = KisResourceLoaderRegistry::instance();

    KisResourceLoaderBase *loader = new KisResourceLoader<DummyResource>(
        PkString("dummy"),
        PkString("dummy"),
        PkString("Dummy"),
        PkStringList{PkString("x-dummy")});
    reg->registerLoader(loader);
    QVERIFY(reg->count() == 1);

    KisResourceLoaderBase *l2 = reg->get(PkString("dummy"));
    QVERIFY(l2 == loader);

    PkMemoryStream stream;
    QVERIFY(stream.open(PkStream::ReadOnly));
    KoResourceSP res = l2->load(PkString("test"), stream, KisGlobalResourcesInterface::instance());
    QVERIFY(res.data());
    QVERIFY(dynamic_cast<DummyResource*>(res.data()));
}

SIMPLE_TEST_MAIN(TestResourceLoaderRegistry)
