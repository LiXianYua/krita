// transition_qt_probe.cpp —— R-38「pk/test/compat/QtGlobal 让位守卫」的 transition TU 探针。
//
// 同一源文件三种编译模式，由 transition_qt_probe.sh 各编一次、各跑一次：
//   模式 A（真 Qt 已进 TU，QT_CORE_LIB 与 QT_GUI_LIB 都定义）：
//     g++ -fsyntax-only -DQT_CORE_LIB -DQT_GUI_LIB -isystem $QT/include{,/QtCore,/QtGui}
//          -I pk/test/compat transition_qt_probe.cpp
//     真 Qt 头在前（R-35 约定）：<QtCore/qglobal.h> 先落地（QGLOBAL_H 定义、真 qAbs
//     可见），再 include kistest.h（经 compat/QObject 拉入 compat/QtGlobal）。守卫生效
//     后 pk 的 qAbs/宏让位，不再与真 Qt qAbs 重定义。static_assert 确认 qAbs 取值。
//   模式 C（QT_CORE_LIB 定义但真 Qt 头未进 TU，QGLOBAL_H 未定义）：
//     g++ -std=gnu++17 -DQT_CORE_LIB -I pk/test/compat transition_qt_probe.cpp
//     守卫第二析取 !QGLOBAL_H 命中：主树编译行全局带 -DQT_CORE_LIB，但本 TU 没有
//     include 真 Qt qglobal.h，pk 提供 qAbs。static_assert + main 钉住取值。
//   模式 B（无 Qt）：
//     g++ -std=gnu++17 -I pk/test/compat transition_qt_probe.cpp
//     pk 提供 qAbs/宏，static_assert + main 钉住取值。
//
// 能编过本身就是断言的一半——模板重定义是硬错误（修复前模式 A 实测报
// pk/test/compat/QtGlobal:10 redefinition of qAbs）。
#if defined(QT_CORE_LIB) && defined(QT_GUI_LIB)
#include <QtCore/qglobal.h>
#endif
#include "kistest.h"

// qAbs 在所有模式都是 constexpr（pk 与真 Qt 版本都是），static_assert 三种模式通用。
static_assert(qAbs(-5) == 5, "qAbs(-5) == 5");
static_assert(qAbs(3) == 3, "qAbs(3) == 3");

int main()
{
    // 三种模式下同一组取值断言：qAbs 正常、qFuzzyCompare/qFuzzyIsNull 可用且为 true。
    // 模式 A 里它们解析到真 Qt 函数，模式 B/C 里 qFuzzyCompare/qFuzzyIsNull 是
    // #define → pkFuzzyCompare/pkFuzzyIsNull（pk 实现，语义等价）。
    if (qAbs(-5) != 5) return 1;
    if (qAbs(3) != 3) return 2;
    if (!qFuzzyCompare(1.0, 1.0)) return 3;
    if (!qFuzzyIsNull(0.0)) return 4;
    return 0;
}
