// transition_qt_probe.cpp —— R-37「pk/time DateFormat 别名让位守卫」的 transition TU 探针。
//
// 同一源文件三种编译模式（transition_qt_probe.sh 各编一次、各跑一次）：
//   模式 A（真 Qt 已进 TU，QT_CORE_LIB 与 QNAMESPACE_H 都定义）：真 Qt 头在前
//     （R-35 约定），include <QDateTime>（拉 qnamespace.h 的 DateFormat）+ PkDateTime.h。
//     守卫生效后 PkDateTime.h 的 constexpr ISODate/RFC2822Date/ISODateWithMs 让位
//     （!QT_CORE_LIB 为假、!QNAMESPACE_H 为假 → 整墙让位），不再与真 Qt 枚举重定义。
//   模式 C（QT_CORE_LIB 定义但真 Qt 头未进 TU，QNAMESPACE_H 未定义）：守卫第二析取
//     !QNAMESPACE_H 命中——主树编译行**全局**带 -DQT_CORE_LIB，但本 TU 没有 include
//     真 Qt qnamespace.h，pk 提供别名。static_assert + main 钉住位值。
//   模式 B（无 Qt）：只 include PkDateTime.h，别名照常提供，位值对拍。
//
// 能编过本身就是断言的一半——redeclared as different kind of entity 是硬错误
// （修复前模式 A 实测报 Qt::ISODate redeclared）。
#if defined(QT_CORE_LIB) && defined(QNAMESPACE_H)
#include <QDateTime>
#endif
#include "PkDateTime.h"

#if defined(QT_CORE_LIB) && defined(QNAMESPACE_H)
// 模式 A：真 Qt 模式下 PkDateTime.h 必须让位：Qt::ISODate 保持真 Qt 的 enum DateFormat 值。
static_assert(int(Qt::ISODate) == 1, "real Qt ISODate value");
static_assert(int(Qt::RFC2822Date) == 8, "real Qt RFC2822Date value");
static_assert(int(Qt::ISODateWithMs) == 9, "real Qt ISODateWithMs value");
#elif defined(QT_CORE_LIB) && !defined(QNAMESPACE_H)
// 模式 C：QT_CORE_LIB 定义但真 Qt qnamespace.h 未进 TU——守卫第二析取 !QNAMESPACE_H
// 命中，pk 提供别名。位值对拍（与模式 B 断言一致：同一套 pk 别名）。
static_assert(PkDateTime::DateFormat(Qt::ISODate) == PkDateTime::DateFormat::ISODate,
              "pk ISODate alias");
static_assert(PkDateTime::DateFormat(Qt::RFC2822Date) == PkDateTime::DateFormat::RFC2822Date,
              "pk RFC2822Date alias");
static_assert(PkDateTime::DateFormat(Qt::ISODateWithMs) == PkDateTime::DateFormat::ISODateWithMs,
              "pk ISODateWithMs alias");
#else
// 模式 B：无 Qt——pk 别名照常提供，位值对拍。
static_assert(PkDateTime::DateFormat(Qt::ISODate) == PkDateTime::DateFormat::ISODate,
              "pk ISODate alias");
static_assert(PkDateTime::DateFormat(Qt::RFC2822Date) == PkDateTime::DateFormat::RFC2822Date,
              "pk RFC2822Date alias");
static_assert(PkDateTime::DateFormat(Qt::ISODateWithMs) == PkDateTime::DateFormat::ISODateWithMs,
              "pk ISODateWithMs alias");
#endif

int main()
{
#if !defined(QNAMESPACE_H)
    // 模式 B / 模式 C：QNAMESPACE_H 不在场，pk 提供别名，验证位值。
    if (PkDateTime::DateFormat(Qt::ISODate) != PkDateTime::DateFormat::ISODate) return 1;
    if (PkDateTime::DateFormat(Qt::RFC2822Date) != PkDateTime::DateFormat::RFC2822Date) return 2;
    if (PkDateTime::DateFormat(Qt::ISODateWithMs) != PkDateTime::DateFormat::ISODateWithMs) return 3;
#endif
    return 0;
}
