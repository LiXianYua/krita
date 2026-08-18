/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISPAINTKERNELSTAGEMARKER_H
#define KISPAINTKERNELSTAGEMARKER_H

#include "paintkernel_export.h"

/**
 * D-06 阶段成果标记：libpaintkernel 聚合壳。
 *
 * 把 D 线当前保留的全部内核 target 链成一个 SHARED 库，用来证明「UI 已剥离、
 * 内核已经能作为一个整体存在」——这是 D 线的阶段性里程碑标记，**不是**阶段二的
 * 最终交付物。这个壳仍然直接/传递链接 Qt（保留 target 尚未经过 S 线剥离 Qt），
 * 也没有防腐层与 C ABI 导出裁剪；真正对外交付的 `libpaintkernel.so` 由
 * `paint_texture_plugin` 在阶段二产出，用 version script 只导出防腐层的 C ABI
 * 符号（见 docs/迁移执行计划.md §2.2）。
 */
PAINTKERNEL_EXPORT const char *kisPaintKernelStageMarker();

#endif // KISPAINTKERNELSTAGEMARKER_H
