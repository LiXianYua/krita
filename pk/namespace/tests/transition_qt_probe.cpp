// transition_qt_probe.cpp —— R-37「pk namespace Qt 墙让位守卫」的 transition TU 探针。
//
// 同一源文件三种编译模式，由 transition_qt_probe.sh 各编一次、各跑一次：
//   模式 A（真 Qt 已进 TU，QT_CORE_LIB 与 QT_GUI_LIB 都定义）：
//     g++ -fsyntax-only -DQT_CORE_LIB -DQT_GUI_LIB -isystem $QT/include{,/QtCore,/QtGui}
//          -I pk/color transition_qt_probe.cpp
//     真 Qt 头在前（R-35 约定）。PkColor.h 经 PkNamespace.h 拉入 pk 的 namespace Qt
//     枚举族——守卫生效后应让位（!QT_CORE_LIB 为假、!QNAMESPACE_H 为假 → 整墙让位），
//     不再与真 Qt qnamespace.h 重定义。static_assert 确认 namespace Qt 保持真 Qt 的
//     （pk 的枚举没漏出来）。
//   模式 C（QT_CORE_LIB 定义但真 Qt 头未进 TU，QNAMESPACE_H 未定义）：
//     g++ -std=gnu++17 -DQT_CORE_LIB -I pk/color transition_qt_probe.cpp
//     守卫第二析取 !QNAMESPACE_H 命中：主树编译行**全局**带 -DQT_CORE_LIB，但本 TU
//     没有 include 真 Qt qnamespace.h，pk 提供枚举族。static_assert + main 钉住
//     位值——这正是 R-35 同型守卫踩过的坑（S-04 KisColorimetryUtils.cpp）对应的
//     独立 TU。
//   模式 B（无 Qt）：
//     g++ -std=gnu++17 -I pk/color transition_qt_probe.cpp
//     pk 提供枚举族，static_assert + main 钉住位值。
//
// 能编过本身就是断言的一半——枚举重定义 / typedef 冲突 / operator| 歧义是硬错误
// （修复前模式 A 实测报 KeyboardModifier 重定义）。另一半（位值对齐）由 pk/namespace
// 单测与 oracle 钉住，这里只证明「让位之后三种模式都成立」。
#include <type_traits>
// include 闸门用 QT_GUI_LIB（sh 模式 A 的唯一标识）而非 QNAMESPACE_H：QNAMESPACE_H 是
// include 真 Qt qnamespace.h **之后**才定义，拿它当闸门会循环依赖（Critical 1 修复）。
#if defined(QT_CORE_LIB) && defined(QT_GUI_LIB)
#include <QColor>
#include <QString>
#include <QFlags>
#endif
#include "PkColor.h"

#if defined(QT_CORE_LIB) && defined(QNAMESPACE_H)
// 模式 A：真 Qt 模式下 PkNamespace.h 必须让位：namespace Qt 保持真 Qt 的（KeyboardModifiers
// 是真 Qt QFlags 的 typedef，不是 PkFlags 的）。
static_assert(std::is_same<Qt::KeyboardModifiers, QFlags<Qt::KeyboardModifier>>::value,
              "namespace Qt must be real Qt's under QT_CORE_LIB");
// 真 Qt 位值对拍（让位后来自真 Qt qnamespace.h）。
static_assert(int(Qt::KeyboardModifier::ShiftModifier) == 0x02000000, "real Qt ShiftModifier");
static_assert(int(Qt::MouseButton::LeftButton) == 0x00000001, "real Qt LeftButton");
#elif defined(QT_CORE_LIB) && !defined(QNAMESPACE_H)
// 模式 C：QT_CORE_LIB 定义但真 Qt qnamespace.h 未进 TU——守卫第二析取 !QNAMESPACE_H
// 命中，pk 提供枚举族。位值对拍（与模式 B 断言一致：同一套 pk 枚举）。
static_assert(int(Qt::KeyboardModifier::ShiftModifier) == 0x02000000, "pk ShiftModifier");
static_assert(int(Qt::MouseButton::LeftButton) == 0x00000001, "pk LeftButton");
static_assert(int(Qt::MouseButton::ExtraButton24) == 0x04000000, "pk MaxMouseButton=ExtraButton24");
static_assert(int(Qt::Key::Key_Escape) == 0x01000000, "pk Key_Escape");
#else
// 模式 B：无 Qt——pk 提供枚举族，位值对拍（与 pk/namespace 单测一致）。
static_assert(int(Qt::KeyboardModifier::ShiftModifier) == 0x02000000, "pk ShiftModifier");
static_assert(int(Qt::MouseButton::LeftButton) == 0x00000001, "pk LeftButton");
static_assert(int(Qt::MouseButton::ExtraButton24) == 0x04000000, "pk MaxMouseButton=ExtraButton24");
static_assert(int(Qt::Key::Key_Escape) == 0x01000000, "pk Key_Escape");
#endif

int main()
{
#if !defined(QNAMESPACE_H)
    // 模式 B / 模式 C：QNAMESPACE_H 不在场，pk 提供枚举，验证位值。
    if (int(Qt::KeyboardModifier::ShiftModifier) != 0x02000000) return 1;
    if (int(Qt::MouseButton::LeftButton) != 0x00000001) return 2;
    if (int(Qt::MouseButton::MaxMouseButton) != 0x04000000) return 3;
#endif
    return 0;
}
