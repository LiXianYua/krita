/*
 * SPDX-FileCopyrightText: 2018 boud <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "TestTagModel.h"

#include <simpletest.h>
#include <QStandardPaths>
#include <QDir>
#include <QVersionNumber>
#include <QDirIterator>
#include <PkSqlQuery.h>
#include <PkFileStream.h>

#include <PkConfigGroup.h>
#include <PkSharedConfig.h>

#include <KisResourceCacheDb.h>
#include <KisResourceLocator.h>
#include <KisTagModel.h>
#include <KisResourceModel.h>
#include <DummyResource.h>
#include <ResourceTestHelper.h>


#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing installing resources"
#endif


void TestTagModel::initTestCase()
{
    ResourceTestHelper::initTestDb();
    ResourceTestHelper::createDummyLoaderRegistry();

    m_srcLocation = QString(FILES_DATA_DIR);
    QVERIFY2(QDir(m_srcLocation).exists(), m_srcLocation.toUtf8());

    m_dstLocation = ResourceTestHelper::filesDestDir();
    ResourceTestHelper::cleanDstLocation(m_dstLocation);

    PkConfigGroup cfg(PkSharedConfig::openConfig(), PkString());
    cfg.writeEntry(KisResourceLocator::resourceLocationKey,
                   ResourceTestHelper::toPkString(m_dstLocation));

    m_locator = KisResourceLocator::instance();

    if (!KisResourceCacheDb::initialize(ResourceTestHelper::toPkString(
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)))) {
        qDebug() << "Could not initialize KisResourceCacheDb on" << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    QVERIFY(KisResourceCacheDb::isValid());

    KisResourceLocator::LocatorError r =
        m_locator->initialize(ResourceTestHelper::toPkString(m_srcLocation));
    for (const PkString &message : m_locator->errorMessages()) {
        qDebug() << ResourceTestHelper::toQString(message);
    }

    QVERIFY(r == KisResourceLocator::LocatorError::Ok);
    QVERIFY(QDir(m_dstLocation).exists());

    m_tag.reset(new KisTag());
    PkFileStream f(ResourceTestHelper::toPkString(
        QString(FILES_DATA_DIR) + "paintoppresets/test.tag"));
    KIS_ASSERT(f.open(PkStream::ReadOnly));
    m_tag->load(f);

    KisTagModel tagModel(m_resourceType);
    m_tag = tagModel.tagForUrl(m_tag->url());
}

void TestTagModel::testWithTagModelTester()
{
    KisTagModel model(m_resourceType);
    for (const KisTagSP &tag : model.tags()) {
        QVERIFY(tag);
        QVERIFY(tag->valid());
        QCOMPARE(model.tagForUrl(tag->url())->id(), tag->id());
    }
}


void TestTagModel::testRowCount()
{
    PkSqlQuery q;
    QVERIFY(q.prepare("SELECT count(*)\n"
                      "FROM   tags\n"
                      ",      resource_types\n"
                      "WHERE  tags.resource_type_id = resource_types.id\n"
                      "AND    resource_types.name = :resource_type"));
    q.bindValue(":resource_type", m_resourceType);
    QVERIFY(q.exec());
    q.first();
    int rowCount = q.value(0).toInt();
    QCOMPARE(rowCount, 1);

    KisTagModel tagModel(m_resourceType);
    // There is always an "All" tag in the first row
    QCOMPARE(static_cast<int>(tagModel.tags().size()), rowCount + 2);
}

void TestTagModel::testData()
{
    KisTagModel tagModel(m_resourceType);
    const auto tags = tagModel.tags();
    QVERIFY(tags.size() >= 3);
    QCOMPARE(ResourceTestHelper::toQString(tags[0]->name()), QString("All"));
    QCOMPARE(ResourceTestHelper::toQString(tags[0]->url()), QString("All"));
    QCOMPARE(ResourceTestHelper::toQString(tags[1]->name()), QString("All untagged"));
    QCOMPARE(ResourceTestHelper::toQString(tags[1]->url()), QString("All untagged"));
    QCOMPARE(ResourceTestHelper::toQString(tags[2]->name()), QString("* Favorites"));
    QCOMPARE(ResourceTestHelper::toQString(tags[2]->url()), QString("* Favorites"));

}

void TestTagModel::testIndexForTag()
{
    KisTagModel tagModel(m_resourceType);
    const KisTagSP tag = tagModel.tagForUrl(m_tag->url());
    QVERIFY(tag);
    QCOMPARE(ResourceTestHelper::toQString(tag->url()),
             ResourceTestHelper::toQString(m_tag->url()));
    QCOMPARE(ResourceTestHelper::toQString(tag->name()),
             ResourceTestHelper::toQString(m_tag->name()));
}

void TestTagModel::testTagForIndex()
{
    KisTagModel tagModel(m_resourceType);

    const auto tags = tagModel.tags();
    QVERIFY(tags.size() >= 3);
    KisTagSP tag = tags[0];
    QCOMPARE(ResourceTestHelper::toQString(tag->url()), QString("All"));

    tag = tags[1];
    QCOMPARE(ResourceTestHelper::toQString(tag->url()), QString("All untagged"));

    tag = tags[2];
    QCOMPARE(ResourceTestHelper::toQString(tag->url()),
             ResourceTestHelper::toQString(m_tag->url()));
}

void TestTagModel::testTagForUrl()
{
    KisTagModel tagModel(m_resourceType);

    KisTagSP tag = tagModel.tagForUrl("All");
    QVERIFY(tag);
    QCOMPARE(ResourceTestHelper::toQString(tag->url()), QString("All"));

    tag = tagModel.tagForUrl("All untagged");
    QVERIFY(tag);
    QCOMPARE(ResourceTestHelper::toQString(tag->url()), QString("All untagged"));

    tag = tagModel.tagForUrl(m_tag->url());
    QVERIFY(tag);
    QCOMPARE(ResourceTestHelper::toQString(tag->url()),
             ResourceTestHelper::toQString(m_tag->url()));
}

void TestTagModel::testAddEmptyTag()
{
    KisTagModel tagModel(m_resourceType);

    QString tagName("A Brand New Tag");

    int rowCount = static_cast<int>(tagModel.tags().size());
    KisTagSP tag = tagModel.addTag(ResourceTestHelper::toPkString(tagName), false, {});

    QCOMPARE(static_cast<int>(tagModel.tags().size()), rowCount + 1);
    QVERIFY(tag);
    QCOMPARE(ResourceTestHelper::toQString(tag->name()), tagName);
    QCOMPARE(tag->id(), 2);
}

void TestTagModel::testAddTag()
{
    KisTagModel tagModel(m_resourceType);

    QString tagName("test1");

    KisTagSP tag(new KisTag);
    tag->setUrl(ResourceTestHelper::toPkString(tagName));
    tag->setName(ResourceTestHelper::toPkString(tagName));
    tag->setComment("A tag for testing");
    tag->setValid(true);
    tag->setActive(true);

    int rowCount = static_cast<int>(tagModel.tags().size());
    tagModel.addTag(tag, false, {});
    QCOMPARE(static_cast<int>(tagModel.tags().size()), rowCount + 1);
    QVERIFY(tag->id() >= 0);

    {
        QCOMPARE(static_cast<int>(tagModel.tags().size()), rowCount + 1);
        KisTagSP stored = tagModel.tagForUrl(tag->url());
        QVERIFY(stored);
        QCOMPARE(ResourceTestHelper::toQString(stored->url()),
                 ResourceTestHelper::toQString(tag->url()));
        QCOMPARE(ResourceTestHelper::toQString(stored->name()),
                 ResourceTestHelper::toQString(tag->name()));
        QCOMPARE(ResourceTestHelper::toQString(stored->name()), tagName);
        QCOMPARE(stored->id(), 3);
    }

    {
        KisTagSP stored = tagModel.tagForUrl(tag->url());
        QVERIFY(stored);
        QCOMPARE(ResourceTestHelper::toQString(stored->url()),
                 ResourceTestHelper::toQString(tag->url()));
        QCOMPARE(ResourceTestHelper::toQString(stored->name()),
                 ResourceTestHelper::toQString(tag->name()));
    }

}

void TestTagModel::testSetTagActiveInactive()
{
    KisTagModel tagModel(m_resourceType);

    int rowCount = static_cast<int>(tagModel.tags().size());

    tagModel.setTagInactive(m_tag);
    QVERIFY(!m_tag->active());
    QCOMPARE(static_cast<int>(tagModel.tags().size()), rowCount - 1);
    tagModel.setTagFilter(KisTagModel::ShowAllTags);
    KisTagSP stored = tagModel.tagForUrl(m_tag->url());
    QVERIFY(stored);
    QVERIFY(!stored->active());


    tagModel.setTagActive(m_tag);
    QVERIFY(m_tag->active());
    QCOMPARE(static_cast<int>(tagModel.tags().size()), rowCount);

    stored = tagModel.tagForUrl(m_tag->url());
    QVERIFY(stored);
    QCOMPARE(ResourceTestHelper::toQString(stored->url()),
             ResourceTestHelper::toQString(m_tag->url()));
    QCOMPARE(ResourceTestHelper::toQString(stored->name()),
             ResourceTestHelper::toQString(m_tag->name()));
    QVERIFY(stored->active());
}

void TestTagModel::testRenameTag()
{
    KisTagModel tagModel(m_resourceType);
    const auto tagsBefore = tagModel.tags();
    QVERIFY(tagsBefore.size() > 2);
    KisTagSP tag = tagsBefore[2];
    QCOMPARE(ResourceTestHelper::toQString(tag->url()),
             ResourceTestHelper::toQString(m_tag->url()));
    QCOMPARE(ResourceTestHelper::toQString(tag->name()),
             ResourceTestHelper::toQString(m_tag->name()));

    QVERIFY(tagModel.renameTag(tag, "Another name altogether", true));

    /// We are renaming "* Favorites" into "Another...", which
    /// changed position of the item due to sorting order

    const auto tagsAfter = tagModel.tags();
    QVERIFY(tagsAfter.size() > 3);
    tag = tagsAfter[3];

    QCOMPARE(ResourceTestHelper::toQString(tag->url()), QString("Another name altogether"));
    QCOMPARE(ResourceTestHelper::toQString(tag->name()), QString("Another name altogether"));
}

void TestTagModel::testChangeTagActive()
{
    KisTagModel tagModel(m_resourceType);

    int rowCount = static_cast<int>(tagModel.tags().size());


    KisTagSP tagToActivate = tagModel.tagForUrl("Another name altogether");

    tagModel.changeTagActive(tagToActivate, false);
    QVERIFY(!tagToActivate->active());
    QCOMPARE(static_cast<int>(tagModel.tags().size()), rowCount - 1);
    tagModel.setTagFilter(KisTagModel::ShowAllTags);
    KisTagSP stored = tagModel.tagForUrl(tagToActivate->url());
    QVERIFY(stored);
    QVERIFY(!stored->active());

    tagModel.changeTagActive(tagToActivate, true);
    QVERIFY(tagToActivate->active());
    QCOMPARE(static_cast<int>(tagModel.tags().size()), rowCount);

    stored = tagModel.tagForUrl(tagToActivate->url());
    QVERIFY(stored);
    QCOMPARE(ResourceTestHelper::toQString(stored->url()),
             ResourceTestHelper::toQString(tagToActivate->url()));
    QCOMPARE(ResourceTestHelper::toQString(stored->name()), QString("Another name altogether"));
    QVERIFY(stored->active());

}

void TestTagModel::testAddEmptyTagWithResources()
{
    KisTagModel tagModel(m_resourceType);
    KisResourceModel resourceModel("paintoppresets");

    QString tagName("A Brand New Tag");
    PkVector<KoResourceSP> resources;
    for (const KisResourceRecord &record : resourceModel.records()) {
        resources << resourceModel.resourceForId(record.id);
    }

    tagModel.addTag(ResourceTestHelper::toPkString(tagName), false, resources);

    // XXX: check KisTagResourceModel
}

void TestTagModel::testAddTagWithResources()
{
    KisTagModel tagModel(m_resourceType);
    KisResourceModel resourceModel("paintoppresets");

    QString tagName("test1");

    const auto records = resourceModel.records();
    QVERIFY(!records.empty());
    KoResourceSP resource = resourceModel.resourceForId(records[0].id);

    KisTagSP tag(new KisTag);
    tag->setUrl(ResourceTestHelper::toPkString(tagName));
    tag->setName(ResourceTestHelper::toPkString(tagName));
    tag->setComment("A tag for testing");
    tag->setValid(true);
    tag->setActive(true);
    tag->setResourceType("paintoppresets");

    tagModel.addTag(tag, true, {resource});
    QVERIFY(tag->id() >= 0);

    // XXX: check KisTagResourceModel

}

void TestTagModel::cleanupTestCase()
{
    ResourceTestHelper::rmTestDb();
    ResourceTestHelper::cleanDstLocation(m_dstLocation);
}





SIMPLE_TEST_MAIN(TestTagModel)
