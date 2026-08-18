#pragma once
// 顶掉 CMake 生成的导出宏头（真实构建里由 GenerateExportHeader 产出，本
// worktree 没有跑那套 CMake 生成流程）。graft 只编成静态 .o，不做动态库导出
// 边界，宏展开成空即可——libs/global/kis_debug.h 与 libs/global/kis_assert.h
// 里的 extern/函数声明都要用到它（KisCumulativeUndoData.cpp 经
// `#include <kis_debug.h>` 传递引入）。
// 与 pk/log/tests/graft/stubs/kritaglobal_export.h 内容一致（同一族脚手架，
// 各 graft 任务各自持有一份自包含副本，不跨任务互相 -I）。
#define KRITAGLOBAL_EXPORT
