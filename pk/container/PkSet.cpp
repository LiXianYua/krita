#include "PkSet.h"

// 与 PkMap.cpp 同因：显式实例化让「每个方法都编得过」在库构建时暴露，而不是
// 等到某个调用点第一次用到那个方法才发现。
//
// PkSet 没有基类，一行就够。元素类型只用内建类型：PkSet 的元素必须有 qHash
// 重载，而 PkHashFunctions.h 只给内建类型。带自定义 qHash 的类型与 PkString
// 那两条链路在 test_pkset.cpp 里压。
template class PkSet<int>;
template class PkSet<unsigned long long>;
