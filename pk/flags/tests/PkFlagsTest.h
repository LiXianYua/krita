// PkFlagsTest.h —— PkFlags 单测。校验值来自 R-20 plan「问 0」的真 Qt 探针。
#pragma once
#include <QObject>
#include "PkFlags.h"

enum class EC { None = 0, A = 0x1, B = 0x2, C = 0x4, D = 0x8, AB = 0x3, ABC = 0x7 };
PK_DECLARE_FLAGS(ECFlags, EC)
PK_DECLARE_OPERATORS_FOR_FLAGS(ECFlags)

enum class EU : unsigned int { U0 = 0, U1 = 1u, U2 = 2u, UHI = 0x80000000u };
PK_DECLARE_FLAGS(EUFlags, EU)
PK_DECLARE_OPERATORS_FOR_FLAGS(EUFlags)

class PkFlagsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testIntStorage();
    void testTestFlag();
    void testSetFlag();
    void testOperators();
    void testFreeOperatorOr();
    void testComparison();
    void testUnsignedEnum();
};