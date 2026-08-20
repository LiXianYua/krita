#include "cases/color_case.h"

// PkTestBinder<PkColorCase> 由 pk_test_moc.py 生成（CMake 的 pk_test_generate
// 触发构建），像 Qt moc 输出一样直接 #include 进本 TU——显式特化必须在
// qExec<PkColorCase> 实例化前对本 TU 可见。先例：
// pk/namespace/tests/test_namespace.cpp。
#include "pk_binder_color_case.inc"

// ---------------------------------------------------------------------------
// 所有期望值都取自**真 Qt 5.15.7** qcolor.cpp / qcolor.h 的源码对照与探针实测。
// 相对 brief 示例的修正（Qt::green 位值、darkYellow 有效性、operator== 是否比较
// alpha）在 README「偏离登记」逐条声明，这里用真 Qt 的取值。
// ---------------------------------------------------------------------------

// ── 构造 / 状态 ───────────────────────────────────────────

void PkColorCase::defaultCtor()
{
    PkColor c;
    PK_VERIFY(!c.isValid());
    PK_COMPARE(int(c.spec()), int(PkColor::Invalid));
    // 默认构造 alpha=65535：无效色 rgba() 的 alpha 仍 255（Qt 3 兼容遗留）。
    PK_COMPARE(c.alpha(), 255);
    PK_COMPARE(c.red(), 0);
    PK_COMPARE(c.green(), 0);
    PK_COMPARE(c.blue(), 0);
    PK_COMPARE(c.rgba(), 0xff000000u);
    // rgb() 对 Invalid 走 qRgb(0,0,0) → alpha 恒 FF：真 Qt 实测 QColor().rgb()==0xff000000。
    PK_COMPARE(c.rgb(), 0xff000000u);
    PK_COMPARE(c.toArgb32(), 0xff000000u);
}

void PkColorCase::rgbCtor()
{
    PkColor c(255, 0, 0);
    PK_VERIFY(c.isValid());
    PK_COMPARE(int(c.spec()), int(PkColor::Rgb));
    PK_COMPARE(c.rgba(), 0xffff0000u);
    PK_COMPARE(c.rgb(), 0xffff0000u);   // rgb() 恒 alpha=FF
    PK_COMPARE(c.red(), 255);
    PK_COMPARE(c.green(), 0);
    PK_COMPARE(c.blue(), 0);
    PK_COMPARE(c.alpha(), 255);
    PK_COMPARE(c.redF(), 1.0);
    PK_COMPARE(c.greenF(), 0.0);
    PK_COMPARE(c.blueF(), 0.0);
    PK_COMPARE(c.alphaF(), 1.0);
}

void PkColorCase::rgbCtorOutOfRange()
{
    // 越界构造：置无效，且**所有分量含 alpha 都清 0**（与默认构造 alpha=255 不同）。
    PkColor c(300, 0, 0);
    PK_VERIFY(!c.isValid());
    PK_COMPARE(c.alpha(), 0);
    PK_COMPARE(c.rgba(), 0x00000000u);
    PkColor c2(-1, 255, 255);
    PK_VERIFY(!c2.isValid());
    PK_COMPARE(c2.rgba(), 0x00000000u);
    PkColor c3(0, 0, 0, 256);
    PK_VERIFY(!c3.isValid());
}

void PkColorCase::rgbCtorAlpha()
{
    PkColor c(255, 0, 0, 128);
    PK_COMPARE(c.rgba(), 0x80ff0000u);
    PK_COMPARE(c.alpha(), 128);
    PK_COMPARE(c.rgb(), 0xffff0000u);   // rgb() 恒 alpha=FF
}

void PkColorCase::copyAssign()
{
    PkColor a(255, 0, 0);
    PkColor b = a;
    PK_VERIFY(a == b);
    PK_COMPARE(b.rgba(), 0xffff0000u);
    PkColor d;
    d = Qt::green;
    PK_COMPARE(d.rgba(), 0xff00ff00u);
    PkColor e;
    e = PkColor(0, 0, 255);
    PK_COMPARE(e.rgba(), 0xff0000ffu);
}

void PkColorCase::wireStateIsLossless()
{
    const PkColor::WireState cmyk{
        PkColor::Cmyk, {0x3232u, 0x0a0au, 0x1414u, 0x1e1eu, 0x2828u}
    };
    const PkColor cmykColor = PkColor::fromWireState(cmyk);
    PK_COMPARE(int(cmykColor.spec()), int(PkColor::Cmyk));
    PK_VERIFY(cmykColor.wireState() == cmyk);

    const PkColor::WireState extended{
        PkColor::ExtendedRgb, {0x3a00u, 0x3d00u, 0xb400u, 0x3800u, 0x0000u}
    };
    const PkColor extendedColor = PkColor::fromWireState(extended);
    PK_COMPARE(int(extendedColor.spec()), int(PkColor::ExtendedRgb));
    PK_VERIFY(extendedColor.wireState() == extended);

    const PkColor::WireState invalid{
        PkColor::Invalid, {0xffffu, 0x0000u, 0x0000u, 0x0000u, 0x0000u}
    };
    PK_VERIFY(PkColor().wireState() == invalid);
    PK_VERIFY(PkColor::fromWireState(invalid).wireState() == invalid);
}

void PkColorCase::setRgbFClearsExtendedWirePad()
{
    PkColor color = PkColor::fromWireState(
        {PkColor::ExtendedRgb, {0x3a00u, 0x3d00u, 0xb400u, 0x3800u, 0xabcdu}});

    color.setRgbF(0.25, 0.5, 0.75, 1.0);

    const PkColor::WireState state = color.wireState();
    PK_COMPARE(int(state.spec), int(PkColor::ExtendedRgb));
    PK_COMPARE(state.channels[4], quint16(0));
}

// ── 数据驱动族试验 ────────────────────────────────────────

void PkColorCase::globalColor_data()
{
    PkTest::addColumn<int>("gc");
    PkTest::addColumn<unsigned>("rgba");
    PkTest::addColumn<bool>("valid");

    PkTest::newRow("color0")       << int(Qt::color0)     << 0xffffffffu << true;
    PkTest::newRow("color1")       << int(Qt::color1)     << 0xff000000u << true;
    PkTest::newRow("black")        << int(Qt::black)      << 0xff000000u << true;
    PkTest::newRow("white")        << int(Qt::white)      << 0xffffffffu << true;
    PkTest::newRow("darkGray")     << int(Qt::darkGray)   << 0xff808080u << true;
    PkTest::newRow("gray")         << int(Qt::gray)       << 0xffa0a0a4u << true;
    PkTest::newRow("lightGray")    << int(Qt::lightGray)  << 0xffc0c0c0u << true;
    PkTest::newRow("red")          << int(Qt::red)        << 0xffff0000u << true;
    // ⚠ brief 探针写 0xff008000 —— 那是 SVG 命名色 "green"，不是 Qt::green。
    PkTest::newRow("green")        << int(Qt::green)      << 0xff00ff00u << true;
    PkTest::newRow("blue")         << int(Qt::blue)       << 0xff0000ffu << true;
    PkTest::newRow("cyan")         << int(Qt::cyan)       << 0xff00ffffu << true;
    PkTest::newRow("magenta")      << int(Qt::magenta)    << 0xffff00ffu << true;
    PkTest::newRow("yellow")       << int(Qt::yellow)     << 0xffffff00u << true;
    PkTest::newRow("darkRed")      << int(Qt::darkRed)    << 0xff800000u << true;
    PkTest::newRow("darkGreen")    << int(Qt::darkGreen)  << 0xff008000u << true;
    PkTest::newRow("darkBlue")     << int(Qt::darkBlue)   << 0xff000080u << true;
    PkTest::newRow("darkCyan")     << int(Qt::darkCyan)   << 0xff008080u << true;
    PkTest::newRow("darkMagenta")  << int(Qt::darkMagenta)<< 0xff800080u << true;
    // ⚠ brief 探针断言 darkYellow 无效 —— 真 Qt 它是 (128,128,0) **有效**。
    PkTest::newRow("darkYellow")   << int(Qt::darkYellow) << 0xff808000u << true;
    PkTest::newRow("transparent")  << int(Qt::transparent)<< 0x00000000u << true;
}

void PkColorCase::globalColor()
{
    PK_FETCH(int, gc);
    PK_FETCH(unsigned, rgba);
    PK_FETCH(bool, valid);
    PkColor c = Qt::GlobalColor(gc);
    PK_COMPARE(c.isValid(), valid);
    PK_COMPARE(c.rgba(), rgba);
}

void PkColorCase::namedColor_data()
{
    PkTest::addColumn<PkString>("name");
    PkTest::addColumn<unsigned>("rgba");
    PkTest::addColumn<bool>("valid");

    // SVG 命名色族（代表性子集 + 所有易错点；全表 148 项由 oracle 对拍）。
    PkTest::newRow("red")          << "red"          << 0xffff0000u << true;
    PkTest::newRow("Red")          << "Red"          << 0xffff0000u << true;   // 大小写不敏感
    PkTest::newRow("RED")          << "RED"          << 0xffff0000u << true;
    PkTest::newRow("red spaces")   << " red "        << 0xffff0000u << true;   // 去空白
    // SVG "green"=(0,128,0) ≠ Qt::green=(0,255,0)（QColor 文档原话，两组颜色不同）。
    PkTest::newRow("green")        << "green"        << 0xff008000u << true;
    PkTest::newRow("blue")         << "blue"         << 0xff0000ffu << true;
    PkTest::newRow("white")        << "white"        << 0xffffffffu << true;
    PkTest::newRow("black")        << "black"        << 0xff000000u << true;
    PkTest::newRow("transparent")  << "transparent"  << 0x00000000u << true;
    PkTest::newRow("gray")         << "gray"         << 0xff808080u << true;
    PkTest::newRow("grey")         << "grey"         << 0xff808080u << true;
    // SVG 灰 ≠ Qt::darkGray/lightGray（SVG 是 #a9a9a9 / #d3d3d3）。
    PkTest::newRow("darkGray")     << "darkGray"     << 0xffa9a9a9u << true;
    PkTest::newRow("darkgrey")     << "darkgrey"     << 0xffa9a9a9u << true;
    PkTest::newRow("lightGray")    << "lightGray"    << 0xffd3d3d3u << true;
    PkTest::newRow("Light Gray")   << "Light Gray"   << 0xffd3d3d3u << true;   // 去空格 + 转小写
    // SVG 表里没有 darkYellow：**无效**（与 Qt::darkYellow 有效相反）。
    PkTest::newRow("darkYellow")   << "darkYellow"   << 0xff000000u << false;
    PkTest::newRow("notacolor")    << "notacolor"    << 0xff000000u << false;
    PkTest::newRow("multi word")   << "ReD NoT a CoLor" << 0xff000000u << false;
    PkTest::newRow("empty")        << ""             << 0xff000000u << false;

    // 常见 SVG 色的抽查（值全部来自 rgbTbl）。
    PkTest::newRow("steelblue")    << "steelblue"    << 0xff4682b4u << true;
    PkTest::newRow("aliceblue")    << "aliceblue"    << 0xfff0f8ffu << true;
    PkTest::newRow("yellowgreen")  << "yellowgreen"  << 0xff9acd32u << true;
    PkTest::newRow("orange")       << "orange"       << 0xffffa500u << true;
    PkTest::newRow("olive")        << "olive"        << 0xff808000u << true;
    PkTest::newRow("navy")         << "navy"         << 0xff000080u << true;
    PkTest::newRow("maroon")       << "maroon"       << 0xff800000u << true;
    PkTest::newRow("purple")       << "purple"       << 0xff800080u << true;
    PkTest::newRow("lime")         << "lime"         << 0xff00ff00u << true;
    PkTest::newRow("silver")       << "silver"       << 0xffc0c0c0u << true;
    PkTest::newRow("teal")         << "teal"         << 0xff008080u << true;
    PkTest::newRow("gold")         << "gold"         << 0xffffd700u << true;
    PkTest::newRow("tomato")       << "tomato"       << 0xffff6347u << true;
    PkTest::newRow("snow")         << "snow"         << 0xfffffafau << true;
}

void PkColorCase::namedColor()
{
    PK_FETCH(PkString, name);
    PK_FETCH(unsigned, rgba);
    PK_FETCH(bool, valid);
    PkColor c(name);
    PK_COMPARE(c.isValid(), valid);
    PK_COMPARE(c.rgba(), rgba);
}

void PkColorCase::hexColor_data()
{
    PkTest::addColumn<PkString>("hex");
    PkTest::addColumn<unsigned>("rgba");
    PkTest::addColumn<bool>("valid");

    // #RGB / #RRGGBB / #AARRGGBB / #RRRGGGBBB / #RRRRGGGGBBBB
    PkTest::newRow("f00")          << "#f00"          << 0xffff0000u << true;
    PkTest::newRow("F00")          << "#F00"          << 0xffff0000u << true;
    PkTest::newRow("ff0000")       << "#ff0000"       << 0xffff0000u << true;
    PkTest::newRow("FF0000")       << "#FF0000"       << 0xffff0000u << true;
    PkTest::newRow("aarrggbb")     << "#80ff0000"     << 0x80ff0000u << true;
    PkTest::newRow("ffff0000")     << "#ffff0000"     << 0xffff0000u << true;
    PkTest::newRow("00000000")     << "#00000000"     << 0x00000000u << true;
    PkTest::newRow("123456")       << "#123456"       << 0xff123456u << true;
    PkTest::newRow("abcdef")       << "#abcdef"       << 0xffabcdefu << true;
    PkTest::newRow("ABCDEF")       << "#ABCDEF"       << 0xffabcdefu << true;
    PkTest::newRow("fff")          << "#fff"          << 0xffffffffu << true;
    PkTest::newRow("000")          << "#000"          << 0xff000000u << true;
    PkTest::newRow("rrrrggggbbbb") << "#112233445566" << 0xff113355u << true;
    PkTest::newRow("ffff all")     << "#ffffffffffff" << 0xffffffffu << true;
    PkTest::newRow("rrrgggbbb")    << "#fff000fff"    << 0xffff00ffu << true;
    PkTest::newRow("GGGGGG")       << "#GGGGGG"       << 0xff000000u << false;
    PkTest::newRow("too short")    << "#12"           << 0xff000000u << false;
    PkTest::newRow("too long")     << "#1234567"      << 0xff000000u << false;
    PkTest::newRow("11 digits")    << "#123456789ab"  << 0xff000000u << false;
}

void PkColorCase::hexColor()
{
    PK_FETCH(PkString, hex);
    PK_FETCH(unsigned, rgba);
    PK_FETCH(bool, valid);
    PkColor c(hex);
    PK_COMPARE(c.isValid(), valid);
    PK_COMPARE(c.rgba(), rgba);
}

void PkColorCase::hsvRoundtrip_data()
{
    PkTest::addColumn<int>("r");
    PkTest::addColumn<int>("g");
    PkTest::addColumn<int>("b");
    PkTest::addColumn<int>("hue");
    PkTest::addColumn<int>("sat");
    PkTest::addColumn<int>("val");

    PkTest::newRow("red")     << 255 <<   0 <<   0 <<   0 << 255 << 255;
    PkTest::newRow("green")   <<   0 << 255 <<   0 << 120 << 255 << 255;
    PkTest::newRow("blue")    <<   0 <<   0 << 255 << 240 << 255 << 255;
    PkTest::newRow("yellow")  << 255 << 255 <<   0 <<  60 << 255 << 255;
    PkTest::newRow("cyan")    <<   0 << 255 << 255 << 180 << 255 << 255;
    PkTest::newRow("magenta") << 255 <<   0 << 255 << 300 << 255 << 255;
    PkTest::newRow("orange")  << 255 << 128 <<   0 <<  30 << 255 << 255;
    PkTest::newRow("dred")    << 128 <<   0 <<   0 <<   0 << 255 << 128;
    PkTest::newRow("white")   << 255 << 255 << 255 <<  -1 <<   0 << 255;
    PkTest::newRow("gray128") << 128 << 128 << 128 <<  -1 <<   0 << 128;
    PkTest::newRow("gray64")  <<  64 <<  64 <<  64 <<  -1 <<   0 <<  64;
}

void PkColorCase::hsvRoundtrip()
{
    PK_FETCH(int, r);
    PK_FETCH(int, g);
    PK_FETCH(int, b);
    PK_FETCH(int, hue);
    PK_FETCH(int, sat);
    PK_FETCH(int, val);
    PkColor c(r, g, b);
    PK_COMPARE(c.hue(), hue);
    PK_COMPARE(c.saturation(), sat);
    PK_COMPARE(c.value(), val);
}

// ── HSV / HSL ─────────────────────────────────────────────

void PkColorCase::hsvBasics()
{
    PkColor c = PkColor::fromHsv(0, 255, 255);
    PK_COMPARE(c.rgba(), 0xffff0000u);
    PK_COMPARE(c.hue(), 0);
    PK_COMPARE(c.saturation(), 255);
    PK_COMPARE(c.value(), 255);
    PK_COMPARE(int(c.spec()), int(PkColor::Hsv));

    // 灰（sat=0）：rgb 用 value 取灰，但 spec 仍是 Hsv → hue() 直接读存储的
    // 16 位字段，返回 123（真 Qt 实测：fromHsv(123,0,255).hue()==123，不是 -1；
    // -1 只出现在 Rgb→Hsv 转换后色度无定义时）。
    PkColor g = PkColor::fromHsv(123, 0, 255);
    PK_COMPARE(g.rgba(), 0xffffffffu);
    PK_COMPARE(g.hue(), 123);
    PK_COMPARE(g.saturation(), 0);
    PK_COMPARE(g.value(), 255);

    PkColor b = PkColor::fromHsv(240, 255, 255);
    PK_COMPARE(b.rgba(), 0xff0000ffu);
    PK_COMPARE(b.hue(), 240);
}

void PkColorCase::hslBasics()
{
    // Qt 真值：HSL(0,255,128) → (255,1,1)，**不是** (255,0,0)。
    // 根因：l=128/255>0.5 → temp2=1.0、temp1≈0.00392，绿/蓝通道取 temp1 舍到 1。
    PkColor c = PkColor::fromHsl(0, 255, 128);
    PK_COMPARE(c.red(), 255);
    PK_COMPARE(c.green(), 1);
    PK_COMPARE(c.blue(), 1);
    PK_COMPARE(c.rgba(), 0xffff0101u);
    PK_COMPARE(c.hslHue(), 0);
    PK_COMPARE(c.hslSaturation(), 255);
    PK_COMPARE(c.lightness(), 128);
    PK_COMPARE(int(c.spec()), int(PkColor::Hsl));

    PK_COMPARE(PkColor::fromHsl(120, 255, 128).rgba(), 0xff01ff01u);   // (1,255,1)
    PK_COMPARE(PkColor::fromHsl(240, 255, 128).rgba(), 0xff0101ffu);   // (1,1,255)
    PK_COMPARE(PkColor::fromHsl(0, 255, 255).rgba(), 0xffffffffu);     // l=1 → 白
    PK_COMPARE(PkColor::fromHsl(0, 255, 0).rgba(), 0xff000000u);       // l=0 → 黑
}

void PkColorCase::hsvFromRgb()
{
    PK_COMPARE(PkColor(255, 0, 0).hue(), 0);
    PK_COMPARE(PkColor(0, 255, 0).hue(), 120);
    PK_COMPARE(PkColor(0, 0, 255).hue(), 240);
    PK_COMPARE(PkColor(255, 255, 255).hue(), -1);
    PK_COMPARE(PkColor(255, 255, 255).saturation(), 0);
    PK_COMPARE(PkColor(255, 255, 255).value(), 255);
    // r==g 时 Qt 的 toHsv 在 r==max 分支先命中 → (g-b)/delta → 60
    PK_COMPARE(PkColor(255, 255, 0).hue(), 60);
}

void PkColorCase::hsvSettersWrap()
{
    PkColor c;
    c.setHsv(720, 255, 255);   // 720%360=0 → 红（setHsv 回绕）
    PK_COMPARE(c.rgba(), 0xffff0000u);
    PK_COMPARE(int(c.spec()), int(PkColor::Hsv));

    PkColor c2;
    c2.setHsv(370, 255, 255);  // 370%360=10
    PK_COMPARE(c2.hue(), 10);

    // fromHsv 与 setHsv 不对称：h>=360 → 无效。
    PK_VERIFY(!PkColor::fromHsv(360, 255, 255).isValid());
    PK_VERIFY(!PkColor::fromHsv(720, 255, 255).isValid());

    PkColor c4;
    c4.setHsv(-2, 255, 255);   // h<-1 → 无效
    PK_VERIFY(!c4.isValid());

    PkColor c5;
    c5.setHsv(0, 300, 255);    // s 越界 → 无效
    PK_VERIFY(!c5.isValid());
}

// ── 浮点工厂 ──────────────────────────────────────────────

void PkColorCase::rgbF()
{
    PK_COMPARE(PkColor::fromRgbF(1.0, 0.0, 0.0).rgba(), 0xffff0000u);
    PK_COMPARE(PkColor::fromRgbF(1.0, 0.0, 0.0, 0.5).rgba(), 0x80ff0000u);
    PK_COMPARE(int(PkColor::fromRgbF(1.0, 0.0, 0.0).spec()), int(PkColor::Rgb));

    // rgb 越界 → ExtendedRgb；redF() 返回存的浮点（pk 用 float，见偏离登记
    // 「ExtendedRgb 精度」），8-bit getter 经 toRgb 夹到 [0,255]。
    PkColor ext = PkColor::fromRgbF(1.2, 0.0, 0.0);
    PK_COMPARE(int(ext.spec()), int(PkColor::ExtendedRgb));
    PK_COMPARE(ext.redF(), 1.2f);
    PK_COMPARE(ext.greenF(), 0.0f);
    PK_COMPARE(ext.red(), 255);
    PK_COMPARE(ext.rgba(), 0xffff0000u);
    PK_COMPARE(ext.alpha(), 255);

    // alpha 越界 → 无效（与 rgb 越界 → ExtendedRgb 不同）。
    PK_VERIFY(!PkColor::fromRgbF(1.0, 0.0, 0.0, 1.5).isValid());
}

void PkColorCase::hsvF()
{
    PK_COMPARE(PkColor::fromHsvF(0.0, 1.0, 1.0).rgba(), 0xffff0000u);
    PK_COMPARE(PkColor::fromHsvF(1.0, 1.0, 1.0).rgba(), 0xffff0000u);   // hue=36000→0
    // 16-bit 取整的实证（probe 实测）：h=0.333 → hue=11988 → i=1 → (1,255,0)。
    PK_COMPARE(PkColor::fromHsvF(0.333, 1.0, 1.0).rgba(), 0xff01ff00u);
    PK_COMPARE(PkColor::fromHsvF(0.0, 1.0, 1.0, 0.5).rgba(), 0x80ff0000u);

    // setHsvF 越界静默 return（不置无效），与 setHsv 不对称。
    PkColor c(255, 0, 0);
    c.setHsvF(2.0, 1.0, 1.0);
    PK_COMPARE(c.rgba(), 0xffff0000u);
    PK_VERIFY(c.isValid());
    PK_COMPARE(int(c.spec()), int(PkColor::Rgb));
}

void PkColorCase::hslF()
{
    PK_COMPARE(PkColor::fromHslF(0.0, 1.0, 0.5).rgba(), 0xffff0000u);
    PK_COMPARE(PkColor::fromHslF(1.0, 1.0, 0.5).rgba(), 0xffff0000u);   // hue 36000→0（fromHslF 特有）
    PK_COMPARE(PkColor::fromHslF(0.5, 1.0, 0.5).rgba(), 0xff00ffffu);
}

// ── 设定 / 越界 ───────────────────────────────────────────

void PkColorCase::setChannelClamp()
{
    PkColor c(100, 100, 100);
    c.setRed(300);      // 截断到 255，**不**置无效
    PK_COMPARE(c.red(), 255);
    PK_VERIFY(c.isValid());
    c.setRed(-5);       // 截断到 0
    PK_COMPARE(c.red(), 0);
    PK_VERIFY(c.isValid());
    c.setGreen(128);
    PK_COMPARE(c.green(), 128);
    c.setAlpha(64);
    PK_COMPARE(c.alpha(), 64);
    c.setAlphaF(0.5);
    PK_COMPARE(c.alpha(), 128);
}

void PkColorCase::setRgbInvalidates()
{
    PkColor c(255, 0, 0);
    c.setRgb(300, 0, 0);
    PK_VERIFY(!c.isValid());

    PkColor c2(255, 0, 0);
    c2.setRgb(0, 0, 0, 300);
    PK_VERIFY(!c2.isValid());
}

void PkColorCase::setRgbaOpaque()
{
    // setRgba 是**单参 QRgb**（Qt 5.15 签名，brief 的 4 参形态不存在）。
    PkColor c3;
    c3.setRgba(0x80ff0000u);
    PK_COMPARE(c3.rgba(), 0x80ff0000u);
    PK_COMPARE(int(c3.spec()), int(PkColor::Rgb));

    // setRgb(QRgb) opaque：alpha 置 255。
    PkColor c4;
    c4.setRgb(0x80ff0000u);
    PK_COMPARE(c4.rgba(), 0xffff0000u);
}

void PkColorCase::setRedOnHsvConvertsToRgb()
{
    PkColor c = PkColor::fromHsv(120, 255, 255);   // (0,255,0)，spec Hsv
    c.setRed(255);                                 // 非 Rgb 色先转 Rgb → (255,255,0)
    PK_COMPARE(c.rgba(), 0xffffff00u);
    PK_COMPARE(int(c.spec()), int(PkColor::Rgb));
}

void PkColorCase::setAlphaOnExtendedRgb()
{
    PkColor c = PkColor::fromRgbF(1.2, 0.0, 0.0);
    PK_COMPARE(int(c.spec()), int(PkColor::ExtendedRgb));
    c.setAlpha(128);
    PK_COMPARE(c.alpha(), 128);
    PK_COMPARE(c.rgba(), 0x80ff0000u);
}

// ── 派生与命名 ────────────────────────────────────────────

void PkColorCase::lighter()
{
    PK_COMPARE(PkColor(128, 128, 128).lighter(150).rgba(), 0xffc0c0c0u);
    PK_COMPARE(PkColor(128, 128, 128).lighter(100).rgba(), 0xff808080u);
    // 真 Qt 实测 0xffff7f7f（HSL l=0.75 经 setHslF 16 位量化 → g=b=127），不是 0x80。
    PK_COMPARE(PkColor(255, 0, 0).lighter(150).rgba(), 0xffff7f7fu);
    PK_COMPARE(PkColor(128, 128, 128).lighter(0).rgba(), 0xff808080u);    // factor<=0 不变
    PK_COMPARE(PkColor(128, 128, 128).lighter(50).rgba(), 0xff404040u);   // <100 交叉调 darker(200)
}

void PkColorCase::darker()
{
    PK_COMPARE(PkColor(128, 128, 128).darker(200).rgba(), 0xff404040u);
    // 真 Qt 实测 0xff00007f（l=0.25 经 setHslF 16 位量化 → b=127），不是 0x80。
    PK_COMPARE(PkColor(0, 0, 255).darker(200).rgba(), 0xff00007fu);
    PK_COMPARE(PkColor(128, 128, 128).darker(100).rgba(), 0xff808080u);
    PK_COMPARE(PkColor(128, 128, 128).darker(0).rgba(), 0xff808080u);     // factor<=0 不变
    PK_COMPARE(PkColor(128, 128, 128).darker(50).rgba(), 0xffffffffu);    // 交叉调 lighter(200)：v 溢出 → 白
}

void PkColorCase::name()
{
    PK_COMPARE(PkColor(255, 0, 0).name(), PkString("#ff0000"));
    PK_COMPARE(PkColor::fromHsv(0, 255, 255).name(), PkString("#ff0000"));
    PK_COMPARE(PkColor(Qt::transparent).name(), PkString("#000000"));     // HexRgb 忽略 alpha
    PK_COMPARE(PkColor(0, 0, 255).name(), PkString("#0000ff"));
    PK_COMPARE(PkColor(255, 255, 255).name(), PkString("#ffffff"));
}

void PkColorCase::nameArgb()
{
    PK_COMPARE(PkColor(255, 0, 0, 128).name(PkColor::HexArgb), PkString("#80ff0000"));
    PK_COMPARE(PkColor(Qt::transparent).name(PkColor::HexArgb), PkString("#00000000"));
}

// ── 比较 ──────────────────────────────────────────────────

void PkColorCase::equality()
{
    PK_VERIFY(PkColor(255, 0, 0) == PkColor(255, 0, 0));
    PK_VERIFY(PkColor(255, 0, 0) == Qt::red);
    PK_VERIFY(PkColor(Qt::green) == PkColor(0, 255, 0));
    PK_VERIFY(PkColor(Qt::transparent) == PkColor(0, 0, 0, 0));
    PK_VERIFY(!(PkColor(255, 0, 0) == PkColor(255, 0, 1)));
    PK_VERIFY(PkColor(255, 0, 0) != PkColor(255, 0, 1));
}

void PkColorCase::equalityAlphaMatters()
{
    // ⚠ brief 探针称 operator== 忽略 alpha —— 真 Qt 5.15 **比较 alpha**
    // （qcolor.cpp:2954-2981）。两条都过才算对。
    PK_VERIFY(PkColor(255, 0, 0) == PkColor(255, 0, 0, 255));
    PK_VERIFY(!(PkColor(255, 0, 0) == PkColor(255, 0, 0, 128)));
}

void PkColorCase::equalitySpecMatters()
{
    // 真 Qt：operator== 要求 cspec 相同——即使 rgba() 相同，Hsv 与 Rgb 不相等。
    PK_VERIFY(!(PkColor::fromHsv(0, 255, 255) == PkColor(255, 0, 0)));
    PK_VERIFY(PkColor::fromHsv(0, 255, 255) == PkColor::fromHsv(0, 255, 255));
    PK_VERIFY(PkColor::fromHsl(0, 255, 128) == PkColor::fromHsl(0, 255, 128));
}

// ── setNamedColor 边界 ────────────────────────────────────

void PkColorCase::setNamedColorEdge()
{
    PkColor c;
    c.setNamedColor("");
    PK_VERIFY(!c.isValid());
    c.setNamedColor(static_cast<const char *>(nullptr));
    PK_VERIFY(!c.isValid());
    c.setNamedColor("#xyz");
    PK_VERIFY(!c.isValid());
    c.setNamedColor("#12");
    PK_VERIFY(!c.isValid());
    c.setNamedColor("notacolor");
    PK_VERIFY(!c.isValid());

    c.setNamedColor("#ff0000");
    PK_COMPARE(c.rgba(), 0xffff0000u);
    PK_COMPARE(int(c.spec()), int(PkColor::Rgb));

    PkColor c2;
    c2.setNamedColor(PkString("darkGray"));
    PK_COMPARE(c2.rgba(), 0xffa9a9a9u);

    PkColor c3("red");   // const char* 构造
    PK_COMPARE(c3.rgba(), 0xffff0000u);
}

// ── 入口 ──────────────────────────────────────────────────

int run_color_tests()
{
    PkColorCase tc;
    const char *argv[] = {"test_pkcolor"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
