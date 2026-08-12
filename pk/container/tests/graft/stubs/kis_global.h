#pragma once

// `libs/global/kis_global.h` 的最小替身。
//
// 真品是个 350 行的杂物间：`<QPoint>`/`<QPointF>`/`<QLineF>`/`<QRect>` 上的
// 几何工具（R-03）、`<QStringConverter>`（R-01/R-13）、`kis_pointer_utils.h`
// 的智能指针工具，以及一批标量工具（`qBound` 一类）。
//
// `kis_fill_interval.h` include 它，但**一项都没用到** —— 它要的是
// 「`kis_assert.h` 被顺带带进来」这条传递关系（真品第 14 行）。本替身因此
// **只做这一件事**：多复刻一项就是多背一份 R-03/R-13 的债，而判据②要压的是容器。
//
// **脚手架，不是交付件。**
#include "kis_assert.h"
