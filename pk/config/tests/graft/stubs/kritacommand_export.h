#pragma once
// 替代 CMake 在真实构建里生成的导出头（generate_export_header）。
// graft 只编成静态 .o 直接链接，不做动态库导出边界，宏展开成空即可——
// KisCumulativeUndoData.h 用它修饰结构体与自由函数。
// 与 pk/port/graft/stubs/kritacommand_export.h 内容一致（同一族脚手架，
// 各 graft 任务各自持有一份自包含副本，不跨任务互相 -I）。
#define KRITACOMMAND_EXPORT
