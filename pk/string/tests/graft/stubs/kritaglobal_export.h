#pragma once
// 替代 CMake 在真实构建里生成的导出头（generate_export_header）。
//
// 补的原因：D-005（父提交 f531d07）把 KoProgressProxy.{h,cpp} 从
// libs/widgetutils 搬到了 libs/global，试接目标的编译单元换了目录、也换了
// export 宏（KRITAWIDGETUTILS_EXPORT → KRITAGLOBAL_EXPORT），本目录原有的
// kritawidgetutils_export.h 不再够用，照同一模式（也照 pk/geometry/graft/stubs/
// 里那份同名文件的写法）补一份 kritaglobal_export.h。
#define KRITAGLOBAL_EXPORT
#define KRITAGLOBAL_NO_EXPORT
#define KRITAGLOBAL_DEPRECATED
