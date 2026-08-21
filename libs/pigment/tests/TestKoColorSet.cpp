/*
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <simpletest.h>
#include "TestKoColorSet.h"
#include <KoColorSet.h>
#include <KisGlobalResourcesInterface.h>
#include <cstdio>
#include <KisSwatch.h>
#include <KisSwatchGroup.h>
#include <kundo2stack.h>
#include <KoColorSpaceRegistry.h>

#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing the importing of files in krita"
#endif

// 原文件扩展名取法（suffix().toLower()）的零 Qt 替身：取路径最后一个 '.' 之后的部分并转小写。
static std::string fileSuffix(const PkString &path)
{
    const auto parts = path.split('.');
    return parts.empty() ? std::string() : parts.back().toLower().PkToUtf8();
}

void TestKoColorSet::testLoadGPL()
{
    KoColorSet set(PkString(FILES_DATA_DIR)+ "/gimp.gpl");
    PK_VERIFY(set.load(KisGlobalResourcesInterface::instance()));
    PK_COMPARE(set.paletteType(), KoColorSet::GPL);

    PK_COMPARE(set.colorCount(), 17);

    set.setFilename("test.gpl");
    PK_VERIFY(set.save());
    PK_VERIFY(set.filename() == "test.gpl");
    PK_COMPARE(set.paletteType(), KoColorSet::GPL);

    KoColorSet set2("test.gpl");
    PK_VERIFY(set2.load(KisGlobalResourcesInterface::instance()));
    PK_COMPARE(set2.paletteType(), KoColorSet::GPL);

}
void TestKoColorSet::testLoadRIFF()
{

    KoColorSet set(PkString(FILES_DATA_DIR)+ "/ms_riff.pal");
    PK_VERIFY(set.load(KisGlobalResourcesInterface::instance()));
    PK_COMPARE(set.paletteType(), KoColorSet::RIFF_PAL);

    PK_COMPARE(set.colorCount(), 17);

    set.setFilename("test_riff.pal");
    PK_VERIFY(set.save());
    PK_VERIFY(set.filename() == "test_riff.pal");
    PK_COMPARE(set.paletteType(), KoColorSet::RIFF_PAL);

    KoColorSet set2("test_riff.pal");
    PK_VERIFY(set2.load(KisGlobalResourcesInterface::instance()));
    PK_COMPARE(set2.paletteType(), KoColorSet::KPL);
}
void TestKoColorSet::testLoadACT()
{
    KoColorSet set(PkString(FILES_DATA_DIR)+ "/photoshop.act");
    PK_VERIFY(set.load(KisGlobalResourcesInterface::instance()));
    PK_COMPARE(set.paletteType(), KoColorSet::ACT);
    PK_VERIFY(set.valid());

    PK_COMPARE(set.colorCount(), 257);

    std::remove("test.act");

    set.setFilename("test.act");
    PK_VERIFY(set.save());

    KoColorSet set2("test.act");
    PK_VERIFY(set2.load(KisGlobalResourcesInterface::instance()));
    PK_COMPARE(set2.paletteType(), KoColorSet::KPL);
}

void TestKoColorSet::testLoadPSP_PAL()
{
    KoColorSet set(PkString(FILES_DATA_DIR)+ "/jasc.pal");
    PK_VERIFY(set.load(KisGlobalResourcesInterface::instance()));
    PK_COMPARE(set.paletteType(), KoColorSet::PSP_PAL);
    PK_VERIFY(set.valid());

    PK_COMPARE(set.colorCount(), 249);

    std::remove("test_jasc.pal");

    set.setFilename("test_jasc.pal");
    PK_VERIFY(set.save());

    KoColorSet set2("test_jasc.pal");
    PK_VERIFY(set2.load(KisGlobalResourcesInterface::instance()));
    PK_COMPARE(set2.paletteType(), KoColorSet::KPL);


}
void TestKoColorSet::testLoadACO()
{
    KoColorSet set(PkString(FILES_DATA_DIR)+ "/photoshop.aco");
    PK_VERIFY(set.load(KisGlobalResourcesInterface::instance()));
    PK_COMPARE(set.paletteType(), KoColorSet::ACO);
    PK_VERIFY(set.valid());

    PK_COMPARE(set.colorCount(), 18);

    std::remove("test.aco");

    set.setFilename("test.aco");
    PK_VERIFY(set.save());

    KoColorSet set2("test.aco");
    PK_VERIFY(set2.load(KisGlobalResourcesInterface::instance()));
    PK_COMPARE(set2.paletteType(), KoColorSet::KPL);
}
void TestKoColorSet::testLoadXML()
{
    KoColorSet set(PkString(FILES_DATA_DIR)+ "/scribus.xml");
    PK_VERIFY(set.load(KisGlobalResourcesInterface::instance()));
    PK_COMPARE(set.paletteType(), KoColorSet::XML);
    PK_VERIFY(set.valid());

    PK_COMPARE(set.colorCount(), 8);

    std::remove("test.xml");

    set.setFilename("test.xml");
    PK_VERIFY(set.save());

    KoColorSet set2("test.xml");
    PK_VERIFY(set2.load(KisGlobalResourcesInterface::instance()));
    PK_COMPARE(set2.paletteType(), KoColorSet::KPL);
}
void TestKoColorSet::testLoadKPL()
{
    KoColorSet set(PkString(FILES_DATA_DIR)+ "/krita.kpl");
    PK_VERIFY(set.load(KisGlobalResourcesInterface::instance()));
    PK_COMPARE(set.paletteType(), KoColorSet::KPL);
    PK_VERIFY(set.valid());

    PK_COMPARE(set.colorCount(), 0);

    std::remove("test.kpl");

    set.setFilename("test.kpl");
    PK_VERIFY(set.save());
    PK_COMPARE(set.paletteType(), KoColorSet::KPL);

    KoColorSet set2("test.kpl");
    PK_VERIFY(set2.load(KisGlobalResourcesInterface::instance()));
    PK_COMPARE(set2.paletteType(), KoColorSet::KPL);

}

void TestKoColorSet::testLoadSBZ_data()
{
    PkTest::addColumn<PkString>("fileName");
    PkTest::addColumn<int>("expectedColorCount");
    PkTest::addColumn<PkString>("sampleGroup");
    PkTest::addColumn<PkPoint>("samplePoint");
    PkTest::addColumn<PkString>("expectedColorModel");
    PkTest::addColumn<PkString>("expectedColorDepth");
    PkTest::addColumn<PkString>("expectedName");
    PkTest::addColumn<PkString>("expectedId");
    PkTest::addColumn<PkVector<float>>("expectedResult");

    PkTest::newRow("swatchbook.sbz")
        << "swatchbook.sbz"
        << 5
        << "Sample swatch book" << PkPoint(1, 0)
        << RGBAColorModelID.id()
        << Float32BitsColorDepthID.id()
        << "Cyan 100%"
        << "C10"
        << PkVector<float>({0,1,1,1});

    PkTest::newRow("sample_swatchbook.sbz")
        << "sample_swatchbook.sbz"
        << 47
        << "" << PkPoint(5, 1)
        << CMYKAColorModelID.id()
        << Float32BitsColorDepthID.id()
        << "Cyan 90%"
        << "C09"
        << PkVector<float>({90,0,0,0,1.0});
}

void TestKoColorSet::testLoadSBZ()
{
    PK_FETCH(PkString, fileName);
    PK_FETCH(int, expectedColorCount);
    PK_FETCH(PkString, sampleGroup);
    PK_FETCH(PkPoint, samplePoint);
    PK_FETCH(PkString, expectedColorModel);
    PK_FETCH(PkString, expectedColorDepth);
    PK_FETCH(PkString, expectedName);
    PK_FETCH(PkString, expectedId);
    PK_FETCH(PkVector<float>, expectedResult);

    KoColorSet set(PkString(FILES_DATA_DIR) + "/" + fileName);
    PK_VERIFY(set.load(KisGlobalResourcesInterface::instance()));
    PK_COMPARE(set.paletteType(), KoColorSet::SBZ);

    PK_COMPARE(set.colorCount(), expectedColorCount);

#if 0
    qDebug() << set.swatchGroupNames();

    const int numColumns = set.getGroup(sampleGroup)->columnCount();
    const int numRows = set.getGroup(sampleGroup)->rowCount();

    for (int row = 0; row < numRows; row++) {
        for (int col = 0; col < numColumns; col++) {
            KisSwatch s = set.getSwatchFromGroup(col, row, sampleGroup);

            qDebug() << row << col << ppVar(s.name()) << ppVar(s.id()) << ppVar(s.color());
        }
    }
#endif

    KisSwatch s = set.getSwatchFromGroup(samplePoint.x(), samplePoint.y(), sampleGroup);
    PK_COMPARE(s.name(), expectedName);
    PK_COMPARE(s.id(), expectedId);

    KoColor c = s.color();
    PK_COMPARE(c.colorSpace()->colorModelId().id(), expectedColorModel);
    PK_COMPARE(c.colorSpace()->colorDepthId().id(), expectedColorDepth);

    const float *ptr = reinterpret_cast<float*>(c.data());
    for (int i = 0; i < expectedResult.size(); i++) {
        PK_COMPARE(ptr[i], expectedResult[i]);
    }

    set.setFilename("test.sbz");
    PK_VERIFY(set.save());
    PK_VERIFY(set.filename() == "test.sbz");
    PK_COMPARE(set.paletteType(), KoColorSet::SBZ);

    KoColorSet set2("test.sbz");
    PK_VERIFY(set2.load(KisGlobalResourcesInterface::instance()));
    PK_COMPARE(set2.paletteType(), KoColorSet::KPL);
}

void TestKoColorSet::testLoadASE()
{
    KoColorSet set(PkString(FILES_DATA_DIR) + "/photoshop.ase");
    PK_VERIFY(set.load(KisGlobalResourcesInterface::instance()));
    PK_COMPARE(set.paletteType(), KoColorSet::ASE);

    PK_COMPARE(set.colorCount(), 249);

    set.setFilename("test.ase");
    PK_VERIFY(set.save());
    PK_VERIFY(set.filename() == "test.ase");
    PK_COMPARE(set.paletteType(), KoColorSet::ASE);

    KoColorSet set2("test.ase");
    PK_VERIFY(set2.load(KisGlobalResourcesInterface::instance()));
    PK_COMPARE(set2.paletteType(), KoColorSet::KPL);
}

void TestKoColorSet::testLoadACB()
{
    KoColorSet set(PkString(FILES_DATA_DIR) + "/photoshop.acb");
    PK_VERIFY(set.load(KisGlobalResourcesInterface::instance()));
    PK_COMPARE(set.paletteType(), KoColorSet::ACB);

    PK_COMPARE(set.colorCount(), 23);
    PK_COMPARE(set.name(), "test");

    set.setFilename("test.acb");
    PK_VERIFY(set.save());
    PK_VERIFY(set.filename() == "test.acb");
    PK_COMPARE(set.paletteType(), KoColorSet::ACB);

    KoColorSet set2("test.acb");
    PK_VERIFY(set2.load(KisGlobalResourcesInterface::instance()));
    PK_COMPARE(set2.paletteType(), KoColorSet::KPL);
}

void TestKoColorSet::TestKoColorSet::testLock()
{
    KoColorSetSP cs = createColorSet();
    PK_VERIFY(!cs->isLocked());
    PK_VERIFY(!cs->isDirty());
    cs->setLocked(true);
    PK_VERIFY(!cs->isDirty());
    PK_VERIFY(cs->isLocked());
    PK_VERIFY(cs->getGlobalGroup()->colorCount() == 0);
    cs->addSwatch(KisSwatch(red(), "red"));
    PK_VERIFY(cs->getGlobalGroup()->colorCount() == 0);
    PK_VERIFY(!cs->isDirty());
    cs->setLocked(false);
    cs->addSwatch(KisSwatch(red(), "red"));
    PK_VERIFY(cs->getGlobalGroup()->colorCount() == 1);
    PK_VERIFY(cs->isDirty());
}

void TestKoColorSet::testColumnCount()
{
    KoColorSetSP cs = createColorSet();
    PK_VERIFY(cs->columnCount() == KisSwatchGroup::DEFAULT_COLUMN_COUNT);
    cs->setColumnCount(200);
    PK_VERIFY(cs->columnCount() == 200);
    PK_VERIFY(cs->isDirty());
    cs->undoStack()->undo();
    PK_VERIFY(cs->columnCount() == KisSwatchGroup::DEFAULT_COLUMN_COUNT);
    PK_VERIFY(!cs->isDirty());
    cs->undoStack()->redo();
    PK_VERIFY(cs->columnCount() == 200);
    PK_VERIFY(cs->isDirty());
}

void TestKoColorSet::testComment()
{
    KoColorSetSP cs = createColorSet();
    PK_VERIFY(cs->comment().isEmpty());
    cs->setComment("dummy");
    PK_VERIFY(cs->comment() == "dummy");
    PK_VERIFY(cs->isDirty());
    cs->undoStack()->undo();
    PK_VERIFY(cs->comment().isEmpty());
    PK_VERIFY(!cs->isDirty());
    cs->undoStack()->redo();
    PK_VERIFY(cs->comment() == "dummy");
    PK_VERIFY(cs->isDirty());
}

void TestKoColorSet::testPaletteType()
{
    KoColorSetSP cs = createColorSet();
    PK_VERIFY(cs->paletteType() == KoColorSet::KPL);
    PK_COMPARE(fileSuffix(cs->filename()), "kpl");

    cs->setPaletteType(KoColorSet::GPL);
    PK_VERIFY(cs->paletteType() == KoColorSet::GPL);
    PK_COMPARE(fileSuffix(cs->filename()), "gpl");
    PK_VERIFY(cs->isDirty());

    cs->undoStack()->undo();
    PK_VERIFY(cs->paletteType() == KoColorSet::KPL);
    PK_COMPARE(fileSuffix(cs->filename()), "kpl");
    PK_VERIFY(!cs->isDirty());

    cs->undoStack()->redo();
    PK_VERIFY(cs->paletteType() == KoColorSet::GPL);
    PK_COMPARE(fileSuffix(cs->filename()), "gpl");
    PK_VERIFY(cs->isDirty());
}

void TestKoColorSet::testAddSwatch()
{
    KoColorSetSP cs = createColorSet();
    cs->addSwatch(KisSwatch(red(), "red"));
    PK_VERIFY(cs->getGlobalGroup()->colorCount() == 1);
    PK_VERIFY(cs->isDirty());

    cs->undoStack()->undo();
    PK_VERIFY(cs->getGlobalGroup()->colorCount() == 0);
    PK_VERIFY(!cs->isDirty());

    cs->undoStack()->redo();
    PK_VERIFY(cs->getGlobalGroup()->colorCount() == 1);
    PK_VERIFY(cs->isDirty());
}

void TestKoColorSet::testRemoveSwatch()
{
    KoColorSetSP cs = createColorSet();
    cs->addSwatch(KisSwatch(red()), KoColorSet::GLOBAL_GROUP_NAME, 5, 5);
    KisSwatch sw = cs->getSwatchFromGroup(5, 5, KoColorSet::GLOBAL_GROUP_NAME);
    PK_VERIFY(sw.isValid());
    PkColor c = sw.color().toQColor();
    PK_VERIFY(c == PkColor(Qt::red));

    KisSwatchGroupSP group = cs->getGlobalGroup();
    PK_VERIFY(group);

    cs->removeSwatch(5, 5, group);
    PK_VERIFY(cs->isDirty());
    sw = cs->getSwatchFromGroup(5, 5, KoColorSet::GLOBAL_GROUP_NAME);
    PK_VERIFY(!sw.isValid());

    cs->undoStack()->undo();
    sw = cs->getSwatchFromGroup(5, 5, KoColorSet::GLOBAL_GROUP_NAME);
    PK_VERIFY(sw.isValid());
    c = sw.color().toQColor();
    PK_VERIFY(c == PkColor(Qt::red));

    PK_VERIFY(cs->isDirty());

    // Undo the initial addSwatch
    cs->undoStack()->undo();
    PK_VERIFY(!cs->isDirty());
}

void TestKoColorSet::testAddGroup()
{
    KoColorSetSP cs = createColorSet();
    cs->addGroup("newgroup");
    PK_VERIFY(cs->swatchGroupNames().size() == 2);
    PK_VERIFY(cs->swatchGroupNames().contains(KoColorSet::GLOBAL_GROUP_NAME));
    PK_VERIFY(cs->swatchGroupNames().contains("newgroup"));
    PK_VERIFY(cs->isDirty());

    cs->undoStack()->undo();
    PK_VERIFY(cs->swatchGroupNames().size() == 1);
    PK_VERIFY(cs->swatchGroupNames().contains(KoColorSet::GLOBAL_GROUP_NAME));
    PK_VERIFY(!cs->swatchGroupNames().contains("newgroup"));
    PK_VERIFY(!cs->isDirty());

    cs->undoStack()->redo();
    PK_VERIFY(cs->swatchGroupNames().size() == 2);
    PK_VERIFY(cs->swatchGroupNames().contains(KoColorSet::GLOBAL_GROUP_NAME));
    PK_VERIFY(cs->swatchGroupNames().contains("newgroup"));
    PK_VERIFY(cs->isDirty());

    cs->addGroup("newgroup2");

    PK_COMPARE(cs->getGroup(11)->name(), "");
    PK_COMPARE(cs->getGroup(21)->name(), "newgroup");
    PK_COMPARE(cs->getGroup(50)->name(), "newgroup2");

}


void TestKoColorSet::testChangeGroupName()
{
    KoColorSetSP cs = createColorSet();
    cs->addGroup("newgroup");
    PK_VERIFY(cs->swatchGroupNames().size() == 2);
    PK_VERIFY(cs->swatchGroupNames().contains(KoColorSet::GLOBAL_GROUP_NAME));
    PK_VERIFY(cs->swatchGroupNames().contains("newgroup"));
    PK_VERIFY(cs->isDirty());

    cs->changeGroupName("newgroup", "newnewgroup");
    PK_VERIFY(cs->swatchGroupNames().size() == 2);
    PK_VERIFY(cs->swatchGroupNames().contains(KoColorSet::GLOBAL_GROUP_NAME));
    PK_VERIFY(cs->swatchGroupNames().contains("newnewgroup"));
    PK_VERIFY(cs->isDirty());

    cs->undoStack()->undo();
    PK_VERIFY(cs->swatchGroupNames().size() == 2);
    PK_VERIFY(cs->swatchGroupNames().contains(KoColorSet::GLOBAL_GROUP_NAME));
    PK_VERIFY(cs->swatchGroupNames().contains("newgroup"));
    PK_VERIFY(cs->isDirty());

    cs->undoStack()->redo();
    PK_VERIFY(cs->swatchGroupNames().size() == 2);
    PK_VERIFY(cs->swatchGroupNames().contains(KoColorSet::GLOBAL_GROUP_NAME));
    PK_VERIFY(cs->swatchGroupNames().contains("newnewgroup"));
    PK_VERIFY(cs->isDirty());

    cs->undoStack()->undo();
    cs->undoStack()->undo();

    PK_VERIFY(cs->swatchGroupNames().size() == 1);
    PK_VERIFY(cs->swatchGroupNames().contains(KoColorSet::GLOBAL_GROUP_NAME));
    PK_VERIFY(!cs->swatchGroupNames().contains("newgroup"));
    PK_VERIFY(!cs->isDirty());
}


void TestKoColorSet::testMoveGroup()
{
    KoColorSetSP cs = createColorSet();
    cs->addGroup("group1");
    cs->addGroup("group2");
    cs->addGroup("group3");
    cs->addGroup("group4");

    PkStringList original({"", "group1", "group2", "group3", "group4"});
    PK_COMPARE(cs->swatchGroupNames(), original);

    cs->moveGroup("group3", "group2");

    PkStringList move3({"", "group1", "group3", "group2", "group4"});
    PK_COMPARE(cs->swatchGroupNames(), move3);

    cs->undoStack()->undo();
    PK_COMPARE(cs->swatchGroupNames(), original);

    cs->undoStack()->redo();
    PK_COMPARE(cs->swatchGroupNames(), move3);
}

void TestKoColorSet::testRemoveGroup()
{
    KoColorSetSP cs = createColorSet();

    // Discard Colors

    cs->addGroup("newgroup");
    PK_VERIFY(cs->swatchGroupNames().size() == 2);
    PK_VERIFY(cs->swatchGroupNames().contains(KoColorSet::GLOBAL_GROUP_NAME));
    PK_VERIFY(cs->swatchGroupNames().contains("newgroup"));
    PK_VERIFY(cs->isDirty());

    cs->removeGroup("newgroup", false);
    PK_VERIFY(cs->swatchGroupNames().size() == 1);
    PK_VERIFY(cs->swatchGroupNames().contains(KoColorSet::GLOBAL_GROUP_NAME));
    PK_VERIFY(!cs->swatchGroupNames().contains("newgroup"));
    PK_VERIFY(cs->isDirty());

    cs->undoStack()->undo();
    PK_VERIFY(cs->swatchGroupNames().size() == 2);
    PK_VERIFY(cs->swatchGroupNames().contains(KoColorSet::GLOBAL_GROUP_NAME));
    PK_VERIFY(cs->swatchGroupNames().contains("newgroup"));
    PK_VERIFY(cs->isDirty());

    cs->undoStack()->redo();
    PK_VERIFY(cs->swatchGroupNames().size() == 1);
    PK_VERIFY(cs->swatchGroupNames().contains(KoColorSet::GLOBAL_GROUP_NAME));
    PK_VERIFY(!cs->swatchGroupNames().contains("newgroup"));
    PK_VERIFY(cs->isDirty());

    cs->undoStack()->undo();
    cs->undoStack()->undo();

    PK_VERIFY(cs->swatchGroupNames().size() == 1);
    PK_VERIFY(cs->swatchGroupNames().contains(KoColorSet::GLOBAL_GROUP_NAME));
    PK_VERIFY(!cs->swatchGroupNames().contains("newgroup"));
    PK_VERIFY(!cs->isDirty());

    // Keep Colors

    cs->clear();
    cs->addGroup("newgroup");
    cs->addSwatch(KisSwatch(red()), "newgroup", 5, 5);

    PK_COMPARE(cs->swatchGroupNames().size(), 2);
    PK_VERIFY(cs->swatchGroupNames().contains(KoColorSet::GLOBAL_GROUP_NAME));
    PK_VERIFY(cs->swatchGroupNames().contains("newgroup"));
    PK_COMPARE(cs->colorCount(), 1);
    PK_VERIFY(cs->isDirty());

    cs->removeGroup("newgroup", true);
    PK_COMPARE(cs->swatchGroupNames().size(), 1);
    PK_VERIFY(cs->swatchGroupNames().contains(KoColorSet::GLOBAL_GROUP_NAME));
    PK_VERIFY(!cs->swatchGroupNames().contains("newgroup"));
    PK_COMPARE(cs->colorCount(), 1);
    PK_VERIFY(cs->isDirty());

    cs->undoStack()->undo();
    PK_COMPARE(cs->swatchGroupNames().size(), 2);
    PK_VERIFY(cs->swatchGroupNames().contains(KoColorSet::GLOBAL_GROUP_NAME));
    PK_VERIFY(cs->swatchGroupNames().contains("newgroup"));
    PK_COMPARE(cs->colorCount(), 2);
    PK_VERIFY(cs->isDirty());

    cs->undoStack()->redo();
    PK_COMPARE(cs->swatchGroupNames().size(), 1);
    PK_VERIFY(cs->swatchGroupNames().contains(KoColorSet::GLOBAL_GROUP_NAME));
    PK_VERIFY(!cs->swatchGroupNames().contains("newgroup"));
    PK_COMPARE(cs->colorCount(), 1);
    PK_VERIFY(cs->isDirty());

    cs->undoStack()->undo();
    cs->undoStack()->undo();
    cs->undoStack()->undo();

    PK_COMPARE(cs->swatchGroupNames().size(), 1);
    PK_VERIFY(cs->swatchGroupNames().contains(KoColorSet::GLOBAL_GROUP_NAME));
    PK_VERIFY(!cs->swatchGroupNames().contains("newgroup"));
    PK_VERIFY(cs->isDirty());

    PK_COMPARE(cs->colorCount(), 1);

    cs->undoStack()->undo();
    PK_COMPARE(cs->colorCount(), 0);
    PK_VERIFY(!cs->isDirty());
}

void TestKoColorSet::testClear()
{
    KoColorSetSP cs = createColorSet();
    cs->addGroup("newgroup");
    cs->addSwatch(KisSwatch(red()), "newgroup", 5, 5);
    cs->addSwatch(KisSwatch(blue()), KoColorSet::GLOBAL_GROUP_NAME, 1, 1);

    PK_VERIFY(cs->colorCount() == 2);

    cs->clear();
    PK_VERIFY(cs->colorCount() == 0);
    PK_VERIFY(cs->swatchGroupNames().size() == 1);

    cs->undoStack()->undo();
    PK_VERIFY(cs->colorCount() == 2);
    PK_VERIFY(cs->swatchGroupNames().size() == 2);

    cs->undoStack()->redo();
    PK_VERIFY(cs->colorCount() == 0);
    PK_VERIFY(cs->swatchGroupNames().size() == 1);
}

void TestKoColorSet::testGetSwatchFromGroup()
{
    KoColorSetSP cs = createColorSet();
    cs->addGroup("newgroup");
    cs->addSwatch(KisSwatch(red()), "newgroup", 5, 5);
    cs->addSwatch(blue(), KoColorSet::GLOBAL_GROUP_NAME, 1, 1);

    KisSwatch sw = cs->getSwatchFromGroup(5, 5, "newgroup");
    PK_VERIFY(sw.isValid());
    PkColor c = sw.color().toQColor();
    PK_VERIFY(c == PkColor(Qt::red));

    sw = cs->getSwatchFromGroup(1, 1, KoColorSet::GLOBAL_GROUP_NAME);
    PK_VERIFY(sw.isValid());
    c = sw.color().toQColor();
    PK_VERIFY(c == PkColor(Qt::blue));

    sw = cs->getSwatchFromGroup(1, 1, "newgroup");
    PK_VERIFY(!sw.isValid());
}

void TestKoColorSet::testIsGroupNameRow()
{
    KoColorSetSP cs = createColorSet();
    cs->getGlobalGroup()->setRowCount(7);
    cs->addGroup("group1", KisSwatchGroup::DEFAULT_COLUMN_COUNT, 6);
    cs->addGroup("group2", KisSwatchGroup::DEFAULT_COLUMN_COUNT, 5);

    for (int i = 0; i < cs->rowCountWithTitles(); ++i) {
        if (i == 7 || i == 14) {
            PK_VERIFY(cs->isGroupTitleRow(i));
        }
        else {
            PK_VERIFY(!cs->isGroupTitleRow(i));
        }
    }
}

void TestKoColorSet::testStartRowForNamedGroup()
{
    KoColorSetSP cs = createColorSet();
    cs->getGlobalGroup()->setRowCount(7);
    cs->addGroup("group1", KisSwatchGroup::DEFAULT_COLUMN_COUNT, 6);
    cs->addGroup("group2", KisSwatchGroup::DEFAULT_COLUMN_COUNT, 5);
    cs->addGroup("group3", KisSwatchGroup::DEFAULT_COLUMN_COUNT, 4);

    PK_COMPARE(cs->rowCountWithTitles(), 25);


    PK_COMPARE(cs->startRowForGroup(""), 0);
    PK_COMPARE(cs->startRowForGroup("group1"), 7);
    PK_COMPARE(cs->startRowForGroup("group2"), 14);
    PK_COMPARE(cs->startRowForGroup("group3"), 20);
}

void TestKoColorSet::testGetClosestSwatchInfo()
{
    KoColorSetSP cs = createColorSet();
    cs->addSwatch(red(), "", 10, 5);
    PkColor c;
    c.setRgb(255,10,10);
    KoColor  kc(c, KoColorSpaceRegistry::instance()->rgb8());
    KisSwatchGroup::SwatchInfo info = cs->getClosestSwatchInfo(kc);
    PK_COMPARE(info.row, 5);
    PK_COMPARE(info.column, 10);
    PK_COMPARE(info.swatch.color().toQColor(), red().toQColor());
}

void TestKoColorSet::testGetGroup()
{
    KoColorSetSP cs = createColorSet();
    cs->getGlobalGroup()->setRowCount(7);
    cs->addGroup("group1", KisSwatchGroup::DEFAULT_COLUMN_COUNT, 6);
    cs->addGroup("group2", KisSwatchGroup::DEFAULT_COLUMN_COUNT, 5);

    PK_COMPARE(cs->rowCount(), 18);
    PK_COMPARE(cs->rowCountWithTitles(), 20);
    PK_VERIFY(cs->getGroup("group1"));
    PK_VERIFY(cs->getGroup("group2"));

    KisSwatchGroupSP grp = cs->getGroup(0);
    PK_VERIFY(grp);
    PK_VERIFY(grp->name().isEmpty());

    grp = cs->getGroup(7); // titlerow
    PK_VERIFY(grp);
    PK_COMPARE(grp->name(), "group1");

    grp = cs->getGroup(8);
    PK_VERIFY(grp);
    PK_COMPARE(grp->name(), "group1");

    grp = cs->getGroup(13); // last row
    PK_VERIFY(grp);
    PK_COMPARE(grp->name(), "group1");

    grp = cs->getGroup(14); // titlerow
    PK_VERIFY(grp);
    PK_COMPARE(grp->name(), "group2");

    grp = cs->getGroup(17);
    PK_VERIFY(grp);
    PK_COMPARE(grp->name(), "group2");

    grp = cs->getGroup(19);
    PK_VERIFY(grp);
    PK_COMPARE(grp->name(), "group2");

    grp = cs->getGroup(40);
    PK_VERIFY(grp.isNull());

}

void TestKoColorSet::testAllRows()
{
    KoColorSetSP cs = createColorSet();
    cs->getGlobalGroup()->setRowCount(7);
    cs->addGroup("group1", KisSwatchGroup::DEFAULT_COLUMN_COUNT, 6);
    cs->addGroup("group2", KisSwatchGroup::DEFAULT_COLUMN_COUNT, 5);

    PK_COMPARE(cs->rowCountWithTitles(), 20);
    PK_VERIFY(cs->isGroupTitleRow(7));
    PK_VERIFY(cs->isGroupTitleRow(14));

    for (int i = 0; i < 21; ++i) {
        KisSwatchGroupSP grp = cs->getGroup(i);

        if (i < 7) {
            PK_VERIFY(grp->name().isEmpty());
        }
        else if (i > 6 && i < 14) {
            PK_COMPARE(grp->name(), "group1");
        }
        else if (i > 13 && i < 20) {
            PK_COMPARE(grp->name(), "group2");
        }
        else if (i > 19) {
            PK_VERIFY(grp.isNull());
        }
    }

}

void TestKoColorSet::testRowNumberInGroup()
{
    KoColorSetSP cs = createColorSet();
    cs->getGlobalGroup()->setRowCount(7);
    cs->addGroup("group1", KisSwatchGroup::DEFAULT_COLUMN_COUNT, 6);
    cs->addGroup("group2", KisSwatchGroup::DEFAULT_COLUMN_COUNT, 5);
    cs->addGroup("group3", KisSwatchGroup::DEFAULT_COLUMN_COUNT, 4);

    PK_COMPARE(cs->rowCountWithTitles(), 25);

    PkVector<int> rowCountsInGroup {    0, 1, 2, 3, 4, 5, 6,
                                   -1, 0, 1, 2, 3, 4, 5,
                                   -1, 0, 1, 2, 3, 4,
                                   -1, 0, 1, 2, 3};


    PK_COMPARE(cs->rowCountWithTitles(), rowCountsInGroup.size());

    for (int i = 0; i < cs->rowCountWithTitles(); ++i) {
        PK_COMPARE(cs->rowNumberInGroup(i), rowCountsInGroup[i]);
    }
}

void TestKoColorSet::testGetColorGlobal()
{
    KoColorSetSP cs = createColorSet();
    cs->getGlobalGroup()->setRowCount(7);
    cs->setColumnCount(3);
    cs->addGroup("group1", 3, 6);
    cs->addGroup("group2", 3, 5);
    cs->addGroup("group3", 3, 4);

    KisSwatch sw(red(), "red");

    Q_FOREACH(const PkString &groupName, cs->swatchGroupNames()) {
        KisSwatchGroupSP group = cs->getGroup(groupName);
        //qDebug() << group->name();
        for (int row = 0; row < group->rowCount(); row++) {
            for (int col = 0; col < group->columnCount(); col++) {
                PkColor c(row, col, 0);
                sw.setColor(KoColor(c, KoColorSpaceRegistry::instance()->rgb8()));
                //qDebug() << "rgb" << c.red() << c.green() << c.blue() << "row/col" << row << "," << col;
                cs->addSwatch(sw, groupName, col, row);
            }
        }
    }


    //qDebug() << "==================";

    for (int row = 0; row < cs->rowCountWithTitles(); row++) {
        if (row == 7 || row == 14 || row == 20) {
            PK_VERIFY(cs->isGroupTitleRow(row));
            //qDebug() << cs->getGroup(row)->name();
        }

        for (int col = 0; col < cs->columnCount(); col++) {
            if (row != 7 && row != 14 && row != 20) {
                PK_VERIFY(!cs->isGroupTitleRow(row));
                KisSwatch sw2 = cs->getColorGlobal(col, row);
                PkColor c = sw2.color().toQColor();
                //qDebug() << "rgb" << c.red() << c.green() << c.blue() << "row/col" << cs->rowNumberInGroup(row) << "," << col;
                PK_COMPARE(c.red(), cs->rowNumberInGroup(row));
                PK_COMPARE(c.green(), col);
            }
        }
    }

}

KoColorSetSP TestKoColorSet::createColorSet()
{
    KoColorSetSP colorSet(new KoColorSet());
    colorSet->setPaletteType(KoColorSet::KPL);
    colorSet->setName("Dummy");
    colorSet->setFilename("dummy.kpl");
    colorSet->setModified(false);
    colorSet->undoStack()->clear();
    return colorSet;
}

KoColor TestKoColorSet::blue()
{
    PkColor c(Qt::blue);
    KoColor  kc(c, KoColorSpaceRegistry::instance()->rgb8());
    return kc;
}

KoColor TestKoColorSet::red()
{
    PkColor c(Qt::red);
    KoColor  kc(c, KoColorSpaceRegistry::instance()->rgb8());
    return kc;
}


SIMPLE_TEST_MAIN(TestKoColorSet)

