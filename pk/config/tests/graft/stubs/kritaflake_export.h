#pragma once
// 替代 CMake 在真实构建里生成的导出头（generate_export_header）——
// libs/flake 的 generate_export_header(kritaflake BASE_NAME kritaflake) 未指定
// EXPORT_MACRO_NAME，宏名走默认规则 KRITAFLAKE_EXPORT（见
// libs/flake/CMakeLists.txt:249）。先例见 pk/port/graft/stubs/kritaflake_export.h。
#define KRITAFLAKE_EXPORT
