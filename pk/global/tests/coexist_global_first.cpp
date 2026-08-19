// 顺序 C：pk/global 的 compat/QtGlobal 先进 TU（-I pk/global/compat 靠前时
// `#include <QtGlobal>` 命中本份）。
//
// ⚠ 为什么不直接 `#include "../PkGlobal.h"` 当头一份：pk/test 的 compat/QtGlobal
// 无条件定义 qAbs（函数模板，无宏痕迹、无 guard），pk/geometry 的 PkGlobal.h 已
// 折叠成薄转发头（不再有独立的 qRound/qMin/qMax/qBound/pkQtFuzzy* 实现，标量全
// 从 pk/global 来）—— 但 pk/test 那份 qAbs 依然无 guard。若让 global 的
// PkGlobal.h 真的第一个落地，后面 pk/test 那份再定义 qAbs 就是 "redefinition of
// qAbs" 硬错误。所以「入口」必须是 compat 垫片：它先把 pk/test 与 pk/geometry
// 拉进来（让它们先落地），再包 PkGlobal.h，后者检测到两份已落地后整段让位。这
// 与 R-03 的 coexist_geometry_first 同构（那里 geometry 的 compat 也是先拉 pk/test）。
//
// 本 TU 等价于真实调用点里 `-I pk/global/compat` 最靠前的情况：<QtGlobal> 命中
// global 的垫片，pk/test 与 pk/geometry 的垫片随后经它被拉进同一个 TU，三份共存。

// —— 匿名 namespace 之外：本 TU 对外的接口，以及它带进来的全部系统头 ——
// PkTestCompare.h 提在这里是必需的：它 #include 了 <cmath>/<sstream>/<string>/
// <type_traits>，落进匿名 namespace 会造出 (anonymous)::std。它本身不是 compat
// 垫片、不定义 qAbs、也不定义那两个 #define，所以提前包它**不改变被测变量**
//（被测变量是三份 compat/QtGlobal 谁先进 TU）。
// <limits> 同理：PkGlobal.h 的 qInf/qQNaN 用 std::numeric_limits，必须在
// namespace 外先落地。
#include "coexist.h"
#include "../../test/PkTestCompare.h"
#include <limits>

namespace {
#include "../compat/QtGlobal"              // 拉 pk/test → pk/geometry → global PkGlobal.h
#include "../../test/compat/QtGlobal"      // 已在 → #pragma once 空转
#include "../../geometry/compat/QtGlobal"  // 已在 → 空转
} // 匿名 namespace：qAbs / qRound / qMin / pkQtFuzzy* 全部内部链接

// 函数本体在全局作用域（对外可见），里面的 qAbs/qRound/… 经匿名 namespace 的
// 隐式 using-directive 找到上面那份。
PK_GLOBAL_COEXIST_DEFINE(pkCoexistGlobalShimFirst)
