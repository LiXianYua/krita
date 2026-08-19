// 顺序 B：pk/geometry 的 compat/QtGlobal 先进 TU（-I pk/geometry/compat 靠前）。
//
// 这是 R-18 之前唯一存在的入口：Krita 的测试 TU 里 `#include <QtGlobal>` 一直由
// pk/geometry 的垫片接（它自己先拉 pk/test）。R-18 的 global 垫片叠在它上面——
// 本 TU 验证「geometry 的垫片先进、global 的垫片随后跟进」这一真实升级路径。
//
// 走通它靠的是 PkGlobal.h 的共存机制：geometry 的 compat 先落地（把 pk/test 一起
// 拉进来，它的 `#include "../PkGlobal.h"` 经 R-18 折叠后的转发头命中 pk/global），
// 随后 global 的 compat 里 `#include "../../geometry/compat/QtGlobal"` 因 #pragma
// once 空转、再包自己的 PkGlobal.h —— 同一份已落地，检测到
// PK_GLOBAL_SCALARS_FROM_PKTEST 后让位。

// —— 匿名 namespace 之外（判据见 coexist.h 与 coexist_global_first.cpp 顶部）——
#include "coexist.h"
#include "../../test/PkTestCompare.h"
#include <limits>

namespace {
#include "../../geometry/compat/QtGlobal"  // 拉 pk/test → geometry PkGlobal.h
#include "../compat/QtGlobal"              // 拉 pk/test(已在)/geometry(已在) → global PkGlobal.h
} // 匿名 namespace

PK_GLOBAL_COEXIST_DEFINE(pkCoexistGeometryShimFirst)
