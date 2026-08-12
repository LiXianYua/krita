#pragma once
// 试接垫片 —— 不是 R-03 的交付物。
//
// 真品是 CMake 的 generate_export_header() 在构建目录里生成的，源树里根本没有
// 这个文件（`find . -name kritaglobal_export.h` 在 fork 里 0 命中）。试接是独立
// shell runner、不跑 CMake，所以要自己补一份。
//
// 归属：S 线把 target 搬成静态库之后，导出宏整类都会消失（内核不再是共享库）。
// 在那之前它属于「构建系统生成物」，不属于任何 Pk 类型线。
#define KRITAGLOBAL_EXPORT
#define KRITAGLOBAL_NO_EXPORT
#define KRITAGLOBAL_DEPRECATED
