/*
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <simpletest.h>
#include "TestKisSwatchGroup.h"
#include <KoColorSet.h>

void TestKisSwatchGroup::testAddingOneEntry()
{
    KisSwatch e;
    e.setName("first");
    g.setSwatch(e, 0, 0);
    PK_VERIFY(g.checkSwatchExists(0, 0));
    PK_VERIFY(!g.checkSwatchExists(1, 2));
    PK_VERIFY(!g.checkSwatchExists(10, 5));
    PK_COMPARE(g.getSwatch(0, 0), e);
    PK_COMPARE(g.colorCount(), 1);
    testSwatches[PkPair<int, int>(0, 0)] = e;
}

void TestKisSwatchGroup::testAddingMultipleEntries()
{
    KisSwatch e2;
    e2.setName("second");
    g.setSwatch(e2, 9, 3);
    PK_COMPARE(g.columnCount(), 16);
    PK_VERIFY(g.checkSwatchExists(9, 3));
    PK_VERIFY(!g.checkSwatchExists(1, 2));
    PK_VERIFY(!g.checkSwatchExists(10, 5));
    PK_VERIFY(g.checkSwatchExists(0, 0));
    PK_COMPARE(g.getSwatch(0, 0).name(), PkString("first"));
    KisSwatch e3;
    e3.setName("third");
    g.setSwatch(e3, 4, 12);
    PK_COMPARE(g.colorCount(), 3);
    PK_VERIFY(g.checkSwatchExists(9, 3));
    PK_COMPARE(g.getSwatch(9, 3).name(), PkString("second"));
    testSwatches[PkPair<int, int>(9, 3)] = e2;
    testSwatches[PkPair<int, int>(4, 12)] = e3;
}

void TestKisSwatchGroup::testReplaceEntries()
{
    KisSwatch e4;
    e4.setName("fourth");
    g.setSwatch(e4, 0, 0);
    PK_COMPARE(g.colorCount(), 3);
    PK_VERIFY(g.checkSwatchExists(0, 0));
    PK_COMPARE(g.getSwatch(0, 0).name(), PkString("fourth"));
    testSwatches[PkPair<int, int>(0, 0)] = e4;
}

void TestKisSwatchGroup::testRemoveEntries()
{
    testSwatches.remove(PkPair<int, int>(9, 3));
    PK_VERIFY(g.removeSwatch(9, 3));
    PK_COMPARE(g.colorCount(), testSwatches.size());
    PK_VERIFY(!g.removeSwatch(13, 10));
    PK_VERIFY(!g.checkSwatchExists(9, 3));
}

void TestKisSwatchGroup::testChangeColumnNumber()
{
    g.setColumnCount(20);
    PK_COMPARE(g.columnCount(), 20);
    for (PkPair<int, int> p : testSwatches.keys()) {
        PK_COMPARE(testSwatches[p], g.getSwatch(p.first, p.second));
    }
    g.setColumnCount(10);
    int keptCount = 0;
    for (PkPair<int, int> p : testSwatches.keys()) {
        if (p.first < 10) {
            keptCount++;
            PK_COMPARE(testSwatches[p], g.getSwatch(p.first, p.second));
        }
    }
    PK_COMPARE(keptCount, g.colorCount());
}

void TestKisSwatchGroup::testAddEntry()
{
    KisSwatchGroup g2;
    g2.setColumnCount(3);
    g2.setRowCount(1);

    g2.addSwatch(KisSwatch(KoColor()));
    g2.addSwatch(KisSwatch(KoColor()));
    g2.addSwatch(KisSwatch(KoColor()));

    PK_COMPARE(g2.rowCount(), 1);
    PK_COMPARE(g2.columnCount(), 3);
    PK_COMPARE(g2.colorCount(), 3);

    g2.addSwatch(KisSwatch(KoColor()));

    PK_COMPARE(g2.rowCount(), 2);
    PK_COMPARE(g2.columnCount(), 3);
    PK_COMPARE(g2.colorCount(), 4);

    g2.setRowCount(1);
    PK_COMPARE(g2.rowCount(), 1);
    PK_COMPARE(g2.columnCount(), 3);
    PK_COMPARE(g2.colorCount(), 3);

    g2.addSwatch(KisSwatch(KoColor()));
    g2.addSwatch(KisSwatch(KoColor()));
    g2.addSwatch(KisSwatch(KoColor()));
    g2.addSwatch(KisSwatch(KoColor()));

    PK_COMPARE(g2.rowCount(), 3);
    PK_COMPARE(g2.columnCount(), 3);
    PK_COMPARE(g2.colorCount(), 7);
}

void TestKisSwatchGroup::testName()
{
    KisSwatchGroup g2;
    PK_COMPARE(g2.name(), "");
    g2.setName("test");
    PK_COMPARE(g2.name(), "test");
}

void TestKisSwatchGroup::testColorCount()
{
    KisSwatchGroup g2;
    g2.addSwatch(KisSwatch(red(), "red"));
    PK_COMPARE(g2.colorCount(), 1);
    g2.addSwatch(KisSwatch(blue(), "blue"));
    PK_COMPARE(g2.colorCount(), 2);
    g2.removeSwatch(0, 0);
    PK_COMPARE(g2.colorCount(), 1);
    g2.removeSwatch(1, 0);
    PK_COMPARE(g2.colorCount(), 0);
}

void TestKisSwatchGroup::testInfoList()
{
    KisSwatchGroup g2;
    g2.addSwatch(KisSwatch(red(), "red"));
    PK_COMPARE(g2.infoList().size(), 1);
    KisSwatchGroup::SwatchInfo info = g2.infoList().first();
    PK_COMPARE(info.column, 0);
    PK_COMPARE(info.row, 0);
    PK_COMPARE(info.group, KoColorSet::GLOBAL_GROUP_NAME);
    PK_COMPARE(info.swatch.color(), red());

    g2.addSwatch(KisSwatch(blue(), "blue"));
    PK_COMPARE(g2.infoList().size(), 2);
    info = g2.infoList().at(1);
    PK_COMPARE(info.column, 1);
    PK_COMPARE(info.row, 0);
    PK_COMPARE(info.group, KoColorSet::GLOBAL_GROUP_NAME);
    PK_COMPARE(info.swatch.color(), blue());
}

KoColor TestKisSwatchGroup::blue()
{
    PkColor c(Qt::blue);
    KoColor  kc(c, KoColorSpaceRegistry::instance()->rgb8());
    return kc;
}

KoColor TestKisSwatchGroup::red()
{
    PkColor c(Qt::red);
    KoColor  kc(c, KoColorSpaceRegistry::instance()->rgb8());
    return kc;
}


PK_TEST_GUILESS_MAIN(TestKisSwatchGroup)
