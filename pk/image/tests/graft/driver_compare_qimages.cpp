// ─────────────────────────────────────────────────────────────────────────────
// 这不是 sdk/tests/qimage_test_util.h —— 是复刻 compareQImagesImpl 调用点形状的
// **driver**（spec「试接怎么做」节的降级路径，R线-spec.md:173-196）。
//
// 挡住的真实原因：sdk/tests/qimage_test_util.h 整个文件体在 `#ifdef FILES_OUTPUT_DIR`
// 里——不定义它时 -fsyntax-only 是空 TU（假通过），定义它时又撞 QString/QFile/
// QDir/QFileInfo/QApplication 这些未迁移类型。真实文件无法有意义地零改动编译，
// 故降级为 driver，只复刻 compareQImagesImpl 这个函数（不是 checkQImage 那些
// 文件 IO 函数）。
//
// 依赖墙指名：QString/QFile/QDir/QFileInfo/QApplication —— 分别归 R-13（字符串）、
// R-14（文件 IO）/ S 线。它们不在本任务 R-15 的 locks 内（pk/image/），这堵墙
// 要等那些模块剥完 Qt 之后才会消失，届时可补一次「编译真实 qimage_test_util.h」。
//
// 与真实调用点的**逐字**对应（qimage_test_util.h:85-165），只做两处已标注的
// 替换，其余一个 token 不动：
//   ① QImage → PkImage（compat/QImage 垫片的 #define，编译参数不是改动）
//   ② QPoint → PkPoint（compat/QRect 传递 include 的 #define）
//   ③ QRgb/qRed/qGreen/qBlue/qAlpha → qrgb_shim.h（待认领缺口⑤的脚手架）
//   ④ ⚠ `image.scanLine(y)` 的 const 重载 → `image.constScanLine(y)` —— 见下。
//
// ④ 是**真实 API 形状缺口**：真 Qt 的 QImage 有 `const uchar *scanLine(int) const`
// 这个 const 重载（qimage.h:226），compareQImagesImpl 拿 `const QImage&` 调
// `.scanLine(y)` 解析到的正是它；PkImage 只有非 const `scanLine(int)` 与
// `constScanLine(int) const`，**没有** const `scanLine(int) const`。Qt 里这两者
// 逐字节是同一个函数（qimage.h:226-227 都转发到 d->scanLine(i)），PkImage 只
// 暴露了 constScanLine 这一个名字。driver 用 constScanLine 拼写它，语义零差；
// 但这意味着 sdk/tests/ 下 20 个共用 compareQImagesImpl 的测试文件将来若走
// 「真实文件零改动 + 机械改名」试接，会卡在这一格——登记为「待认领缺口⑦」，
// 待后续任务给 PkImage 补 const scanLine 重载（一行转发）。
// ─────────────────────────────────────────────────────────────────────────────

#include "QImage"          // compat 垫片：QImage→PkImage，并传递 include QRect/QPoint/QSize
#include "qrgb_shim.h"     // 待认领缺口⑤：QRgb/qRed/qGreen/qBlue/qAlpha/qRgb

#include <cstdio>
#include <cstring>

// 逐字照抄 qimage_test_util.h:85-93（qAbs/qMax 来自 PkGlobal，经 compat/QRect
// → PkRect.h → PkGlobal.h 传递进来，R-03 已交付）。
static inline bool compareChannels(int ch1, int ch2, int fuzzy)
{
    return qAbs(ch1 - ch2) <= fuzzy;
}

static inline bool compareChannelsPremultiplied(int ch1, int alpha1, int ch2, int alpha2, int fuzzy, int fuzzyAlpha)
{
    return qAbs(ch1 * alpha1 - ch2 * alpha2) / 255 <= fuzzy * qMax(1, fuzzyAlpha);
}

// 逐字照抄 qimage_test_util.h:96-165（唯一替换见文件头注释④：scanLine 的 const
// 重载拼写为 constScanLine）。
static inline bool compareQImagesImpl(QPoint & pt, const QImage & image1, const QImage & image2, int fuzzy = 0, int fuzzyAlpha = 0, int maxNumFailingPixels = 0, bool showDebug = true, bool premultipliedMode = false)
{
    const int w1 = image1.width();
    const int h1 = image1.height();
    const int w2 = image2.width();
    const int h2 = image2.height();
    const int bytesPerLine = image1.bytesPerLine();

    if (w1 != w2 || h1 != h2) {
        pt.setX(-1);
        pt.setY(-1);
        return false;
    }

    int numFailingPixels = 0;

    for (int y = 0; y < h1; ++y) {
        const QRgb * const firstLine = reinterpret_cast<const QRgb *>(image2.constScanLine(y));
        const QRgb * const secondLine = reinterpret_cast<const QRgb *>(image1.constScanLine(y));

        if (memcmp(firstLine, secondLine, bytesPerLine) != 0) {
            for (int x = 0; x < w1; ++x) {
                const QRgb a = firstLine[x];
                const QRgb b = secondLine[x];

                bool same = false;

                if (!premultipliedMode) {
                    same =
                            compareChannels(qRed(a), qRed(b), fuzzy) &&
                            compareChannels(qGreen(a), qGreen(b), fuzzy) &&
                            compareChannels(qBlue(a), qBlue(b), fuzzy);
                } else {
                    same =
                            compareChannelsPremultiplied(qRed(a), qAlpha(a), qRed(b), qAlpha(b), fuzzy, fuzzyAlpha) &&
                            compareChannelsPremultiplied(qGreen(a), qAlpha(a), qGreen(b), qAlpha(b), fuzzy, fuzzyAlpha) &&
                            compareChannelsPremultiplied(qBlue(a), qAlpha(a), qBlue(b), qAlpha(b), fuzzy, fuzzyAlpha);
                }
                const bool sameAlpha = compareChannels(qAlpha(a), qAlpha(b), fuzzyAlpha);
                const bool bothTransparent = sameAlpha && qAlpha(a)==0;

                if (!bothTransparent && (!same || !sameAlpha)) {
                    pt.setX(x);
                    pt.setY(y);
                    numFailingPixels++;

                    if (numFailingPixels > maxNumFailingPixels) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

static int failures = 0;

static void check(const char *name, bool actual, bool expected, int ax, int ay, int ex, int ey)
{
    bool ok = (actual == expected) && (ax == ex) && (ay == ey);
    printf("  %-28s verdict=%d pt=(%d,%d)  expect verdict=%d pt=(%d,%d)  %s\n",
           name, actual, ax, ay, expected, ex, ey, ok ? "OK" : "FAIL");
    if (!ok) ++failures;
}

int main()
{
    // 期望值全部来自真 Qt 探针（/tmp/graft_probe_bin，命令与原始输出见
    // task-5-report.md「真 Qt 探针」一节）：
    //   [A1-identical]              verdict=1 pt=(-1,-2)
    //   [A2-one-pixel-blue-diff-fuzzy0] verdict=0 pt=(2,1)
    //   [A3-fuzzy100-covers]        verdict=1 pt=(-1,-2)
    //   [A4-size-mismatch]          verdict=0 pt=(-1,-1)
    //   [A5-premultiplied]          verdict=0 pt=(0,0)

    QImage image1(4, 3, QImage::Format_ARGB32);
    image1.fill(0xFF102030);
    QImage image2(4, 3, QImage::Format_ARGB32);
    image2.fill(0xFF102030);

    // 注意：compareQImagesImpl 会改 pt（out 参数），所以先求值再读 pt.x()/pt.y()
    // —— 把「读 pt」和「改 pt」塞进同一条函数调用的多个实参会撞 C++ 实参求值
    // 顺序未定义（GCC 从右往左，读 pt 发生在改 pt 之前），那是 harness 的 bug
    // 不是 driver 的。逐条拆开。
    QPoint pt(-1, -2);
    bool v = compareQImagesImpl(pt, image1, image2);
    check("A1-identical", v, true, pt.x(), pt.y(), -1, -2);

    image2.setPixel(2, 1, 0xFF102040);
    pt = QPoint(-1, -2);
    v = compareQImagesImpl(pt, image1, image2);
    check("A2-one-pixel-blue-diff-fuzzy0", v, false, pt.x(), pt.y(), 2, 1);

    pt = QPoint(-1, -2);
    v = compareQImagesImpl(pt, image1, image2, 100);
    check("A3-fuzzy100-covers", v, true, pt.x(), pt.y(), -1, -2);

    QImage image5(5, 3, QImage::Format_ARGB32);
    image5.fill(0xFF102030);
    pt = QPoint(-1, -2);
    v = compareQImagesImpl(pt, image1, image5);
    check("A4-size-mismatch", v, false, pt.x(), pt.y(), -1, -1);

    QImage image4(4, 3, QImage::Format_ARGB32);
    image4.fill(0xFFFFFFFF);
    QImage image6(4, 3, QImage::Format_ARGB32);
    image6.fill(0xFFFFFFFF);
    image6.setPixel(0, 0, 0x00000000);
    pt = QPoint(-1, -2);
    v = compareQImagesImpl(pt, image4, image6, 0, 0, 0, true, true);
    check("A5-premultiplied", v, false, pt.x(), pt.y(), 0, 0);

    printf("driver_compare_qimages: %s (%d failure%s)\n",
           failures == 0 ? "PASS" : "FAIL", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
