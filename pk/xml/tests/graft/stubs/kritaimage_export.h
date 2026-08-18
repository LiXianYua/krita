#pragma once
// graft 自己的构建期胶水，不是对 Krita 源树的改动——同目录
// kritaglobal_export.h 同因，顶掉 CMake 生成的导出宏头。
// 抄自 pk/pointer/graft/stubs/kritaimage_export.h（内容逐字相同）。
#define KRITAIMAGE_EXPORT
