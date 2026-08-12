// 顺序 A：pk/test 那份 compat/QtGlobal 先进 TU。
// 试接时 -I pk/test/compat 排在 -I pk/geometry/compat 前面就是这个形态：
// <QtGlobal> 命中 pk/test 那份，之后 <QRect> 一类才把 pk/geometry 这边拉进来。
//
// 走通它靠的是 PkGlobal.h 的机制①：检测到宏 qFuzzyCompare 已存在就整段让位，
// 不重复定义 qAbs/qFuzzyCompare/qFuzzyIsNull。
//
// ⚠ 两个 compat/QtGlobal 落在**匿名 namespace** 里，理由见 coexist.h 末尾
//「为什么两个探针 TU 都要匿名 namespace」。不这么做，本 TU 与 coexist_geometry_first
// 编出来的 `qAbs<double>` 会以弱符号发射、被链接器合并成一份，
//「两种 include 顺序各测一遍」当场变成「两个 TU 测同一份实现」。

// —— 匿名 namespace 之外：本 TU 对外的接口，以及它带进来的全部系统头 ——
// PkTestCompare.h 提在这里是必需的：它 #include 了 <cmath>/<sstream>/<string>/
// <type_traits>，落进匿名 namespace 会造出 (anonymous)::std。它本身不是 compat
// 垫片、不定义 qAbs、也不定义那两个 #define，所以提前包它**不改变被测变量**
//（被测变量是两份 compat/QtGlobal 谁先进 TU）。
#include "../../test/PkTestCompare.h"
#include "coexist.h"

namespace {
#include "../../test/compat/QtGlobal"
#include "../compat/QtGlobal"
} // 匿名 namespace：qAbs / qRound / qMin / qBound / pkQtFuzzy* 全部内部链接

// 函数本体在全局作用域（对外可见），里面的 qAbs/qRound/… 经匿名 namespace 的
// 隐式 using-directive 找到上面那份。
PK_COEXIST_DEFINE(pkCoexistTestShimFirst)
