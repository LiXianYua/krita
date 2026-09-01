/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_paint_device_debug_utils.cpp 阻塞登记（S-06 Task 8 批次B）
//
// 本文件不进薄壳，保留 Qt 原样。阻塞原因：
//   * PkImage 缺 save()（kis_debug_save_device_incremental 中
//     device->convertToQImage(...).save(filename) 无法替换成 Pk 等价物）
//   * QString("%1_%2.png").arg(i).arg(suffix) 依赖 PkString::arg()（未实现）
// 本文件是调试工具（把 device dump 成 png 文件），非核心路径。
// 关闭条件：PkImage::save() 与 PkString::arg() 交付后，剥类型并编入薄壳。

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
