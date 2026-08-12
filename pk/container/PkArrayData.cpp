#include "PkArrayData.h"

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// PkArrayData<C> 是模板，实现全在头里。本 TU 的存在有两个理由：
//
// 1) STATIC 库需要至少一个源文件（否则 CMake 拒绝 add_library）；
// 2) 对 Task 2–6 计划要用的每一种内层容器做一次**显式实例化**，把「地基对这些
//    C 都编得过」这件事挪到库构建时暴露，而不是等各容器 Task 才发现。
//
// 每种内层容器都给两个元素/值类型：int（平凡可拷贝）与 std::string（带分配器、
// 非平凡拷贝与移动），足以压出 make_shared<C>(*d) 那条深拷贝路径上的要求。
//
// 这些实例化**不构成 API 承诺**——PkArrayData 是模板，调用点用什么 C 都行；
// 这里只是让 R-02 范围内的组合有一次编译期体检。
//
// **不补 std::multimap / std::unordered_multimap / std::list**：对应的
// QMultiMap / QMultiHash / QLinkedList 已定为不在 R-02 范围内（归 S 线按点改写）。

// 给 pk/string 复用用：PkString 的 buffer 是 std::vector<char16_t>，
// 所以它拿 PkArrayData<std::vector<char16_t>> 就能直接替掉自己那份 PkStringData。
// 放这条显式实例化，是把「字符串与容器共用一份 COW 地基」从一句声明变成
// **编译期验证过的事实**（线级 spec「已裁决的岔路」）。
// R-13 迁移 pk/string 时照此形态接；R-02 不碰 pk/string。
template class PkArrayData<std::vector<char16_t>>;                     // PkString 的 buffer

template class PkArrayData<std::vector<int>>;                          // PkVector<T> / PkList<T> / PkStack / PkQueue
template class PkArrayData<std::vector<std::string>>;                  // 同上（PkStringList 的内层形态）
template class PkArrayData<std::map<int, int>>;                        // PkMap<K,V>
template class PkArrayData<std::map<std::string, std::string>>;        // 同上
template class PkArrayData<std::unordered_map<int, int>>;              // PkHash<K,V>
template class PkArrayData<std::unordered_map<std::string, std::string>>;  // 同上
template class PkArrayData<std::unordered_set<int>>;                   // PkSet<T>
template class PkArrayData<std::unordered_set<std::string>>;           // 同上
template class PkArrayData<std::set<int>>;                             // PkMap 的 key 视图 / 有序集合场景
template class PkArrayData<std::set<std::string>>;                     // 同上
template class PkArrayData<std::vector<std::pair<int, int>>>;          // PkVector<PkPair<A,B>>
