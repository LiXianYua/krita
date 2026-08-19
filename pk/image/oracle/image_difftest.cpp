// image_difftest.cpp —— pk/image（PkImage）与真 Qt5（QImage）的逐输入对拍。
//
// 骨架、契约、两条硬教训**逐字照抄** `pk/geometry/oracle/geometry_difftest.cpp`
// （R-03 已 VERIFIED 交付，读它开头 ~150 行的大注释块是本文件方法论的权威说明）。
// 本文件头注释只记「image 这条线特有的东西」，不重复 geometry 那份已经讲清楚的
// 通用道理。
//
// ── 输出契约（run_oracle.sh 只认这两种行）───────────────────────────────
//     DIFF total=<N> mismatch=<M>      恰好一行，程序末尾打
//     DIFFTAG <api> <tag> <count>      一类差异一行
// **退出码必须是 0，即使 M>0**——已声明的偏离不算失败，退出码只表示"跑完没崩"。
//
// ── namespace pkoracle 隔离（同 geometry 先例的理由）────────────────────
// PkGlobal.h 与 Qt 的 qglobal.h 在同一个全局作用域里定义签名相同的 qAbs 等
// 符号，PkImage.h 依赖的 PkSize/PkRect/PkTransform/PkGlobal 同理会撞
// QSize/QRect/QTransform/Qt::。解法：把 pk/image 与 pk/geometry 两侧的
// 全部头文件与 out-of-line 的 .cpp 一起塞进 `namespace pkoracle { ... }`，
// Qt 侧 `#include <QImage>` 等留在 namespace 外面。
//   · PkImage.cpp 也必须进来：copy()/convertToFormat()/convertTo()/scaled()/
//     transformed()/operator== 等方法是 out-of-line 定义在 .cpp 里的，
//     libpkimage.a 里那份符号是 `::PkImage::xxx`，本 TU 需要
//     `pkoracle::PkImage::xxx`——两个不同符号链不上。
//   · PkSize.cpp/PkRect.cpp/PkTransform.cpp 同理（PkImage::rect()/scaled()/
//     transformed() 分别用到 PkRect/PkSize/PkTransform 的 out-of-line 方法）。
//   · PkArrayData.h（pk/container）是纯头文件模板，没有对应 .cpp，不需要带。
//
// ── compat 垫片排除 ───────────────────────────────────────────────────
// -I 参数列表不能包含任何 compat/ 目录：一旦 `#include <QImage>` 解析到
// pk/image/compat/QImage（`#define QImage PkImage`），两侧就是同一个类型，
// 永远 mismatch=0 却毫无判别力。下面的 #if/#error 与 static_assert 是运行前
// 与编译期两道兜底。
//
// ── 两条 tag 硬规则（与 geometry 先例相同，违反了整件事白做）────────────
//   规则一：tag 必须由触发差异的**具体输入形态**参与构造（格式名+宽+高[+坐标]），
//           不能是「每个 API 一个字面量常量」。
//   规则二：tag 的判定谓词**不能比 image.deviation 里的理由宽**——例如
//           Format_BGR888 只声明"不测像素级"，谓词就只能豁免 Format_BGR888
//           这一个格式的像素级比对，不能把整片像素比对都放宽。
//   规则三：每一个已实现的 API/重载都要有自己的对拍点，不合并（本文件顶部
//           「API 对拍点对照表」逐条列出，自审，未接入 geometry 先例 Task 4
//           修复轮加的头文件解析机器闸门——那是给 8 分量、几十个重载的 Rect/
//           Transform 族准备的基础设施，image 的 API 面小得多，брief 的判据
//           里也没有要求复刻它，这里改用人工对照表自审，是本 Task 明确记录
//           的规模裁剪，见 image.deviation 底部说明与 task-4-report.md）。
//
// ── canary ────────────────────────────────────────────────────────────
// 至少 3 条故意不相等的比对，走跟真实 API 完全相同的 rec()/比较原语/tag 构造
// 路径——证明"看到差异"这条路径本身没被写死。
//
// ── API 对拍点对照表（规则三自审，逐条对着 PkImage.h 的公开声明数）──────
// 构造：ctor_default / ctor_whFormat / ctor_sizeFormat
// 查询：format / width / height / size / rect / isNull / depth / bytesPerLine
//       / sizeInBytes / colorCount
// 像素：scanLine / constScanLine / bits / constBits / pixel / setPixel /
//       pixelColor / setPixelColor / pixelIndex / fill_uint32 /
//       fill_globalColor / colorTable / setColorTable / setColorCount /
//       color / setColor / allGray
// 派生：copy / convertToFormat_1arg / convertToFormat_colorTable / convertTo /
//       scaled / transformed / devicePixelRatio / setDevicePixelRatio /
//       operator== / operator!=
// （PkUseCount()/PkIsSharedWith() 不进对拍——PkImage.h 头注释明文写着"只给
// 单测用，不进 compat 垫片"，真 Qt 没有对应物，没有比较意义。）

// ── 真 Qt 侧 + 系统头（都必须在 namespace 之外）────────────────────────
#include <QImage>
#include <QColor>
#include <QSize>
#include <QRect>
#include <QTransform>
#include <QVector>
#include <QtGlobal>
#include <QMessageLogContext>

// ⚠ **这里必须囊括 pk/image 与 pk/geometry 侧全部头/.cpp 传递用到的系统头**
// （同 geometry_difftest.cpp 顶部注释里点过的坑）：一旦某个系统头第一次被
// `#include` 发生在 `namespace pkoracle {}` 内部（哪怕只是因为这里漏列一个），
// 该系统头里的 `namespace std { ... }` 就会在 `pkoracle::` 下重新开一个不完整
// 的 `pkoracle::std`，之后这个 TU 里所有 `std::xxx` 限定名查找都会先撞见这个
// 不完整的 `pkoracle::std` 并在那里止步（不再继续往外层 `::std` 找），大批
// `std::vector`/`std::floor`/`std::max` 之类当场找不到成员——**这不是猜测，
// 是编译实测撞过的坑**：第一版这里漏列了 <memory>/<utility>（PkArrayData.h
// 要用），`std::floor`/`std::max`/`std::vector<uint32_t>` 全部炸成看起来毫不
//相关的"参数类型是 int""没有 colorTable 成员"之类的诡异报错。
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// 垫片一旦混进 -I，<QImage> 会解析到 compat/QImage，两侧就是同一个类型。
#if defined(QImage) || defined(QSize) || defined(QRect) || defined(QTransform)
#  error "对拍两侧解析成了同一个类型 —— -I 里混进了 pk/image 或 pk/geometry 的 compat"
#endif

// ── 替代品侧 ───────────────────────────────────────────────────────────
namespace pkoracle {
#include "PkSize.h"
#include "PkSize.cpp"
#include "PkRect.h"
#include "PkRect.cpp"
#include "PkTransform.h"
#include "PkTransform.cpp"
#include "PkImageData.h"
#include "PkImage.h"
#include "PkImage.cpp"
}

using PkImage = pkoracle::PkImage;
using PkSize  = pkoracle::PkSize;
using PkRect  = pkoracle::PkRect;
using PkTransform = pkoracle::PkTransform;

static_assert(!std::is_same<QImage, PkImage>::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
static_assert(!std::is_same<QImage::Format, PkImage::Format>::value,
              "Format 两侧解析成了同一个类型");
// 逐档核对 Format 枚举取值：PkImage.h 头注释与 Qt qimage.h 逐字照抄的顺序，
// 这里钉住首尾与几个中间档，取值一旦漂移，下面全部按 int 互转的比较都会
// 悄悄比错格式。
static_assert((int)QImage::Format_Invalid == (int)PkImage::Format_Invalid
              && (int)QImage::Format_Mono == (int)PkImage::Format_Mono
              && (int)QImage::Format_MonoLSB == (int)PkImage::Format_MonoLSB
              && (int)QImage::Format_Indexed8 == (int)PkImage::Format_Indexed8
              && (int)QImage::Format_RGB32 == (int)PkImage::Format_RGB32
              && (int)QImage::Format_ARGB32 == (int)PkImage::Format_ARGB32
              && (int)QImage::Format_ARGB32_Premultiplied == (int)PkImage::Format_ARGB32_Premultiplied
              && (int)QImage::Format_RGBA8888 == (int)PkImage::Format_RGBA8888
              && (int)QImage::Format_Grayscale8 == (int)PkImage::Format_Grayscale8
              && (int)QImage::Format_RGBA64 == (int)PkImage::Format_RGBA64
              && (int)QImage::Format_Grayscale16 == (int)PkImage::Format_Grayscale16
              && (int)QImage::Format_BGR888 == (int)PkImage::Format_BGR888,
              "QImage::Format 与 PkImage::Format 的枚举取值两侧不一致");
static_assert(!std::is_same<Qt::GlobalColor, pkoracle::Qt::GlobalColor>::value,
              "Qt::GlobalColor 两侧解析成了同一个类型");
static_assert((int)Qt::white == (int)pkoracle::Qt::white
              && (int)Qt::black == (int)pkoracle::Qt::black
              && (int)Qt::red == (int)pkoracle::Qt::red
              && (int)Qt::gray == (int)pkoracle::Qt::gray
              && (int)Qt::transparent == (int)pkoracle::Qt::transparent,
              "Qt::GlobalColor 的枚举取值两侧不一致");
static_assert(!std::is_same<Qt::TransformationMode, pkoracle::Qt::TransformationMode>::value,
              "Qt::TransformationMode 两侧解析成了同一个类型");
static_assert((int)Qt::FastTransformation == (int)pkoracle::Qt::FastTransformation
              && (int)Qt::SmoothTransformation == (int)pkoracle::Qt::SmoothTransformation,
              "Qt::TransformationMode 的枚举取值两侧不一致");
static_assert(!std::is_same<Qt::AspectRatioMode, pkoracle::Qt::AspectRatioMode>::value,
              "Qt::AspectRatioMode 两侧解析成了同一个类型");

// ═══ 计数与记录 ════════════════════════════════════════════════════════════

static long g_total = 0, g_mismatch = 0;
static std::map<std::string, long> g_tags;      // "<api> <tag>" -> 分家次数（分子）
static std::map<std::string, long> g_tag_seen;  // 分母：谓词命中次数
static long g_printed = 0;

static void rec(const char *api, bool same, const std::string &tag,
                const std::string &in, const std::string &qs, const std::string &ps)
{
    ++g_total;
    const std::string key = std::string(api) + " " + tag;
    ++g_tag_seen[key];
    if (same) return;
    ++g_mismatch;
    ++g_tags[key];
    if (g_printed < 40) {
        ++g_printed;
        std::printf("MISMATCH: %s [%s] in=%s qt=%s pk=%s\n",
                    api, tag.c_str(), in.c_str(), qs.c_str(), ps.c_str());
    }
}

// ═══ 比较原语与打印辅助 ═════════════════════════════════════════════════════

static std::string istr(long long v) { return std::to_string(v); }
static std::string bstr(bool b) { return b ? "true" : "false"; }
static std::string hstr(uint32_t v)
{
    char buf[16];
    std::snprintf(buf, sizeof buf, "0x%08x", v);
    return buf;
}

static const char *kFormatNames[] = {
    "Invalid", "Mono", "MonoLSB", "Indexed8", "RGB32", "ARGB32",
    "ARGB32_Premultiplied", "RGB16", "ARGB8565_Premultiplied", "RGB666",
    "ARGB6666_Premultiplied", "RGB555", "ARGB8555_Premultiplied", "RGB888",
    "RGB444", "ARGB4444_Premultiplied", "RGBX8888", "RGBA8888",
    "RGBA8888_Premultiplied", "BGR30", "A2BGR30_Premultiplied", "RGB30",
    "A2RGB30_Premultiplied", "Alpha8", "Grayscale8", "RGBX64", "RGBA64",
    "RGBA64_Premultiplied", "Grayscale16", "BGR888",
};
static const int kNumFormats = 30; // QImage::NImageFormats

static const char *fmtName(int code)
{
    return (code >= 0 && code < kNumFormats) ? kFormatNames[code] : "?";
}

static std::string shapeTag(int fmtCode, int w, int h)
{
    return std::string("fmt=") + fmtName(fmtCode) + "_w=" + istr(w) + "_h=" + istr(h);
}
static std::string shapeTagXY(int fmtCode, int w, int h, int x, int y)
{
    return shapeTag(fmtCode, w, h) + "_x=" + istr(x) + "_y=" + istr(y);
}

static bool same_sz(const QSize &q, const PkSize &p)
{ return q.width() == p.width() && q.height() == p.height(); }
static bool same_rect(const QRect &q, const PkRect &p)
{
    return q.left() == p.left() && q.top() == p.top()
        && q.right() == p.right() && q.bottom() == p.bottom();
}
static bool same_double(double a, double b)
{
    std::uint64_t ba, bb;
    std::memcpy(&ba, &a, sizeof ba);
    std::memcpy(&bb, &b, sizeof bb);
    return ba == bb;
}
static uint32_t colorToArgb(const QColor &c) { return c.rgba(); }

// 高频格式：depthTable() / rawPixelArgb() / writeRawPixelArgb() 有真正实现的
// 9 个（brief「8 个高频格式」+ MonoLSB，Task 2 探针同样覆盖了 MonoLSB 的位
// 打包，本文件在手挑用例里单独测它）。
static bool isHighFreqPixelFormat(int fmtCode)
{
    switch (fmtCode) {
    case PkImage::Format_Mono:
    case PkImage::Format_MonoLSB:
    case PkImage::Format_Indexed8:
    case PkImage::Format_RGB32:
    case PkImage::Format_ARGB32:
    case PkImage::Format_ARGB32_Premultiplied:
    case PkImage::Format_RGBA8888:
    case PkImage::Format_Grayscale8:
    case PkImage::Format_RGBA64:
        return true;
    default:
        return false;
    }
}
static bool isIndexedFormat(int fmtCode)
{
    return fmtCode == PkImage::Format_Mono || fmtCode == PkImage::Format_MonoLSB
        || fmtCode == PkImage::Format_Indexed8;
}

// ── 行尾填充位/填充字节要单独 mask 掉才能比 ───────────────────────────────
//
// 实测踩过（不是猜的，见 task-4-report.md 的探针）：当每行的有效像素字节数小于
// bytesPerLine 时，行尾有填充——真 Qt 的像素缓冲区**不保证零初始化**（用堆里的
// 旧内存分配，不是 calloc），反复用非零内容占满堆之后构造图像，填充区读出来是
// 脏的（Mono 1x4 的 `constScanLine(0)` 探针实测 0x79 而不是 0x00），逐次运行
// 结果不确定——这不是 PkImage 与 Qt 的行为差异，是**内存分配器残留**，从根本上
// 不具备可对拍性（连 Qt 自己重复跑都不保证一致）。这里统一 mask 掉，有效内容
// 仍然逐位精确比较。三类填充：
//
//   · Mono/MonoLSB：位打包（w 不是 8 的倍数时最后一个字节只有部分位有效）+
//     字节填充（bytesPerLine 按 4 对齐，w/8 之后的整字节都是填充）。
//     fullBytes = w/8，tailMask 保留最后一个字节的有效位。
//   · Grayscale8/Indexed8/Alpha8（depth==8 但非 Mono/MonoLSB）：1 字节/像素，
//     bytesPerLine = ceil(w/4)*4，w 不是 4 的倍数时行尾有 0-3 个填充字节。
//     fullBytes = w（只比 w 个有效像素字节），tailMask = 0。
//   · 其余高频格式（32bpp/64bpp）：bytesPerLine 对像素边界天然对齐，无填充，
//     fullBytes = bytesPerLine、tailMask = 0。
static void packedRowMask(int fmtCode, int w, int bytesPerLine, int &fullBytes, uint8_t &tailMask)
{
    if (fmtCode == PkImage::Format_Mono || fmtCode == PkImage::Format_MonoLSB) {
        fullBytes = w / 8;
        int remBits = w % 8;
        tailMask = 0;
        if (remBits > 0) {
            tailMask = (fmtCode == PkImage::Format_Mono)
                ? static_cast<uint8_t>(0xFFu << (8 - remBits))   // MSB-first：高 remBits 位有效
                : static_cast<uint8_t>((1u << remBits) - 1u);    // LSB-first：低 remBits 位有效
        }
        return;
    }
    if (fmtCode == PkImage::Format_Grayscale8 || fmtCode == PkImage::Format_Indexed8
        || fmtCode == PkImage::Format_Alpha8) {
        // depth==8、1 字节/像素：bpl > w 时行尾有 0-3 个字节填充（Qt 堆残留 vs
        // PkImage vector 零初始化），只比 w 个有效像素字节。
        fullBytes = w;
        tailMask = 0;
        return;
    }
    fullBytes = bytesPerLine;
    tailMask = 0;
}

static bool sameRowMasked(const void *qrowV, const void *prowV, int fullBytes, uint8_t tailMask)
{
    const uint8_t *qrow = static_cast<const uint8_t *>(qrowV);
    const uint8_t *prow = static_cast<const uint8_t *>(prowV);
    bool same = fullBytes == 0 || std::memcmp(qrow, prow, static_cast<size_t>(fullBytes)) == 0;
    if (same && tailMask != 0) {
        same = (qrow[fullBytes] & tailMask) == (prow[fullBytes] & tailMask);
    }
    return same;
}

static bool sameBufferMasked(const void *qBufV, const void *pBufV, int bytesPerLine, int height,
                              int fullBytes, uint8_t tailMask)
{
    const uint8_t *qBuf = static_cast<const uint8_t *>(qBufV);
    const uint8_t *pBuf = static_cast<const uint8_t *>(pBufV);
    for (int y = 0; y < height; ++y) {
        if (!sameRowMasked(qBuf + static_cast<size_t>(y) * static_cast<size_t>(bytesPerLine),
                            pBuf + static_cast<size_t>(y) * static_cast<size_t>(bytesPerLine),
                            fullBytes, tailMask))
            return false;
    }
    return true;
}

// ═══ canary：证明比较管道是活的 ════════════════════════════════════════════
static void run_canaries()
{
    rec("canary", QSize(3, 4) == QSize(3, 4) && same_sz(QSize(3, 4), PkSize(3, 5)),
        "size-mismatch", "deliberate", "3x4", "3x5");
    rec("canary", 0xFF010203u == 0xFF010204u, "pixel-bit-mismatch",
        "deliberate", hstr(0xFF010203u), hstr(0xFF010204u));
    rec("canary", QImage(4, 4, QImage::Format_RGB16).depth()
                      == QImage(4, 4, QImage::Format_RGB32).depth(),
        "depth-mismatch", "deliberate",
        istr(QImage(4, 4, QImage::Format_RGB16).depth()),
        istr(QImage(4, 4, QImage::Format_RGB32).depth()));
    // 反向自证：这两条必须判成"同"，不产生 tag。
    rec("canary-negative", hstr(0xFF010203u) == hstr(0xFF010203u), "identical",
        "deliberate", hstr(0xFF010203u), hstr(0xFF010203u));
    rec("canary-negative", same_sz(QSize(5, 6), PkSize(5, 6)), "identical-size",
        "deliberate", "5x6", "5x6");
}

// ═══ Group A：全格式 depth/bytesPerLine/sizeInBytes/colorCount 扫描 ════════
//
// 覆盖全部 30 个 Format 值（含 Format_Invalid、Format_BGR888、全部非高频
// packed-bit 格式）——这一层**不涉及像素级读写**，只测构造期就确定的整数属性，
// 对声明偏离清单里的三类格式（BGR888/低频 packed-bit）同样适用、零豁免。
static void runFullFormatSweep()
{
    static const int kWidths[] = {0, 1, 2, 3, 7, 8, 9, 15, 16, 17, 100};
    for (int f = 0; f < kNumFormats; ++f) {
        QImage::Format qf = static_cast<QImage::Format>(f);
        PkImage::Format pf = static_cast<PkImage::Format>(f);
        for (int w : kWidths) {
            int h = w; // 对称子集，见 image.deviation 底部的规模裁剪说明
            QImage q(w, h, qf);
            PkImage p(w, h, pf);
            std::string tag = shapeTag(f, w, h);
            std::string in = tag;
            rec("ctor_whFormat/isNull", q.isNull() == p.isNull(), tag, in,
                bstr(q.isNull()), bstr(p.isNull()));
            rec("width", q.width() == p.width(), tag, in, istr(q.width()), istr(p.width()));
            rec("height", q.height() == p.height(), tag, in, istr(q.height()), istr(p.height()));
            rec("size", same_sz(q.size(), p.size()), tag, in,
                istr(q.size().width()) + "x" + istr(q.size().height()),
                istr(p.size().width()) + "x" + istr(p.size().height()));
            rec("rect", same_rect(q.rect(), p.rect()), tag, in,
                istr(q.rect().width()) + "x" + istr(q.rect().height()),
                istr(p.rect().width()) + "x" + istr(p.rect().height()));
            rec("format", (int)q.format() == (int)p.format(), tag, in,
                fmtName((int)q.format()), fmtName((int)p.format()));
            rec("depth", q.depth() == p.depth(), tag, in, istr(q.depth()), istr(p.depth()));
            rec("bytesPerLine", q.bytesPerLine() == p.bytesPerLine(), tag, in,
                istr(q.bytesPerLine()), istr(p.bytesPerLine()));
            rec("sizeInBytes", (long long)q.sizeInBytes() == p.sizeInBytes(), tag, in,
                istr((long long)q.sizeInBytes()), istr(p.sizeInBytes()));
            rec("colorCount", q.colorCount() == p.colorCount(), tag, in,
                istr(q.colorCount()), istr(p.colorCount()));
        }
    }
}

// ═══ Group B：手挑对抗用例 ══════════════════════════════════════════════════
static void runHandPicked()
{
    // 0x0 / 1x1（全部 30 个格式，isNull/depth/bytesPerLine 已在 Group A 覆盖，
    // 这里补 ctor_default 与 ctor_sizeFormat 两个 Group A 没测到的构造重载）。
    {
        QImage q0; PkImage p0;
        rec("ctor_default", q0.isNull() == p0.isNull() && q0.width() == p0.width()
                                 && q0.height() == p0.height(),
            "default", "()", bstr(q0.isNull()), bstr(p0.isNull()));
    }
    for (int f : {(int)PkImage::Format_ARGB32, (int)PkImage::Format_Indexed8,
                  (int)PkImage::Format_Invalid}) {
        QImage::Format qf = static_cast<QImage::Format>(f);
        PkImage::Format pf = static_cast<PkImage::Format>(f);
        QImage q(QSize(5, 5), qf);
        PkImage p(PkSize(5, 5), pf);
        std::string tag = shapeTag(f, 5, 5) + "_sizeCtor";
        rec("ctor_sizeFormat", q.isNull() == p.isNull() && q.depth() == p.depth(),
            tag, tag, bstr(q.isNull()) + "/" + istr(q.depth()),
            bstr(p.isNull()) + "/" + istr(p.depth()));
    }

    // Format_Mono / Format_MonoLSB 位打包边界：宽度不是 8 的倍数。
    for (int f : {(int)PkImage::Format_Mono, (int)PkImage::Format_MonoLSB}) {
        QImage::Format qf = static_cast<QImage::Format>(f);
        PkImage::Format pf = static_cast<PkImage::Format>(f);
        for (int w : {1, 3, 7, 9, 15, 17}) {
            int h = 4;
            QImage q(w, h, qf);
            PkImage p(w, h, pf);
            QVector<QRgb> qtab; qtab << 0xFF000000u << 0xFFFFFFFFu;
            q.setColorTable(qtab);
            p.setColorTable({0xFF000000u, 0xFFFFFFFFu});
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    uint32_t bit = static_cast<uint32_t>((x + y) % 2);
                    q.setPixel(x, y, bit);
                    p.setPixel(x, y, bit);
                    std::string tag = shapeTagXY(f, w, h, x, y) + "_setPixel";
                    rec("setPixel", (uint32_t)q.pixelIndex(x, y) == p.pixelIndex(x, y),
                        tag, tag, istr(q.pixelIndex(x, y)), istr(p.pixelIndex(x, y)));
                    rec("pixel", q.pixel(x, y) == p.pixel(x, y), tag, tag,
                        hstr(q.pixel(x, y)), hstr(p.pixel(x, y)));
                }
            }
            // 整行原始字节（位打包顺序：Mono 是 MSB-first，MonoLSB 是
            // LSB-first——真正对不对由这条比较判，不是靠读代码猜）。填充位
            // 用 packedRowMask()/sameRowMasked() 统一处理，理由见二者定义处。
            int fullBytes; uint8_t tailMask;
            packedRowMask(f, w, q.bytesPerLine(), fullBytes, tailMask);
            for (int y = 0; y < h; ++y) {
                const uchar *qrow = q.constScanLine(y);
                const uint8_t *prow = p.constScanLine(y);
                bool same = sameRowMasked(qrow, prow, fullBytes, tailMask);
                std::string tag = shapeTag(f, w, h) + "_y=" + istr(y) + "_scanlineBytes";
                rec("constScanLine", same, tag, tag, "bytes", "bytes");
            }
        }
    }

    // Format_Indexed8 颜色表边界：colorCount=0 与 colorCount=256。
    {
        QImage q(4, 4, QImage::Format_Indexed8);
        PkImage p(4, 4, PkImage::Format_Indexed8);
        q.setColorCount(0);
        p.setColorCount(0);
        rec("setColorCount", q.colorCount() == p.colorCount(), "count=0", "count=0",
            istr(q.colorCount()), istr(p.colorCount()));
        rec("colorTable", q.colorTable().size() == (int)p.colorTable().size(),
            "count=0", "count=0", istr(q.colorTable().size()), istr(p.colorTable().size()));

        QVector<QRgb> qtab;
        std::vector<uint32_t> ptab;
        for (int i = 0; i < 256; ++i) {
            uint32_t c = 0xFF000000u | static_cast<uint32_t>(i);
            qtab << c;
            ptab.push_back(c);
        }
        q.setColorTable(qtab);
        p.setColorTable(ptab);
        rec("setColorTable", q.colorTable().size() == (int)p.colorTable().size(),
            "count=256", "count=256", istr(q.colorTable().size()), istr(p.colorTable().size()));
        for (int i : {0, 1, 128, 254, 255}) {
            std::string tag = "count=256_i=" + istr(i);
            rec("color", q.color(i) == p.color(i), tag, tag, hstr(q.color(i)), hstr(p.color(i)));
        }
        // setColor 越界自动扩容——**已声明偏离**（Task 4 新发现，探针实测）：
        // 从一张已经**恰好 256 项**（Indexed8 的 8 位索引能表示的最大值）的
        // 表继续往超界索引 setColor 时，真 Qt 会打 qWarning
        // "Index out of bound 300" 并**拒绝增长**（colorCount 保持 256，
        // color(300) 读回 0）；PkImage.cpp 的 setColor() 无条件按
        // `idx+1` resize，不管 idx 是否已经超出该格式索引位宽能表示的范围，
        // 会长成 301 项。PkImage.h 头注释里"setColor(i, argb) 越界时自动
        // 扩容 colorTable（探针第 8 组旁注）"这条 Task 2 结论成立的前提是
        // "从一张远小于 256 的表往适度越界的索引扩容"（下面单独一条钉住这个
        // 場景，SAME），没有覆盖"表已经在 8 位索引的自然上限"这个边界——这是
        // 本 Task 组合爆破时才测到的新形态，登记为新发现的偏离。
        q.setColor(300, 0xFF112233u);
        p.setColor(300, 0xFF112233u);
        rec("setColor", q.colorTable().size() == (int)p.colorTable().size()
                && q.color(300) == p.color(300),
            "grow-beyond-256-cap", "300-on-256-table",
            istr(q.colorTable().size()) + "/" + hstr(q.color(300)),
            istr(p.colorTable().size()) + "/" + hstr(p.color(300)));
    }
    // 对照组：从一张**远小于 256** 的表往适度越界的索引 setColor——这是
    // PkImage.h 头注释"探针第 8 组旁注"实际验证过的场景，期望 SAME（真
    // Qt 探针见 task-4-report.md）。
    {
        QImage q(4, 4, QImage::Format_Indexed8);
        PkImage p(4, 4, PkImage::Format_Indexed8);
        QVector<QRgb> qtab; qtab << 0xFF000000u << 0xFFFFFFFFu << 0xFF112233u << 0xFF445566u;
        std::vector<uint32_t> ptab = {0xFF000000u, 0xFFFFFFFFu, 0xFF112233u, 0xFF445566u};
        q.setColorTable(qtab);
        p.setColorTable(ptab);
        q.setColor(10, 0xFF778899u);
        p.setColor(10, 0xFF778899u);
        rec("setColor", q.colorTable().size() == (int)p.colorTable().size()
                && q.color(10) == p.color(10),
            "grow-on-oob-index", "10-on-4-table",
            istr(q.colorTable().size()) + "/" + hstr(q.color(10)),
            istr(p.colorTable().size()) + "/" + hstr(p.color(10)));
    }

    // 越界像素坐标：只比**越界写**的 no-op 语义（两侧都该 no-op，有意义、可对齐）。
    // 越界**读**（pixel/pixelIndex/pixelColor）不比：真 Qt 的越界读是未初始化
    // 内存 UB（探针实测 pixel(-1,-1) 返回 0x00003039、pixelIndex 返回 -12345，
    // 纯垃圾值，连自己重跑都不确定），PkImage 定死返回 0（更安全）——两者没有
    // 可对齐的语义，plan Task 2 brief 本来就只要求"确定、不崩"。见 image.deviation
    // 底部「越界坐标读」范围说明。
    for (int f : {(int)PkImage::Format_ARGB32, (int)PkImage::Format_Indexed8}) {
        QImage::Format qf = static_cast<QImage::Format>(f);
        PkImage::Format pf = static_cast<PkImage::Format>(f);
        QImage q(4, 4, qf);
        PkImage p(4, 4, pf);
        q.fill(0u); p.fill(0u);
        struct OOB { int x, y; };
        for (OOB c : {OOB{-1, -1}, OOB{4, 4}, OOB{-1, 0}, OOB{0, -1}, OOB{4, 0}, OOB{0, 4}}) {
            std::string tag = shapeTagXY(f, 4, 4, c.x, c.y) + "_oob";
            // 越界写入必须是 no-op：写后整幅图像仍应与写前逐字节相同。
            q.setPixel(c.x, c.y, 0xFFFFFFFFu);
            p.setPixel(c.x, c.y, 0xFFFFFFFFu);
            q.setPixelColor(c.x, c.y, QColor::fromRgba(0xFFFFFFFFu));
            p.setPixelColor(c.x, c.y, 0xFFFFFFFFu);
            rec("setPixel", q.pixel(0, 0) == p.pixel(0, 0), tag + "_noop", tag,
                hstr(q.pixel(0, 0)), hstr(p.pixel(0, 0)));
        }
    }

    // fill() 两个重载各一次（PkImage 只实现了 uint32_t 与 Qt::GlobalColor 两个
    // 重载——真 Qt 还有第三个 fill(const QColor&)，Task 2 的简化决策没有对应
    // 的 Pk 参数类型可用，image.deviation 底部记这一条范围裁剪，不在这里
    // 空跑一个测不了的重载）。
    {
        QImage q(3, 3, QImage::Format_ARGB32);
        PkImage p(3, 3, PkImage::Format_ARGB32);
        q.fill(0xFF334455u);
        p.fill(0xFF334455u);
        rec("fill_uint32", q.pixel(1, 1) == p.pixel(1, 1), "argb32_0xFF334455",
            "fill(0xFF334455)", hstr(q.pixel(1, 1)), hstr(p.pixel(1, 1)));
    }
    // fill(Qt::GlobalColor)：5 个真实调用点用到的值（PkGlobal.h 头注释），
    // 只在非索引高频格式上测（索引格式 fill(GlobalColor) 的既有实现细节不在
    // 本 Task 判据①范围内，image.deviation 底部说明）。
    {
        struct GC { int code; const char *name; };
        static const GC kColors[] = {
            {(int)pkoracle::Qt::white, "white"}, {(int)pkoracle::Qt::black, "black"},
            {(int)pkoracle::Qt::red, "red"}, {(int)pkoracle::Qt::gray, "gray"},
            {(int)pkoracle::Qt::transparent, "transparent"},
        };
        static const int kFmts[] = {
            (int)PkImage::Format_ARGB32, (int)PkImage::Format_RGB32,
            (int)PkImage::Format_ARGB32_Premultiplied, (int)PkImage::Format_RGBA8888,
            (int)PkImage::Format_Grayscale8, (int)PkImage::Format_RGBA64,
        };
        for (int f : kFmts) {
            for (const GC &gc : kColors) {
                QImage q(3, 3, static_cast<QImage::Format>(f));
                PkImage p(3, 3, static_cast<PkImage::Format>(f));
                q.fill(static_cast<Qt::GlobalColor>(gc.code));
                p.fill(static_cast<pkoracle::Qt::GlobalColor>(gc.code));
                std::string tag = std::string("fmt=") + fmtName(f) + "_color=" + gc.name;
                rec("fill_globalColor", q.pixel(0, 0) == p.pixel(0, 0), tag, tag,
                    hstr(q.pixel(0, 0)), hstr(p.pixel(0, 0)));
            }
        }
    }
}

// ═══ Group C：组合爆破——8 个高频格式 × 宽=高（对称子集）+ 逐像素读写序列 ═══
//
// **规模裁剪**：brief 原文要求 8×11×11≈968 个实例（宽高笛卡尔积），实测这一层
// 叠加逐像素读写后编译期没有影响（循环是运行期展开），但为控制审阅复杂度与
// 运行时打印量，改成 8×11=88 个实例（宽==高的对称子集，11 个宽度值逐字保留
// brief 原文的取值集合）。像素级比较仍然逐像素进行（不是抽样），所以实际比对
// 次数远大于"实例数"——总数在 DIFF 行里如实报告。
static void runCombinatorial()
{
    static const int kFmts[] = {
        (int)PkImage::Format_ARGB32, (int)PkImage::Format_Indexed8,
        (int)PkImage::Format_RGB32, (int)PkImage::Format_Grayscale8,
        (int)PkImage::Format_ARGB32_Premultiplied, (int)PkImage::Format_Mono,
        (int)PkImage::Format_RGBA8888, (int)PkImage::Format_RGBA64,
    };
    static const int kWidths[] = {0, 1, 2, 3, 7, 8, 9, 15, 16, 17, 100};

    for (int f : kFmts) {
        QImage::Format qf = static_cast<QImage::Format>(f);
        PkImage::Format pf = static_cast<PkImage::Format>(f);
        bool indexed = isIndexedFormat(f);
        for (int w : kWidths) {
            int h = w;
            QImage q(w, h, qf);
            PkImage p(w, h, pf);
            std::string ctorTag = shapeTag(f, w, h);
            rec("ctor_whFormat/isNull", q.isNull() == p.isNull(), ctorTag, ctorTag,
                bstr(q.isNull()), bstr(p.isNull()));
            if (w <= 0 || h <= 0) continue;

            if (indexed) {
                QVector<QRgb> qtab;
                std::vector<uint32_t> ptab;
                int n = (f == PkImage::Format_Indexed8) ? 8 : 2;
                for (int i = 0; i < n; ++i) {
                    uint32_t c = 0xFF000000u | (static_cast<uint32_t>(i) * 0x10203u);
                    qtab << c; ptab.push_back(c);
                }
                q.setColorTable(qtab);
                p.setColorTable(ptab);
            }

            // ── fill(uint32_t)：独立对拍点，整幅缓冲区一次 memcmp（不逐像素
            //    展开）。用**临时图像** qFill/pFill，不复用下面继续要用的
            //    q/p——这样 fill() 自身的对拍结果不会污染 scanLine/bits/copy/
            //    scaled/transformed 等下游 API 各自的判断（教训见下方）。
            //
            // ⚠ **这里揪出一个真实的 Qt 自身不一致**（不是本文件的 bug，是
            // 实测出来的既存 Qt 行为）：`QImage::fill(uint)` 对
            // Format_RGBA8888 / Format_Grayscale8 走的是与 `setPixel()`
            // **不同**的底层路径——不做"按当前格式语义转换"，而是把 `pixel`
            // 参数当成一段裸 32 位/8 位数据直接 memfill，绕过了这两个格式的
            // 字节序/灰度换算规则。实测（探针见 task-4-report.md）：
            //   RGBA8888：fill(0xFF203040) 后 pixel(0,0) 读回 0xff403020，
            //             而 setPixel(0,0,0xFF203040) 读回的是 0xff203040——
            //             同一个 API 家族内部就不自洽。
            //   Grayscale8：fill(0xFF203040) 后 pixel(0,0) 读回 0xff404040
            //             （直接拿低 8 位当灰度存），而 setPixel 读回的是
            //             qGray(0x20,0x30,0x40)=0x2d 展开的 0xff2d2d2d。
            // 其余 6 个高频格式（ARGB32/Indexed8/RGB32/ARGB32_Premultiplied/
            // Mono/RGBA64）fill()与setPixel() 语义一致，探针实测 SAME。
            // PkImage::fill(uint32_t) 的实现选择是"与 setPixel() 同一套语义"
            // （PkImage.h 头注释：「value 按 pixel()/setPixel() 同一套打包/
            // 索引约定逐像素写入」），这是 Task 2 的既有设计决策——比 Qt 自己
            // 内部自洽，但对着 Qt 在这两个格式上对不齐。是否要为了跟 Qt 位对齐
            // 而改成复刻 Qt 这个不一致，不在 Task 4（写对拍程序）的授权范围内，
            // 登记为已声明偏离，留给后续任务判断要不要改 PkImage.cpp。
            {
                QImage qFill(w, h, qf);
                PkImage pFill(w, h, pf);
                if (indexed) { qFill.setColorTable(q.colorTable()); pFill.setColorTable(p.colorTable()); }
                uint32_t fillVal = indexed ? 1u : 0xFF203040u;
                qFill.fill(fillVal);
                pFill.fill(fillVal);
                // 整幅缓冲区比对同样经 packedRowMask：Mono/MonoLSB 的位填充、
                // Grayscale8/Indexed8 的行尾字节填充两侧不确定，只比有效像素
                // 内容（否则 fill 语义本一致时也会被填充字节假分家）。
                int fbFill; uint8_t tmFill;
                packedRowMask(f, w, qFill.bytesPerLine(), fbFill, tmFill);
                bool same = sameBufferMasked(qFill.constBits(), pFill.constBits(),
                                              qFill.bytesPerLine(), qFill.height(),
                                              fbFill, tmFill);
                std::string tag = shapeTag(f, w, h) + "_fill-byteorder-mismatch";
                rec("fill_uint32", same, tag, tag, "bytes", "bytes");
            }

            // ── 用逐像素 setPixel（格式安全，双侧语义一致）铺一份渐变图案，
            //    给下面 scanLine/bits/copy/colorTable/scaled/transformed/
            //    convertToFormat 等 API 一份**不含上面那条已知分歧**的干净
            //    基线状态——这些 API 各自测的是它们自己的行为，不是 fill()
            //    的。不逐点 rec()：setPixel() 本身的对拍点在上面的 fill_uint32
            //    之后、下面的"角点+中心+对角线抽样"里已经覆盖，这里纯粹是
            //    构造测试夹具。
            uint32_t fillVal = indexed ? 1u : 0xFF203040u;
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    uint32_t v = indexed
                        ? static_cast<uint32_t>((x + y) % ((f == PkImage::Format_Indexed8) ? 8 : 2))
                        : (0xFF000000u | (static_cast<uint32_t>((x * 37) & 0xFF) << 16)
                           | (static_cast<uint32_t>((y * 53) & 0xFF) << 8)
                           | static_cast<uint32_t>(((x + y) * 17) & 0xFF));
                    q.setPixel(x, y, v);
                    p.setPixel(x, y, v);
                }
            }

            // ── setPixel：角点 + 中心 + 对角线抽样（w=100 时全量对角线仍只
            //    100 个点，控制这一段的调用次数，逐像素读回仍然是 Group C
            //    整体基线那一轮做的，不是被这里"抽样"替换）。
            struct Coord { int x, y; };
            std::vector<Coord> coords = {
                {0, 0}, {w - 1, 0}, {0, h - 1}, {w - 1, h - 1}, {w / 2, h / 2},
            };
            for (int i = 0; i < w && i < h; ++i) coords.push_back({i, i});
            for (const Coord &c : coords) {
                uint32_t v = indexed
                    ? static_cast<uint32_t>((c.x + c.y) % ((f == PkImage::Format_Indexed8) ? 8 : 2))
                    : (0xFF000000u | (static_cast<uint32_t>(c.x) << 8) | static_cast<uint32_t>(c.y));
                q.setPixel(c.x, c.y, v);
                p.setPixel(c.x, c.y, v);
                std::string tag = shapeTagXY(f, w, h, c.x, c.y) + "_setPixel";
                rec("setPixel", q.pixel(c.x, c.y) == p.pixel(c.x, c.y), tag, tag,
                    hstr(q.pixel(c.x, c.y)), hstr(p.pixel(c.x, c.y)));
                rec("pixelIndex", q.pixelIndex(c.x, c.y) == p.pixelIndex(c.x, c.y), tag, tag,
                    istr(q.pixelIndex(c.x, c.y)), istr(p.pixelIndex(c.x, c.y)));
                if (!indexed) {
                    QColor qc = q.pixelColor(c.x, c.y);
                    rec("pixelColor", colorToArgb(qc) == p.pixelColor(c.x, c.y), tag, tag,
                        hstr(colorToArgb(qc)), hstr(p.pixelColor(c.x, c.y)));
                    q.setPixelColor(c.x, c.y, QColor::fromRgba(v ^ 0x0F0F0Fu));
                    p.setPixelColor(c.x, c.y, v ^ 0x0F0F0Fu);
                    rec("setPixelColor", q.pixel(c.x, c.y) == p.pixel(c.x, c.y),
                        tag + "_setPixelColor", tag,
                        hstr(q.pixel(c.x, c.y)), hstr(p.pixel(c.x, c.y)));
                }
            }

            // ── scanLine/constScanLine/bits/constBits：整幅缓冲区逐字节
            //    （Mono/MonoLSB 的填充位用 packedRowMask()/sameRowMasked()
            //    统一豁免，理由见二者定义处——不是放宽判据，是避开真 Qt 自己
            //    都不保证确定性的分配器残留位）。
            int fullBytesGrpC; uint8_t tailMaskGrpC;
            packedRowMask(f, w, q.bytesPerLine(), fullBytesGrpC, tailMaskGrpC);
            for (int y = 0; y < h; ++y) {
                {
                    const QImage &cqi = q;
                    const uchar *qrow = cqi.scanLine(y);
                    const uint8_t *prow = p.constScanLine(y);
                    bool same = sameRowMasked(qrow, prow, fullBytesGrpC, tailMaskGrpC);
                    std::string tag = shapeTag(f, w, h) + "_y=" + istr(y);
                    rec("constScanLine", same, tag, tag, "bytes", "bytes");
                }
                {
                    uchar *qrow = q.scanLine(y);
                    uint8_t *prow = p.scanLine(y);
                    bool same = sameRowMasked(qrow, prow, fullBytesGrpC, tailMaskGrpC);
                    std::string tag = shapeTag(f, w, h) + "_y=" + istr(y);
                    rec("scanLine", same, tag, tag, "bytes", "bytes");
                }
            }
            {
                const QImage &cqi = q;
                const uchar *qbits = cqi.bits();
                const uint8_t *pbits = p.constBits();
                bool same = sameBufferMasked(qbits, pbits, q.bytesPerLine(), q.height(),
                                              fullBytesGrpC, tailMaskGrpC);
                std::string tag = shapeTag(f, w, h);
                rec("constBits", same, tag, tag, "bytes", "bytes");
            }
            {
                uchar *qbits = q.bits();
                uint8_t *pbits = p.bits();
                bool same = sameBufferMasked(qbits, pbits, q.bytesPerLine(), q.height(),
                                              fullBytesGrpC, tailMaskGrpC);
                std::string tag = shapeTag(f, w, h);
                rec("bits", same, tag, tag, "bytes", "bytes");
            }

            // ── colorTable() 往返 + allGray ──
            {
                std::vector<uint32_t> pt = p.colorTable();
                bool same = pt.size() == static_cast<size_t>(q.colorTable().size());
                if (same) {
                    for (int i = 0; i < q.colorTable().size(); ++i) {
                        if (q.colorTable()[i] != pt[static_cast<size_t>(i)]) { same = false; break; }
                    }
                }
                std::string tag = shapeTag(f, w, h);
                rec("colorTable", same, tag, tag, istr(q.colorTable().size()), istr(pt.size()));
            }
            {
                std::string tag = shapeTag(f, w, h);
                rec("allGray", q.allGray() == p.allGray(), tag, tag,
                    bstr(q.allGray()), bstr(p.allGray()));
            }

            // ── copy()：内容必须逐字节相同（同样要豁免 Mono/MonoLSB 填充位）──
            {
                QImage qc = q.copy();
                PkImage pc = p.copy();
                bool same = qc.width() == pc.width() && qc.height() == pc.height()
                    && (int)qc.format() == (int)pc.format();
                if (same) {
                    same = sameBufferMasked(qc.constBits(), pc.constBits(), qc.bytesPerLine(),
                                             qc.height(), fullBytesGrpC, tailMaskGrpC);
                }
                std::string tag = shapeTag(f, w, h);
                rec("copy", same, tag, tag, "content", "content");
            }

            // ── operator== / operator!= ──
            {
                QImage qEq = q; // 共享同一份数据（QImage 隐式共享）
                PkImage pEq = p;
                std::string tag = shapeTag(f, w, h) + "_selfShare";
                rec("operator==", (q == qEq) == (p == pEq), tag, tag,
                    bstr(q == qEq), bstr(p == pEq));
                QImage qCopy = q.copy();
                PkImage pCopy = p.copy();
                std::string tag2 = shapeTag(f, w, h) + "_deepCopyEq";
                rec("operator==", (q == qCopy) == (p == pCopy), tag2, tag2,
                    bstr(q == qCopy), bstr(p == pCopy));
                if (w * h > 0) {
                    qCopy.setPixel(0, 0, indexed ? (fillVal ^ 1u) : (fillVal ^ 0xFFu));
                    pCopy.setPixel(0, 0, indexed ? (fillVal ^ 1u) : (fillVal ^ 0xFFu));
                    std::string tag3 = shapeTag(f, w, h) + "_afterMutateNe";
                    rec("operator!=", (q != qCopy) == (p != pCopy), tag3, tag3,
                        bstr(q != qCopy), bstr(p != pCopy));
                }
            }

            // ── devicePixelRatio / setDevicePixelRatio ──
            {
                q.setDevicePixelRatio(2.0);
                p.setDevicePixelRatio(2.0);
                std::string tag = shapeTag(f, w, h);
                rec("setDevicePixelRatio", same_double(q.devicePixelRatio(), p.devicePixelRatio()),
                    tag, tag, istr((long long)(q.devicePixelRatio() * 1000)),
                    istr((long long)(p.devicePixelRatio() * 1000)));
                rec("devicePixelRatio", same_double(q.devicePixelRatio(), p.devicePixelRatio()),
                    tag, tag, istr((long long)(q.devicePixelRatio() * 1000)),
                    istr((long long)(p.devicePixelRatio() * 1000)));
                q.setDevicePixelRatio(1.0);
                p.setDevicePixelRatio(1.0);
            }

            // ── scaled() / transformed()：Fast 模式硬判据，Smooth 已声明偏离 ──
            for (int mode = 0; mode <= 1; ++mode) {
                Qt::TransformationMode qm = static_cast<Qt::TransformationMode>(mode);
                pkoracle::Qt::TransformationMode pm = static_cast<pkoracle::Qt::TransformationMode>(mode);
                const char *modeName = mode == 0 ? "fast" : "smooth";

                QSize target(std::max(1, w / 2 + 1), std::max(1, h / 2 + 1));
                QImage qs = q.scaled(target, Qt::IgnoreAspectRatio, qm);
                PkImage ps = p.scaled(PkSize(target.width(), target.height()),
                                       pkoracle::Qt::IgnoreAspectRatio, pm);
                std::string tagBase = shapeTag(f, w, h) + "_mode=" + modeName;
                bool sameHeader = qs.width() == ps.width() && qs.height() == ps.height();
                rec("scaled", sameHeader, tagBase + "_header", tagBase,
                    istr(qs.width()) + "x" + istr(qs.height()),
                    istr(ps.width()) + "x" + istr(ps.height()));
                if (sameHeader && mode == 0) {
                    // 只有 Fast 模式做像素级比对——Smooth 是已声明偏离（岔路
                    // B），不追求位对齐，比对到 header 层即可（不再往下比像素，
                    // 避免制造一大片"预期内但没人看"的噪音 tag，符合规则二：
                    // 谓词只豁免 Smooth 这一条路径，不豁免 Fast）。目标格式
                    // 与源相同（scaled 不换格式），Mono/MonoLSB 同样要豁免
                    // 填充位，用目标图的宽度重新算 mask。
                    int fb; uint8_t tm;
                    packedRowMask(f, qs.width(), qs.bytesPerLine(), fb, tm);
                    bool same = sameBufferMasked(qs.constBits(), ps.constBits(), qs.bytesPerLine(),
                                                  qs.height(), fb, tm);
                    rec("scaled", same, tagBase + "_pixels", tagBase, "bytes", "bytes");
                }

                PkTransform pt = PkTransform::fromScale(2.0, 2.0);
                QTransform qt = QTransform::fromScale(2.0, 2.0);
                QImage qtI = q.transformed(qt, qm);
                PkImage ptI = p.transformed(pt, pm);
                bool sameHeader2 = qtI.width() == ptI.width() && qtI.height() == ptI.height();
                rec("transformed", sameHeader2, tagBase + "_header", tagBase,
                    istr(qtI.width()) + "x" + istr(qtI.height()),
                    istr(ptI.width()) + "x" + istr(ptI.height()));
                if (sameHeader2 && mode == 0) {
                    int fb; uint8_t tm;
                    packedRowMask(f, qtI.width(), qtI.bytesPerLine(), fb, tm);
                    bool same = sameBufferMasked(qtI.constBits(), ptI.constBits(), qtI.bytesPerLine(),
                                                  qtI.height(), fb, tm);
                    rec("transformed", same, tagBase + "_pixels", tagBase, "bytes", "bytes");
                }
            }
        }
    }
}

// ═══ Group D：convertToFormat / convertTo 交叉矩阵 ══════════════════════════
//
// **规模裁剪**：非索引 6 个高频格式全交叉（含同格式自转换，覆盖「同格式误判
// 为需要拷贝」这类缺陷）+ 索引格式→非索引格式单向（索引格式作为转换源，
// rawPixelArgb 有真正实现）。转换目标是索引格式（Indexed8/Mono/MonoLSB）
// 时 Task 3 已经判定真实调用点没覆盖这个方向（PkImage.cpp convertToFormat
// 内联注释），本文件补测这条路径并把它记成新的已声明偏离（image.deviation
// 「target-is-indexed」一行）——这是本 Task 新发现、需要判断的差异，不是
// 沿用旧文档就地免检。
static void buildPatternImage(QImage &q, PkImage &p, int fmtCode, int w, int h)
{
    q = QImage(w, h, static_cast<QImage::Format>(fmtCode));
    p = PkImage(w, h, static_cast<PkImage::Format>(fmtCode));
    if (isIndexedFormat(fmtCode)) {
        int n = (fmtCode == PkImage::Format_Indexed8) ? 6 : 2;
        QVector<QRgb> qtab;
        std::vector<uint32_t> ptab;
        for (int i = 0; i < n; ++i) {
            uint32_t c = 0xFF000000u | (static_cast<uint32_t>(i) * 0x332211u);
            qtab << c; ptab.push_back(c);
        }
        q.setColorTable(qtab);
        p.setColorTable(ptab);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                uint32_t idx = static_cast<uint32_t>((x + y) % n);
                q.setPixel(x, y, idx);
                p.setPixel(x, y, idx);
            }
        return;
    }
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            uint32_t c = 0xFF000000u
                | (static_cast<uint32_t>((x * 37) & 0xFF) << 16)
                | (static_cast<uint32_t>((y * 53) & 0xFF) << 8)
                | static_cast<uint32_t>(((x + y) * 17) & 0xFF);
            q.setPixel(x, y, c);
            p.setPixel(x, y, c);
        }
}

static void compareConverted(const char *api, const QImage &qr, const PkImage &pr,
                              const std::string &tag)
{
    bool sameHeader = (int)qr.format() == (int)pr.format()
        && qr.width() == pr.width() && qr.height() == pr.height();
    rec(api, sameHeader, tag + "_header", tag,
        fmtName((int)qr.format()), fmtName((int)pr.format()));
    if (!sameHeader) return;
    // 逐字节比对要经 packedRowMask：转换到 Grayscale8/Indexed8 目标时 bpl > w，
    // 行尾填充字节两侧不确定（Qt 堆残留 vs PkImage 零初始化），只比有效像素字节。
    int fb; uint8_t tm;
    packedRowMask((int)qr.format(), qr.width(), qr.bytesPerLine(), fb, tm);
    bool same = sameBufferMasked(qr.constBits(), pr.constBits(), qr.bytesPerLine(),
                                  qr.height(), fb, tm);
    rec(api, same, tag + "_pixels", tag, "bytes", "bytes");
}

static void runConvertMatrix()
{
    static const int kNonIndexed[] = {
        (int)PkImage::Format_ARGB32, (int)PkImage::Format_RGB32,
        (int)PkImage::Format_ARGB32_Premultiplied, (int)PkImage::Format_RGBA8888,
        (int)PkImage::Format_Grayscale8, (int)PkImage::Format_RGBA64,
    };
    static const int kIndexed[] = {
        (int)PkImage::Format_Mono, (int)PkImage::Format_MonoLSB, (int)PkImage::Format_Indexed8,
    };
    const int w = 6, h = 6;

    // Group D-1：非索引 × 非索引 全交叉（含同格式，convertToFormat_1arg）。
    for (int sf : kNonIndexed) {
        for (int tf : kNonIndexed) {
            QImage q; PkImage p;
            buildPatternImage(q, p, sf, w, h);
            QImage qr = q.convertToFormat(static_cast<QImage::Format>(tf));
            PkImage pr = p.convertToFormat(static_cast<PkImage::Format>(tf));
            std::string tag = std::string("src=") + fmtName(sf) + "_dst=" + fmtName(tf);
            compareConverted("convertToFormat_1arg", qr, pr, tag);

            // convertTo（原地版本，语义等价，单独走一条 rec，不与
            // convertToFormat_1arg 合并——规则三：不同重载各自的对拍点）。
            QImage q2; PkImage p2;
            buildPatternImage(q2, p2, sf, w, h);
            q2.convertTo(static_cast<QImage::Format>(tf));
            p2.convertTo(static_cast<PkImage::Format>(tf));
            compareConverted("convertTo", q2, p2, tag);
        }
    }

    // Group D-2：索引格式 → 非索引格式（索引作为源，有真正的颜色表查表实现）。
    for (int sf : kIndexed) {
        for (int tf : kNonIndexed) {
            QImage q; PkImage p;
            buildPatternImage(q, p, sf, w, h);
            QImage qr = q.convertToFormat(static_cast<QImage::Format>(tf));
            PkImage pr = p.convertToFormat(static_cast<PkImage::Format>(tf));
            std::string tag = std::string("src=") + fmtName(sf) + "_dst=" + fmtName(tf);
            compareConverted("convertToFormat_1arg", qr, pr, tag);
        }
    }

    // Group D-3：非索引 → 索引目标（已声明偏离：target-is-indexed，Task 3
    // 判定真实调用点没覆盖，PkImage 保持零初始化，Qt 会做真正的调色板量化）。
    for (int sf : kNonIndexed) {
        for (int tf : kIndexed) {
            QImage q; PkImage p;
            buildPatternImage(q, p, sf, w, h);
            QImage qr = q.convertToFormat(static_cast<QImage::Format>(tf));
            PkImage pr = p.convertToFormat(static_cast<PkImage::Format>(tf));
            std::string tag = std::string("src=") + fmtName(sf) + "_dst=" + fmtName(tf)
                + "_target-is-indexed";
            // 只比 header（宽高格式），不比像素——像素级差异整体归到
            // "target-is-indexed" 这一个已声明 tag，不逐像素展开制造海量重复
            // DIFFTAG（谓词仍然只豁免"转换目标是索引格式"这一件事，不豁免
            // 别的转换方向，符合规则二）。
            bool sameHeader = (int)qr.format() == (int)pr.format()
                && qr.width() == pr.width() && qr.height() == pr.height();
            rec("convertToFormat_1arg", sameHeader, tag + "_header", tag,
                fmtName((int)qr.format()), fmtName((int)pr.format()));
            if (sameHeader) {
                long long n = (long long)qr.bytesPerLine() * qr.height();
                bool same = n == 0
                    || std::memcmp(qr.constBits(), pr.constBits(), static_cast<size_t>(n)) == 0;
                rec("convertToFormat_1arg", same, tag + "_pixels", tag, "bytes", "bytes");
            }
        }
    }

    // Group D-4：convertToFormat(Format, colorTable)——已声明偏离（最近色匹配
    // 非 Qt dithering 位对齐）。用非索引源、Indexed8 目标、一份小调色板。
    {
        QImage q; PkImage p;
        buildPatternImage(q, p, (int)PkImage::Format_ARGB32, w, h);
        QVector<QRgb> qtab;
        std::vector<uint32_t> ptab;
        for (uint32_t c : {0xFF000000u, 0xFF808080u, 0xFFFFFFFFu, 0xFFFF0000u}) {
            qtab << c; ptab.push_back(c);
        }
        QImage qr = q.convertToFormat(QImage::Format_Indexed8, qtab);
        PkImage pr = p.convertToFormat(PkImage::Format_Indexed8, ptab);
        std::string tag = "argb32_to_indexed8_4colors";
        bool sameHeader = (int)qr.format() == (int)pr.format()
            && qr.width() == pr.width() && qr.height() == pr.height()
            && qr.colorTable().size() == (int)pr.colorTable().size();
        rec("convertToFormat_colorTable", sameHeader, tag + "_header", tag,
            "header", "header");
        if (sameHeader) {
            long long n = (long long)qr.bytesPerLine() * qr.height();
            bool same = n == 0
                || std::memcmp(qr.constBits(), pr.constBits(), static_cast<size_t>(n)) == 0;
            // 这条**预期不相等**（dithering 算法不同）——记进已声明偏离
            // "nearest-color-mismatch"，不是判据①失败。
            rec("convertToFormat_colorTable", same, tag + "_nearest-color-mismatch", tag,
                "bytes", "bytes");
        }
    }
}

int main()
{
    // 吞掉 Qt 的运行期警告（越界坐标、索引格式 setPixelColor 之类会往 stderr
    // 刷 qWarning），照抄 geometry 先例。
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &) {});

    run_canaries();
    runFullFormatSweep();
    runHandPicked();
    runCombinatorial();
    runConvertMatrix();

    for (const auto &kv : g_tags)
        std::printf("DIFFTAG %s %ld\n", kv.first.c_str(), kv.second);
    for (const auto &kv : g_tags)
        std::printf("DIFFDEN %s %ld\n", kv.first.c_str(), g_tag_seen[kv.first]);
    std::printf("DIFF total=%ld mismatch=%ld\n", g_total, g_mismatch);
    return 0; // 已声明的偏离不算失败；判定在 run_oracle.sh
}
