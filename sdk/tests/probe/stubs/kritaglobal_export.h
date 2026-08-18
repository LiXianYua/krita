#pragma once
// 试接垫片 —— 仅供本探针（sdk/tests/probe/）使用，不是任何 Pk 类型线的交付物。
//
// 真品是 CMake 的 generate_export_header() 在构建目录里生成的，源码树里没有
// 这个文件（探针是独立薄壳工程，不跑 libs/global 的 CMakeLists，不会触发那条
// generate_export_header 规则）。libs/global/kis_assert.h 第 11 行无条件
// `#include <kritaglobal_export.h>`——这条 #include 指令本身要求文件在 include
// 路径上能被找到，光靠 smoke.cpp 里预先 `#define KRITAGLOBAL_EXPORT` 解决不了
// "文件不存在"这一步，所以在这里补一份最小垫片，把它的目录加进
// target_include_directories。
#define KRITAGLOBAL_EXPORT
