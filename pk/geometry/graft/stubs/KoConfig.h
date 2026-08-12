#pragma once
// 试接垫片 —— 不是 R-03 的交付物。
//
// 真品由 KoConfig.h.cmake 在构建期 configure 出来（源树里只有 `.cmake` 模板），
// 内容是一堆 `#cmakedefine HAVE_XXX`。libs/global/kis_global.h:13 无条件包它，
// 但两个试接目标用到的那部分 kis_global.h（pow2 / kisTrimLeft 一类）一个
// HAVE_* 都不读，所以这里留空即可 —— **不要往里塞猜出来的 HAVE_ 开关**，
// 那会让试接悄悄走进一条与真实构建不同的分支。
//
// 归属：构建系统生成物，不属于任何 Pk 类型线。
