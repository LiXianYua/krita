// 顺序 A：pk/test 的 compat/QtGlobal 先进 TU（-I pk/test/compat 靠前时
// `#include <QtGlobal>` 命中本份，这是 R-11 之后测试 TU 一直存在的形态）。
//
// 走通它靠的是 PkGlobal.h 的机制①：pk/test 那份先落地，宏 qFuzzyCompare 已生效，
// 随后 global 的 compat 把 pk/geometry 拉进来（geometry 的 qAbs/qFuzzy* 因
// PK_GLOBAL_SCALARS_FROM_PKTEST 让位）、再包 PkGlobal.h，后者检测到宏后对
// qAbs/qFuzzy* 让位。

// —— 匿名 namespace 之外（判据见 coexist.h 与 coexist_global_first.cpp 顶部）——
#include "coexist.h"
#include "../../test/PkTestCompare.h"
#include <limits>

namespace {
#include "../../test/compat/QtGlobal"     // pk/test 那份
#include "../compat/QtGlobal"             // 拉 pk/test(已在)/geometry → global PkGlobal.h
} // 匿名 namespace

PK_GLOBAL_COEXIST_DEFINE(pkCoexistTestShimFirst)
