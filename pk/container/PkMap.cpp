#include "PkMap.h"

#include <map>
#include <string>

// PkMap<K,V> 是模板，实现全在头里。本 TU 的存在只有一个理由：对两种代表性的
// 值类型做**显式实例化**，把「每一个方法都真的编得过」这件事挪到库构建时暴露，
// 而不是等到某个调用点第一次用到那个方法才发现。
//
// 模板的成员函数只有被调用时才实例化——没有这几行，一个从未被单测碰过的方法
// 里就算写了编译错误也照样"全绿"。
//
// int 与 std::string 两种值类型：一个平凡可拷贝、一个带分配器与非平凡拷贝/移动，
// 足以压出 take/insert/erase/keys/values 这些路径上对元素类型的全部要求。
// 两者都有 operator==，keys(value)/key(value) 才实例化得出来。
//
// 基类要单独写一行：显式实例化派生类**不**连带实例化基类的成员函数，
// 而共同 API 全在基类里——漏了这行，等于这批实例化只体检了
// lowerBound/upperBound 两个方法。
template class PkAssocContainer<int, int, std::map<int, int>, PkMap<int, int>>;
template class PkMap<int, int>;

template class PkAssocContainer<int, std::string, std::map<int, std::string>,
                                PkMap<int, std::string>>;
template class PkMap<int, std::string>;

template class PkAssocContainer<std::string, int, std::map<std::string, int>,
                                PkMap<std::string, int>>;
template class PkMap<std::string, int>;
