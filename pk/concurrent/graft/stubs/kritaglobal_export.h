#pragma once
// 顶掉 CMake 生成的导出宏头（真实构建里由 GenerateExportHeader 产出，本
// worktree 没有跑那套 CMake 生成流程）。graft 只编成静态 .o，不做动态库导出
// 边界，宏展开成空即可——kis_debug.h / kis_assert.h 里 21 处
// `extern const KRITAGLOBAL_EXPORT QLoggingCategory &...()` 与若干函数声明
// 都要用到它。
//
// 内容与 pk/log/tests/graft/stubs/kritaglobal_export.h 逐字一致（同一族
// 脚手架，压的是同一段 kis_debug.h 头文件链，各 graft 任务各自持有一份
// 自包含副本，不跨任务互相 -I）。
#define KRITAGLOBAL_EXPORT
