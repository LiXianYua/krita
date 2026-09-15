// R-87：PkColor 的 `std::ostream` 插入运算符 —— PK_COMPARE 判红时给「实际值」。
//
// **为什么在这里（类型所有者）而不是 pk/test**：pk/test/PkTestCompare.h:77-82 的
// 设计就是「SFINAE 检测 ostream 可插入性，不需要为每个类型写重载」，所以值文本的
// **生产者**落类型所有者、`pk/test` 一个字节都不用改（本任务硬约束）。
//
// **文本来源** = 真 Qt 5.15.7 的 `QDebug operator<<(QDebug, const QColor&)`
// （qcolor.cpp「QColor stream functions」；声明在 QtGui/qcolor.h:56-57，
//  `#ifndef QT_NO_DEBUG_STREAM`）。分支与文本逐条照抄，只把类型名换成 PkColor。
//
// ⚠ **「超出 Qt」的登记（Q2-a 主会话裁定）**：真 Qt 的 `QCOMPARE(QColor, QColor)`
//   判红时打的是 `<unprintable>` —— `QTest::toString<QColor>` 返回 `nullptr`
//   （探针① 实测，探针输出里那一段**根本没有 Actual 行**）。Qt 里这段文本只活在
//   QDebug 通道。**所以 Qt 没有对应物，这段文本是本仓的选择**，不是对齐。
//   选它的唯一理由：它是 Qt 对 QColor **唯一存在的**文本（QDebug 通道那一份）。
//   收益是可复现的：R-75 曾把 `<unprintable>` 误判成「剥 Qt 后的行为差异」、
//   R-78 因它白立一次案。登记见 pk/color/README.md 偏离 9。
//
// ⚠ 与 pk/geometry 的 `PkRect` 运算符同理：那个 TU 与 `PkGeometryDebug.cpp` 零耦合
//   （免得把 `PkLogEmit` 带进链接）；这里同样独立成 TU，不碰任何 PkDebug 通道。
#include "PkColor.h"

#include <ostream>

std::ostream &operator<<(std::ostream &os, const PkColor &c)
{
    if (!c.isValid())
        return os << "PkColor(Invalid)";

    if (c.spec() == PkColor::Rgb)
        return os << "PkColor(ARGB " << c.alphaF() << ", " << c.redF() << ", "
                  << c.greenF() << ", " << c.blueF() << ')';

    if (c.spec() == PkColor::ExtendedRgb)
        return os << "PkColor(Ext. ARGB " << c.alphaF() << ", " << c.redF() << ", "
                  << c.greenF() << ", " << c.blueF() << ')';

    if (c.spec() == PkColor::Hsv) {
        // Qt 那条用的是 hueF()/saturationF()/valueF()；PkColor 没有这三个单分量
        // 浮点取值器（范围表实测用量 0，未交付），改用等价的组合取值器 getHsvF()
        // —— 真 Qt 的 hueF() 就是 getHsvF 的第一个出参（qcolor.cpp 同源）。
        qreal h = 0.0, s = 0.0, v = 0.0;
        c.getHsvF(&h, &s, &v);
        return os << "PkColor(AHSV " << c.alphaF() << ", " << h << ", " << s << ", " << v << ')';
    }

    if (c.spec() == PkColor::Hsl) {
        // 同上：Qt 用 hslHueF()/hslSaturationF()/lightnessF()，PkColor 只有
        // getHslF() 与 lightnessF()。
        qreal h = 0.0, s = 0.0, l = 0.0;
        c.getHslF(&h, &s, &l);
        return os << "PkColor(AHSL " << c.alphaF() << ", " << h << ", " << s << ", " << l << ')';
    }

    // Cmyk —— **唯一复刻不出 Qt 原文的一支**：Qt 那条打 `QColor(ACMYK …)`，用的是
    // cyanF()/magentaF()/yellowF()/blackF() 四个取值器，而 PkColor 一个都没有
    // （范围表实测用量 0，未交付，见 pk/color/README.md 偏离 2）。**不假装**是
    // ACMYK：改打它转成 Rgb 之后的取值，并在文本里显式标注这不是 CMYK 分量。
    // 判据只要「有实际值、能区分两个色」，这一支不参与判据（判据用 Rgb 色）。
    const PkColor rgb = c.toRgb();
    return os << "PkColor(Cmyk->ARGB " << rgb.alphaF() << ", " << rgb.redF() << ", "
              << rgb.greenF() << ", " << rgb.blueF() << ')';
}
