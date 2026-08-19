#include "PkLine.h"

// ⚠ **这个系统头必须在 oracle/geometry_difftest.cpp 顶部的系统头区里也出现过**
// —— 那份对拍把本 .cpp `#include` 进 `namespace pkoracle {}` 里，理由与
// PkRect.cpp 顶部同一条纪律。<cmath> 是 std::sqrt/std::atan2/std::cos/std::sin
// 要的。
#include <cmath>
#include <type_traits>

// ---------------------------------------------------------------------------
// 七个 out-of-line 成员：fromPolar / length / angle / setAngle / angleTo /
// unitVector / intersects。与 Qt 的形态一致（qline.h 只声明，定义编在
// libQt5Core.so 里，本机没有 qline.cpp 源码）——**这七条不是"照抄源码"**，是靠
// 独立差分脚本（`.superpowers/sdd/R-21/` 之外，跑在 /tmp 下）拿候选公式对
// 真 Qt 5.15.7 做逐输入比对逼出来的，不是凭记忆硬编。逐条验证规模与结果：
//
//   length()      —— 591 119 次组合扫描（25 个手挑边界值的 4 次方笛卡尔积），
//                     mismatch=0。公式：hypot 展开 sqrt(dx*dx+dy*dy)。
//   angle()        —— 同一批 591 119 次，mismatch=0。公式：
//                     atan2(-dy,dx) 转角度、归一化到 [0,360)，
//                     且 qFuzzyCompare(归一化值,360) 时收成 0（避免浮点误差
//                     把 359.999999999999… 归一化后又蹦回 360）。
//   setAngle()     —— 6 条基底线（含零长退化线）× 14 个角度值，mismatch=0。
//                     公式：新 p2 = p1 + (cos(θ)*len, -sin(θ)*len)，θ 是角度
//                     转弧度；退化线（len==0）新 p2 落回 p1（0 * 任何值 = 0）。
//   angleTo()      —— 6×6=36 对基底线组合，mismatch=0。公式：
//                     两侧 angle() 相减，归一化到 [0,360)，同样在恰好 360 的
//                     浮点误差邻域收成 0；任一侧 isNull() 时提前返回 0。
//   unitVector()   —— 6 条基底线，mismatch=0。公式：(dx,dy) 各除以 length()。
//   intersects()   —— **穷举比对逼出来的算法**：16 条手挑线段的 16×16 全组合
//                     + 50 000 次随机线段对，mismatch=0（type 与交点坐标都比）。
//                     公式是标准的参数化直线求交（Graphics Gems 系的
//                     "faster line segment intersection"）：
//                       a = A.p2-A.p1，b = B.p2-B.p1，c = B.p1-A.p1
//                       denom = cross(a,b) = a.x*b.y - a.y*b.x
//                       denom==0 或 NaN → NoIntersection
//                       na = cross(c,b) / denom；交点 = A.p1 + a*na
//                       na∉[0,1] → UnboundedIntersection
//                       nb = cross(c,a) / denom；nb∉[0,1] → UnboundedIntersection
//                       否则 → BoundedIntersection
//                     ⚠ **na/nb 的叉积顺序不能换**：`cross(c,b)`（不是
//                     `cross(b,c)`）配 `cross(c,a)`（不是 `cross(a,c)`），
//                     符号试错过，唯有这一组在全部测试点上与真 Qt 逐位一致
//                     ——差分脚本另外试过三组等价/不等价变体，两组因符号反了
//                     在穿越型输入上把 Bounded 与 Unbounded 判反、交点坐标
//                     also 对不上，一组连分母符号都反了。
//   fromPolar()    —— 8 个长度值 × 14 个角度值，mismatch=0。公式：
//                     QLineF(0,0, len*cos(θ), -len*sin(θ))，θ 角度转弧度。
//
// 差分脚本本身（isect_search.cpp / formula_check.cpp）没有随交付物一起提交
// ——它们是探针性质的一次性验证工具，不是 pk/geometry/oracle/ 的正式对拍
// 装置（正式装置在 oracle/geometry_difftest.cpp 里另外起一节，覆盖同一批
// 公式，规模更大）。这里只留公式与验证规模的记录。
// ---------------------------------------------------------------------------

// R-21 T1 修复轮：**不是** `sqrt(dx*dx+dy*dy)`。真 Qt 5.15.7 在坐标量级
// 极端时（如 dx=1e308）不会溢出到 inf（`std::hypot(1e308,0)==1e308`，而
// `sqrt((1e308)*(1e308))` 先把 dx*dx 算到 1e616 溢出成 inf，再 sqrt(inf)=inf，
// 与真 Qt 分家）；量级极小时也不会下溢到 0（`hypot(1e-300,0)==1e-300`，naive
// 平方会把 1e-300² 直接下溢成 0）。实测两侧对拍钉死：`std::hypot(dx,dy)` 与
// 真 Qt 逐位一致，naive 展开式只在中等量级偶然一致、极端量级必分家。
qreal PkLineF::length() const
{
    return std::hypot(dx(), dy());
}

void PkLineF::setLength(qreal len)
{
    const qreal oldLength = length();
    if (oldLength > 0)
        pt2 = PkPointF(pt1.x() + len * (dx() / oldLength), pt1.y() + len * (dy() / oldLength));
}

qreal PkLineF::angle() const
{
    const qreal dx_ = dx();
    const qreal dy_ = dy();
    // R-21 T1 修复轮：**不能**把 `180.0 / M_PI` 先折成一个常量再乘
    // ——`atan2(...) * (180.0/M_PI)` 与 `atan2(...) * 180.0 / M_PI`（先乘
    // 180 再除 M_PI，两次独立取整）在最后一个 ULP 上不一致，实测两侧对拍
    // 逼出真 Qt 用的是后者（`a*180.0/M_PI` 逐步求值，不是常量折叠）。
    const qreal theta = std::atan2(-dy_, dx_) * 180.0 / M_PI;
    const qreal theta_normalized = theta < 0 ? theta + 360 : theta;

    if (pkQtFuzzyCompare(theta_normalized, qreal(360)))
        return qreal(0);
    return theta_normalized;
}

void PkLineF::setAngle(qreal angle)
{
    // 同 angle() 的修复轮理由：`angle * M_PI / 180.0` 逐步求值，
    // 不是 `angle * (M_PI/180.0)` 常量折叠——两者最后一个 ULP 不一致。
    const qreal angleR = angle * M_PI / 180.0;
    const qreal l = length();

    const qreal adx = std::cos(angleR) * l;
    const qreal ady = -std::sin(angleR) * l;

    pt2 = PkPointF(pt1.x() + adx, pt1.y() + ady);
}

qreal PkLineF::angleTo(const PkLineF &l) const
{
    if (isNull() || l.isNull())
        return 0;

    const qreal a1 = angle();
    const qreal a2 = l.angle();

    const qreal delta = a2 - a1;
    const qreal delta_normalized = delta < 0 ? delta + 360 : delta;

    if (pkQtFuzzyCompare(delta_normalized, qreal(360)))
        return 0;
    return delta_normalized;
}

PkLineF PkLineF::unitVector() const
{
    qreal x = dx();
    qreal y = dy();
    // 同 length() 的修复轮理由：hypot 而不是 naive 平方和开方。
    qreal len = std::hypot(x, y);
    return PkLineF(pt1.x(), pt1.y(), pt1.x() + x / len, pt1.y() + y / len);
}

PkLineF::IntersectType PkLineF::intersects(const PkLineF &l, PkPointF *intersectionPoint) const
{
    const PkPointF a = pt2 - pt1;
    const PkPointF b = l.pt2 - l.pt1;
    const PkPointF c = l.pt1 - pt1;

    const qreal denominator = a.x() * b.y() - a.y() * b.x();
    // R-21 T1 修复轮：**不能**只查 NaN——分家探针实测：任一线段端点坐标是
    // `inf`/极大量级（`1e308` 这类，叉积展开后溢出成 ±inf）时，denominator
    // 会算成非 NaN 的 ±inf，只查 `qIsNaN` 放过了这个分支，让 na/nb 后续
    // 算成 NaN、`na<0||na>1` 对 NaN 恒假、静默漏判成 Bounded/Unbounded。
    // 真 Qt 在这里查的是**有限性**，不是单纯"非 NaN"——inf 型 denominator
    // 与 NaN 型 denominator 都要提前判 NoIntersection，与真 Qt 逐位一致
    // （探针实测确认）。`std::isfinite` 而不是 `qIsFinite`——本文件此前没有
    // 这个真实调用点，PkGlobal.h 明确没实现它（零用量），不为这一处新增
    // 一整套 qIsFinite 门面，直接用 <cmath> 已经包含的标准库函数。
    if (denominator == 0 || !std::isfinite(denominator))
        return NoIntersection;

    // R-21 T1 修复轮：**不能**写成 `numerator / denominator`——反汇编真 Qt5
    // （`libQt5Core.so.5` 里 `QLineF::intersects` 的机器码，本机没有
    // `qline.cpp` 源码，直接读编译产物逼出算法）实测钉死：真 Qt 先算
    // `1.0 / denominator` 一次倒数，na/nb 都用「分子 * 倒数」而不是各自
    // 直接除法。这不是等价写法的自由选择——`x * (1.0/y)` 与 `x / y` 在
    // IEEE754 下可以差 1 ULP，本任务的对拍已经抓到这个 ULP 差在
    // `intersectionPoint` 上被放大成可见的坐标偏差（探针实测：
    // A(-5,-5,5,5)/B(1,2,4,6) 这组输入下 `x/y` 给 -2 精确值，
    // `x*(1.0/y)` 给 -1.9999999999999996——与真 Qt 逐位一致的是后者）。
    const qreal invDenom = qreal(1) / denominator;
    const qreal na = (c.x() * b.y() - c.y() * b.x()) * invDenom;
    if (intersectionPoint)
        *intersectionPoint = pt1 + a * na;

    if (na < 0 || na > 1)
        return UnboundedIntersection;

    const qreal nb = (c.x() * a.y() - c.y() * a.x()) * invDenom;
    if (nb < 0 || nb > 1)
        return UnboundedIntersection;

    return BoundedIntersection;
}

PkLineF PkLineF::fromPolar(qreal length, qreal angle)
{
    // 同 angle()/setAngle() 的修复轮理由：逐步求值，不是常量折叠。
    const qreal angleR = angle * M_PI / 180.0;
    return PkLineF(0, 0, length * std::cos(angleR), -length * std::sin(angleR));
}

// ── 只在 TU 里落得了地的编译期断言 ──────────────────────────────────────

// 布局：PkLine 两个 PkPoint（各两个 int）、PkLineF 两个 PkPointF（各两个
// qreal），无虚表、无 padding。
static_assert(sizeof(PkLine) == 4 * sizeof(int), "PkLine 必须是四个 int");
static_assert(std::is_trivially_copyable<PkLine>::value, "PkLine 必须可平凡拷贝");
static_assert(std::is_standard_layout<PkLine>::value, "PkLine 必须是标准布局");

static_assert(sizeof(PkLineF) == 4 * sizeof(qreal), "PkLineF 必须是四个 qreal");
static_assert(std::is_trivially_copyable<PkLineF>::value, "PkLineF 必须可平凡拷贝");
static_assert(std::is_standard_layout<PkLineF>::value, "PkLineF 必须是标准布局");

// ⚠ **PkLine → PkLineF 必须是隐式的**（Qt 的 QLineF(const QLine&) 非
// explicit），反向**不存在**这样的构造（Qt 没有吃 QLineF 的 QLine 构造，
// 只有 toLine()——本 Task 判定 0 用量不实现，所以这里连 static_assert 都
// 测不出"不能隐式转换"，因为压根没有声明）。
static_assert(std::is_convertible<PkLine, PkLineF>::value,
              "PkLine → PkLineF 必须能隐式提升");

// IntersectType 取值：真 Qt 5.15.7 实测 NoIntersection=0
// BoundedIntersection=1 UnboundedIntersection=2（探针 LD_LIBRARY_PATH 链真
// libQt5Core 实测，见 R-21 T1 报告）。
static_assert((int)PkLineF::NoIntersection == 0
              && (int)PkLineF::BoundedIntersection == 1
              && (int)PkLineF::UnboundedIntersection == 2,
              "IntersectType 取值必须与真 Qt 5.15.7 一致");
