/*
 *  SPDX-FileCopyrightText: 2024 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_PORTING_UTILS_H
#define KIS_PORTING_UTILS_H

#include <QTextStream>

// S-08 恢复：上一版被过度剥离（去掉 namespace、setUtf8OnStream 改收不存在的
// PkTextStream、伪造 screens/widgetScreen/pixelsToPoints 引用不存在的
// PkScreen/PkGuiApplication/PkWidget/PkString::number），7 个真实调用点
// （SvgStyleWriter/SvgWriter/SvgParser/HtmlWriter + libs/image×2 + libs/brush）
// 传的全是**真 QTextStream**。恢复为 namespace + 真 QTextStream 签名；未使用的
// 三个伪造函数删除（原版 Krita 也没有它们，见官方 v6.0.3 同名头）。libs/pigment
// 的纯 Pk TU 不调本函数（PkTextStream 原生 UTF-8，setUtf8OnStream 为空操作）。

namespace KisPortingUtils
{

inline void setUtf8OnStream(QTextStream &stream)
{
    stream.setCodec("UTF-8");
}

}

// libs/canvas/kis_coordinates_converter.cpp 仍用 Qt5 形态的 Q_UNREACHABLE_RETURN。
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#define Q_UNREACHABLE_RETURN(...) Q_UNREACHABLE(); return __VA_ARGS__
#endif

#endif /* KIS_PORTING_UTILS_H */
