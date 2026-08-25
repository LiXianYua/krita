// transition_qt_probe.cpp —— R-37「pk namespace Qt 墙让位守卫」的 transition TU 探针。
//
// 同一源文件两种编译模式，由 transition_qt_probe.sh 各编一次、各跑一次：
//   模式 A（真 Qt 已进 TU，QT_CORE_LIB 定义）：
//     g++ -fsyntax-only -DQT_CORE_LIB -DQT_GUI_LIB -isystem $QT/include{,/QtCore,/QtGui}
//          -I pk/color transition_qt_probe.cpp
//     真 Qt 头在前（R-35 约定）。PkColor.h 经 PkNamespace.h 拉入 pk 的 namespace Qt
//     枚举族——守卫生效后应让位，不再与真 Qt qnamespace.h 重定义。static_assert
//     确认 namespace Qt 保持真 Qt 的（pk 的枚举没漏出来）。
//   模式 B（无 Qt）：
//     g++ -fsyntax-only -I pk/color transition_qt_probe.cpp
//     pk 提供枚举族，static_assert + main 钉住位值。
//
// 能编过本身就是断言的一半——枚举重定义 / typedef 冲突 / operator| 歧义是硬错误
// （修复前模式 A 实测报 KeyboardModifier 重定义）。另一半（位值对齐）由 pk/namespace
// 单测与 oracle 钉住，这里只证明「让位之后两种模式都成立」。
#if defined(QT_CORE_LIB)
#include <QColor>
#include <QString>
#include <QFlags>
#endif
#include "PkColor.h"

#if defined(QT_CORE_LIB)
// 真 Qt 模式下 PkNamespace.h 必须让位：namespace Qt 保持真 Qt 的（KeyboardModifiers
// 是真 Qt QFlags 的 typedef，不是 PkFlags 的）。
static_assert(std::is_same<Qt::KeyboardModifiers, QFlags<Qt::KeyboardModifier>>::value,
              "namespace Qt must be real Qt's under QT_CORE_LIB");
// 真 Qt 位值对拍（让位后来自真 Qt qnamespace.h）。
static_assert(int(Qt::KeyboardModifier::ShiftModifier) == 0x02000000, "real Qt ShiftModifier");
static_assert(int(Qt::MouseButton::LeftButton) == 0x00000001, "real Qt LeftButton");
#else
// 无 Qt：pk 提供枚举族，位值对拍（与 pk/namespace 单测一致）。
static_assert(int(Qt::KeyboardModifier::ShiftModifier) == 0x02000000, "pk ShiftModifier");
static_assert(int(Qt::MouseButton::LeftButton) == 0x00000001, "pk LeftButton");
static_assert(int(Qt::MouseButton::ExtraButton24) == 0x04000000, "pk MaxMouseButton=ExtraButton24");
static_assert(int(Qt::Key::Key_Escape) == 0x01000000, "pk Key_Escape");
#endif

int main()
{
#if !defined(QT_CORE_LIB)
    if (int(Qt::KeyboardModifier::ShiftModifier) != 0x02000000) return 1;
    if (int(Qt::MouseButton::LeftButton) != 0x00000001) return 2;
    if (int(Qt::MouseButton::MaxMouseButton) != 0x04000000) return 3;
#endif
    return 0;
}
