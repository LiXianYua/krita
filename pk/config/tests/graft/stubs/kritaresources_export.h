#pragma once
// 替代 CMake 在真实构建里生成的导出头（generate_export_header）——
// libs/resources 的 generate_export_header(kritaresources BASE_NAME
// kritaresources) 未指定 EXPORT_MACRO_NAME，宏名走默认规则
// KRITARESOURCES_EXPORT（见 libs/resources/CMakeLists.txt:66）。先例见
// pk/port/graft/stubs/kritaresources_export.h。
#define KRITARESOURCES_EXPORT
