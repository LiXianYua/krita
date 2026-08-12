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
//    元素类型取 int 与 std::string 两种：一个平凡可拷贝、一个带分配器与非平凡
//    拷贝/移动，足以压出 make_shared<C>(*d) 这条深拷贝路径上的要求。
//
// 这些实例化**不构成 API 承诺**——PkArrayData 是模板，调用点用什么 C 都行；
// 这里只是让常见组合有一次编译期体检。

template class PkArrayData<std::vector<int>>;
template class PkArrayData<std::vector<std::string>>;
template class PkArrayData<std::map<int, int>>;
template class PkArrayData<std::map<std::string, std::string>>;
template class PkArrayData<std::unordered_map<int, int>>;
template class PkArrayData<std::unordered_map<std::string, std::string>>;
template class PkArrayData<std::unordered_set<int>>;
template class PkArrayData<std::unordered_set<std::string>>;
template class PkArrayData<std::set<int>>;
template class PkArrayData<std::vector<std::pair<int, int>>>;
