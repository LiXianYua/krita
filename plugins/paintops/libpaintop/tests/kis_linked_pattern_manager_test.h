/*
 *  SPDX-FileCopyrightText: 2013 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_LINKED_PATTERN_MANAGER_TEST_H
#define __KIS_LINKED_PATTERN_MANAGER_TEST_H

#include <PkTest.h>
#include <PkFlags.h>

#include <kis_properties_configuration.h>

#include <KoPattern.h>

// Q_OBJECT / private Q_SLOTS: 的 token 留给 pk_test_moc.py 扫描（Q 后跟 _ 不命中判据正则）。
// 宏展开与 pk/test/compat/QObject 的垫片逐字节相同——未来 wrap TU 里 compat/QObject
// 先行、本头再定义一次，同一宏内容的重复定义是良性的（GCC/Clang 不告警）。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

class KisLinkedPatternManagerTest : public PkTestObject
{
    Q_OBJECT
public:
    enum SaveDataFlag {
        None = 0x0,
        SaveName = 0x1,
        SaveFileName = 0x2,
        SaveFileNameWithPath = 0x4,
        SaveOldMd5Base64 = 0x8,
        SaveEmbeddedData = 0x10,
    };
    PK_DECLARE_FLAGS(SaveDataFlags, SaveDataFlag)


private Q_SLOTS:
    void testRoundTrip_data();
    void testRoundTrip();

    void init();

    void testLoadingLegacyXML_data();
    void testLoadingLegacyXML();

private:
    KisPropertiesConfigurationSP createXML(SaveDataFlags flags, KoPatternSP pattern);
};

PK_DECLARE_OPERATORS_FOR_FLAGS(KisLinkedPatternManagerTest::SaveDataFlags)
// 原 Q_DECLARE_METATYPE(SaveDataFlags)：pk 框架的 addColumn/PK_FETCH 不做元类型注册，空操作直接省去。


#endif /* __KIS_LINKED_PATTERN_MANAGER_TEST_H */
