#include "PkSize.h"

#include <type_traits>

// ---------------------------------------------------------------------------
// 两个 scaled(const Pk*&, Qt::AspectRatioMode) 是**非 inline** 的 —— 与 Qt 的
// 形态一致（qsize.h 只声明，定义在 qsize.cpp 里，编进 libQt5Core）。
// 逐字抄自 qtbase v5.15.7-lts-lgpl 的 src/corelib/tools/qsize.cpp:225-244 与
// :666-685；装的 Qt 只有 .so 没有源，所以每条分支都另用探针核对过取值
//（探针输出见 Task 3 报告 §E/§F，一共 18+15 组输入 × 3 模式）。
//
// 本 TU 还放了那批**只有在一个翻译单元里才落得了地**的 static_assert，
// 理由与 PkPoint.cpp 相同（布局 / constexpr 能力 / noexcept 面）。
// ---------------------------------------------------------------------------

// qsize.cpp:225-244。⚠ 三个要点，少一个就换了行为：
//   ① `wd == 0 || ht == 0` 这条**短路分支**：源尺寸任一分量为 0 时直接返回目标
//      尺寸，不做任何比例运算（实测 QSize(0,0).scaled(30,30,Keep) == 30x30）。
//   ② 中间量是 **qint64**（本平台 = long long），不是 int：写成 int 会在
//      INT_MAX 一族输入上先溢出再比较，选错分支（单测 sizeScaledUsesInt64Intermediate
//      钉住 9 条）。**不引入 qint8..quint64 那批 typedef**——R-03 用量表没点名
//      它们，归 R-02；这里直接写 long long。
//   ③ 除法是**整数截断**，不是四舍五入（(3,7)→10x10 Keep 得 (4,10)）。
// 返回时 qint64 → int 的窄化按二补数回绕（-fwrapv 只管有符号溢出，
// 窄化是实现定义行为；两侧同一条指令，实测取值一致，README 覆盖度缺口有登记）。
PkSize PkSize::scaled(const PkSize &s, Qt::AspectRatioMode mode) const noexcept
{
    if (mode == Qt::IgnoreAspectRatio || wd == 0 || ht == 0) {
        return s;
    } else {
        bool useHeight;
        long long rw = (long long)(s.ht) * (long long)(wd) / (long long)(ht);

        if (mode == Qt::KeepAspectRatio) {
            useHeight = (rw <= s.wd);
        } else { // mode == Qt::KeepAspectRatioByExpanding
            useHeight = (rw >= s.wd);
        }

        if (useHeight) {
            return PkSize(rw, s.ht);
        } else {
            return PkSize(s.wd,
                          (int)((long long)(s.wd) * (long long)(ht) / (long long)(wd)));
        }
    }
}

// qsize.cpp:666-685。与整数版的三处不同：
//   ① 短路条件是 qIsNull(wd) || qIsNull(ht)，即 `== 0.0` —— **-0.0 也算**，
//      而次正规数（5e-324）**不算**（实测：它会正常参与比例运算）。
//   ② 除法是浮点除法，不截断。
//   ③ 不做任何 NaN/inf 特判：`rw <= s.wd` 在 NaN 上恒假，于是自动走 else 分支
//      （实测 QSizeF(nan,2).scaled(5,5,Keep) == (5,nan)）。
PkSizeF PkSizeF::scaled(const PkSizeF &s, Qt::AspectRatioMode mode) const noexcept
{
    if (mode == Qt::IgnoreAspectRatio || wd == 0.0 || ht == 0.0) {
        return s;
    } else {
        bool useHeight;
        qreal rw = s.ht * wd / ht;

        if (mode == Qt::KeepAspectRatio) {
            useHeight = (rw <= s.wd);
        } else { // mode == Qt::KeepAspectRatioByExpanding
            useHeight = (rw >= s.wd);
        }

        if (useHeight) {
            return PkSizeF(rw, s.ht);
        } else {
            return PkSizeF(s.wd, s.wd * ht / wd);
        }
    }
}

// ── 布局 ────────────────────────────────────────────────────────────────
static_assert(sizeof(PkSize) == 8, "PkSize 必须是两个 int，实测 sizeof(QSize)==8");
static_assert(sizeof(PkSizeF) == 16, "PkSizeF 必须是两个 double，实测 sizeof(QSizeF)==16");
static_assert(std::is_standard_layout<PkSize>::value, "PkSize 必须是标准布局");
static_assert(std::is_standard_layout<PkSizeF>::value, "PkSizeF 必须是标准布局");
static_assert(std::is_trivially_copyable<PkSize>::value, "PkSize 必须可 memcpy");
static_assert(std::is_trivially_copyable<PkSizeF>::value, "PkSizeF 必须可 memcpy");
static_assert(std::is_same<decltype(PkSize().width()), int>::value, "PkSize::width() 返回 int");
static_assert(std::is_same<decltype(PkSizeF().width()), qreal>::value, "PkSizeF::width() 返回 qreal");

// ── 枚举（qnamespace.h:1235-1239，实测 0/1/2）───────────────────────────
static_assert((int)Qt::IgnoreAspectRatio == 0, "实测真 Qt：IgnoreAspectRatio==0");
static_assert((int)Qt::KeepAspectRatio == 1, "实测真 Qt：KeepAspectRatio==1");
static_assert((int)Qt::KeepAspectRatioByExpanding == 2, "实测真 Qt：ByExpanding==2");

// ── constexpr 能力（编译期求值 + 取值正确）────────────────────────────
// ⚠ 默认构造是 (-1,-1)：这一条同时钉住"不是 (0,0)"。
static_assert(PkSize().width() == -1 && PkSize().height() == -1, "实测 QSize()==(-1,-1)");
static_assert(!PkSize().isValid() && PkSize().isEmpty() && !PkSize().isNull(), "默认尺寸无效且为空");
static_assert(PkSize(3, 4).width() == 3 && PkSize(3, 4).height() == 4, "两参构造");
// 三谓词互不等价，最险的一条在第三行：(0,0) 是 **valid**。
static_assert(PkSize(0, 0).isNull() && PkSize(0, 0).isEmpty() && PkSize(0, 0).isValid(),
              "实测 QSize(0,0)：null+empty+**valid**（与 QRect 相反）");
static_assert(!PkSize(10, 0).isNull() && PkSize(10, 0).isEmpty() && PkSize(10, 0).isValid(),
              "实测 QSize(10,0)：非 null、空、有效");
static_assert(!PkSize(-1, -1).isValid() && PkSize(-1, -1).isEmpty(), "实测 QSize(-1,-1)：空且无效");
static_assert(!PkSizeF(0.5, 0.5).isEmpty(), "⚠ 浮点版门槛是 <=0. 不是 <1：(0.5,0.5) 非空");
static_assert(PkSize(0, 0).isEmpty() && !PkSizeF(0.5, 0.5).isEmpty(),
              "整数版与浮点版的 isEmpty 公式不同，这一条把两者钉在一起");

// 放宽的 constexpr（C++14 起）：setWidth/rwidth 能在编译期改状态。
static_assert([] { PkSize s; s.setWidth(5); s.rheight() = -2; return s; }() == PkSize(5, -2),
              "setWidth/rheight() 必须是放宽 constexpr，且 rheight() 返回可写引用");

// qRound 的取整方向（负半值向 +∞）与 expandedTo 的 qMax
static_assert(PkSize(-1, -1) * 0.5 == PkSize(0, 0), "qRound(-0.5)==0");
static_assert(PkSize(-3, -3) * 0.5 == PkSize(-1, -1), "qRound(-1.5)==-1");
static_assert(0.5 * PkSize(5, 5) == PkSize(3, 3), "左乘同语义");
static_assert(PkSize(3, 7).expandedTo(PkSize(5, 2)) == PkSize(5, 7), "expandedTo 是 qMax 不是 qMin");
static_assert(PkSize(-3, -7).expandedTo(PkSize(-5, -2)) == PkSize(-3, -2), "负分量上的 qMax");

// PkSizeF 的 constexpr 面。isNull() 照 Qt **不是** constexpr，故不在此列。
static_assert(PkSizeF().width() == -1. && PkSizeF().height() == -1., "实测 QSizeF()==(-1,-1)");
static_assert(PkSizeF(PkSize(3, -4)).height() == -4.0, "PkSize → PkSizeF 隐式提升");
static_assert(std::is_convertible<PkSize, PkSizeF>::value, "提升必须是隐式的（构造函数非 explicit）");
static_assert(!std::is_convertible<PkSizeF, PkSize>::value, "反方向没有隐式转换，只有 toSize()");
static_assert(PkSizeF(-0.5, 0.5).toSize() == PkSize(0, 1), "实测 QSizeF(-0.5,0.5).toSize()==(0,1)");
static_assert(PkSizeF(2.5, -2.5).toSize() == PkSize(3, -2), "负半值向 +∞");
static_assert(PkSizeF(1.0, 1.0) == PkSizeF(1.0 + 1e-13, 1.0), "== 是模糊比较，实测为 true");
static_assert(!(PkSizeF(1.0, 1.0) == PkSizeF(1.0 + 1e-11, 1.0)), "超出相对阈值，实测为 false");
// ⚠ 与 PkPointF 相反的一条：零侧没有 fuzzyIsNull 分支。
static_assert(!(PkSizeF(0.0, 0.0) == PkSizeF(1e-300, 0.0)),
              "QSizeF 的 == 没有零分支，实测为 false（QPointF 上同样的值是 true）");
static_assert(PkSizeF(0.0, 0.0) == PkSizeF(0.0, 0.0), "两侧恰好都是 0 时相等");

// ── noexcept 面（qsize.h 逐个成员标了，qpoint.h 没有，所以只有这里查）──
static_assert(noexcept(PkSize()), "QSize() 是 noexcept");
static_assert(noexcept(PkSize(1, 1).isEmpty()), "isEmpty 是 noexcept");
static_assert(noexcept(PkSize(1, 1).scaled(PkSize(1, 1), Qt::KeepAspectRatio)), "scaled 是 noexcept");
static_assert(noexcept(PkSize(1, 1) * 2.0), "operator* 是 noexcept");
static_assert(!noexcept(PkSize(1, 1) / 2.0), "⚠ operator/ Qt 里没标 noexcept（带 Q_ASSERT）");
static_assert(!noexcept(PkSizeF(1., 1.) / 2.0), "浮点版的除法同样没标");
static_assert(noexcept(PkSizeF(1., 1.).toSize()), "toSize 是 noexcept");
