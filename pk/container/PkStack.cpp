#include "PkStack.h"

#include <string>

// 与 PkVector.cpp 同因：显式实例化两种代表性元素类型，让「每个方法都编得过」
// 在库构建时暴露，而不是等调用点第一次用到才发现。模板的成员函数只有被调用时
// 才实例化——没有这两行，一个从未被单测碰过的方法里就算写了编译错误也照样"全绿"。
//
// **这里只点 PkStack 自己这一层**：继承链上的 PkVector<int> / PkVector<string>
// 与它们的 PkArrayContainer 基类已经由 PkVector.cpp 显式实例化过了，同一个
// 程序里对同一个特化做两次显式实例化定义是 ill-formed（[temp.explicit]/12），
// 重复写在这里会变成一条静默的标准违规。PkQueue.cpp 与 PkList.cpp 同理。
template class PkStack<int>;
template class PkStack<std::string>;
