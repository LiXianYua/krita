// difftest_color.cpp —— QColor ↔ PkColor 逐输入对拍（甲类核心判据）
//
// 两侧真分别 include：真 Qt 的 <QColor>（全局作用域）+ PkColor.h。PkColor.h 的
// 依赖链（PkNamespace.h 会在 namespace Qt 里定义 enum GlobalColor，PkGlobal.h 会
// 在全局作用域定义 qAbs/qRound/qMin/qMax/qBound —— 与真 Qt 的同名定义硬冲突），
// 所以 Pk 侧整条链（头 + 实现 .cpp）被包进 `namespace pkoracle`：PkNamespace 的
// Qt::GlobalColor → pkoracle::Qt::GlobalColor，PkGlobal 的 q* → pkoracle::q*，
// 与真 Qt 彻底隔离。std 系统头必须在包外层先 include（include guard 让包内的
// 二次 include 空转），否则 std 会被卷进 pkoracle::std。
//
// 判据③ 由 run_oracle.sh 用 nm -u 自证；-I 绝不能给 compat/（否则两侧同型恒等）。
// 已知偏离（color.deviation 登记）在 ExtendedRgb 与 convertTo(Cmyk/ExtendedRgb)：
//   · ExtendedRgb 分量 pk 用 float(32bit)，真 Qt 用 qfloat16(16bit half) →
//     对拍谓词放宽（float 分量容差 5e-4、int 分量容差 2），并实测最大偏差；
//   · convertTo 的 Cmyk/ExtendedRgb 目标：范围表判「不实现」，pk 返回无效色，
//     真 Qt 返回有效色（toCmyk/toExtendedRgb）→ 这两支单独 DIFFTAG 登记。
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <locale.h>
#include <map>
#include <memory>
#include <stdlib.h>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>
#include <QColor>

#undef Q_UNUSED
#undef Q_ASSERT
namespace pkoracle {
#include "PkColor.h"
#include "../string/PkString_core.cpp"
#include "../string/PkString_query.cpp"
#include "../string/PkString_format.cpp"
#include "../string/PkStringCodec.cpp"
#include "../color/PkColor.cpp"
// toHsv/toHsl 的 Q_ASSERT(false && "internal error") 分支在公开 API 下不可达；
// 这里提供一个最小定义防链接失败（与 PkGlobal.cpp 的 pk_qt_assert 同形态）。
void pk_qt_assert(const char *what, const char *file, int line)
{
    std::fprintf(stderr, "ASSERT: %s in file %s, line %d\n", what, file, line);
    std::abort();
}
}

static_assert(!std::is_same<QColor, pkoracle::PkColor>::value,
              "对拍两侧解析成了同一类型");

static long g_total = 0, g_mismatch = 0;
static std::map<std::string, long> g_tags, g_cover;
static int g_printed = 0;
static double g_xrgbMaxFloat = 0.0;
static long g_xrgbMaxInt = 0;
static long g_xrgbCases = 0;

static std::string s(long v) { char b[32]; std::snprintf(b, sizeof b, "%ld", v); return b; }
static std::string hex(unsigned long v) { char b[32]; std::snprintf(b, sizeof b, "0x%lx", v); return b; }
static std::string sf(double v) { char b[64]; std::snprintf(b, sizeof b, "%.9g", v); return b; }

static void rec(const std::string &api, bool same, const std::string &tag,
                const std::string &in, const std::string &qt, const std::string &pk) {
    ++g_total; ++g_cover[api];
    if (same) return;
    ++g_mismatch; ++g_tags[api + " " + tag];
    if (g_printed < 60) { ++g_printed; std::printf("MISMATCH: %s [%s] in=%s qt=%s pk=%s\n", api.c_str(), tag.c_str(), in.c_str(), qt.c_str(), pk.c_str()); }
}

// ExtendedRgb 谓词：pk float(32bit) vs 真 Qt qfloat16(16bit half) 的偏离宽度。
static bool fl(double a, double b) { return std::fabs(a - b) <= 5e-4; }
static bool il(long a, long b) { return (a > b ? a - b : b - a) <= 2; }
static bool rgbByteNear(unsigned long a, unsigned long b) {
    for (int sh = 0; sh < 32; sh += 8) {
        long da = (long)((a >> sh) & 0xff), db = (long)((b >> sh) & 0xff);
        if (da > db ? da - db > 1 : db - da > 1) return false;
    }
    return true;
}

// 全套可观测面：isValid/spec/rgb/rgba/分量 getter/F getter/HSV-HSL 派生/name/转换。
// ExtendedRgb 色走容差谓词并记录最大偏差，其余精确相等。
static void cmp_color(const char *api, const QColor &q, const pkoracle::PkColor &p, const std::string &in) {
    const std::string pfx(api);
    const bool ext = (p.spec() == pkoracle::PkColor::ExtendedRgb);
    if (ext) ++g_xrgbCases;
    if (ext) { g_xrgbMaxInt = std::max(g_xrgbMaxInt, std::labs((long)q.rgba() - (long)p.rgba())); }

    rec(pfx, q.isValid() == p.isValid(), "isValid", in, s(q.isValid()), s(p.isValid()));
    rec(pfx, int(q.spec()) == int(p.spec()), "spec", in, s(int(q.spec())), s(int(p.spec())));
    rec(pfx, ext ? rgbByteNear(q.rgb(), p.rgb()) : q.rgb() == p.rgb(), "rgb", in, hex(q.rgb()), hex(p.rgb()));
    rec(pfx, ext ? rgbByteNear(q.rgba(), p.rgba()) : q.rgba() == p.rgba(), "rgba", in, hex(q.rgba()), hex(p.rgba()));

    rec(pfx, ext ? il(q.red(), p.red()) : q.red() == p.red(), "red", in, s(q.red()), s(p.red()));
    rec(pfx, ext ? il(q.green(), p.green()) : q.green() == p.green(), "green", in, s(q.green()), s(p.green()));
    rec(pfx, ext ? il(q.blue(), p.blue()) : q.blue() == p.blue(), "blue", in, s(q.blue()), s(p.blue()));
    rec(pfx, ext ? il(q.alpha(), p.alpha()) : q.alpha() == p.alpha(), "alpha", in, s(q.alpha()), s(p.alpha()));

    rec(pfx, ext ? fl(q.redF(), p.redF()) : q.redF() == p.redF(), "redF", in, sf(q.redF()), sf(p.redF()));
    rec(pfx, ext ? fl(q.greenF(), p.greenF()) : q.greenF() == p.greenF(), "greenF", in, sf(q.greenF()), sf(p.greenF()));
    rec(pfx, ext ? fl(q.blueF(), p.blueF()) : q.blueF() == p.blueF(), "blueF", in, sf(q.blueF()), sf(p.blueF()));
    rec(pfx, ext ? fl(q.alphaF(), p.alphaF()) : q.alphaF() == p.alphaF(), "alphaF", in, sf(q.alphaF()), sf(p.alphaF()));
    if (ext) {
        g_xrgbMaxFloat = std::max(g_xrgbMaxFloat, std::fabs(q.redF() - p.redF()));
        g_xrgbMaxFloat = std::max(g_xrgbMaxFloat, std::fabs(q.greenF() - p.greenF()));
        g_xrgbMaxFloat = std::max(g_xrgbMaxFloat, std::fabs(q.blueF() - p.blueF()));
        g_xrgbMaxFloat = std::max(g_xrgbMaxFloat, std::fabs(q.alphaF() - p.alphaF()));
    }

    rec(pfx, ext ? il(q.hue(), p.hue()) : q.hue() == p.hue(), "hue", in, s(q.hue()), s(p.hue()));
    rec(pfx, ext ? il(q.saturation(), p.saturation()) : q.saturation() == p.saturation(), "saturation", in, s(q.saturation()), s(p.saturation()));
    rec(pfx, ext ? il(q.value(), p.value()) : q.value() == p.value(), "value", in, s(q.value()), s(p.value()));
    rec(pfx, ext ? il(q.hslHue(), p.hslHue()) : q.hslHue() == p.hslHue(), "hslHue", in, s(q.hslHue()), s(p.hslHue()));
    rec(pfx, ext ? il(q.hslSaturation(), p.hslSaturation()) : q.hslSaturation() == p.hslSaturation(), "hslSaturation", in, s(q.hslSaturation()), s(p.hslSaturation()));
    rec(pfx, ext ? il(q.lightness(), p.lightness()) : q.lightness() == p.lightness(), "lightness", in, s(q.lightness()), s(p.lightness()));

    // name 由 rgba 推导；ExtendedRgb 的 rgba 已按字节容差核对，字符串比较跳过。
    if (!ext) {
        rec(pfx, q.name().toStdString() == p.name().PkToUtf8(), "name", in,
            q.name().toStdString(), p.name().PkToUtf8());
        rec(pfx, q.name(QColor::HexArgb).toStdString() == p.name(pkoracle::PkColor::HexArgb).PkToUtf8(), "nameArgb", in,
            q.name(QColor::HexArgb).toStdString(), p.name(pkoracle::PkColor::HexArgb).PkToUtf8());
    }

    rec(pfx, ext ? rgbByteNear(q.toRgb().rgba(), p.toRgb().rgba()) : q.toRgb().rgba() == p.toRgb().rgba(), "toRgb.rgba", in, hex(q.toRgb().rgba()), hex(p.toRgb().rgba()));
    rec(pfx, int(q.toRgb().spec()) == int(p.toRgb().spec()), "toRgb.spec", in, s(int(q.toRgb().spec())), s(int(p.toRgb().spec())));
    rec(pfx, ext ? il(q.toHsv().hue(), p.toHsv().hue()) : q.toHsv().hue() == p.toHsv().hue(), "toHsv.hue", in, s(q.toHsv().hue()), s(p.toHsv().hue()));
    rec(pfx, int(q.toHsv().spec()) == int(p.toHsv().spec()), "toHsv.spec", in, s(int(q.toHsv().spec())), s(int(p.toHsv().spec())));
    rec(pfx, ext ? il(q.toHsl().hslHue(), p.toHsl().hslHue()) : q.toHsl().hslHue() == p.toHsl().hslHue(), "toHsl.hslHue", in, s(q.toHsl().hslHue()), s(p.toHsl().hslHue()));
    rec(pfx, int(q.toHsl().spec()) == int(p.toHsl().spec()), "toHsl.spec", in, s(int(q.toHsl().spec())), s(int(p.toHsl().spec())));
}

int main()
{
    // ── 1. Qt::GlobalColor 全 20 项 ────────────────────────────────────────
    for (int gc = 0; gc < 20; ++gc) {
        QColor q((Qt::GlobalColor)gc);
        pkoracle::PkColor p((pkoracle::Qt::GlobalColor)gc);
        cmp_color("gc", q, p, "gc=" + s(gc));
    }

    // ── 2. RGB 组合爆破（整数构造 → Rgb spec）───────────────────────────────
    const int rgbv[] = {0, 1, 127, 128, 254, 255, 256};
    const int alv[] = {0, 128, 255, 256};
    for (int r : rgbv) for (int g : rgbv) for (int b : rgbv) for (int a : alv) {
        char in[64]; std::snprintf(in, sizeof in, "r=%d,g=%d,b=%d,a=%d", r, g, b, a);
        QColor q(r, g, b, a);
        pkoracle::PkColor p(r, g, b, a);
        cmp_color("ctor.rgb", q, p, in);
    }

    // ── 3. fromRgb/fromRgba/setRgb/setRgba 的 QRgb 形态 ─────────────────────
    const unsigned long rgbaVals[] = {0x00000000u, 0x01010101u, 0x80808080u, 0x80ff0000u,
                                      0xffffffffu, 0x12345678u, 0x00ffffffu, 0x7f7f7f7fu};
    for (unsigned long v : rgbaVals) {
        std::string in = "v=" + hex(v);
        {
            QColor q = QColor::fromRgb(QRgb(v));
            pkoracle::PkColor p = pkoracle::PkColor::fromRgb((unsigned int)v);
            cmp_color("fromRgb.qrgb", q, p, in);
        }
        {
            QColor q = QColor::fromRgba(QRgb(v));
            pkoracle::PkColor p = pkoracle::PkColor::fromRgba((unsigned int)v);
            cmp_color("fromRgba.qrgb", q, p, in);
        }
        {
            QColor q; q.setRgb(QRgb(v));
            pkoracle::PkColor p; p.setRgb((unsigned int)v);
            cmp_color("setRgb.qrgb", q, p, in);
        }
        {
            QColor q; q.setRgba(QRgb(v));
            pkoracle::PkColor p; p.setRgba((unsigned int)v);
            cmp_color("setRgba.qrgb", q, p, in);
        }
    }

    // ── 4. fromRgbF / setRgbF（越界 → ExtendedRgb；alpha 越界 → 无效）────────
    const double rgbf[][4] = {
        {0.0, 0.0, 0.0, 1.0}, {1.0, 1.0, 1.0, 1.0}, {0.5, 0.25, 0.125, 0.75},
        {1.0, 0.0, 0.0, 1.0}, {0.0, 1.0, 0.0, 0.5}, {0.2, 0.4, 0.6, 0.8},
        {0.1, 0.9, 0.3, 0.7}, {0.0, 0.0, 0.0, 0.0},
        {1.5, 0.0, 0.0, 1.0}, {0.0, -0.5, 0.0, 1.0}, {0.0, 0.0, 2.0, 1.0}, {-1.0, 0.5, 0.5, 1.0},
        {0.5, 0.5, 0.5, 1.5}, {0.5, 0.5, 0.5, -1.0},
    };
    for (auto &v : rgbf) {
        char in[64]; std::snprintf(in, sizeof in, "r=%g,g=%g,b=%g,a=%g", v[0], v[1], v[2], v[3]);
        {
            QColor q = QColor::fromRgbF(v[0], v[1], v[2], v[3]);
            pkoracle::PkColor p = pkoracle::PkColor::fromRgbF(v[0], v[1], v[2], v[3]);
            cmp_color("fromRgbF", q, p, in);
        }
        {
            QColor q; q.setRgbF(v[0], v[1], v[2], v[3]);
            pkoracle::PkColor p; p.setRgbF(v[0], v[1], v[2], v[3]);
            cmp_color("setRgbF", q, p, in);
        }
    }

    // ── 5. HSV / HSL 边界 ──────────────────────────────────────────────────
    const int hv[] = {-180, -1, 0, 1, 119, 120, 121, 239, 240, 359, 360, 361, 400, 720, -360};
    const int sv[] = {0, 1, 127, 128, 254, 255, 256};
    const int av[] = {0, 1, 254, 255, 256};
    for (int h : hv) for (int s : sv) {
        char in[64]; std::snprintf(in, sizeof in, "h=%d,s=%d,v=255,a=255", h, s);
        {
            QColor q = QColor::fromHsv(h, s, 255, 255);
            pkoracle::PkColor p = pkoracle::PkColor::fromHsv(h, s, 255, 255);
            cmp_color("fromHsv", q, p, in);
        }
        {
            QColor q = QColor::fromHsl(h, s, 128, 255);
            pkoracle::PkColor p = pkoracle::PkColor::fromHsl(h, s, 128, 255);
            cmp_color("fromHsl", q, p, in);
        }
    }
    for (int h : hv) for (int a : av) {
        char in[64]; std::snprintf(in, sizeof in, "h=%d,s=255,v=128,a=%d", h, a);
        QColor q = QColor::fromHsv(h, 255, 128, a);
        pkoracle::PkColor p = pkoracle::PkColor::fromHsv(h, 255, 128, a);
        cmp_color("fromHsv.a", q, p, in);
    }
    const double hvf[][3] = {
        {0.0, 1.0, 1.0}, {0.333, 1.0, 1.0}, {-1.0, 0.0, 1.0}, {0.5, 0.5, 0.5},
        {1.5, 0.5, 0.5}, {0.0, -0.1, 0.5}, {0.0, 1.1, 0.5}, {0.0, 0.5, -0.2},
    };
    for (auto &v : hvf) {
        char in[64]; std::snprintf(in, sizeof in, "h=%g,s=%g,v=%g", v[0], v[1], v[2]);
        {
            QColor q = QColor::fromHsvF(v[0], v[1], v[2], 1.0);
            pkoracle::PkColor p = pkoracle::PkColor::fromHsvF(v[0], v[1], v[2], 1.0);
            cmp_color("fromHsvF", q, p, in);
        }
        {
            QColor q = QColor::fromHslF(v[0], v[1], v[2], 1.0);
            pkoracle::PkColor p = pkoracle::PkColor::fromHslF(v[0], v[1], v[2], 1.0);
            cmp_color("fromHslF", q, p, in);
        }
    }

    // ── 6. lighter / darker 因子谱 ──────────────────────────────────────────
    const int factors[] = {0, 1, 49, 50, 51, 99, 100, 101, 149, 150, 151, 199, 200, 201, 300};
    const int cols[][3] = {{128, 128, 128}, {255, 0, 0}, {0, 0, 255}, {255, 255, 255}, {0, 0, 0}, {64, 128, 192}};
    for (auto &c : cols) for (int f : factors) {
        QColor q(c[0], c[1], c[2]);
        pkoracle::PkColor p(c[0], c[1], c[2]);
        char in[64]; std::snprintf(in, sizeof in, "rgb(%d,%d,%d),f=%d", c[0], c[1], c[2], f);
        rec("lighter", q.lighter(f).rgba() == p.lighter(f).rgba(), "lighter.f" + s(f), in,
            hex(q.lighter(f).rgba()), hex(p.lighter(f).rgba()));
        rec("darker", q.darker(f).rgba() == p.darker(f).rgba(), "darker.f" + s(f), in,
            hex(q.darker(f).rgba()), hex(p.darker(f).rgba()));
    }

    // ── 7. 命名色全表（真 Qt colorNames() 驱动）+ setNamedColor ─────────────
    {
        const QStringList names = QColor::colorNames();
        for (const QString &n : names) {
            std::string name = n.toStdString();
            {
                QColor q(n);
                pkoracle::PkColor p(name.c_str());
                cmp_color("named", q, p, "name=" + name);
            }
            {
                QColor q; q.setNamedColor(n);
                pkoracle::PkColor p; p.setNamedColor(name.c_str());
                cmp_color("setNamed", q, p, "name=" + name);
            }
        }
    }
    // 无效名 + hex 全形态（setNamedColor）
    const char *bad[] = {"notacolor", "", "rgb(255,0,0)", "#", "#12345", "#1234567",
                         "#gggggg", "   ", "redx", "darkYellow ", "transparentx", "  red  "};
    for (const char *b : bad) {
        QColor q; q.setNamedColor(QString::fromLatin1(b));
        pkoracle::PkColor p; p.setNamedColor(b);
        cmp_color("named.bad", q, p, std::string("name=[") + b + "]");
    }
    const char *hexf[] = {"#f00", "#ff0000", "#80ff0000", "#aabbccdd", "#aabbcc", "#ffffffff", "#123", "#1234", "#12345678"};
    for (const char *h : hexf) {
        QColor q; q.setNamedColor(QString::fromLatin1(h));
        pkoracle::PkColor p; p.setNamedColor(h);
        cmp_color("hexf", q, p, std::string("name=[") + h + "]");
    }

    // ── 8. 分量 setter（越界截断 / 非 Rgb 先转 Rgb）──────────────────────────
    const int chv[][4] = {{0, 0, 0, 255}, {255, 128, 64, 0}, {300, 0, 0, 255}, {-1, 255, 255, 128}, {128, 300, -1, 200}};
    for (auto &v : chv) {
        QColor q(128, 128, 128); pkoracle::PkColor p(128, 128, 128);
        q.setRed(v[0]); p.setRed(v[0]);
        q.setGreen(v[1]); p.setGreen(v[1]);
        q.setBlue(v[2]); p.setBlue(v[2]);
        q.setAlpha(v[3]); p.setAlpha(v[3]);
        char in[64]; std::snprintf(in, sizeof in, "r=%d,g=%d,b=%d,a=%d", v[0], v[1], v[2], v[3]);
        cmp_color("setChannel", q, p, in);
    }
    {
        QColor q(255, 0, 0); pkoracle::PkColor p(255, 0, 0);
        q.setAlphaF(0.5); p.setAlphaF(0.5);
        cmp_color("setAlphaF", q, p, "alphaF=0.5");
    }

    // ── 9. operator== / != ─────────────────────────────────────────────────
    struct Pair { int r1, g1, b1, a1, r2, g2, b2, a2; };
    const Pair pairs[] = {
        {255, 0, 0, 255, 255, 0, 0, 255},   // 相同
        {255, 0, 0, 255, 255, 0, 0, 128},   // alpha 不同（真 Qt：== 比较 alpha）
        {255, 0, 0, 255, 0, 255, 0, 255},   // 不同色
        {0, 0, 0, 0, 0, 0, 0, 0},           // transparent == transparent
        {128, 128, 128, 255, 128, 128, 128, 0},
    };
    for (auto &pr : pairs) {
        char in[128]; std::snprintf(in, sizeof in, "(%d,%d,%d,%d)==(%d,%d,%d,%d)",
                                    pr.r1, pr.g1, pr.b1, pr.a1, pr.r2, pr.g2, pr.b2, pr.a2);
        QColor q1(pr.r1, pr.g1, pr.b1, pr.a1), q2(pr.r2, pr.g2, pr.b2, pr.a2);
        pkoracle::PkColor p1(pr.r1, pr.g1, pr.b1, pr.a1), p2(pr.r2, pr.g2, pr.b2, pr.a2);
        rec("eq", (q1 == q2) == (p1 == p2), "eq", in, s(q1 == q2), s(p1 == p2));
        rec("ne", (q1 != q2) == (p1 != p2), "ne", in, s(q1 != q2), s(p1 != p2));
    }

    // ── 10. convertTo（Rgb/Hsv/Hsl 应一致；Cmyk/ExtendedRgb 为登记偏离）──────
    // 非 Rgb spec 的 rgba() 是内部 union 裸位（布局相关），改为比语义面。
    {
        QColor q(64, 128, 192); pkoracle::PkColor p(64, 128, 192);
        // Rgb
        {
            QColor qc = q.convertTo(QColor::Rgb);
            pkoracle::PkColor pc = p.convertTo(pkoracle::PkColor::Rgb);
            rec("convertTo", int(qc.spec()) == int(pc.spec()) && qc.rgba() == pc.rgba(), "convertTo.sp1",
                "rgb(64,128,192)->Rgb", hex(qc.rgba()), hex(pc.rgba()));
        }
        // Hsv
        {
            QColor qc = q.convertTo(QColor::Hsv);
            pkoracle::PkColor pc = p.convertTo(pkoracle::PkColor::Hsv);
            bool same = int(qc.spec()) == int(pc.spec()) && qc.hue() == pc.hue()
                        && qc.saturation() == pc.saturation() && qc.value() == pc.value();
            rec("convertTo", same, "convertTo.sp2", "rgb(64,128,192)->Hsv",
                s(qc.hue()) + "," + s(qc.saturation()) + "," + s(qc.value()), s(pc.hue()) + "," + s(pc.saturation()) + "," + s(pc.value()));
        }
        // Hsl
        {
            QColor qc = q.convertTo(QColor::Hsl);
            pkoracle::PkColor pc = p.convertTo(pkoracle::PkColor::Hsl);
            bool same = int(qc.spec()) == int(pc.spec()) && qc.hslHue() == pc.hslHue()
                        && qc.hslSaturation() == pc.hslSaturation() && qc.lightness() == pc.lightness();
            rec("convertTo", same, "convertTo.sp4", "rgb(64,128,192)->Hsl",
                s(qc.hslHue()) + "," + s(qc.hslSaturation()) + "," + s(qc.lightness()), s(pc.hslHue()) + "," + s(pc.hslSaturation()) + "," + s(pc.lightness()));
        }
        // 登记偏离：范围表判 CMYK / ExtendedRgb 目标不实现 → pk 返回无效色，真 Qt 有效。
        QColor qc = q.convertTo(QColor::Cmyk);
        pkoracle::PkColor pc = p.convertTo(pkoracle::PkColor::Cmyk);
        rec("convertTo.cmyk", int(qc.spec()) == int(pc.spec()), "convertTo.cmyk.deviation",
            "rgb(64,128,192)->Cmyk", s(int(qc.spec())), s(int(pc.spec())));
        QColor qx = q.convertTo(QColor::ExtendedRgb);
        pkoracle::PkColor px = p.convertTo(pkoracle::PkColor::ExtendedRgb);
        rec("convertTo.xrgb", int(qx.spec()) == int(px.spec()), "convertTo.xrgb.deviation",
            "rgb(64,128,192)->ExtendedRgb", s(int(qx.spec())), s(int(px.spec())));
    }

    // ── 汇总 ───────────────────────────────────────────────────────────────
    for (const auto &kv : g_cover) std::printf("ORACLE-COVER %s %ld\n", kv.first.c_str(), kv.second);
    std::printf("ORACLE-XRGB cases=%ld maxFloatDev=%.6g maxIntDev=%ld\n",
                g_xrgbCases, g_xrgbMaxFloat, g_xrgbMaxInt);
    for (const auto &kv : g_tags) std::printf("DIFFTAG %s %ld\n", kv.first.c_str(), kv.second);
    std::printf("DIFF total=%ld mismatch=%ld\n", g_total, g_mismatch);
    return 0;   // 即使 mismatch>0 也退 0，判定归 reviewer
}
