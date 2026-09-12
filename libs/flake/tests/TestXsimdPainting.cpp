/*
 *  SPDX-FileCopyrightText: 2023 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "TestXsimdPainting.h"

// 渲染面已迁 Pk：本 TU 原来用 QPainter 画到 QImage 上，现在改画到 PkImage 上——
// PkPainter + PkImageRasterBackend 是已跑通的现成形制
// （先例：pk/render/oracle/shape_primitive_cases.h:121 → PkImageRasterBackend backend(image);
//          PkPainter painter(backend);）。
// 本 TU 是 qt 桶（-DQT_CORE_LIB、无 pk/*/compat），所以这里补的是真 Pk 头
// （IMPACT §1 的桶纪律：不把 pk/*/compat 拉进 qt 桶 TU）。
#include <PkPainter.h>
#include <PkImageRasterBackend.h>

#include "KoClipMaskApplicatorBase.h"
#include "KoClipMaskPainter.h"

#include "kistest.h"
#include "SvgParserTestingUtils.h"

// PkColor 要进 QTest::addColumn<T> 的数据表，必须先声明成 metatype
// （形制照抄 libs/flake/tests/TestPathShape.cpp:16-17）。
Q_DECLARE_METATYPE(PkColor)

void TestXsimdPainting::testKoClipMaskPainting_data()
{
    QTest::addColumn<PkColor>("colorSource");
    QTest::addColumn<PkColor>("colorMask");
    QTest::addColumn<PkColor>("colorFinal");

    QTest::addRow("visibleWhite") << PkColor(255, 255, 255, 255) << PkColor(255, 255, 255, 255)<< PkColor(255, 255, 255, 255);
    QTest::addRow("completelyMasked") << PkColor(255, 255, 255, 255) << PkColor(0, 0, 0, 255) << PkColor(0, 0, 0, 0);
    QTest::addRow("greyMask") << PkColor(255, 255, 255, 255) << PkColor(128, 128, 128, 255) << PkColor(255, 255, 255, 128);
    QTest::addRow("semiTransparent") << PkColor(255, 255, 255, 255) << PkColor(255, 255, 255, 128) << PkColor(255, 255, 255, 128);
    QTest::addRow("semiCyan") << PkColor(255, 255, 255, 255) << PkColor(128, 255, 255, 128) << PkColor(255, 255, 255, 114);
    QTest::addRow("semiMagenta") << PkColor(255, 255, 255, 255) << PkColor(255, 128, 255, 128) << PkColor(255, 255, 255, 82);
    QTest::addRow("semiYellow") << PkColor(255, 255, 255, 255) << PkColor(255, 255, 128, 128) << PkColor(255, 255, 255, 123);
    QTest::addRow("color1") << PkColor(255, 0, 0, 255) << PkColor(64, 128, 255, 128) << PkColor(255, 0, 0, 62);
    QTest::addRow("color2") << PkColor(0, 255, 0, 255) << PkColor(255, 128, 64, 128) << PkColor(0, 255, 0, 75);
    QTest::addRow("color3") << PkColor(0, 0, 255, 255) << PkColor(128, 64, 255, 128) << PkColor(0, 0, 255, 46);
}

void TestXsimdPainting::testKoClipMaskPainting()
{
    QFETCH(PkColor, colorSource);
    QFETCH(PkColor, colorMask);
    QFETCH(PkColor, colorFinal);

    const PkRect imgRect(0, 0, 5, 3);

    PkImage compareImg = PkImage(imgRect.size(), PkImage::Format_ARGB32);
    // PkImage::fill 只收 uint32_t / Pk::GlobalColor（pk/image/PkImage.h），
    // PkColor 没有到这两者的隐式转换；rgba() 给的就是合入用的 0xaarrggbb 字面量
    // （pk/color/PkColor.h:116），与 Qt 侧 QImage::fill(QColor) 落进 ARGB32 的字节一致。
    compareImg.fill(colorFinal.rgba());
    PkImage img = PkImage(imgRect.size(), PkImage::Format_ARGB32);

    // PkPainter 需要一个后端；PkImage 目的地的后端是 PkImageRasterBackend
    // （libs/flake/PkImageRasterBackend.h:11）。backend 必须先于 painter 构造，
    // 且都活到 clip.renderOnGlobalPainter() 之后（painter 持有 backend 引用）。
    PkImageRasterBackend backend(img);
    PkPainter p(backend);
    p.save();
    // Pk 侧的合成模式枚举在 pk/namespace/PkNamespace.h:424（Pk::CompositionMode_Source = 3，
    // 注释明写「取值与 Qt 对齐」）；Qt::transparent 的对应物是 Pk::GlobalColor::transparent
    // （pk/global/PkGlobal.h:316），经 PkColor(Pk::GlobalColor) 构造（pk/color/PkColor.h:66）。
    p.setCompositionMode(Pk::CompositionMode_Source);
    p.fillRect(imgRect, PkColor(Pk::transparent));
    p.restore();

    // ctor 形参是 (PkPainter*, const PkRectF&)（libs/flake/KoClipMaskPainter.h:21），
    // PkRectF 有 constexpr 的 PkRect 转换构造（pk/geometry/PkRect.h:586）。
    KoClipMaskPainter clip(&p, PkRectF(imgRect));
    clip.shapePainter()->fillRect(imgRect, colorSource);
    clip.maskPainter()->fillRect(imgRect, colorMask);

    clip.renderOnGlobalPainter();

    // compareQImages 的形参是 (QPoint&, const QImage&, const QImage&)（sdk/tests/qimage_test_util.h:178），
    // 所以只在调用点做 PkImage→QImage 的边界转换（libs/flake/PkFlakeBridge.h:533 的 toQImage），
    // 比较强度不变。
    QPoint errpoint;
    if (!TestUtil::compareQImages(errpoint, toQImage(img), toQImage(compareImg))) {
        // PkString 没有 toLatin1()/const char* 转换：PkToUtf8() 给 std::string（pk/string/PkString.h:141），
        // 再取 c_str() 交给 QFAIL 的 const char* 形参。
        QFAIL(PkString("XSimd painting test failed, first different pixel: %1,%2 \n").arg(errpoint.x()).arg(errpoint.y()).PkToUtf8().c_str());
    }
}

KISTEST_MAIN(TestXsimdPainting)
