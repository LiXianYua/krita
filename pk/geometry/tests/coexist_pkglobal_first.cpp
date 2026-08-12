// 顺序 C：先直接包库头 PkGlobal.h，之后才撞上 pk/test 那份垫片。
//
// 机制①（检测宏 qFuzzyCompare）在这个顺序下够不着——包 PkGlobal.h 的时候
// pk/test 那份还没进来；机制②（pk/geometry/compat/QtGlobal 预先拉一把）也够不着
// ——这个 TU 根本没经过 compat 垫片。所以本编译行显式定义
// PK_GEOMETRY_WITH_PKTEST_COMPAT（见 CMakeLists.txt 的 set_source_files_properties），
// 让 PkGlobal.h 走机制②的同一条路自己把 pk/test 那份先拉进来。
//
// 没有这个定义时本 TU 会编译失败（qAbs 重定义）——这正是那个开关存在的理由，
// 把它去掉这条测试就变红。
#include "../PkGlobal.h"
#include "../../test/compat/QtGlobal"

#include "coexist.h"

PK_COEXIST_DEFINE(pkCoexistPkGlobalFirst)
