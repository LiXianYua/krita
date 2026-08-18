#pragma once
// 试接垫片 —— 不是 pk/xml（R-07）的交付物。与 pk/geometry/graft/stubs/KoConfig.h
// 处置一致：真品由 KoConfig.h.cmake 在构建期 configure 出来（源树里只有 `.cmake`
// 模板），内容是一堆 `#cmakedefine HAVE_XXX`。libs/global/kis_global.h 无条件
// 包它，但候选 A（kis_dom_utils.cpp）用到的那部分 kis_global.h 一个 HAVE_* 都
// 不读，留空即可——不要往里塞猜出来的 HAVE_ 开关，那会让试接悄悄走进一条与
// 真实构建不同的分支。
//
// 归属：构建系统生成物，不属于任何 Pk 类型线。
