// R-65（Task 2+4）· sdk/tests/compat/QtGlobalColorAliases.h
// ---------------------------------------------------------------------------
// 为什么测试面需要它（impact-map.md §3.7.2）：`Qt::<GlobalColor 枚举名>` 在 pk 栈上
// **没有解析目标** —— PkGlobal.h:296 的 GlobalColor 20 个枚举量定义在 `namespace Pk`
// （不是 `namespace Qt`），PkNamespace.h 也只在 `namespace Pk` 里补齐枚举族；
// 于是测试源里的 `Qt::red` 报 `no member named 'red' in namespace 'Qt'`
// （TestKoColorSet 实测）或 `use of undeclared identifier 'Qt'`（TestCompositeOpInversion
// 实测，该 TU 连 `namespace Qt` 都没被拉进来）。
//
// 修法（brief §3.7.2 指定）：**别名**，不是重定义 —— `namespace Qt { using Pk::red; }`
// 里 `using` 是对已存在实体的声明，不产生新实体，与 R-28「删重复」不冲突；
// 也**不**把枚举搬回 `pk/namespace`（会与 pk/global 的 namespace Pk 撞）。
//
// 别名哪些名字：**按实测调用点，一项不多**。本 Task 16 个 target + 全测试面
// （`grep -rhoE 'Qt::[A-Za-z_]+' libs/*/tests benchmarks` 实测计数）里 GlobalColor
// 族只出现这 6 个名字：
//   red 31 · white 30 · black 23 · transparent 17 · blue 15 · green 9
// 其余 14 个枚举量（color0/color1/darkGray/gray/lightGray/darkRed/darkGreen/
// darkBlue/darkCyan/darkMagenta/darkYellow/cyan/magenta/yellow）在测试面**零调用点**，
// 故不别名（PkGlobal.h 的注释也记了 gray/darkGray 只有生产侧调用点）。
//
// 待办的边界：`Qt::GlobalColor` 作为**类型名**的用法（4 处，均在降级 driver 的注释里
// 描述源码，不在本 Task 的编译面）——需要时照样加 `using Pk::GlobalColor;`。
//
// 守卫：只在真 Qt 的 qnamespace.h（QNAMESPACE_H）**不在场**时提供，与 R-35/R-37
// 的让位口径一致。pk 栈的目标（kritatestsdk_pk）拿不到真 Qt 头，故恒为真；一旦某个
// TU 又把真 Qt 头拉进来（本 Task 正在消除这类 include），真 Qt 的 Qt::red 在场，
// 本头让位、不撞。
// ---------------------------------------------------------------------------
#pragma once

#if !defined(QNAMESPACE_H)
#include "PkGlobal.h"

namespace Qt {
using Pk::black;
using Pk::white;
using Pk::red;
using Pk::green;
using Pk::blue;
using Pk::transparent;
} // namespace Qt
#endif
