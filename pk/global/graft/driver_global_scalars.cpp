// ============================================================================
// driver_global_scalars.cpp —— R-18 判据② 目标②：标量调用点 driver（**降级路径**）。
//
// ⚠ **这不是真实测试文件。** 它是复刻 libs/global/KisLager.h、KisZug.h、
// kis_algebra_2d.h 真实调用点代码形状的 driver，因为环境凑不齐这三个头的依赖墙，
// 真实测试类编不过：
//
//   ① lager / zug / boost —— Krita 根 CMake 用 FetchContent vendored 的库，
//     本机不存在。KisLager.h 里 #include <lager/lenses.hpp>、<lager/reader.hpp>，
//     KisZug.h 里 #include <zug/transducer/map.hpp>、<zug/reducing/last.hpp>。
//   ② QPointF / QLineF / QRectF / QSizeF 几何类型 —— 属于 R-21/R-22（几何类型
//     迁移），R-18 的交付面里没有，而 kis_algebra_2d.h 依赖它们。
//
// 墙拆掉后（lager/zug 落地、几何类型交付），理论上可以直接让真实测试类编真实头
// （对应 libs/global/tests 下的 KisLagerTest / KisZugTest，以及依赖
// kis_algebra_2d.h 的几何测试），本 driver 退役。
//
// 四条要求（2026-08-18 裁决，全满足）：
//   1. **逐行复刻真实调用点的代码形状** —— 每个复刻块上方标着来源文件:行号，
//      函数名、参数类型/个数/顺序与真品一致。几何部分用 PkGraft* 替代
//      QPointF/QSizeF（依赖墙②），但 .x()/.y()/.width()/.height() 的返回类型
//      qreal 不变，qMin/qMax/qAbs 的实参形态因此与真品逐字一致。
//   2. **校验值来源分两类**：qRound(-1.5)==-1、qFuzzyCompare(1.0,
//      1.000000000001)==false 等来自 Task 2 探针（探针口径见
//      pk/global/oracle/global_difftest.cpp 的输入宇宙 kD[]）；
//      qMin/qMax/qAbs 取普通输入形态（4.0/7.0 不在 kD[] 里），语义已由 oracle
//      在 kD[] 全集上逐位对拍过，driver 只验调用形状。
//   3. 本文件显式标注替代品身份（就是这段注释）。
//   4. 指名依赖墙（上一段）。
//
// 标量来源：`#include <QtGlobal>` 与三个真实头一致（它们都直接或经几何头
// 传递 include <QtGlobal>），经 pk/global/compat 的超集链解析到 PkGlobal.h。
// ============================================================================
#include <QtGlobal>

#include <cstdio>

// ---------------------------------------------------------------------------
// 断言宏 —— 打印与 pk/test 同形态的证据行（PASS/FAIL/Totals），graft_run.sh
// 的 grep 按它们判定。
// ---------------------------------------------------------------------------
static int g_total = 0;
static int g_failed = 0;

#define PK_GRAFT_CHECK(cond, label) do {                                     \
    ++g_total;                                                               \
    if (cond) { std::printf("PASS: %s\n", label); }                          \
    else { std::printf("FAIL: %s\n", label); ++g_failed; }                   \
} while (0)

// ---------------------------------------------------------------------------
// KisLager.h:54-59 —— scale_int_to_real（复刻两个 lambda 的标量形状）。
//
// 真品（KisLager.h:54-59）：
//   constexpr auto scale_int_to_real = [] (qreal multiplier) {
//       return lager::lenses::getset(
//           [multiplier] (int value) { return value * multiplier; },
//           [multiplier] (int, qreal value) { return qRound(value / multiplier); }
//           );
//   };
//
// lager::lenses::getset 本身是依赖墙①（lager vendored 库），这里只复刻它
// 包装的那两个 lambda，get/set 分开命名，lambda 体逐字照抄。
// ---------------------------------------------------------------------------
constexpr auto pkGraftScaleIntToRealGet = [] (qreal multiplier) {
    return [multiplier] (int value) { return value * multiplier; };
};
constexpr auto pkGraftScaleIntToRealSet = [] (qreal multiplier) {
    return [multiplier] (int, qreal value) { return qRound(value / multiplier); };
};

// ---------------------------------------------------------------------------
// KisZug.h:53 —— map_equal<qreal>（复刻谓词 lambda 的形状）。
//
// 真品（KisZug.h:53）：
//   template <>
//   inline constexpr auto map_equal<qreal> =  [] (qreal value) { return zug::map([value](auto&& x) { return qFuzzyCompare(x, value); }); };
//
// zug::map 是依赖墙①（zug vendored 库），这里复刻它内层的谓词 lambda，
// 函数名/参数形态与真品逐字一致。
// ---------------------------------------------------------------------------
constexpr auto pkGraftMapEqualQreal = [] (qreal value) {
    return [value] (auto&& x) { return qFuzzyCompare(x, value); };
};

// ---------------------------------------------------------------------------
// KisZug.h:64 —— map_round（复刻变换 lambda 的形状）。
//
// 真品（KisZug.h:64）：
//   constexpr auto map_round = zug::map([](qreal x) -> int { return qRound(x); });
// ---------------------------------------------------------------------------
constexpr auto pkGraftMapRound = [] (qreal x) -> int { return qRound(x); };

// ---------------------------------------------------------------------------
// 几何调用形状的替代类型 —— 依赖墙②（QPointF/QLineF/QSizeF/QRectF 归 R-21/R-22）。
//
// 复刻目标只关心调用形状：qMin(corner1.x(), corner2.x()) 里的 corner1.x() 返回
// qreal（QPointF::x() 就是 qreal），maxDimension 的 size.width() 返回 qreal
// （QSizeF::width() 就是 qreal）。所以替代类型的这些成员都返回 qreal，qMin/
// qMax/qAbs 的实参类型因此与真品一致。
// ---------------------------------------------------------------------------
template <class T> struct PkGraftPointTypeTraits {};   // 替代 PointTypeTraits

struct PkGraftPointF   // 替代 QPointF（依赖墙②）
{
    qreal m_x;
    qreal m_y;
    qreal x() const { return m_x; }
    qreal y() const { return m_y; }
};

struct PkGraftRectF    // 替代 QRectF（依赖墙②）
{
    qreal m_x, m_y, m_w, m_h;
    PkGraftRectF(qreal x, qreal y, qreal w, qreal h) : m_x(x), m_y(y), m_w(w), m_h(h) {}
    qreal x() const { return m_x; }
    qreal y() const { return m_y; }
    qreal width() const { return m_w; }
    qreal height() const { return m_h; }
};

struct PkGraftSizeF    // 替代 QSizeF（依赖墙②）
{
    qreal m_w;
    qreal m_h;
    qreal width() const { return m_w; }
    qreal height() const { return m_h; }
};

template <> struct PkGraftPointTypeTraits<PkGraftPointF>
{
    typedef qreal value_type;
    typedef qreal calculation_type;
    typedef PkGraftRectF rect_type;
};

// ---------------------------------------------------------------------------
// kis_algebra_2d.h:321-326 —— createRectFromCorners（复刻模板形状）。
//
// 真品（kis_algebra_2d.h:321-326）：
//   template <class Point>
//   inline typename PointTypeTraits<Point>::rect_type
//   createRectFromCorners(Point corner1, Point corner2)
//   {
//       return typename PointTypeTraits<Point>::rect_type(qMin(corner1.x(), corner2.x()), qMin(corner1.y(), corner2.y()), qAbs(corner1.x() - corner2.x()), qAbs(corner1.y() - corner2.y()));
//   }
// ---------------------------------------------------------------------------
template <class Point>
inline typename PkGraftPointTypeTraits<Point>::rect_type
createRectFromCorners(Point corner1, Point corner2)
{
    return typename PkGraftPointTypeTraits<Point>::rect_type(
        qMin(corner1.x(), corner2.x()),
        qMin(corner1.y(), corner2.y()),
        qAbs(corner1.x() - corner2.x()),
        qAbs(corner1.y() - corner2.y()));
}

// ---------------------------------------------------------------------------
// kis_algebra_2d.h:337-345 —— maxDimension / minDimension（复刻模板形状）。
//
// 真品（kis_algebra_2d.h:337-345）：
//   template <class Size>
//   auto maxDimension(Size size) -> decltype(size.width()) {
//       return qMax(size.width(), size.height());
//   }
//   template <class Size>
//   auto minDimension(Size size) -> decltype(size.width()) {
//       return qMin(size.width(), size.height());
//   }
// ---------------------------------------------------------------------------
template <class Size>
auto maxDimension(Size size) -> decltype(size.width()) {
    return qMax(size.width(), size.height());
}

template <class Size>
auto minDimension(Size size) -> decltype(size.width()) {
    return qMin(size.width(), size.height());
}

int main()
{
    // ── KisLager.h:56-57 scale_int_to_real 的 get/set 调用形状 ───────────
    //    探针：qRound(-1.5) == -1（Task 2 探针值，kD[] 里有 -1.5）。
    {
        const auto get = pkGraftScaleIntToRealGet(2.0);
        const auto set = pkGraftScaleIntToRealSet(2.0);
        PK_GRAFT_CHECK(get(3) == 6.0,
            "KisLager.h:56 scale_int_to_real getter: (3 * 2.0) == 6.0");
        PK_GRAFT_CHECK(set(0, -3.0) == -1,
            "KisLager.h:57 scale_int_to_real setter: qRound(-3.0/2.0) == qRound(-1.5) == -1");
        PK_GRAFT_CHECK(set(0, 3.0) == 2,
            "KisLager.h:57 scale_int_to_real setter: qRound(3.0/2.0) == qRound(1.5) == 2");
    }

    // ── KisZug.h:53 map_equal<qreal> 的谓词调用形状 ──────────────────────
    //    探针：qFuzzyCompare(1.0, 1.000000000001) == false（Task 2 探针值，
    //    kD[] 里 1.000000000001 与 1.0 的相对差 1.0000889e-12 > 1e-12，超阈为假）。
    {
        const auto mapEq1 = pkGraftMapEqualQreal(1.0);
        PK_GRAFT_CHECK(mapEq1(1.0) == true,
            "KisZug.h:53 map_equal<qreal>: qFuzzyCompare(1.0, 1.0) == true");
        PK_GRAFT_CHECK(mapEq1(1.000000000001) == false,
            "KisZug.h:53 map_equal<qreal>: qFuzzyCompare(1.0, 1.000000000001) == false");
        PK_GRAFT_CHECK(mapEq1(1.0000005) == false,
            "KisZug.h:53 map_equal<qreal>: qFuzzyCompare(1.0, 1.0000005) == false");
    }

    // ── KisZug.h:64 map_round 的变换调用形状 ──────────────────────────────
    {
        PK_GRAFT_CHECK(pkGraftMapRound(-1.5) == -1,
            "KisZug.h:64 map_round: qRound(-1.5) == -1");
        PK_GRAFT_CHECK(pkGraftMapRound(1.5) == 2,
            "KisZug.h:64 map_round: qRound(1.5) == 2");
    }

    // ── kis_algebra_2d.h:325 createRectFromCorners 的调用形状 ────────────
    //    探针：qMin/qMax/qAbs 在 oracle 的 kD[] 全集上逐位对拍过；这里取
    //    (3.0, -2.0) / (4.0, 1.0) 两个普通形态 + 一个减法差。
    {
        PkGraftPointF corner1{3.0, 4.0};
        PkGraftPointF corner2{-2.0, 1.0};
        PkGraftRectF rect = createRectFromCorners(corner1, corner2);
        PK_GRAFT_CHECK(rect.x() == -2.0,
            "kis_algebra_2d.h:325 createRectFromCorners: qMin(3.0, -2.0) == -2.0");
        PK_GRAFT_CHECK(rect.y() == 1.0,
            "kis_algebra_2d.h:325 createRectFromCorners: qMin(4.0, 1.0) == 1.0");
        PK_GRAFT_CHECK(rect.width() == 5.0,
            "kis_algebra_2d.h:325 createRectFromCorners: qAbs(3.0 - (-2.0)) == 5.0");
        PK_GRAFT_CHECK(rect.height() == 3.0,
            "kis_algebra_2d.h:325 createRectFromCorners: qAbs(4.0 - 1.0) == 3.0");
    }

    // ── kis_algebra_2d.h:339/344 maxDimension / minDimension 的调用形状 ──
    {
        PkGraftSizeF size{3.0, 7.0};
        PK_GRAFT_CHECK(maxDimension(size) == 7.0,
            "kis_algebra_2d.h:339 maxDimension: qMax(3.0, 7.0) == 7.0");
        PK_GRAFT_CHECK(minDimension(size) == 3.0,
            "kis_algebra_2d.h:344 minDimension: qMin(3.0, 7.0) == 3.0");
    }

    std::printf("Totals: %d passed, %d failed, 0 skipped\n",
                g_total - g_failed, g_failed);
    return g_failed ? 1 : 0;
}
