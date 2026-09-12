/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_paint_device_debug_utils.cpp 阻塞登记（S-06 Task 8 批次B；
//       S-11 2026-09-11 复核并订正原因）
//
// 本文件不进任何构建（libs/image/CMakeLists.txt 的 [GAP 排除] 名单），
// 保留 Qt 原样。**当前唯一的阻塞原因**：
//   * pk/image 没有任何写盘/编码 API —— `PkImage.h` 无 save()，
//     `pkimageio` 只有解码器（PkImageFileDecoder / PkPngReader /
//     Bmp|Pnm|Xbm|Xpm|Ico handler）。而
//     `device->convertToQImage(0, saveRect)` 现在返回的就是 **PkImage**，
//     末尾的 `.save(filename)` 没有任何 Pk 等价物可落。
//
// 已解除的阻塞（S-11 实测订正，原登记里那条已过时）：
//   * `Qt 的 string("%1_%2.png").arg(i).arg(suffix)` 依赖的 `PkString::arg()`
//     **已交付**（R-13：PkString.h 有 arg(PkString) 三个重载 + arg(int) /
//     arg(int,int) / arg(double)）。本条不再阻塞。
//
// 本文件是调试工具（把 device dump 成 png 文件），非核心路径。
// 关闭条件：pk/image 交付写盘/编码 API 后，剥类型并编入构建。
// 归属：依赖满足后接手的任务（pk/image 的编码能力面属 R 线）。

#include "kis_paint_device_debug_utils.h"

#include <QRect>
#include <QImage>
#include <PkRect.h>

#include "kis_paint_device.h"


void kis_debug_save_device_incremental(KisPaintDeviceSP device,
                                       int i,
                                       const QRect &rc,
                                       const QString &suffix, const QString &prefix)
{
    QString filename = QString("%1_%2.png").arg(i).arg(suffix);

    if (!prefix.isEmpty()) {
        filename = QString("%1_%2.png").arg(prefix).arg(filename);
    }

    QRect saveRect(rc);

    if (saveRect.isEmpty()) {
        saveRect = device->exactBounds();
    }

    qDebug() << "Dumping:" << filename;
    device->convertToQImage(0, saveRect).save(filename);
}

void kis_debug_save_device_incremental(KisPaintDeviceSP device,
                                       int i,
                                       const PkRect &rc,
                                       const QString &suffix, const QString &prefix)
{
    kis_debug_save_device_incremental(device, i,
                                      QRect(rc.x(), rc.y(), rc.width(), rc.height()),
                                      suffix, prefix);
}

void kis_debug_save_device_incremental(KisPaintDeviceSP device,
                                       int i,
                                       const PkRect &rc,
                                       const char *suffix, const char *prefix)
{
    kis_debug_save_device_incremental(device, i, rc,
                                      QString::fromUtf8(suffix),
                                      QString::fromUtf8(prefix));
}
