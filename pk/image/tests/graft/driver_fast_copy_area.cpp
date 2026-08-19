// ─────────────────────────────────────────────────────────────────────────────
// 这不是 libs/image/kis_grid_interpolation_tools.h —— 是复刻 QImagePolygonOp::
// fastCopyArea 调用点形状的 **driver**（spec「试接怎么做」节的降级路径，
// R线-spec.md:173-196）。
//
// 挡住的真实原因：kis_grid_interpolation_tools.h 的 include 闭包（第 17-23 行）
// 包含 kis_algebra_2d.h / kis_four_point_interpolator_*.h / kis_iterator_ng.h /
// kis_random_sub_accessor.h / kis_painter.h / KisRegion.h —— 全是 kritaimage/
// kis-global 的头，依赖 QImage/QVector/KoColorSpace，在 R-15 范围内物理编不过。
// 测试文件 KisGridInterpolationToolsTest.cpp:330-338 的
// testQImagePolygonOpStructFastAreaCopy() 还用了 QImage(fileName)（PNG 解码，
// 岔路 A 已排除）+ TestUtil::fetchDataFileLazy（文件 IO）。
//
// 依赖墙指名：kritaimage 整库（782 文件平铺的 SHARED target，S-06 要处理的对象，
// 当前还没剥 Qt）+ PNG 文件解码（岔路 A 排除，归 impex 插件各自的 S 批次）。
// 两者都不在本任务 R-15 的 locks 内（pk/image/）。这堵墙要等 S-06 把 kritaimage
// 剥完 Qt、impex 用外部编解码库替换之后才会消失。
//
// 与真实调用点的**逐字**对应（kis_grid_interpolation_tools.h:355-418 + 511-523
// 的成员声明），替换与省略都逐条标注：
//   ① QImage → PkImage、QRect → PkRect、QPointF → PkPointF（compat 垫片的
//      #define，编译参数不是改动）
//   ② QVector<QRect> → PkVector<PkRect>（compat/QVector 的 #define）
//   ③ 省略：析构函数里的 KIS_SAFE_ASSERT_RECOVER_NOOP（debug 断言宏，非 QImage
//      API）、operator()/copyPreviousRects/finalize/setCanMergeRects（需要
//      QPolygonF/KisAlgebra2D/KisFourPointInterpolator/KisRegion，全是 QImage
//      之外的依赖）、m_epsilon（只在 operator() 里用）。fastCopyArea 本身一个
//      token 没动。
// ─────────────────────────────────────────────────────────────────────────────

#include "QImage"   // compat → PkImage，并传递 include QRect/QPoint/QSize（PkRect 等）
#include "QVector"  // compat → PkVector（m_rectsToCopy 的容器，lazy 分支用）

#include <cstdio>
#include <cstring>

// Qt 标量 typedef：qglobal.h 的 `typedef unsigned char uchar;` / `typedef unsigned
// int uint;`。真实 graft 里它们由 -include 的 QtGlobal 垫片提供（这批标量 typedef
// 归属 R-02 的「qint8..quint64」族，见 pk/geometry/PkRect.h 顶部注释，不是 QImage
// 方法、不归 R-15）。本 driver 不引完整 QtGlobal 垫片，本地补这两个——与
// qrgb_shim.h 同一性质，但它是**已归属的缺口**（R-02），不是 R-15 新增的待认领
// 缺口，所以只在 driver 里本地补，不进报告缺口清单。
typedef unsigned char uchar;
typedef unsigned int uint;

// 逐字照抄 kis_grid_interpolation_tools.h:355-418 的构造 + fastCopyArea（两个
// 重载），只按文件头注释③省略与 QImage 无关的成员/方法。
struct QImagePolygonOp
{
    QImagePolygonOp(const QImage &srcImage, QImage &dstImage,
                    const QPointF &srcImageOffset,
                    const QPointF &dstImageOffset)
        : m_srcImage(srcImage), m_dstImage(dstImage),
          m_srcImageOffset(srcImageOffset),
          m_dstImageOffset(dstImageOffset),
          m_srcImageRect(m_srcImage.rect()),
          m_dstImageRect(m_dstImage.rect())
    {
    }

    void fastCopyArea(QRect areaToCopy) {
        fastCopyArea(areaToCopy, m_canMergeRects);
    }

    void fastCopyArea(QRect areaToCopy, bool lazy) {
        if (lazy) {
            m_rectsToCopy.append(areaToCopy.adjusted(0, 0, -1, -1));
            return;
        }

        // only handling saved offsets
        QRect srcArea = areaToCopy.translated(-m_srcImageOffset.toPoint());
        QRect dstArea = areaToCopy.translated(-m_dstImageOffset.toPoint());

        srcArea = srcArea.intersected(m_srcImageRect);
        dstArea = dstArea.intersected(m_dstImageRect);

        // it might look pointless but it cuts off unneeded areas on both rects based on where they end up
        // since *I know* they are the same rectangle before translation
        // TODO: I'm pretty sure this logic is correct, but let's check it when I'm less sleepy
        QRect srcAreaUntranslated = srcArea.translated(m_srcImageOffset.toPoint());
        QRect dstAreaUntranslated = dstArea.translated(m_dstImageOffset.toPoint());

        QRect actualCopyArea = srcAreaUntranslated.intersected(dstAreaUntranslated);
        srcArea = actualCopyArea.translated(-m_srcImageOffset.toPoint());
        dstArea = actualCopyArea.translated(-m_dstImageOffset.toPoint());

        int bytesPerPixel = m_srcImage.sizeInBytes()/m_srcImage.height()/m_srcImage.width();

        int srcX = srcArea.left()*bytesPerPixel;
        int dstX = dstArea.left()*bytesPerPixel;

        for (int srcY = srcArea.top(); srcY <= srcArea.bottom(); ++srcY) {

            int dstY = dstArea.top() + srcY - srcArea.top();
            const uchar *srcLine = m_srcImage.constScanLine(srcY);
            uchar *dstLine = m_dstImage.scanLine(dstY);
            memcpy(dstLine + dstX, srcLine + srcX, srcArea.width()*bytesPerPixel);

        }
    }

    const QImage &m_srcImage;
    QImage &m_dstImage;
    QPointF m_srcImageOffset;
    QPointF m_dstImageOffset;

    QRect m_srcImageRect;
    QRect m_dstImageRect;

private:
    bool m_canMergeRects {false};
    QVector<QRect> m_rectsToCopy;
};

static int failures = 0;

static void checkGrid(const char *name, const QImage &img,
                      const unsigned int expected[3][4])
{
    bool ok = true;
    for (int y = 0; y < 3 && ok; ++y)
        for (int x = 0; x < 4 && ok; ++x)
            if (img.pixel(x, y) != expected[y][x]) ok = false;
    if (!ok) {
        ++failures;
        printf("  %-28s FAIL\n", name);
        printf("    actual:  ");
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 4; ++x) printf(" %08x", img.pixel(x, y));
            if (y < 2) printf(" /");
        }
        printf("\n    expected:");
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 4; ++x) printf(" %08x", expected[y][x]);
            if (y < 2) printf(" /");
        }
        printf("\n");
    } else {
        printf("  %-28s OK\n", name);
    }
}

int main()
{
    // 期望值全部来自真 Qt 探针（/tmp/graft_probe_bin，命令与原始输出见
    // task-5-report.md「真 Qt 探针」一节）：
    //   [B1-full-copy-offset0] ff000000 ff000001 ff000002 ff000003 /
    //                          ff000100 ff000101 ff000102 ff000103 /
    //                          ff000200 ff000201 ff000202 ff000203
    //   [B2-offset-x1-clip]    ff000001 ff000002 ff000003 ff000000 /
    //                          ff000101 ff000102 ff000103 ff000000 /
    //                          ff000201 ff000202 ff000203 ff000000

    QImage src(4, 3, QImage::Format_ARGB32);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 4; ++x)
            src.setPixel(x, y, 0xFF000000u | (uint(y) << 8) | uint(x));

    const unsigned int expectB1[3][4] = {
        { 0xFF000000u, 0xFF000001u, 0xFF000002u, 0xFF000003u },
        { 0xFF000100u, 0xFF000101u, 0xFF000102u, 0xFF000103u },
        { 0xFF000200u, 0xFF000201u, 0xFF000202u, 0xFF000203u },
    };
    const unsigned int expectB2[3][4] = {
        { 0xFF000001u, 0xFF000002u, 0xFF000003u, 0xFF000000u },
        { 0xFF000101u, 0xFF000102u, 0xFF000103u, 0xFF000000u },
        { 0xFF000201u, 0xFF000202u, 0xFF000203u, 0xFF000000u },
    };

    QImage dst1(4, 3, QImage::Format_ARGB32);
    dst1.fill(0xFF000000);
    {
        QImagePolygonOp op(src, dst1, QPointF(), QPointF());
        op.fastCopyArea(QRect(0, 0, 4, 3));
    }
    checkGrid("B1-full-copy-offset0", dst1, expectB1);

    QImage dst2(4, 3, QImage::Format_ARGB32);
    dst2.fill(0xFF000000);
    {
        QImagePolygonOp op(src, dst2, QPointF(0, 0), QPointF(1, 0));
        op.fastCopyArea(QRect(0, 0, 4, 3));
    }
    checkGrid("B2-offset-x1-clip", dst2, expectB2);

    printf("driver_fast_copy_area: %s (%d failure%s)\n",
           failures == 0 ? "PASS" : "FAIL", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
