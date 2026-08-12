#include "PkList.h"

#include <string>

// 与 PkVector.cpp 同因：显式实例化两种代表性元素类型，让「每个方法都编得过」
// 在库构建时暴露，而不是等调用点第一次用到才发现。基类单独一行的理由见
// PkVector.cpp 的注释。
template class PkArrayContainer<int, PkList<int>>;
template class PkList<int>;

template class PkArrayContainer<std::string, PkList<std::string>>;
template class PkList<std::string>;
