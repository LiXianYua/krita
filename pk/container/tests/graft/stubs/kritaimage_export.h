#pragma once

// `libs/image/floodfill/*` 的导出宏。真品由 CMake 的 generate_export_header
// 生成，内容是可见性属性 + DLL 导入导出。**试接是静态链接单个可执行文件**，
// 一个符号都不需要导出 —— 展开成空即可。
//
// 这是**脚手架**，不是 R-02 的交付件：真正剥离时它由构建系统照旧生成。
#define KRITAIMAGE_EXPORT
#define KRITAIMAGE_NO_EXPORT
