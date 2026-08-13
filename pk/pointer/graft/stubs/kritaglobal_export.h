#pragma once
// graft 自己的构建期胶水，不是对 Krita 源树的改动——顶掉 CMake 生成的导出宏头
// （真实构建里由 GenerateExportHeader 产出，本 worktree 没有跑那套 CMake 生成
// 流程）。graft 只编成静态 .o，不做动态库导出边界，宏展开成空即可。
// 抄自 pk/log/tests/graft/stubs/kritaglobal_export.h（R-08，内容逐字相同）。
#define KRITAGLOBAL_EXPORT
