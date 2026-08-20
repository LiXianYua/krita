/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "TestTag.h"
#include <simpletest.h>
#include <PkFileStream.h>
#include <PkMemoryStream.h>

#include <KisTag.h>
#include <KoResource.h>

#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing installing resources"
#endif

void TestTag::testLoadTag()
{
    KisTag tagLoader;
    const QString filePath = QString(FILES_DATA_DIR) + "paintoppresets/test.tag";
    const QByteArray filePathUtf8 = filePath.toUtf8();
    PkFileStream stream(PkString::PkFromUtf8(filePathUtf8.constData(), filePathUtf8.size()));

    QVERIFY(QFileInfo::exists(filePath));
    QVERIFY(stream.open(PkStream::ReadOnly));

    bool r = tagLoader.load(stream);

    stream.close();

    QVERIFY(r);
    QVERIFY(KisTag::currentLocale() == PkString("en_US"));
    QVERIFY(tagLoader.name() == PkString("* Favorites"));
    QVERIFY(tagLoader.comment() == PkString("Your favorite brush presets"));
    QVERIFY(tagLoader.url() == PkString("* Favorites"));

    const PkMap<PkString, PkString> names = tagLoader.names();
    const PkMap<PkString, PkString> comments = tagLoader.comments();
    QVERIFY(names.value(PkString("nl")) == PkString("* Favorieten"));
    QVERIFY(comments.value(PkString("nl")) ==
            PkString("Uw favorite voorinstellingen van penselen"));
}

void TestTag::testSaveTag()
{
    KisTag tag1;
    const QString filePath = QString(FILES_DATA_DIR) + "paintoppresets/test.tag";
    const QByteArray filePathUtf8 = filePath.toUtf8();
    PkFileStream stream(PkString::PkFromUtf8(filePathUtf8.constData(), filePathUtf8.size()));

    QVERIFY(QFileInfo::exists(filePath));
    QVERIFY(stream.open(PkStream::ReadOnly));

    bool r = tag1.load(stream);
    QVERIFY(r);
    stream.close();

    tag1.setName(PkString("Test"));

    PkMemoryStream buffer;
    QVERIFY(buffer.open(PkStream::WriteOnly));

    QVERIFY(tag1.save(buffer));

    buffer.close();
    QVERIFY(buffer.open(PkStream::ReadOnly));

    KisTag tag2;
    QVERIFY(tag2.load(buffer));
    QVERIFY(tag2.url() == tag1.url());
    QVERIFY(tag2.name() == tag1.name());
    QVERIFY(tag2.resourceType() == tag1.resourceType());
    QVERIFY(tag2.comment() == tag1.comment());
    QVERIFY(tag2.defaultResources() == tag1.defaultResources());

}

SIMPLE_TEST_MAIN(TestTag)
