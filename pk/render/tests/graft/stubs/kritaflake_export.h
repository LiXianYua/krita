#pragma once
// R-55 Task 5：替代 CMake 在真实构建里生成的导出头（generate_export_header，
// libs/flake/CMakeLists.txt:574）。
//
// 用途只有一个：让 tests/graft/text_draw_driver.cpp 能把**真实生产头**
// libs/flake/kis_painting_tweaks.h 编进来（该头第 11 行 `#include
// "kritaflake_export.h"`）。这是**编译参数**、不是对调用点的改动——形态照抄
// pk/port/graft/stubs/kritaflake_export.h 与 pk/port/graft/graft_check.sh 的说明。
//
// 只在本 driver 的 -I 面上生效，不进任何产物，也不改变 kritaflake 自身。
#define KRITAFLAKE_EXPORT
