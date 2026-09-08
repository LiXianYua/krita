/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_svg_brush_test.h"

#include <cstring>
#include <cstdint>
#include <string>

#include <KisGlobalResourcesInterface.h>
#include <PkMemoryStream.h>
#include <simpletest.h>

#include "../kis_svg_brush.h"

void KisSvgBrushTest::testUtf8NameAndSvgRoundTrip()
{
    static const char svg[] =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 2 1\">"
        "<rect width=\"1\" height=\"1\" fill=\"black\"/>"
        "</svg>";

    const char filename[] = u8"/tmp/画笔.测试.svg";
    KisSvgBrush brush(PkString::PkFromUtf8(filename, sizeof(filename) - 1));

    PkMemoryStream input;
    QVERIFY(input.open(PkStream::ReadWrite));
    QCOMPARE(input.write(svg, sizeof(svg) - 1), PkStream::pk_int64(sizeof(svg) - 1));
    QVERIFY(input.seek(0));
    QVERIFY(brush.loadFromDevice(&input, KisGlobalResourcesInterface::instance()));

    QVERIFY(brush.valid());
    QCOMPARE(brush.brushType(), MASK);
    QCOMPARE(brush.name(), PkString::PkFromUtf8(u8"画笔.测试", sizeof(u8"画笔.测试") - 1));
    QCOMPARE(brush.brushTipImage().format(), PkImage::Format_Indexed8);
    QCOMPARE(brush.brushTipImage().size(), PkSize(1000, 500));
    QCOMPARE(brush.brushTipImage().pixelIndex(250, 250), 0);
    QCOMPARE(brush.brushTipImage().pixelIndex(750, 250), 255);
    QCOMPARE(brush.resourceType().first, PkString(ResourceType::Brushes));
    QCOMPARE(brush.resourceType().second, PkString(ResourceSubType::SvgBrushes));

    PkMemoryStream output;
    QVERIFY(output.open(PkStream::ReadWrite));
    QVERIFY(brush.saveToDevice(&output));
    QCOMPARE(output.size(), PkStream::pk_int64(sizeof(svg) - 1));
    QCOMPARE(std::memcmp(output.data(), svg, sizeof(svg) - 1), 0);
}

void KisSvgBrushTest::testTransformOpacityAndColorMatchQt515()
{
    static const char svg[] =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 4 2\">"
        "<g transform=\"translate(1 0)\" opacity=\"0.5\">"
        "<rect width=\"2\" height=\"2\" fill=\"#ff0000\"/>"
        "</g></svg>";
    PkMemoryStream input;
    QVERIFY(input.open(PkStream::ReadWrite));
    QCOMPARE(input.write(svg, sizeof(svg) - 1), PkStream::pk_int64(sizeof(svg) - 1));
    QVERIFY(input.seek(0));

    KisSvgBrush brush("/tmp/oracle.svg");
    QVERIFY(brush.loadFromDevice(&input, KisGlobalResourcesInterface::instance()));
    QCOMPARE(brush.brushTipImage().size(), PkSize(1000, 500));
    QCOMPARE(brush.brushTipImage().pixelIndex(100, 250), 255);
    QCOMPARE(brush.brushTipImage().pixelIndex(300, 250), 128);
    QCOMPARE(brush.brushTipImage().pixelIndex(700, 250), 128);

    std::uint64_t hash = 1469598103934665603ull;
    for (int y = 0; y < brush.brushTipImage().height(); ++y) {
        for (int x = 0; x < brush.brushTipImage().width(); ++x) {
            hash ^= static_cast<unsigned>(brush.brushTipImage().pixelIndex(x, y));
            hash *= 1099511628211ull;
        }
    }
    QCOMPARE(hash, std::uint64_t(15120048081377001843ull));
}

SIMPLE_TEST_MAIN(KisSvgBrushTest)
