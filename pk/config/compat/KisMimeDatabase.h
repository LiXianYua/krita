#pragma once
#include "../PkMimeDatabase.h"
// 兼容垫片：真实消费者 `#include <KisMimeDatabase.h>` 在零改动前提下解析到
// PkMimeDatabase。KisMimeDatabase 不是 Qt 类型，是 Krita 自己的类——用同一套
// #define 手法的先例是 R-11 对 KISTEST_MAIN 的处理（Krita 自有宏，同样靠垫片
// 转发）。真实类没有前置声明依赖问题，但为保持整条线的手法一致，仍用
// #define 不用 using。
#define KisMimeDatabase PkMimeDatabase
