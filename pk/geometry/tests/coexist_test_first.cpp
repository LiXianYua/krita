// 顺序 A：pk/test 那份 compat/QtGlobal 先进 TU。
// 试接时 -I pk/test/compat 排在 -I pk/geometry/compat 前面就是这个形态：
// <QtGlobal> 命中 pk/test 那份，之后 <QRect> 一类才把 pk/geometry 这边拉进来。
//
// 走通它靠的是 PkGlobal.h 的机制①：检测到宏 qFuzzyCompare 已存在就整段让位，
// 不重复定义 qAbs/qFuzzyCompare/qFuzzyIsNull。
#include "../../test/compat/QtGlobal"
#include "../compat/QtGlobal"

#include "coexist.h"

PK_COEXIST_DEFINE(pkCoexistTestShimFirst)
