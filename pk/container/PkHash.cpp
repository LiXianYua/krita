#include "PkHash.h"

#include <string>
#include <unordered_map>

// 与 PkMap.cpp 同因：显式实例化两种代表性值类型，让「每个方法都编得过」在库
// 构建时暴露。基类单独一行的理由见 PkMap.cpp 的注释。
//
// key 类型只用 int：PkHash 的 key 必须有 qHash 重载，而 PkHashFunctions.h 只给
// 内建类型（std::string 没有 qHash，Qt 也不给 std::string 给）。PkString 作为
// key 的那条链路在 test_pkhash.cpp 里压——那里才有 pkstring 可链。
template class PkAssocContainer<int, int, std::unordered_map<int, int, PkHasher<int>>,
                                PkHash<int, int>>;
template class PkHash<int, int>;

template class PkAssocContainer<int, std::string,
                                std::unordered_map<int, std::string, PkHasher<int>>,
                                PkHash<int, std::string>>;
template class PkHash<int, std::string>;
