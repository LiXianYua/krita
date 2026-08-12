#pragma once
// 替代 CMake 在真实构建里生成的导出头（generate_export_header）。
// libs/brush 的 EXPORT_MACRO_NAME 是 BRUSH_EXPORT，不是 KRITABRUSH_EXPORT
// （见 libs/brush/CMakeLists.txt: generate_export_header(... BASE_NAME
// kritabrush EXPORT_MACRO_NAME BRUSH_EXPORT)）——文件名跟 BASE_NAME 走，
// 宏名跟 EXPORT_MACRO_NAME 走，两者不同名。
#define BRUSH_EXPORT
