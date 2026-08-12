#pragma once
// 顶掉 CMake 生成的导出宏头（真实构建里由 GenerateExportHeader 产出，本
// worktree 没有跑那套 CMake 生成流程）。graft 只编成静态 .o，不做动态库导出
// 边界，宏展开成空即可——kis_debug.h / kis_assert.h 里 21 处
// `extern const KRITAGLOBAL_EXPORT QLoggingCategory &...()` 与若干函数声明
// 都要用到它。
#define KRITAGLOBAL_EXPORT
