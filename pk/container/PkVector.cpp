#include "PkVector.h"

#include <string>

// PkVector<T> 是模板，实现全在头里。本 TU 的存在只有一个理由：对两种代表性的
// 元素类型做**显式实例化**，把「每一个方法都真的编得过」这件事挪到库构建时
// 暴露，而不是等到某个调用点第一次用到那个方法才发现。
//
// 模板的成员函数只有被调用时才实例化——没有这两行，一个从未被单测碰过的方法
// 里就算写了编译错误也照样"全绿"。
//
// int 与 std::string 两种：一个平凡可拷贝、一个带分配器与非平凡拷贝/移动，
// 足以压出 resize/fill/insert/erase 这些路径上对元素类型的全部要求。
//
// 基类要单独写一行：显式实例化派生类**不**连带实例化基类的成员函数，
// 而共同 API 全在基类里——漏了这行，等于这批实例化只体检了 PkVector 专有的
// 四个方法。
template class PkArrayContainer<int, PkVector<int>>;
template class PkVector<int>;

template class PkArrayContainer<std::string, PkVector<std::string>>;
template class PkVector<std::string>;
