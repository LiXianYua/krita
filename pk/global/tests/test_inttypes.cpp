#include "cases/inttypes_case.h"
#include "../PkGlobal.h"

#include <type_traits>

#include "pk_binder_inttypes_case.inc"

// ---------------------------------------------------------------------------
// 整数别名照抄真 Qt 5.15.7 qglobal.h:232-257。尺寸与符号性在编译期钉死
//（static_assert），运行期再镜像一遍给 PK_VERIFY 一条条核对，两路都有。
// 平台口径：Linux x86-64（LP64），long == 8 字节、long long == 8 字节。
// ---------------------------------------------------------------------------

// 尺寸：qint8==1 … quint64==8。
static_assert(sizeof(qint8) == 1, "qint8 尺寸");
static_assert(sizeof(quint8) == 1, "quint8 尺寸");
static_assert(sizeof(qint16) == 2, "qint16 尺寸");
static_assert(sizeof(quint16) == 2, "quint16 尺寸");
static_assert(sizeof(qint32) == 4, "qint32 尺寸");
static_assert(sizeof(quint32) == 4, "quint32 尺寸");
static_assert(sizeof(qint64) == 8, "qint64 尺寸");
static_assert(sizeof(quint64) == 8, "quint64 尺寸");

// 符号性：名字带 q/无 q 前缀区分 signed/unsigned。
static_assert(std::is_signed<qint8>::value, "qint8 signed");
static_assert(std::is_unsigned<quint8>::value, "quint8 unsigned");
static_assert(std::is_signed<qint16>::value, "qint16 signed");
static_assert(std::is_unsigned<quint16>::value, "quint16 unsigned");
static_assert(std::is_signed<qint32>::value, "qint32 signed");
static_assert(std::is_unsigned<quint32>::value, "quint32 unsigned");
static_assert(std::is_signed<qint64>::value, "qint64 signed");
static_assert(std::is_unsigned<quint64>::value, "quint64 unsigned");

// qreal 就是 double（桌面/Android 均无 QT_COORD_TYPE）。
static_assert(std::is_same<qreal, double>::value, "qreal == double");
static_assert(sizeof(qreal) == 8, "qreal 尺寸");

// 对称别名：qlonglong==qint64、qulonglong==quint64。
static_assert(std::is_same<qlonglong, qint64>::value, "qlonglong == qint64");
static_assert(std::is_same<qulonglong, quint64>::value, "qulonglong == quint64");

// C 兼容别名：uchar/ushort/uint/ulong（qglobal.h:273-276）。尺寸按 LP64。
static_assert(std::is_unsigned<uchar>::value, "uchar unsigned");
static_assert(std::is_unsigned<ushort>::value, "ushort unsigned");
static_assert(std::is_unsigned<uint>::value, "uint unsigned");
static_assert(std::is_unsigned<ulong>::value, "ulong unsigned");
static_assert(sizeof(uchar) == 1, "uchar 尺寸");
static_assert(sizeof(ushort) == 2, "ushort 尺寸");
static_assert(sizeof(uint) == 4, "uint 尺寸");
static_assert(sizeof(ulong) == 8, "ulong 尺寸（LP64）");

void PkInttypesCase::sizesMatchStdInt()
{
    PK_COMPARE(sizeof(qint8), 1u);
    PK_COMPARE(sizeof(quint8), 1u);
    PK_COMPARE(sizeof(qint16), 2u);
    PK_COMPARE(sizeof(quint16), 2u);
    PK_COMPARE(sizeof(qint32), 4u);
    PK_COMPARE(sizeof(quint32), 4u);
    PK_COMPARE(sizeof(qint64), 8u);
    PK_COMPARE(sizeof(quint64), 8u);
}

void PkInttypesCase::signednessMatchesNames()
{
    PK_VERIFY(std::is_signed<qint8>::value);
    PK_VERIFY(std::is_unsigned<quint8>::value);
    PK_VERIFY(std::is_signed<qint16>::value);
    PK_VERIFY(std::is_unsigned<quint16>::value);
    PK_VERIFY(std::is_signed<qint32>::value);
    PK_VERIFY(std::is_unsigned<quint32>::value);
    PK_VERIFY(std::is_signed<qint64>::value);
    PK_VERIFY(std::is_unsigned<quint64>::value);
}

void PkInttypesCase::qrealIsDouble()
{
    PK_VERIFY((std::is_same<qreal, double>::value));
    PK_COMPARE(sizeof(qreal), 8u);
}

void PkInttypesCase::symmetricAliasesMatch()
{
    PK_VERIFY((std::is_same<qlonglong, qint64>::value));
    PK_VERIFY((std::is_same<qulonglong, quint64>::value));
    // qlonglong 上的算术与 qint64 同类型：能一起出现在一个表达式里。
    qlonglong a = -5;
    qint64 b = 3;
    PK_COMPARE(a + b, static_cast<qint64>(-2));
}

void PkInttypesCase::cCompatibleAliasesMatch()
{
    PK_VERIFY(std::is_unsigned<uchar>::value);
    PK_VERIFY(std::is_unsigned<ushort>::value);
    PK_VERIFY(std::is_unsigned<uint>::value);
    PK_VERIFY(std::is_unsigned<ulong>::value);
    PK_COMPARE(sizeof(uchar), 1u);
    PK_COMPARE(sizeof(ushort), 2u);
    PK_COMPARE(sizeof(uint), 4u);
    PK_COMPARE(sizeof(ulong), 8u);
}

int run_inttypes_tests()
{
    PkInttypesCase tc;
    const char *argv[] = {"test_pkglobal"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
