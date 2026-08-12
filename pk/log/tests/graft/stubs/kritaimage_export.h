#pragma once
// 顶掉 CMake 生成的导出宏头（真实构建里由 GenerateExportHeader 产出，本
// worktree 没有跑那套 CMake 生成流程）。与同目录 kritaglobal_export.h 同因：
// graft 只编成静态 .o，不做动态库导出边界，宏展开成空即可——
// KisRandomGenerator2D.h 里 `class KRITAIMAGE_EXPORT KisRandomGenerator2D` 要用。
//
// 这是 graft 自己的构建期胶水，不是对 Krita 源树的改动。
#define KRITAIMAGE_EXPORT
