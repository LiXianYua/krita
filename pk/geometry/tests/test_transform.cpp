#include "cases/transform_case.h"
#include "../PkTransform.h"

#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <limits>
#include <type_traits>

// PkTestBinder<PkTransformCase> 由 pk_test_moc.py 生成，像 Qt moc 输出一样直接
// #include 进本 TU（理由与 test_rectf.cpp 相同）。
#include "pk_binder_transform_case.inc"

// ---------------------------------------------------------------------------
// 期望值全部取自**真 Qt 5.15.7** 的实测输出（探针链
// /mnt/ssd-disk/liyang/projects/krita-ci-env/_install 的 libQt5Core + libQt5Gui，
// QT_VERSION_STR "5.15.7"，`-DQT_NO_DEBUG`），
// 不是"3x3 矩阵乘法当然是这样"这类直觉。这一族里直觉错得最多的四处：
//   · **行向量约定**：x' = m11*x + m21*y + m31（不是 m11*x + m12*y + m13）
//   · **枚举是位标志**：TxNone=0 TxTranslate=1 TxScale=2 TxRotate=4
//     TxShear=8 TxProject=16 —— 不是 0..5
//   · **type() 有状态**：同一个矩阵、同一串操作，中间问过一次 type() 与没问过，
//     答案不同（TxProject vs TxNone）。这不是缓存优化，是可观测语义
//   · **map 的四个重载分两族**：map(点) 不夹持 w，map(指针出参) 夹持到 1e-6
//
// ⚠ 本文件里几乎所有量是 double，而 **PK_COMPARE 对 double 走的是 pk/test 的
// 模糊比较（相对 1e-12），不是位相等**（R-11 harness 的能力边界，写进 README
// 覆盖度缺口）。所以凡是要主张"位一致"（±0.0、NaN、直角特判的**精确** 0）的
// 断言一律用 PK_VERIFY + 下面的 sameD()/mAre()，不用 PK_COMPARE。
//   —— 直角特判那一条尤其要紧：rotate(90) 的 m11 与 rotateRadians(pi/2) 的
//   6.123233995736766e-17 在 PK_COMPARE 眼里……不，它们其实分得开（相对误差
//   无穷大），但 rotate(180) 的 m21 = -0.0 与 +0.0 在 PK_COMPARE 眼里是一样的，
//   而那正是"照抄了 -sina"的唯一证据。
// ---------------------------------------------------------------------------

namespace {

const double kInf = std::numeric_limits<double>::infinity();
const double kNaN = std::numeric_limits<double>::quiet_NaN();

// **位精确**比较：`==` 会把 +0.0/-0.0 判等、把 NaN 判不等，两者都不是我们要的。
bool sameD(double a, double b)
{
    std::uint64_t ba, bb;
    std::memcpy(&ba, &a, sizeof ba);
    std::memcpy(&bb, &b, sizeof bb);
    if (ba == bb) return true;
    return (a != a) && (b != b);               // 都是 NaN 就算同
}

// 九个分量一起按位断言。**刻意不走 operator==**：那个自己也在被测（而且它用的是
// 裸 `==`，会把 +0/-0 判等），拿它当检查手段会让一整类差异自己把自己藏起来。
bool mAre(const PkTransform &t,
          double m11, double m12, double m13,
          double m21, double m22, double m23,
          double m31, double m32, double m33)
{
    return sameD(t.m11(), m11) && sameD(t.m12(), m12) && sameD(t.m13(), m13)
        && sameD(t.m21(), m21) && sameD(t.m22(), m22) && sameD(t.m23(), m23)
        && sameD(t.m31(), m31) && sameD(t.m32(), m32) && sameD(t.m33(), m33);
}

bool coordsAre(const PkRect &r, int x, int y, int w, int h)
{
    return r.x() == x && r.y() == y && r.width() == w && r.height() == h;
}

bool fieldsAre(const PkRectF &r, double x, double y, double w, double h)
{
    return sameD(r.x(), x) && sameD(r.y(), y)
        && sameD(r.width(), w) && sameD(r.height(), h);
}

// 一个真投影矩阵（m13/m23 非 0），下面多处复用。
PkTransform proj()
{
    return PkTransform(1, 0, 0.5, 0, 1, 0.25, 0, 0, 1);
}

} // namespace

// ═══ 枚举与布局 ═══════════════════════════════════════════════════════════

void PkTransformCase::transformEnumIsBitFlagsNotZeroToFive()
{
    // 实测真 Qt 5.15.7：TxNone=0 TxTranslate=1 TxScale=2 TxRotate=4
    // TxShear=8 TxProject=16。照 `enum { TxNone, TxTranslate, ... }` 的顺序写
    // 会得到 0,1,2,3,4,5 —— 下面四条会红，而且 qMax(thisType, otherType)
    // 与 `t <= TxTranslate` 这类比较会跟着全错。
    PK_COMPARE((int)PkTransform::TxNone, 0);
    PK_COMPARE((int)PkTransform::TxTranslate, 1);
    PK_COMPARE((int)PkTransform::TxScale, 2);
    PK_COMPARE((int)PkTransform::TxRotate, 4);
    PK_COMPARE((int)PkTransform::TxShear, 8);
    PK_COMPARE((int)PkTransform::TxProject, 16);

    // 严格递增是 isAffine()/isIdentity()/qMax 的地基。
    PK_VERIFY(PkTransform::TxNone < PkTransform::TxTranslate);
    PK_VERIFY(PkTransform::TxTranslate < PkTransform::TxScale);
    PK_VERIFY(PkTransform::TxScale < PkTransform::TxRotate);
    PK_VERIFY(PkTransform::TxRotate < PkTransform::TxShear);
    PK_VERIFY(PkTransform::TxShear < PkTransform::TxProject);
}

void PkTransformCase::transformLayoutIsNineQrealPlusCache()
{
    // ⚠ **这里刻意不与 QTransform 比 sizeof**（Point/Size/Rect 三族都比了）：
    // 实测 sizeof(QTransform) == 88，其中 8 个字节是 Qt5 尾部那个恒为 nullptr
    // 的 `Private *d`（Qt6 已删）。它不经任何 API 露出来，本类不留，
    // 于是 sizeof 是 80 = 9 个 double + 两个 5 位位域所在的那个 unsigned + 对齐。
    // 登记在 PkTransform.h 头部与 README 偏离清单。
    // 9 个 double（72）+ 两个 5 位位域挤在一个 unsigned 里（4）+ 补齐到 8 的倍数
    // = 80，比 QTransform 的 88 少的正好是那个指针。
    PK_COMPARE(sizeof(PkTransform), (std::size_t)80);
    PK_VERIFY(sizeof(PkTransform) == sizeof(qreal) * 9 + sizeof(qreal));
    PK_VERIFY(alignof(PkTransform) == alignof(qreal));
    PK_VERIFY(std::is_trivially_copyable<PkTransform>::value);
    PK_VERIFY(std::is_trivially_destructible<PkTransform>::value);
    // 有位域 + private 数据成员，所以**不是** standard_layout 的必然要求；
    // 只钉平凡可复制（那才是"编译器生成的拷贝 == Qt 那份 memcpy"的依据）。
}

// ═══ 三个构造 ═════════════════════════════════════════════════════════════

void PkTransformCase::transformDefaultIsIdentity()
{
    const PkTransform d;
    PK_VERIFY(mAre(d, 1, 0, 0, 0, 1, 0, 0, 0, 1));
    PK_COMPARE((int)d.type(), (int)PkTransform::TxNone);
    PK_VERIFY(d.isIdentity());
    PK_VERIFY(d.isAffine());
    PK_COMPARE(d.determinant(), 1.0);
}

void PkTransformCase::transformNineArgCtorTakesRowsOfThree()
{
    // 实测：QTransform(1,2,3,4,5,6,7,8,9) →
    //   m11=1 m12=2 m13=3 | m21=4 m22=5 m23=6 | m31=7 m32=8 m33=9
    // 也就是**按行**填：(h11,h12,h13) (h21,h22,h23) (h31,h32,h33)。
    const PkTransform t(1, 2, 3, 4, 5, 6, 7, 8, 9);
    PK_VERIFY(mAre(t, 1, 2, 3, 4, 5, 6, 7, 8, 9));
    PK_COMPARE((int)t.type(), (int)PkTransform::TxProject);
    PK_VERIFY(!t.isAffine());

    // 第九个参数默认 1.0（不是 0）。
    const PkTransform u(1, 2, 3, 4, 5, 6, 7, 8);
    PK_VERIFY(mAre(u, 1, 2, 3, 4, 5, 6, 7, 8, 1));
}

void PkTransformCase::transformSixArgCtorLeavesProjectiveRowIdentity()
{
    // 实测：QTransform(2,3,5,7,11,13) →
    //   m11=2 m12=3 m13=0 | m21=5 m22=7 m23=0 | m31=11 m32=13 m33=1
    // ⚠ 六个参数的顺序是 (m11,m12,m21,m22,dx,dy) —— **不是** 前两行。
    const PkTransform t(2, 3, 5, 7, 11, 13);
    PK_VERIFY(mAre(t, 2, 3, 0, 5, 7, 0, 11, 13, 1));
    PK_COMPARE((int)t.type(), (int)PkTransform::TxShear);
    PK_VERIFY(t.isAffine());

    // 六参构造出的单位阵 type() 是 TxNone（重算从 TxShear 那一档进，一路贯穿）。
    PK_COMPARE((int)PkTransform(1, 0, 0, 1, 0, 0).type(), (int)PkTransform::TxNone);
}

void PkTransformCase::transformCtorsSeedDifferentDirtyLevels()
{
    // 三个构造的 m_dirty 初值不同（TxNone / TxProject / TxShear），
    // 而 type() 的重算是**从 m_dirty 那一档往下贯穿**的 —— 抄错了就看得见。
    // 探针方式：造一个只有 m33 != 1 的矩阵。
    //   · 九参构造 m_dirty=TxProject → 会检查 m33 → TxProject
    //   · 六参构造摸不到 m33（恒为 1），所以这一档在六参上不可达 ——
    //     用 setMatrix（同样 m_dirty=TxProject）走一遍，取值必须一致
    const PkTransform nine(1, 0, 0, 0, 1, 0, 0, 0, 3);
    PK_COMPARE((int)nine.type(), (int)PkTransform::TxProject);

    PkTransform viaSet(1, 0, 0, 1, 0, 0);            // 六参：先是 TxNone
    PK_COMPARE((int)viaSet.type(), (int)PkTransform::TxNone);
    viaSet.setMatrix(1, 0, 0, 0, 1, 0, 0, 0, 3);     // m_dirty 抬回 TxProject
    PK_COMPARE((int)viaSet.type(), (int)PkTransform::TxProject);
}

// ═══ 行向量约定 ═══════════════════════════════════════════════════════════

void PkTransformCase::transformMapUsesRowVectorConvention()
{
    // 实测（探针 §I 与 probe2 §3）：
    //   QTransform(2,3,5,7,11,13).map(QPointF(1,0)) == (13,16)
    //   .map(QPointF(0,1)) == (16,20)   .map(QPointF(1,1)) == (18,23)
    // 行向量：x' = m11*x + m21*y + m31 = 2*1 + 5*0 + 11 = 13
    //         y' = m12*x + m22*y + m32 = 3*1 + 7*0 + 13 = 16
    // **列向量约定（x' = m11*x + m12*y + m13）会给 (14,18)，是错的。**
    // 矩阵刻意非对称：对称矩阵上两种约定给同一个答案，看不出破绽。
    const PkTransform t(2, 3, 5, 7, 11, 13);
    const PkPointF a = t.map(PkPointF(1, 0));
    PK_VERIFY(sameD(a.x(), 13.0) && sameD(a.y(), 16.0));
    const PkPointF b = t.map(PkPointF(0, 1));
    PK_VERIFY(sameD(b.x(), 16.0) && sameD(b.y(), 20.0));
    const PkPointF c = t.map(PkPointF(1, 1));
    PK_VERIFY(sameD(c.x(), 18.0) && sameD(c.y(), 23.0));

    const PkPoint i = t.map(PkPoint(1, 0));
    PK_VERIFY(i.x() == 13 && i.y() == 16);
}

void PkTransformCase::transformGetterNamesMatchStorage()
{
    // m31/m32 与 dx/dy 是**同一对存储**的两个名字（Qt 里都返回 affine._dx/_dy）。
    const PkTransform t(2, 3, 5, 7, 11, 13);
    PK_VERIFY(sameD(t.m31(), t.dx()));
    PK_VERIFY(sameD(t.m32(), t.dy()));
    PK_VERIFY(sameD(t.dx(), 11.0));
    PK_VERIFY(sameD(t.dy(), 13.0));
}

void PkTransformCase::transformDeterminantIsThirdOrderExpansion()
{
    // 实测：determinant(1,2,3,4,5,6,7,8,9) == 0（那个矩阵奇异），identity == 1。
    PK_COMPARE(PkTransform(1, 2, 3, 4, 5, 6, 7, 8, 9).determinant(), 0.0);
    PK_COMPARE(PkTransform().determinant(), 1.0);
    // ⚠ 这是**三阶**行列式，不是 QMatrix 那个二阶的 m11*m22-m12*m21：
    // 拿一个二阶式为 0 而三阶式非 0 的矩阵分开它们。
    //   二阶：1*1 - 0*0 = 1；换成 m11=0,m22=0,m12=1,m21=1 → 二阶 = -1
    // 直接用投影阵：m13 参与三阶式而不参与二阶式。
    const PkTransform p(1, 0, 2, 0, 1, 0, 0, 0, 1);   // m13=2
    // 三阶展开：m11*(m33*m22 - m32*m23) - m21*(m33*m12 - m32*m13)
    //           + m31*(m23*m12 - m22*m13) = 1*(1*1-0*0) - 0 + 0 = 1
    PK_COMPARE(p.determinant(), 1.0);
    const PkTransform q(1, 0, 0, 0, 1, 0, 2, 0, 1);   // m31=2 参与三阶式
    PK_COMPARE(q.determinant(), 1.0);
}

// ═══ type() 的六档与 dot 判据 ═════════════════════════════════════════════

void PkTransformCase::transformTypeLadderCoversAllSixLevels()
{
    // 六档实测值（probe2 §4），一档一行。
    PK_COMPARE((int)PkTransform().type(), (int)PkTransform::TxNone);
    PK_COMPARE((int)PkTransform::fromTranslate(5, 7).type(), (int)PkTransform::TxTranslate);
    PK_COMPARE((int)PkTransform::fromScale(2, 3).type(), (int)PkTransform::TxScale);
    { PkTransform t; t.rotate(30); PK_COMPARE((int)t.type(), (int)PkTransform::TxRotate); }
    { PkTransform t; t.shear(1, 0); PK_COMPARE((int)t.type(), (int)PkTransform::TxShear); }
    PK_COMPARE((int)proj().type(), (int)PkTransform::TxProject);

    // isAffine / isIdentity 就是这条阶梯上的两条切线。
    PK_VERIFY(PkTransform().isIdentity());
    PK_VERIFY(!PkTransform::fromTranslate(5, 7).isIdentity());
    PK_VERIFY(PkTransform::fromTranslate(5, 7).isAffine());
    PK_VERIFY(!proj().isAffine());

    // m33 != 1 也算投影（不只是 m13/m23）。
    PK_COMPARE((int)PkTransform(1, 0, 0, 0, 1, 0, 0, 0, 3).type(), (int)PkTransform::TxProject);
}

void PkTransformCase::transformTypeSplitsRotateFromShearByDotProduct()
{
    // 实测（探针 §D5 与 probe2 §4）：判据是 dot = **m11*m21 + m12*m22**。
    //   t(2,1,-1,2,0,0)：m11=2 m12=1 m21=-1 m22=2 → dot = 2*(-1)+1*2 = 0 → TxRotate
    //   t(2,1, 2,1,0,0)：m11=2 m12=1 m21= 2 m22=1 → dot = 2*2+1*1 = 5    → TxShear
    // 写成 m11*m12 + m21*m22 的话第一个是 2+2=4（→Shear）、第二个是 2+2=4，
    // **两条都会红**。
    PK_COMPARE((int)PkTransform(2, 1, -1, 2, 0, 0).type(), (int)PkTransform::TxRotate);
    PK_COMPARE((int)PkTransform(2, 1, 2, 1, 0, 0).type(), (int)PkTransform::TxShear);
}

void PkTransformCase::transformTypeAfterRotateRoundTripIsNone()
{
    // 实测：rotate(90) 之后 rotate(-90)，矩阵**精确**回到单位阵，type()==TxNone。
    // 靠的是直角特判（不走 sin/cos），去掉特判之后 m11 会是 6.1e-17 之类，
    // 而 qFuzzyIsNull 仍然把它当 0 —— 所以 type() 还是 TxNone，**光看 type()
    // 抓不到那个缺陷**。所以这里同时按位断言九个分量。
    PkTransform t;
    t.rotate(90);
    PK_VERIFY(mAre(t, 0, 1, 0, -1, 0, 0, 0, 0, 1));
    PK_COMPARE((int)t.type(), (int)PkTransform::TxRotate);
    t.rotate(-90);
    PK_VERIFY(mAre(t, 1, 0, 0, 0, 1, 0, 0, 0, 1));
    PK_COMPARE((int)t.type(), (int)PkTransform::TxNone);
    PK_VERIFY(t.isIdentity());
}

// ═══ 惰性缓存：**可观测语义** ═════════════════════════════════════════════

void PkTransformCase::transformTypeCacheCanGoStaleAndItShows()
{
    // ⚠ 本 Task 最反直觉的一条，实测（探针 §A / probe2 §5，真 Qt 5.15.7）：
    //
    //   QTransform t(2,0,0, 0,2,0, 0,0,2);
    //   t.type();      // → 16 (TxProject)，顺带把 m_dirty 清零
    //   t *= 0.5;      // 矩阵变成**单位阵**；operator*=(qreal) 只把 m_dirty
    //                  //   抬到 TxScale(2)，而 type() 的第一行是
    //                  //   `if (m_dirty == TxNone || m_dirty < m_type) return m_type;`
    //   t.type();      // → **16**，isIdentity() == false
    //
    // 同一串操作、中间不问那一次 type()：
    //   QTransform t(2,0,0, 0,2,0, 0,0,2); t *= 0.5; t.type();   // → 0 (TxNone)
    //
    // 所以 type() **不是九个分量的纯函数**。「照直算 type()」在这一整类序列上
    // 与 Qt 分家，计划里"缓存不是可观测语义"那句被实测推翻了。
    PkTransform stale(2, 0, 0, 0, 2, 0, 0, 0, 2);
    PK_COMPARE((int)stale.type(), (int)PkTransform::TxProject);
    stale *= 0.5;
    PK_VERIFY(mAre(stale, 1, 0, 0, 0, 1, 0, 0, 0, 1));      // 矩阵确实是单位阵
    PK_COMPARE((int)stale.type(), (int)PkTransform::TxProject);   // 但档位是过期的
    PK_VERIFY(!stale.isIdentity());
    PK_VERIFY(!stale.isAffine());

    PkTransform fresh(2, 0, 0, 0, 2, 0, 0, 0, 2);
    fresh *= 0.5;
    PK_COMPARE((int)fresh.type(), (int)PkTransform::TxNone);
    PK_VERIFY(fresh.isIdentity());

    // 两个矩阵九个分量逐位相同，type() 却不同 —— 这一条就是"有状态"的定义。
    PK_VERIFY(mAre(fresh, stale.m11(), stale.m12(), stale.m13(),
                   stale.m21(), stale.m22(), stale.m23(),
                   stale.m31(), stale.m32(), stale.m33()));
    PK_VERIFY(stale.type() != fresh.type());
}

void PkTransformCase::transformStaleCacheAlsoDrivesMapAndIsAffine()
{
    // 过期档位会**传染到 map/mapRect**：它们走 inline_type()，拿到的是同一个
    // 过期值，于是单位阵也会走 TxProject 那条带除法的分支。
    // 实测（probe2 §5）：过期单位阵上 map(3,4)=(3,4)、mapRect(0,0,10,10)=(0,0,10,10)
    // —— 取值恰好相同（单位阵除以 w=1），但**走的分支不同**，
    // 这条测试钉的是"档位确实是过期的那个"，取值一致只是巧合。
    PkTransform stale(2, 0, 0, 0, 2, 0, 0, 0, 2);
    (void)stale.type();
    stale *= 0.5;
    PK_COMPARE((int)stale.type(), (int)PkTransform::TxProject);
    const PkPointF p = stale.map(PkPointF(3, 4));
    PK_VERIFY(sameD(p.x(), 3.0) && sameD(p.y(), 4.0));
    PK_VERIFY(fieldsAre(stale.mapRect(PkRectF(0, 0, 10, 10)), 0, 0, 10, 10));

    // 反过来的一条（探针 probe3）：真投影阵问过 type() 之后 *= 2，
    // m13/m23/m33 全变了但档位仍是 TxProject —— 这次过期值恰好是对的。
    PkTransform u = proj();
    (void)u.type();
    u *= 2.0;
    PK_COMPARE((int)u.type(), (int)PkTransform::TxProject);
    PK_VERIFY(sameD(u.m13(), 1.0) && sameD(u.m23(), 0.5) && sameD(u.m33(), 2.0));
}

void PkTransformCase::transformStaleCacheSurvivesCopy()
{
    // 实测（probe2 §5 最后一行）：过期矩阵的**副本**也过期 —— 拷贝把 m_type
    // 与 m_dirty 一起带走。这一条钉的是"编译器生成的拷贝与 Qt 那份手写
    // memcpy 等价"（Qt 的 operator= 逐字段拷，最后两个字段就是这两个）。
    PkTransform stale(2, 0, 0, 0, 2, 0, 0, 0, 2);
    (void)stale.type();
    stale *= 0.5;
    const PkTransform copy = stale;
    PK_COMPARE((int)copy.type(), (int)PkTransform::TxProject);
    PK_VERIFY(!copy.isIdentity());

    PkTransform assigned;
    assigned = stale;
    PK_COMPARE((int)assigned.type(), (int)PkTransform::TxProject);
}

void PkTransformCase::transformSetMatrixAndResetClearTheCache()
{
    // 实测（probe2 §5）：setMatrix 把 m_dirty 钉成 TxProject（全量重算）、
    // reset 把两个都钉成 TxNone —— 两条都能把过期状态清掉。
    PkTransform t(2, 0, 0, 0, 2, 0, 0, 0, 2);
    (void)t.type();
    t.setMatrix(1, 0, 0, 0, 1, 0, 0, 0, 1);
    PK_COMPARE((int)t.type(), (int)PkTransform::TxNone);
    PK_VERIFY(t.isIdentity());

    PkTransform u(2, 0, 0, 0, 2, 0, 0, 0, 2);
    (void)u.type();
    u.reset();
    PK_VERIFY(mAre(u, 1, 0, 0, 0, 1, 0, 0, 0, 1));
    PK_COMPARE((int)u.type(), (int)PkTransform::TxNone);

    // setMatrix 之后确实是从最高档重算的：把一个真投影阵 setMatrix 成纯缩放。
    PkTransform v = proj();
    (void)v.type();
    v.setMatrix(2, 0, 0, 0, 2, 0, 0, 0, 1);
    PK_COMPARE((int)v.type(), (int)PkTransform::TxScale);
}

// ═══ rotate ═══════════════════════════════════════════════════════════════

void PkTransformCase::transformRotateRightAnglesAreExact()
{
    // 实测（probe2 §6）：90/-270 → sina=1、cosa **留 0**；
    // 270/-90 → sina=-1；180 → cosa=-1、sina 留 0。
    // ⚠ 一律**按位**断言：rotate(180) 的 m21 是 **-0.0**（`-sina` 且 sina=0），
    // 而 PK_COMPARE 会把 -0.0 与 +0.0 判成一样 —— 那正是"照抄了 -sina"的唯一证据。
    { PkTransform t; t.rotate(90);   PK_VERIFY(mAre(t, 0, 1, 0, -1, 0, 0, 0, 0, 1)); }
    { PkTransform t; t.rotate(-270); PK_VERIFY(mAre(t, 0, 1, 0, -1, 0, 0, 0, 0, 1)); }
    { PkTransform t; t.rotate(-90);  PK_VERIFY(mAre(t, 0, -1, 0, 1, 0, 0, 0, 0, 1)); }
    { PkTransform t; t.rotate(270);  PK_VERIFY(mAre(t, 0, -1, 0, 1, 0, 0, 0, 0, 1)); }
    { PkTransform t; t.rotate(180);  PK_VERIFY(mAre(t, -1, 0, 0, -0.0, -1, 0, 0, 0, 1));
      PK_VERIFY(std::signbit(t.m21()));
      // 实测 rotate(180).type() == TxScale（m12/m21 都 fuzzy-null，m11-1 = -2）
      PK_COMPARE((int)t.type(), (int)PkTransform::TxScale); }

    // 90 度是**精确**的：m11 恰好是 0，不是 6.1e-17。
    { PkTransform t; t.rotate(90); PK_VERIFY(t.m11() == 0.0 && !std::signbit(t.m11())); }
}

void PkTransformCase::transformRotateMinus180IsNotSpecialCased()
{
    // 实测：**-180 不在特判里**（特判只有 90 / -270 / 270 / -90 / 180），
    // 所以它走 sin/cos，m12 是 -1.2246467991473532e-16 而不是 0。
    // 这条与上一条一起，把"特判的边界画在哪"钉死。
    PkTransform t;
    t.rotate(-180);
    PK_VERIFY(sameD(t.m11(), -1.0));
    PK_VERIFY(!sameD(t.m12(), 0.0));
    PK_VERIFY(sameD(t.m12(), -1.2246467991473532e-16));
    PK_VERIFY(sameD(t.m21(), 1.2246467991473532e-16));
    PK_VERIFY(sameD(t.m22(), -1.0));
    // 但 qFuzzyIsNull 仍把 1.2e-16 当 0，所以 type() 与 rotate(180) 一样是 TxScale。
    PK_COMPARE((int)t.type(), (int)PkTransform::TxScale);
}

void PkTransformCase::transformRotateZeroReturnsEarly()
{
    // 实测：rotate(0) 提前返回，矩阵与缓存都不动。
    PkTransform t;
    t.rotate(0);
    PK_VERIFY(mAre(t, 1, 0, 0, 0, 1, 0, 0, 0, 1));
    PK_COMPARE((int)t.type(), (int)PkTransform::TxNone);
}

void PkTransformCase::transformRotateRadiansHasNoSpecialCaseAndNoEarlyReturn()
{
    // 实测：rotateRadians(pi/2) 的 m11 是 6.123233995736766e-17（**不是 0**）——
    // 弧度版没有直角特判。把两个函数合并成一个会让这一条红。
    PkTransform t;
    t.rotateRadians(3.14159265358979323846 / 2);
    PK_VERIFY(sameD(t.m11(), 6.123233995736766e-17));
    PK_VERIFY(sameD(t.m12(), 1.0));
    PK_VERIFY(sameD(t.m21(), -1.0));
    PK_VERIFY(sameD(t.m22(), 6.123233995736766e-17));
    PK_COMPARE((int)t.type(), (int)PkTransform::TxRotate);

    // 实测：rotateRadians(0) **没有**提前返回（rotate(0) 有）——
    // 它照走 sin/cos，于是 m21 变成 **-0.0**（`-sina`，sina=+0.0）。
    PkTransform u;
    u.rotateRadians(0);
    PK_VERIFY(mAre(u, 1, 0, 0, -0.0, 1, 0, 0, 0, 1));
    PK_VERIFY(std::signbit(u.m21()));
    PK_COMPARE((int)u.type(), (int)PkTransform::TxNone);
}

void PkTransformCase::transformRotateDispatchesOnCurrentType()
{
    // 实测（probe2 §7）：rotate 按**当前档位**走四条不同公式。
    // rotate(90) 作用在 fromScale(2,3) 上（TxScale 档）：
    //   tm11 = cosa*m11 = 0, tm12 = sina*m22 = 3, tm21 = -sina*m11 = -2, tm22 = 0
    PkTransform t = PkTransform::fromScale(2, 3);
    t.rotate(90);
    PK_VERIFY(mAre(t, 0, 3, 0, -2, 0, 0, 0, 0, 1));

    // 投影档：m13/m23 也要转（tm13 = cosa*m13 + sina*m23）。
    PkTransform p = proj();          // m13=0.5 m23=0.25
    p.rotate(90);                    // cosa=0 sina=1
    PK_VERIFY(sameD(p.m13(), 0.25));         // 0*0.5 + 1*0.25
    PK_VERIFY(sameD(p.m23(), -0.5));         // -1*0.5 + 0*0.25
}

void PkTransformCase::transformRotateAboutYAndXAxisGoesProjective()
{
    // 实测（probe2 §6）：非 Z 轴走完全另一套 —— 造一个 result（m_type 直接钉成
    // TxProject）再 `*this = result * *this`。inv_dist_to_plane = 1/1024。
    //   rotate(45,YAxis): m11=cos45=0.70710678118654757,
    //                     m13=-sin45/1024=-0.00069053396600248776
    PkTransform y;
    y.rotate(45, Qt::YAxis);
    PK_VERIFY(sameD(y.m11(), 0.70710678118654757));
    PK_VERIFY(sameD(y.m13(), -0.00069053396600248776));
    PK_VERIFY(sameD(y.m22(), 1.0));
    PK_VERIFY(sameD(y.m33(), 1.0));
    PK_COMPARE((int)y.type(), (int)PkTransform::TxProject);

    PkTransform x;
    x.rotate(45, Qt::XAxis);
    PK_VERIFY(sameD(x.m22(), 0.70710678118654757));
    PK_VERIFY(sameD(x.m23(), -0.00069053396600248776));
    PK_VERIFY(sameD(x.m11(), 1.0));
    PK_COMPARE((int)x.type(), (int)PkTransform::TxProject);

    // 默认实参就是 Qt::ZAxis（少写一个参数与显式写 ZAxis 必须同结果）。
    PkTransform a, b;
    a.rotate(30);
    b.rotate(30, Qt::ZAxis);
    PK_VERIFY(mAre(a, b.m11(), b.m12(), b.m13(), b.m21(), b.m22(), b.m23(),
                   b.m31(), b.m32(), b.m33()));
}

// ═══ translate / scale / shear 的分档 ═════════════════════════════════════

void PkTransformCase::transformTranslateDispatchesOnCurrentType()
{
    // 实测（probe2 §7），四档各一行。
    { PkTransform t; t.translate(3, 4);
      PK_VERIFY(mAre(t, 1, 0, 0, 0, 1, 0, 3, 4, 1));
      PK_COMPARE((int)t.type(), (int)PkTransform::TxTranslate); }
    { PkTransform t = PkTransform::fromScale(2, 3); t.translate(3, 4);
      // TxScale 档：dx += dx*m11 = 6，dy += dy*m22 = 12
      PK_VERIFY(mAre(t, 2, 0, 0, 0, 3, 0, 6, 12, 1)); }
    { PkTransform t; t.rotate(90); t.translate(3, 4);
      // TxRotate 档：dx += dx*m11 + dy*m21 = 0 + 4*(-1) = -4
      //              dy += dy*m22 + dx*m12 = 0 + 3*1 = 3
      PK_VERIFY(mAre(t, 0, 1, 0, -1, 0, 0, -4, 3, 1)); }
    { PkTransform t = proj(); t.translate(3, 4);
      // TxProject 档：**先** m33 += dx*m13 + dy*m23 = 1 + 3*0.5 + 4*0.25 = 3.5
      //               再走 TxRotate/TxShear 那条
      PK_VERIFY(mAre(t, 1, 0, 0.5, 0, 1, 0.25, 3, 4, 3.5)); }
}

void PkTransformCase::transformScaleDispatchesOnCurrentType()
{
    { PkTransform t; t.scale(2, 3);
      PK_VERIFY(mAre(t, 2, 0, 0, 0, 3, 0, 0, 0, 1));
      PK_COMPARE((int)t.type(), (int)PkTransform::TxScale); }
    { PkTransform t; t.rotate(90); t.scale(2, 3);
      // TxRotate 档贯穿到 TxScale：m12 *= sx、m21 *= sy，再 m11 *= sx、m22 *= sy
      PK_VERIFY(mAre(t, 0, 2, 0, -3, 0, 0, 0, 0, 1)); }
    { PkTransform t = proj(); t.scale(2, 3);
      // TxProject 档：m13 *= sx = 1、m23 *= sy = 0.75，再贯穿
      PK_VERIFY(mAre(t, 2, 0, 1, 0, 3, 0.75, 0, 0, 1)); }
}

void PkTransformCase::transformShearDispatchesOnCurrentType()
{
    { PkTransform t; t.shear(1, 2);
      // TxNone 档：**直接摆** m12 = sv、m21 = sh（不是加）
      PK_VERIFY(mAre(t, 1, 2, 0, 1, 1, 0, 0, 0, 1));
      PK_COMPARE((int)t.type(), (int)PkTransform::TxShear); }
    { PkTransform t = PkTransform::fromScale(2, 3); t.shear(1, 2);
      // TxScale 档：m12 = sv*m22 = 6、m21 = sh*m11 = 2
      PK_VERIFY(mAre(t, 2, 6, 0, 2, 3, 0, 0, 0, 1)); }
    { PkTransform t; t.rotate(90); t.shear(1, 2);
      // TxRotate 档：四个乘积先进临时量再一起写回
      PK_VERIFY(mAre(t, -2, 1, 0, -1, 1, 0, 0, 0, 1)); }
    { PkTransform t = proj(); t.shear(1, 2);
      PK_VERIFY(mAre(t, 1, 2, 1, 1, 1, 0.75, 0, 0, 1)); }
}

void PkTransformCase::transformMutatorsReturnEarlyOnNeutralArgs()
{
    // 实测：translate(0,0) / scale(1,1) / rotate(0) 都提前返回，**连 m_dirty 都不动**。
    { PkTransform t; t.translate(0, 0);
      PK_VERIFY(mAre(t, 1, 0, 0, 0, 1, 0, 0, 0, 1));
      PK_COMPARE((int)t.type(), (int)PkTransform::TxNone); }
    { PkTransform t; t.scale(1, 1);
      PK_VERIFY(mAre(t, 1, 0, 0, 0, 1, 0, 0, 0, 1)); }
    // shear(0,0) 同理。
    { PkTransform t = proj(); const PkTransform before = t; t.shear(0, 0);
      PK_VERIFY(mAre(t, before.m11(), before.m12(), before.m13(),
                     before.m21(), before.m22(), before.m23(),
                     before.m31(), before.m32(), before.m33())); }
    // ⚠ 判据是裸 `==`，所以 -0.0 也算 0：translate(-0.0,-0.0) 同样提前返回。
    { PkTransform t = PkTransform::fromScale(2, 3); t.translate(-0.0, -0.0);
      PK_VERIFY(sameD(t.m31(), 0.0) && !std::signbit(t.m31())); }
}

void PkTransformCase::transformFromTranslateAndFromScalePinTheCache()
{
    // 实测（探针 §A6 与 probe2 §4）：这两个静态工厂**不走重算**，直接钉档位。
    PK_COMPARE((int)PkTransform::fromTranslate(5, 7).type(), (int)PkTransform::TxTranslate);
    PK_COMPARE((int)PkTransform::fromTranslate(0, 0).type(), (int)PkTransform::TxNone);
    PK_COMPARE((int)PkTransform::fromScale(2, 3).type(), (int)PkTransform::TxScale);
    PK_COMPARE((int)PkTransform::fromScale(1, 1).type(), (int)PkTransform::TxNone);
    PK_VERIFY(mAre(PkTransform::fromTranslate(5, 7), 1, 0, 0, 0, 1, 0, 5, 7, 1));
    PK_VERIFY(mAre(PkTransform::fromScale(2, 3), 2, 0, 0, 0, 3, 0, 0, 0, 1));
    // ⚠ 判据是裸 `==`：fromTranslate(-0.0,-0.0) 的档位是 TxNone，
    // 但**分量仍然是 -0.0**（钉的是档位，不改矩阵）。
    const PkTransform z = PkTransform::fromTranslate(-0.0, -0.0);
    PK_COMPARE((int)z.type(), (int)PkTransform::TxNone);
    PK_VERIFY(std::signbit(z.dx()) && std::signbit(z.dy()));
}

// ═══ 乘法 ═════════════════════════════════════════════════════════════════

void PkTransformCase::transformMultiplicationOrderMatters()
{
    // 实测（探针 §I）：fromScale(2,3)*fromTranslate(4,5) 的 dx=4；
    //                  fromTranslate(4,5)*fromScale(2,3) 的 dx=8 dy=15。
    const PkTransform a = PkTransform::fromScale(2, 3) * PkTransform::fromTranslate(4, 5);
    PK_VERIFY(mAre(a, 2, 0, 0, 0, 3, 0, 4, 5, 1));
    const PkTransform b = PkTransform::fromTranslate(4, 5) * PkTransform::fromScale(2, 3);
    PK_VERIFY(mAre(b, 2, 0, 0, 0, 3, 0, 8, 15, 1));

    // 复合版与二元版在这一对上取值相同。
    PkTransform c = PkTransform::fromScale(2, 3);
    c *= PkTransform::fromTranslate(4, 5);
    PK_VERIFY(mAre(c, 2, 0, 0, 0, 3, 0, 4, 5, 1));

    // 纯平移 × 纯平移：TxTranslate 分支
    const PkTransform d = PkTransform::fromTranslate(4, 5) * PkTransform::fromTranslate(1, 2);
    PK_VERIFY(mAre(d, 1, 0, 0, 0, 1, 0, 5, 7, 1));
    PK_COMPARE((int)d.type(), (int)PkTransform::TxTranslate);
}

void PkTransformCase::transformMultiplyReturnsEarlyOnIdentitySide()
{
    // 实测：任一侧是 TxNone 时直接返回另一侧的**副本**（连缓存状态一起）。
    PK_VERIFY(mAre(PkTransform() * PkTransform::fromScale(2, 3), 2, 0, 0, 0, 3, 0, 0, 0, 1));
    PK_VERIFY(mAre(PkTransform::fromScale(2, 3) * PkTransform(), 2, 0, 0, 0, 3, 0, 0, 0, 1));
    PkTransform t = PkTransform::fromScale(2, 3);
    t *= PkTransform();
    PK_VERIFY(mAre(t, 2, 0, 0, 0, 3, 0, 0, 0, 1));
}

void PkTransformCase::transformProjectiveProductUsesFullNineTerms()
{
    // 实测（probe2 §8）：t(1..9) * t(9..1) →
    //   m11=30 m12=24 m13=18 | m21=84 m22=69 m23=54 | m31=138 m32=114 m33=90
    const PkTransform a(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const PkTransform b(9, 8, 7, 6, 5, 4, 3, 2, 1);
    PK_VERIFY(mAre(a * b, 30, 24, 18, 84, 69, 54, 138, 114, 90));
    PK_COMPARE((int)(a * b).type(), (int)PkTransform::TxProject);
}

void PkTransformCase::transformStarAndStarEqualsDifferOnNegativeZeroDy()
{
    // ⚠ Qt 的 operator* 在 TxTranslate 分支上写 dy 用的是 **`+=`** 而不是 `=`
    //（qtransform.cpp:962），而 operator*= 用的是 `+=` 作用在自己身上。
    // 于是两侧 dy 都是 -0.0 时两者分家 —— 实测（probe2 §8）：
    //   fromTranslate(5,-0) *  fromTranslate(3,-0)  → dy = **+0.0**（signbit 0）
    //   fromTranslate(5,-0) *= fromTranslate(3,-0)  → dy = **-0.0**（signbit 1）
    // 把 operator* 那句"顺手改成 `=`"会让这一条红。
    const PkTransform a = PkTransform::fromTranslate(5, -0.0);
    const PkTransform b = PkTransform::fromTranslate(3, -0.0);

    const PkTransform prod = a * b;
    PK_VERIFY(sameD(prod.dx(), 8.0));
    PK_VERIFY(sameD(prod.dy(), 0.0));
    PK_VERIFY(!std::signbit(prod.dy()));

    PkTransform comp = a;
    comp *= b;
    PK_VERIFY(sameD(comp.dx(), 8.0));
    PK_VERIFY(sameD(comp.dy(), -0.0));
    PK_VERIFY(std::signbit(comp.dy()));
}

// ═══ inverted ═════════════════════════════════════════════════════════════

void PkTransformCase::transformInvertedTranslatePath()
{
    bool ok = false;
    const PkTransform t = PkTransform::fromTranslate(3, 4).inverted(&ok);
    PK_VERIFY(ok);
    PK_VERIFY(mAre(t, 1, 0, 0, 0, 1, 0, -3, -4, 1));
    PK_COMPARE((int)t.type(), (int)PkTransform::TxTranslate);
}

void PkTransformCase::transformInvertedScalePathUsesFuzzyNullPerAxis()
{
    bool ok = false;
    // 实测：fromScale(2,4).inverted() → m11=0.5 m22=0.25，
    // 且 dx/dy 是 **-0.0**（`-m_dx * invert.m11` = -0*0.5）。
    const PkTransform t = PkTransform::fromScale(2, 4).inverted(&ok);
    PK_VERIFY(ok);
    PK_VERIFY(sameD(t.m11(), 0.5) && sameD(t.m22(), 0.25));
    PK_VERIFY(sameD(t.m31(), -0.0) && std::signbit(t.m31()));
    PK_VERIFY(sameD(t.m32(), -0.0) && std::signbit(t.m32()));

    // 带平移的缩放阵
    PkTransform s = PkTransform::fromScale(2, 4);
    s.translate(3, 4);
    PK_VERIFY(mAre(s, 2, 0, 0, 0, 4, 0, 6, 16, 1));
    const PkTransform si = s.inverted(&ok);
    PK_VERIFY(ok);
    PK_VERIFY(mAre(si, 0.5, 0, 0, 0, 0.25, 0, -3, -4, 1));

    // ⚠ 判据是**每个轴各自** qFuzzyIsNull：只要一个轴是 0 就整体失败。
    const PkTransform z = PkTransform::fromScale(0, 1).inverted(&ok);
    PK_VERIFY(!ok);
    PK_VERIFY(mAre(z, 1, 0, 0, 0, 1, 0, 0, 0, 1));
    // 1e-300 不是精确 0，但 qFuzzyIsNull(1e-300) 为真 → 同样失败（探针 §D2）。
    bool ok2 = true;
    (void)PkTransform::fromScale(1e-300, 1).inverted(&ok2);
    PK_VERIFY(!ok2);
}

void PkTransformCase::transformInvertedAffinePathUsesExactZeroDeterminant()
{
    // TxRotate/TxShear 走 QMatrix::inverted 那条，判据是 **`dtr == 0.0` 精确零**
    //（不是 qFuzzyIsNull），且行列式是**二阶**式 m11*m22 - m12*m21。
    bool ok = false;
    PkTransform r;
    r.rotate(90);
    const PkTransform ri = r.inverted(&ok);
    PK_VERIFY(ok);
    // 实测：m11=0 m12=-1 m21=1 m22=0，dx 是 **-0.0**
    PK_VERIFY(sameD(ri.m11(), 0.0) && sameD(ri.m12(), -1.0));
    PK_VERIFY(sameD(ri.m21(), 1.0) && sameD(ri.m22(), 0.0));
    PK_VERIFY(sameD(ri.m31(), -0.0) && std::signbit(ri.m31()));
    PK_COMPARE((int)ri.type(), (int)PkTransform::TxRotate);

    // 精确奇异的 shear：t(1,2,2,4,0,0) 的二阶式 = 1*4 - 2*2 = 0
    const PkTransform sg(1, 2, 2, 4, 0, 0);
    PK_COMPARE((int)sg.type(), (int)PkTransform::TxShear);
    const PkTransform sgi = sg.inverted(&ok);
    PK_VERIFY(!ok);
    PK_VERIFY(mAre(sgi, 1, 0, 0, 0, 1, 0, 0, 0, 1));
}

void PkTransformCase::transformInvertedProjectivePathUsesAdjointOverDet()
{
    bool ok = false;
    // 实测（probe2 §9）：proj(1,0,0.5, 0,1,0.25, 0,0,1) 的逆是
    //   m11=1 m12=0 m13=-0.5 | m21=0 m22=1 m23=-0.25 | m31=0 m32=0 m33=1
    const PkTransform pi = proj().inverted(&ok);
    PK_VERIFY(ok);
    PK_VERIFY(mAre(pi, 1, 0, -0.5, 0, 1, -0.25, 0, 0, 1));
    PK_COMPARE((int)pi.type(), (int)PkTransform::TxProject);

    // 投影档上判据是 qFuzzyIsNull(三阶行列式)：t(1,1,1,1,1,1,1,1,1) 的 det 是 0
    const PkTransform pz(1, 1, 1, 1, 1, 1, 1, 1, 1);
    PK_COMPARE(pz.determinant(), 0.0);
    PK_COMPARE((int)pz.type(), (int)PkTransform::TxProject);
    const PkTransform pzi = pz.inverted(&ok);
    PK_VERIFY(!ok);
    PK_VERIFY(mAre(pzi, 1, 0, 0, 0, 1, 0, 0, 0, 1));
}

void PkTransformCase::transformInvertedFailureReturnsIdentityAndDropsType()
{
    // 失败路径的三件事：返回**单位阵**、出参 false、**档位不从源拷贝**（TxNone）。
    // ⚠ 第三条是规则二的靶子：只比 `invertible` 标志位的谓词看不见矩阵内容，
    //  只比矩阵不看档位的谓词看不见这一条。
    bool ok = true;
    const PkTransform sg(1, 2, 2, 4, 0, 0);
    const PkTransform r = sg.inverted(&ok);
    PK_VERIFY(!ok);
    PK_VERIFY(mAre(r, 1, 0, 0, 0, 1, 0, 0, 0, 1));
    PK_COMPARE((int)r.type(), (int)PkTransform::TxNone);
    PK_VERIFY(r.isIdentity());
    // 源矩阵是 TxShear，失败结果却是 TxNone —— 就是"不拷贝"的证据。
    PK_COMPARE((int)sg.type(), (int)PkTransform::TxShear);
}

void PkTransformCase::transformInvertedKeepsTypeOnSuccess()
{
    // 成功路径上档位**原样带走**（Qt 注释：inverting doesn't change the type）。
    bool ok = false;
    PK_COMPARE((int)PkTransform::fromTranslate(3, 4).inverted(&ok).type(),
               (int)PkTransform::TxTranslate);
    PK_COMPARE((int)PkTransform::fromScale(2, 4).inverted(&ok).type(),
               (int)PkTransform::TxScale);
    PK_COMPARE((int)PkTransform().inverted(&ok).type(), (int)PkTransform::TxNone);
    // 出参可以是 nullptr（默认实参），取值不变。
    PK_VERIFY(mAre(PkTransform::fromScale(2, 4).inverted(),
                   0.5, 0, 0, 0, 0.25, 0, -0.0, -0.0, 1));
}

// ═══ 标量运算符 ═══════════════════════════════════════════════════════════

void PkTransformCase::transformScalarOperatorsMatchQt()
{
    // 实测（probe2 §10）
    { PkTransform t(1, 2, 3, 4, 5, 6, 7, 8, 9); t *= 2.0;
      PK_VERIFY(mAre(t, 2, 4, 6, 8, 10, 12, 14, 16, 18)); }
    { PkTransform t(1, 2, 3, 4, 5, 6, 7, 8, 9); t /= 2.0;
      PK_VERIFY(mAre(t, 0.5, 1, 1.5, 2, 2.5, 3, 3.5, 4, 4.5)); }
    { PkTransform t(1, 2, 3, 4, 5, 6, 7, 8, 9); t += 1.0;
      PK_VERIFY(mAre(t, 2, 3, 4, 5, 6, 7, 8, 9, 10)); }
    { PkTransform t(1, 2, 3, 4, 5, 6, 7, 8, 9); t -= 1.0;
      PK_VERIFY(mAre(t, 0, 1, 2, 3, 4, 5, 6, 7, 8)); }
}

void PkTransformCase::transformScalarOperatorsReturnEarlyOnNeutral()
{
    // 实测：*=1 / /=0 / +=0 / -=0 都提前返回，矩阵与缓存都不动。
    // ⚠ **`/= 0` 不是造 inf，是原样返回** —— 这一条最容易"顺手修正"成除零。
    const double src[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    { PkTransform t(1, 2, 3, 4, 5, 6, 7, 8, 9); t *= 1.0;
      PK_VERIFY(mAre(t, src[0], src[1], src[2], src[3], src[4], src[5], src[6], src[7], src[8])); }
    { PkTransform t(1, 2, 3, 4, 5, 6, 7, 8, 9); t /= 0.0;
      PK_VERIFY(mAre(t, src[0], src[1], src[2], src[3], src[4], src[5], src[6], src[7], src[8])); }
    { PkTransform t(1, 2, 3, 4, 5, 6, 7, 8, 9); t += 0.0;
      PK_VERIFY(mAre(t, src[0], src[1], src[2], src[3], src[4], src[5], src[6], src[7], src[8])); }
    { PkTransform t(1, 2, 3, 4, 5, 6, 7, 8, 9); t -= 0.0;
      PK_VERIFY(mAre(t, src[0], src[1], src[2], src[3], src[4], src[5], src[6], src[7], src[8])); }
}

void PkTransformCase::transformDivideIsMultiplyByReciprocal()
{
    // ⚠ `/=` 是 `1/div` 再乘，**不是**逐个除 —— 两者取值不同。
    // 实测（probe2 §10）：7.0/3.0 = 2.3333333333333335，
    //                     7.0*(1.0/3.0) = 2.333333333333333。
    // t(1..9) /= 3 之后 m31（源值 7）实测是 **2.333333333333333**。
    PkTransform t(1, 2, 3, 4, 5, 6, 7, 8, 9);
    t /= 3.0;
    PK_VERIFY(sameD(t.m31(), 7.0 * (1.0 / 3.0)));
    PK_VERIFY(!sameD(t.m31(), 7.0 / 3.0));
    PK_VERIFY(sameD(t.m31(), 2.333333333333333));
    PK_VERIFY(sameD(t.m11(), 0.33333333333333331));
}

void PkTransformCase::transformFreeScalarOperatorsMatchCompound()
{
    // 四个自由函数就是"拷一份再用复合版"，取值必须与复合版逐位相同。
    const PkTransform s(1, 2, 3, 4, 5, 6, 7, 8, 9);
    PK_VERIFY(mAre(s * 2.0, 2, 4, 6, 8, 10, 12, 14, 16, 18));
    PK_VERIFY(mAre(s / 2.0, 0.5, 1, 1.5, 2, 2.5, 3, 3.5, 4, 4.5));
    PK_VERIFY(mAre(s + 1.0, 2, 3, 4, 5, 6, 7, 8, 9, 10));
    PK_VERIFY(mAre(s - 1.0, 0, 1, 2, 3, 4, 5, 6, 7, 8));
    // 源不被改动
    PK_VERIFY(mAre(s, 1, 2, 3, 4, 5, 6, 7, 8, 9));
}

// ═══ map 的四个重载 ═══════════════════════════════════════════════════════

void PkTransformCase::transformMapPointOverloadsDoNotClampW()
{
    // ⚠ map(PkPoint) / map(PkPointF) 自己那份 `w = 1./(m13*x+m23*y+m33)`
    // **没有 Q_NEAR_CLIP 夹持**。实测（probe2 §11）在 t(1,0,-1, 0,1,0, 0,0,1)
    // （w = -x + 1）上：
    //   x=2  w=-1 → map(QPointF) = (-2, -0)      map(QPoint) = (-2, 0)
    //   x=10 w=-9 → map(QPointF) = (-1.1111111111111112, -0)  map(QPoint) = (-1, 0)
    //   x=1  w=0  → map(QPointF) = (inf, -nan)
    const PkTransform t(1, 0, -1, 0, 1, 0, 0, 0, 1);
    PK_COMPARE((int)t.type(), (int)PkTransform::TxProject);

    const PkPointF a = t.map(PkPointF(2, 0));
    PK_VERIFY(sameD(a.x(), -2.0));
    PK_VERIFY(sameD(a.y(), -0.0) && std::signbit(a.y()));

    const PkPointF b = t.map(PkPointF(10, 0));
    PK_VERIFY(sameD(b.x(), -1.1111111111111112));

    const PkPoint bi = t.map(PkPoint(10, 0));
    PK_VERIFY(bi.x() == -1 && bi.y() == 0);

    // w 恰为 0：**除零，得 inf/nan**，不夹持。
    const PkPointF z = t.map(PkPointF(1, 0));
    PK_VERIFY(std::isinf(z.x()) && z.x() > 0);
    PK_VERIFY(std::isnan(z.y()));
}

void PkTransformCase::transformMapPointerOverloadsClampW()
{
    // ⚠ map(int*,int*) 与 map(qreal*,qreal*) 走 MAP 宏，**夹持** w >= 1e-6，
    // 于是同一个矩阵同一个点，答案与上一条完全不同。实测（probe2 §11）：
    //   x=2  → map(qreal*) = (2000000, 0)      map(int*) = (2000000, 0)
    //   x=10 → map(qreal*) = (10000000, 0)     map(int*) = (10000000, 0)
    //   x=1（w=0）→ map(qreal*) = (1000000, 0)
    // 把两族合并成一条实现会让这一整类差异消失。
    const PkTransform t(1, 0, -1, 0, 1, 0, 0, 0, 1);

    double qx = 0, qy = 0;
    t.map(2.0, 0.0, &qx, &qy);
    PK_VERIFY(sameD(qx, 2000000.0) && sameD(qy, 0.0));

    t.map(10.0, 0.0, &qx, &qy);
    PK_VERIFY(sameD(qx, 10000000.0));

    t.map(1.0, 0.0, &qx, &qy);              // w == 0 → 夹到 1e-6
    PK_VERIFY(sameD(qx, 1000000.0) && sameD(qy, 0.0));

    int ix = 0, iy = 0;
    t.map(10, 0, &ix, &iy);
    PK_VERIFY(ix == 10000000 && iy == 0);

    // 同一个点上两族分家 —— 这一条就是"不能合并"的判据本身。
    const PkPointF viaPoint = t.map(PkPointF(10, 0));
    PK_VERIFY(!sameD(viaPoint.x(), qx));
}

void PkTransformCase::transformMapOverloadsAgreeOnAffineMatrices()
{
    // 非投影阵上四个重载必须一致（夹持那条 `if (t == TxProject)` 根本不进）。
    // 实测：shear 阵 t(2,3,5,7,11,13) 在 (1,1) 上四个重载都给 (18,23)。
    const PkTransform t(2, 3, 5, 7, 11, 13);
    const PkPointF pf = t.map(PkPointF(1, 1));
    const PkPoint pi = t.map(PkPoint(1, 1));
    double qx = 0, qy = 0;
    t.map(1.0, 1.0, &qx, &qy);
    int ix = 0, iy = 0;
    t.map(1, 1, &ix, &iy);
    PK_VERIFY(sameD(pf.x(), 18.0) && sameD(pf.y(), 23.0));
    PK_VERIFY(pi.x() == 18 && pi.y() == 23);
    PK_VERIFY(sameD(qx, 18.0) && sameD(qy, 23.0));
    PK_VERIFY(ix == 18 && iy == 23);
}

void PkTransformCase::transformFreePointOperatorsForwardToMap()
{
    // 实测：QPointF(1,0)*t == (13,16)，QPoint(1,0)*t == (13,16)。
    // ⚠ 方向是 `p * m`（点在左）—— Qt 头文件里就这么声明的，没有 `m * p`。
    const PkTransform t(2, 3, 5, 7, 11, 13);
    const PkPointF a = PkPointF(1, 0) * t;
    PK_VERIFY(sameD(a.x(), 13.0) && sameD(a.y(), 16.0));
    const PkPoint b = PkPoint(1, 0) * t;
    PK_VERIFY(b.x() == 13 && b.y() == 16);
}

// ═══ mapRect ══════════════════════════════════════════════════════════════

void PkTransformCase::transformMapRectTranslateFastPathRoundsTheOffset()
{
    // 实测：fromTranslate(3.4,4.6).mapRect(QRect(0,0,10,10)) == QRect(3,5,10,10)
    // —— 整数版把偏移**各自 qRound** 之后平移（qRound(3.4)=3、qRound(4.6)=5）。
    // 浮点版不取整。
    const PkTransform t = PkTransform::fromTranslate(3.4, 4.6);
    PK_VERIFY(coordsAre(t.mapRect(PkRect(0, 0, 10, 10)), 3, 5, 10, 10));
    PK_VERIFY(fieldsAre(t.mapRect(PkRectF(0, 0, 10, 10)),
                        3.3999999999999999, 4.5999999999999996, 10, 10));
    // 单位阵原样返回
    PK_VERIFY(coordsAre(PkTransform().mapRect(PkRect(0, 0, 10, 10)), 0, 0, 10, 10));
}

void PkTransformCase::transformMapRectScaleFastPathFlipsNegativeExtent()
{
    // 实测：fromScale(2,3) → (0,0,20,30)；fromScale(-2,3) → **(-20,0,20,30)**
    // （负宽度取反并把 x 往回挪，不是留着负宽度）。
    PK_VERIFY(coordsAre(PkTransform::fromScale(2, 3).mapRect(PkRect(0, 0, 10, 10)), 0, 0, 20, 30));
    PK_VERIFY(fieldsAre(PkTransform::fromScale(2, 3).mapRect(PkRectF(0, 0, 10, 10)), 0, 0, 20, 30));
    PK_VERIFY(coordsAre(PkTransform::fromScale(-2, 3).mapRect(PkRect(0, 0, 10, 10)), -20, 0, 20, 30));
    PK_VERIFY(fieldsAre(PkTransform::fromScale(-2, 3).mapRect(PkRectF(0, 0, 10, 10)), -20, 0, 20, 30));
    // 退化矩形
    PK_VERIFY(coordsAre(PkTransform::fromScale(2, 3).mapRect(PkRect()), 0, 0, 0, 0));
    PK_VERIFY(fieldsAre(PkTransform::fromScale(2, 3).mapRect(PkRectF()), 0, 0, 0, 0));
}

void PkTransformCase::transformMapRectIntegerUsesRightPlusOne()
{
    // 实测（探针 §I）：rotate(45).mapRect(QRect(0,0,10,10)) == QRect(-7,0,14,14)。
    // 整数版四角取的是 (left,top) (right+1,top) (right+1,bottom+1) (left,bottom+1)
    // —— 那个 **+1** 是 QRect 差一语义的补偿。去掉它这一条会红（宽会变成 12）。
    PkTransform t;
    t.rotate(45);
    PK_VERIFY(coordsAre(t.mapRect(PkRect(0, 0, 10, 10)), -7, 0, 14, 14));

    PkTransform r90;
    r90.rotate(90);
    PK_VERIFY(coordsAre(r90.mapRect(PkRect(0, 0, 10, 10)), -10, 0, 10, 10));

    // 不需裁剪的投影阵：实测 QRect(0,0,9,10)
    const PkTransform p(1, 0, 0.01, 0, 1, 0, 0, 0, 1);
    PK_VERIFY(coordsAre(p.mapRect(PkRect(0, 0, 10, 10)), 0, 0, 9, 10));
}

void PkTransformCase::transformMapRectFloatHasNoOffByOne()
{
    // 实测（探针 §I）：rotate(45).mapRect(QRectF(0,0,10,10)) ==
    //   (-7.0710678118654746, 0, 14.142135623730951, 14.142135623730951)
    // 浮点版四角是 (x,y) (x+w,y) (x+w,y+h) (x,y+h) —— **没有 +1**。
    PkTransform t;
    t.rotate(45);
    PK_VERIFY(fieldsAre(t.mapRect(PkRectF(0, 0, 10, 10)),
                        -7.0710678118654746, 0,
                        14.142135623730951, 14.142135623730951));

    PkTransform r90;
    r90.rotate(90);
    PK_VERIFY(fieldsAre(r90.mapRect(PkRectF(0, 0, 10, 10)), -10, 0, 10, 10));

    const PkTransform p(1, 0, 0.01, 0, 1, 0, 0, 0, 1);
    PK_VERIFY(fieldsAre(p.mapRect(PkRectF(0, 0, 10, 10)), 0, 0, 9.0909090909090899, 10));
}

void PkTransformCase::transformMapRectPerspectiveClipIsADeclaredGap()
{
    // ⚠ **这是本 Task 唯一一处与 Qt 的真实行为偏离**，登记在
    // oracle/geometry.deviation 与 README 覆盖度缺口。
    //
    // Qt 在 `type() == TxProject && needsPerspectiveClipping(rect)` 时改走
    // QPainterPath（把矩形当路径、在近裁剪面上真的裁一刀再取包围盒）；
    // QPainterPath 不在 R-03 交付范围（归属未定），所以本类落回四角包围盒。
    //
    // 实测（探针 §C1）：t(1,0,-1, 0,1,0, 0,0,1) 对 (0,0,10,10)，
    //   needsClip = 1（wx+wy+m33 = -9 < 1e-6）
    //   Qt   mapRect(QRectF) = (0, 0, 999999.0000000007, 10000000)
    //   Qt   mapRect(QRect)  = (0, 0, 999999, 10000000)
    //   本类（四角包围盒）   = (0, 0, 10000000, 10000000)
    //
    // 这条测试钉的是**我们这一侧的取值**（四角包围盒），并把与 Qt 的差额写在
    // 眼前 —— 哪天补上 QPainterPath，它会红，那时候该改的是这条测试。
    const PkTransform t(1, 0, -1, 0, 1, 0, 0, 0, 1);
    PK_COMPARE((int)t.type(), (int)PkTransform::TxProject);

    // 四角：(0,0) w=1 →(0,0)；(10,0) w=-9 夹到 1e-6 →(1e7,0)；
    //       (10,10)→(1e7,1e7)；(0,10)→(0,10)
    PK_VERIFY(fieldsAre(t.mapRect(PkRectF(0, 0, 10, 10)), 0, 0, 10000000.0, 10000000.0));
    // 整数版四角用 right()+1 = 10、bottom()+1 = 10，与浮点版同形。
    PK_VERIFY(coordsAre(t.mapRect(PkRect(0, 0, 10, 10)), 0, 0, 10000000, 10000000));

    // 与 Qt 的差额（实测值写在这里当活的文档）：
    PK_VERIFY(10000000.0 != 999999.0000000007);
}

// ═══ 相等 ═════════════════════════════════════════════════════════════════

void PkTransformCase::transformEqualityIsExactNotFuzzy()
{
    // 实测（probe3）：m33 差 5e-12 时 `==` 为假、qFuzzyCompare 为真。
    // ⚠ 差 1e-16 时**两者都为真** —— 因为 9+1e-16 在 double 里就是 9，
    // 拿它当"精确 vs 模糊"的分界只会得到一条恒真的测试，分不开这两条门槛。
    const PkTransform a(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const PkTransform b(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const PkTransform c(1, 2, 3, 4, 5, 6, 7, 8, 9 + 5e-12);
    PK_VERIFY(a == b);
    PK_VERIFY(!(a != b));
    PK_VERIFY(c.m33() != 9.0);              // 先证明它真的不是同一个 double
    PK_VERIFY(!(a == c));
    PK_VERIFY(a != c);

    // 裸 `==` 的两个后果：NaN 阵不等于自己、±0 判等。
    const PkTransform n(kNaN, 0, 0, 0, 1, 0, 0, 0, 1);
    PK_VERIFY(!(n == n));
    const PkTransform z1(0.0, 0, 0, 0, 1, 0, 0, 0, 1);
    const PkTransform z2(-0.0, 0, 0, 0, 1, 0, 0, 0, 1);
    PK_VERIFY(z1 == z2);
    PK_VERIFY(!sameD(z1.m11(), z2.m11()));  // 但按位不同
    (void)kInf;
}

void PkTransformCase::transformEqualityIgnoresCacheState()
{
    // 实测（probe2 §13）：`==` **只比九个分量**，不比 m_type/m_dirty。
    const PkTransform a(1, 2, 3, 4, 5, 6, 7, 8, 9);
    PkTransform d(1, 2, 3, 4, 5, 6, 7, 8, 9);
    (void)d.type();                         // 把 d 的缓存算出来
    PK_VERIFY(d == a);

    // 更尖锐的一对：档位过期的单位阵 == 全新的单位阵
    PkTransform stale(2, 0, 0, 0, 2, 0, 0, 0, 2);
    (void)stale.type();
    stale *= 0.5;
    PK_VERIFY(stale == PkTransform());
    PK_VERIFY(stale.type() != PkTransform().type());   // 但档位不同
}

void PkTransformCase::transformFuzzyCompareIsTheFuzzyOne()
{
    // 实测（probe3）：qFuzzyCompare 在 5e-12 上为真、在 9e-12 与 1e-11 上为假
    //（门槛是 |diff| * 1e12 <= min(|p1|,|p2|) = 9）。
    const PkTransform a(1, 2, 3, 4, 5, 6, 7, 8, 9);
    PK_VERIFY(qFuzzyCompare(a, PkTransform(1, 2, 3, 4, 5, 6, 7, 8, 9 + 5e-12)));
    PK_VERIFY(!qFuzzyCompare(a, PkTransform(1, 2, 3, 4, 5, 6, 7, 8, 9 + 9e-12)));
    PK_VERIFY(!qFuzzyCompare(a, PkTransform(1, 2, 3, 4, 5, 6, 7, 8, 9 + 1e-11)));
    PK_VERIFY(qFuzzyCompare(a, a));
}

// ═══ transposed ═══════════════════════════════════════════════════════════

void PkTransformCase::transformTransposedSwapsAcrossDiagonal()
{
    // 实测：t(1..9).transposed() → m11=1 m12=4 m13=7 | m21=2 m22=5 m23=8 |
    //                              m31=3 m32=6 m33=9
    const PkTransform a(1, 2, 3, 4, 5, 6, 7, 8, 9);
    PK_VERIFY(mAre(a.transposed(), 1, 4, 7, 2, 5, 8, 3, 6, 9));
    PK_VERIFY(mAre(PkTransform().transposed(), 1, 0, 0, 0, 1, 0, 0, 0, 1));
    PK_COMPARE((int)PkTransform().transposed().type(), (int)PkTransform::TxNone);
    // 转两次回到自己
    PK_VERIFY(mAre(a.transposed().transposed(), 1, 2, 3, 4, 5, 6, 7, 8, 9));
}

// ═══ 跨切面 ═══════════════════════════════════════════════════════════════

void PkTransformCase::transformNoexceptSurfaceMatchesQt()
{
    // QTransform 的公开成员**一个都没有 noexcept**（只有拷贝/移动与 qHash 有）。
    // noexcept 是可观察的（能进 type_traits、能改重载决议），多加一个就是多一档
    // 能力 —— 与 PkRect 那边 getRect/getCoords 恰好没有 noexcept 是同一条口径。
    PkTransform t;
    PK_VERIFY(!noexcept(t.type()));
    PK_VERIFY(!noexcept(t.determinant()));
    PK_VERIFY(!noexcept(t.m11()));
    PK_VERIFY(!noexcept(t.isIdentity()));
    PK_VERIFY(!noexcept(t.map(PkPointF(0, 0))));
    PK_VERIFY(!noexcept(t.mapRect(PkRectF(0, 0, 1, 1))));
    PK_VERIFY(!noexcept(t.inverted(nullptr)));
    PK_VERIFY(!noexcept(t.translate(1, 1)));
    PK_VERIFY(!noexcept(PkTransform()));
    PK_VERIFY(!noexcept(PkTransform(1, 2, 3, 4, 5, 6)));

    // 反过来：拷贝**是** noexcept（Qt 的手写版带 noexcept，编译器生成的对九个
    // double 加两个位域也是 noexcept）。
    PK_VERIFY(std::is_nothrow_copy_constructible<PkTransform>::value);
    PK_VERIFY(std::is_nothrow_copy_assignable<PkTransform>::value);
}

void PkTransformCase::transformConstexprSurfaceIsGatedByText()
{
    // QTransform **一个 constexpr 成员都没有**（qtransform.h 里连 Q_DECL_CONSTEXPR
    // 都没出现过）——与 PkPoint/PkSize/PkRect 三族全是 constexpr 恰好相反。
    // 所以本类也一个都不加：多一个 constexpr 就是替代品比 Qt 多一档能力。
    //
    // ⚠ **这条判据不住在这里，住在 run_oracle.sh 的 §CONSTEXPR 文本闸门里。**
    // C++17 没法用 static_assert 反证「某表达式不是常量表达式」，所以真正守住
    // 「没人顺手加 constexpr」的是那道 `grep constexpr PkTransform.h` ——
    // 函数名如实说明这一点，别把它读成"这里断言了 PkTransform 不是 constexpr"。
    //
    // 本函数负责的是**另一半**：证明 harness 分得开这两种情况。下面三族的
    // constexpr 变量能编过、Transform 那边只能是运行期变量 —— 哪天有人把
    // PkTransform 的取值器改成 constexpr，文本闸门当场 FAIL，而这里的对照
    // 说明"分得开"这件事本身是成立的（不是因为 harness 根本测不了）。
    constexpr PkPointF p(1, 2);
    constexpr PkRectF r(1, 2, 3, 4);
    constexpr PkRect ri(1, 2, 3, 4);
    PK_COMPARE(p.x(), 1.0);
    PK_COMPARE(r.width(), 3.0);
    PK_COMPARE(ri.width(), 3);

    // Transform 侧只能是运行期的。
    const PkTransform t(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const double v = t.m11();
    PK_COMPARE(v, 1.0);
}

int run_transform_tests()
{
    PkTransformCase tc;
    const char *argv[] = {"test_pkgeometry"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
