#include "PkPoint.h"

#include <type_traits>

// ---------------------------------------------------------------------------
// PkPoint / PkPointF 的成员**全部是 inline/constexpr**（QPoint 也是：qpoint.h 里
// 没有一个成员定义在 .cpp 里，Q_CORE_EXPORT 只影响 QDataStream/QDebug 那几个
// 自由函数，而那三组归 R-02/R-12/R-08，本线不实现）。所以这个 TU 里没有函数
// 定义可放。
//
// 它不是空文件：下面这批 static_assert 是**只有在一个翻译单元里才能落地**的
// 断言，测的是「PkPoint 是不是真的 POD 且真的能在编译期求值」——
//   · 布局：sizeof / 标准布局 / 平凡可拷贝。替代品要能被 memcpy、能塞进
//     QImage 那种按字节搬运的路径，这三条一旦破了是静默的内存错误。
//     实测真 Qt 5.15.7：sizeof(QPoint)=8、sizeof(QPointF)=16。
//   · constexpr 能力：Qt 把这批成员标成 Q_DECL_CONSTEXPR / Q_DECL_RELAXED_CONSTEXPR，
//     调用点里有 `constexpr QPoint kOrigin(0,0);` 这类写法。少一个 constexpr
//     不是行为差异而是**编译不过**，单测（运行期）看不见，只有 static_assert 看得见。
// 这些断言放头文件会让每个包含者都付一遍编译代价，放这里只付一次。
// ---------------------------------------------------------------------------

// ── 布局 ────────────────────────────────────────────────────────────────
static_assert(sizeof(PkPoint) == 8, "PkPoint 必须是两个 int，实测 sizeof(QPoint)==8");
static_assert(sizeof(PkPointF) == 16, "PkPointF 必须是两个 double，实测 sizeof(QPointF)==16");
static_assert(std::is_standard_layout<PkPoint>::value, "PkPoint 必须是标准布局");
static_assert(std::is_standard_layout<PkPointF>::value, "PkPointF 必须是标准布局");
static_assert(std::is_trivially_copyable<PkPoint>::value, "PkPoint 必须可 memcpy");
static_assert(std::is_trivially_copyable<PkPointF>::value, "PkPointF 必须可 memcpy");
static_assert(std::is_same<decltype(PkPointF().x()), qreal>::value, "PkPointF::x() 必须返回 qreal");
static_assert(std::is_same<decltype(PkPoint().x()), int>::value, "PkPoint::x() 必须返回 int");

// ── constexpr 能力（编译期求值 + 取值正确）────────────────────────────
static_assert(PkPoint().isNull(), "默认构造是 (0,0)");
static_assert(PkPoint(3, 4).x() == 3 && PkPoint(3, 4).y() == 4, "两参构造");
static_assert(PkPoint(-3, 4).manhattanLength() == 7, "实测 QPoint(-3,4).manhattanLength()==7");
static_assert(PkPoint(3, -7).transposed() == PkPoint(-7, 3), "transposed 交换两个分量");
static_assert(PkPoint(1, 1) == PkPoint(1, 1) && PkPoint(1, 1) != PkPoint(1, 2), "整数点用位相等");

// 放宽的 constexpr（C++14 起）：setX/rx 能在编译期改状态。
// Qt 用 Q_DECL_RELAXED_CONSTEXPR 标它们，调用点可以在 constexpr 函数体里改点。
static_assert([] { PkPoint p; p.setX(5); p.ry() = -2; return p; }() == PkPoint(5, -2),
              "setX/ry() 必须是放宽 constexpr，且 ry() 返回可写引用");

// ⚠ 取整方向：qRound 对负半值向 +∞，不是"远离零"。实测真 Qt 5.15.7：
// QPoint(-1,-1)*0.5 == (0,0)、QPoint(-3,-3)*0.5 == (-1,-1)、QPoint(-5,-5)*0.5 == (-2,-2)。
static_assert(PkPoint(-1, -1) * 0.5 == PkPoint(0, 0), "qRound(-0.5)==0");
static_assert(PkPoint(-3, -3) * 0.5 == PkPoint(-1, -1), "qRound(-1.5)==-1");
static_assert(0.5 * PkPoint(-5, -5) == PkPoint(-2, -2), "qRound(-2.5)==-2，且左乘同语义");
static_assert(PkPoint(-3, -3) / 2.0 == PkPoint(-1, -1), "除法同样走 qRound");

// PkPointF 的 constexpr 面。isNull() 照 Qt **不是** constexpr，故不在此列。
static_assert(PkPointF(1.5, -2.5).transposed().x() == -2.5, "PkPointF::transposed");
static_assert(PkPointF(-0.5, -0.5).toPoint() == PkPoint(0, 0), "实测 QPointF(-0.5,-0.5).toPoint()==(0,0)");
static_assert(PkPointF(0.5, 0.5).toPoint() == PkPoint(1, 1), "实测 QPointF(0.5,0.5).toPoint()==(1,1)");
static_assert(PkPointF(PkPoint(3, -4)).y() == -4.0, "PkPoint → PkPointF 隐式提升");
static_assert(std::is_convertible<PkPoint, PkPointF>::value, "提升必须是隐式的（构造函数非 explicit）");
static_assert(!std::is_convertible<PkPointF, PkPoint>::value, "反方向没有隐式转换，只有 toPoint()");
static_assert(PkPointF(1.0, 1.0) == PkPointF(1.0 + 1e-13, 1.0), "== 是模糊比较，实测真 Qt 为 true");
// dotProduct 是 constexpr 静态成员。实测 (3,4)·(5,-6) == -9、(1.5,-2.5)·(2,4) == -7。
static_assert(PkPoint::dotProduct(PkPoint(3, 4), PkPoint(5, -6)) == -9, "整数点积");
static_assert(PkPointF::dotProduct(PkPointF(1.5, -2.5), PkPointF(2.0, 4.0)) == -7.0, "浮点点积");
static_assert(!(PkPointF(1.0, 1.0) == PkPointF(1.0 + 1e-11, 1.0)), "超出相对阈值，实测为 false");
