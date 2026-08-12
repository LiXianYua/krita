// 顺序 B：pk/geometry 那份 compat/QtGlobal 先进 TU（-I pk/geometry/compat 靠前）。
//
// 走通它靠的是 PkGlobal.h 的机制②：pk/geometry/compat/QtGlobal 自己先把 pk/test
// 那份 #include 进来，它是 #pragma once，所以下面那行显式 include 变成空转，
// 不会第二次定义 qAbs。
//
// ⚠ 匿名 namespace 的理由与 coexist_test_first.cpp 完全相同（见 coexist.h 末尾）。

#include "../../test/PkTestCompare.h"
#include "coexist.h"

namespace {
#include "../compat/QtGlobal"
#include "../../test/compat/QtGlobal"
} // 匿名 namespace：见 coexist_test_first.cpp 顶部注释

PK_COEXIST_DEFINE(pkCoexistGeometryShimFirst)
