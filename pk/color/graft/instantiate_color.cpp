// instantiate_color.cpp —— 复刻真实调用点形状的 driver（QColor → PkColor 垫片）。
//
// 与 pk/flags/graft/instantiate.cpp 同理：源文件按真 Krita 调用点的写法写
// （`#include <QColor>`、QColor 裸类型、Qt::GlobalColor、成员调用），在垫片下编译
// 链接并核对取值。编译行给 `-I pk/color/compat`，`#include <QColor>` 解析到
// compat/QColor（→ PkColor）；PkColor.h 在全局作用域展开（无真 Qt 冲突），
// PkNamespace 的 `namespace Qt` 即全局 Qt，`Qt::red` 等照常可写。
#include <QColor>
#include <cstdio>
#include <cstring>
#include <string>

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::fprintf(stderr, "  instantiate FAIL: %s\n", msg); g_fail = 1; } } while (0)

int main()
{
    // 构造形态：GlobalColor / int / 命名色 / hex
    QColor red(Qt::red);
    QColor r2(255, 0, 0);
    QColor named("steelblue");
    QColor fromhex("#80ff0000");
    CHECK(red.rgba() == 0xffff0000u, "red.rgba");
    CHECK(r2 == red, "int-ctor == GlobalColor");
    CHECK(named.isValid(), "named steelblue");
    CHECK(named.rgb() == 0xff4682b4u, "steelblue rgb");
    CHECK(fromhex.rgba() == 0x80ff0000u, "#80ff0000 rgba");

    // 分量 getter / setter
    CHECK(red.red() == 255 && red.green() == 0 && red.blue() == 0, "red channels");
    red.setGreen(128);
    CHECK(red.green() == 128, "setGreen");
    CHECK(red.alphaF() == 1.0, "alphaF");

    // HSV / HSL
    QColor h = QColor::fromHsv(120, 255, 255);
    CHECK(h.hue() == 120 && h.saturation() == 255 && h.value() == 255, "fromHsv 120");
    QColor gray(128, 128, 128);
    CHECK(gray.hue() == -1 && gray.saturation() == 0, "gray achromatic");
    QColor hsl = QColor::fromHsl(0, 255, 128);
    CHECK(hsl.hslHue() == 0 && hsl.hslSaturation() == 255 && hsl.lightness() == 128, "fromHsl");

    // 浮点构造
    QColor f = QColor::fromRgbF(1.0, 0.5, 0.25);
    CHECK(f.red() == 255 && f.green() == 128 && f.blue() == 64, "fromRgbF");

    // name
    QColor nm(255, 0, 0);
    CHECK(std::strcmp(nm.name().PkToUtf8().c_str(), "#ff0000") == 0, "name hexrgb");
    QColor nma(128, 255, 0, 0);
    CHECK(std::strcmp(nma.name(QColor::HexArgb).PkToUtf8().c_str(), "#0080ff00") == 0, "name hexargb");

    // lighter / darker
    CHECK(QColor(255, 0, 0).lighter(150).rgba() == 0xffff7f7fu, "lighter 150");
    CHECK(QColor(0, 0, 255).darker(200).rgba() == 0xff00007fu, "darker 200");

    // setNamedColor 边缘
    QColor snc;
    snc.setNamedColor("  red  ");
    CHECK(snc.rgba() == 0xffff0000u, "setNamedColor trimmed");

    return g_fail;
}
