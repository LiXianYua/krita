// R-18 折叠：pk/global 是标量地基的唯一实现；本头只做转发，不再自用定义。
// 消费者（PkPoint.h/PkRect.h/PkSize.h/PkTransform.h/compat/ 与 R-03 的 tests/oracle/graft）
// 全部 #include "PkGlobal.h"，经此转发命中 pk/global 的标量与 Qt 枚举。
// 注（R-27 恢复，2026-08-19）：R-18 折叠曾因 R-21/R-22 rebase 被覆盖丢失，此处
// 按 R-18 plan「折叠」段的形态重建；R-21/R-22 加的 SizeMode/FillRule/GlobalColor/
// TransformationMode 四个枚举随折叠一并迁移进 pk/global（见 pk/global/PkGlobal.h）。
#pragma once
#include "../global/PkGlobal.h"   // 本头在 pk/geometry/，../global/ → pk/global/
