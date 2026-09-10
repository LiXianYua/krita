/*
 *  SPDX-FileCopyrightText: 2015 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef FLAKE_DEBUG_H_
#define FLAKE_DEBUG_H_

#include <QDebug>
#include <QLoggingCategory>
#include <kritaflake_export.h>

#if defined(QT_CORE_LIB)
// qt 桶：本桶自己的真 Qt 分类对象，头内 inline —— 跨桶符号不存在。
//
// 返回类型不进 mangled name：两桶共用同一个 FLAKE_LOG() 时，qt 侧拿到的是
// native 桶定义的 PkLoggingCategory（={const char*; PkLogLevel;}，没有真 Qt 的
// 内部 d 指针），而 qCDebug 会拿它当真 QLoggingCategory 读字段 —— 真 UB。
// 让每个桶各有自己的分类对象就地把这条缝封死；qt 侧按同一个名字
// "krita.lib.flake" 走真 Qt 的按名过滤，行为不变。
inline const QLoggingCategory &flakeHostLogCategory()
{
    static const QLoggingCategory category("krita.lib.flake", QtInfoMsg);
    return category;
}
#define FLAKE_LOG() flakeHostLogCategory()
#else
extern const KRITAFLAKE_EXPORT QLoggingCategory &FLAKE_LOG();
#endif

#define debugFlake qCDebug(FLAKE_LOG)
#define warnFlake qCWarning(FLAKE_LOG)
#define errorFlake qCCritical(FLAKE_LOG)

#endif
