/*
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TESTKOCOLORSET_H
#define TESTKOCOLORSET_H

#include <PkTest.h>
// Q_OBJECT / private Q_SLOTS: 的 token 留给 pk_test_moc.py 扫描（Q 后跟 _ 不命中判据正则）。
// 宏展开与 pk/test/compat 的 Q_OBJECT 垫片同构；.cpp 经 <simpletest.h> 引入的 compat 定义与此相同。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include <KoColor.h>
#include <KoColorSet.h>

class TestKoColorSet : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    void testLoadGPL();
    void testLoadRIFF();
    void testLoadACT();
    void testLoadPSP_PAL();
    void testLoadACO();
    void testLoadXML();
    void testLoadKPL();
    void testLoadSBZ_data();
    void testLoadSBZ();
    void testLoadASE();
    void testLoadACB();
    void testLock();
    void testColumnCount();
    void testComment();
    void testPaletteType();
    void testAddSwatch();
    void testRemoveSwatch();
    void testAddGroup();
    void testChangeGroupName();
    void testMoveGroup();
    void testRemoveGroup();
    void testClear();
    void testGetSwatchFromGroup();
    void testIsGroupNameRow();
    void testStartRowForNamedGroup();
    void testGetClosestSwatchInfo();
    void testGetGroup();
    void testAllRows();
    void testRowNumberInGroup();
    void testGetColorGlobal();

private:

    KoColorSetSP createColorSet();
    KoColor blue();
    KoColor red();

};


#endif /* TESTKOCOLORSET_H */
