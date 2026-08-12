#ifndef PK_GEOMETRY_PKGLOBAL_H
#define PK_GEOMETRY_PKGLOBAL_H

// ---------------------------------------------------------------------------
// R-03 的标量工具：qreal / qAbs / qMin / qMax / qBound / qRound /
// qFuzzyCompare / qFuzzyIsNull / qIsNaN / qInf。
//
// **逐字抄自真 Qt 5.15.7**（/mnt/ssd-disk/liyang/projects/krita-ci-env/_install/
// include/QtCore/qglobal.h 与 qnumeric.h，QT_VERSION_STR "5.15.7"），来源行号
// 标在各项上方。对齐口径：与 Qt 的任何行为差异默认都是缺陷 —— 所以 Qt 那些
// 看着像 bug 的地方也照抄（qRound 对负半值向 +∞ 取整、int(d+0.5) 让
// 0.49999999999999994 进位到 1、qAbs(-0.0) 返回 -0.0 而不是 +0.0），并在
// tests/test_global.cpp 里逐条钉住，免得以后有人"顺手修正"。
//
// 范围：只做实测有调用点的项。qRound64 实测 0 次 → 不做；qIsNull / qIsInf /
// qIsFinite / qQNaN / qSNaN / qFpClassify / qFloatDistance 同理不在本头。
//
// 全局作用域、无 namespace：compat 垫片靠 `#define QRect PkRect` 这类改写工作，
// 而 Krita 里有 `class QRect;` 前置声明，套 namespace 这个技巧就废。
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// 与 pk/test/compat/QtGlobal（R-11 交付）的共存
//
// 那份垫片也提供 qAbs（函数模板）与 qFuzzyCompare / qFuzzyIsNull（指向
// pkFuzzyCompare / pkFuzzyIsNull 的 #define）。试接时编译行会同时有
// -I pk/test/compat 与 -I pk/geometry/compat，两份垫片可能落进同一个 TU。
//
// 「两份共用同一个 include guard 宏名、让后来者空转」这条路走不通：pk/test 那份
// 用的是 #pragma once，认的是文件身份，两个不同文件各自都会落地一次；而它不在
// R-03 的 locks 里，不能改。于是用两个方向的机制：
//
//   ① 本头检测 pk/test 那份唯一可探测的痕迹 —— 宏 qFuzzyCompare。已经生效就
//      整段让位，不重复定义 qAbs / qFuzzyCompare / qFuzzyIsNull（重复定义函数
//      模板是硬错误；而那个 #define 会把本头的函数名当场改写成 pkFuzzyCompare，
//      与 PkTestCompare.h 里的真身撞签名）。覆盖「pk/test 那份先进 TU」。
//   ② pk/geometry/compat/QtGlobal 先把 pk/test 那份 #include 进来（#pragma once
//      之后再解析到它就空转），再包本头。覆盖「pk/geometry 那份先进 TU」。
//
// **机制 ② 生效的前提是「compat/ 的每一个垫片都先包 compat/QtGlobal」这条纪律，
// 不是「compat/QtGlobal 自己被谁包」。** compat/QRect / QPoint / QSize / QTransform
// 这些垫片直接包各自的 Pk 头 → 本头，少了那行 include 就整个绕开机制 ②：真实
// 调用点里 `#include <QRect>` 在前、`#include <QtGlobal>` 在后（如
// libs/global/KisRectsGrid.h:10 + libs/global/kis_assert.h:10）时，本头先自己
// 定义 qAbs，pk/test 那份随后再定义一次 —— "redefinition of qAbs" 硬错。
// **新增 compat 垫片必须照做**；这同时也是对真 Qt 的忠实复刻（Qt 的每个公开头
// 都以 #include <QtCore/qglobal.h> 开头）。
//
// 三条 include 路径各有一个 TU 在 tests/coexist_*.cpp 里编译并核对取值，
// 其中 coexist_compat_rect_first.cpp 走的就是上面那条真实调用点顺序，
// 兼做「compat/QRect 少了那行 include 就变红」的回归守卫。
//
// 让位时的语义差（两处都不构成行为差异，核对过）：
//   · pk/test 的 qAbs 写作 `t >= T(0)`、Qt 写作 `t >= 0`，对全部算术类型等价
//     （含 -0.0：`-0.0 >= T(0)` 与 `-0.0 >= 0` 都为真，两边都原样返回 -0.0）；
//   · pkFuzzyCompare 用 std::fabs/std::fmin，Qt 用 qAbs/qMin。只有实参含 NaN 时
//     fmin 与 qMin 的取值不同，而那种情况下两边的 <= 都被 NaN 拉成 false。
// 这条「语义等价」不是断言：tests/coexist.h 的探针把让位路径上的取值（含
// qFuzzyCompare 的零侧语义）逐条钉住，pk/test 漂离 Qt 时测试会变红。
// ---------------------------------------------------------------------------
#if defined(qFuzzyCompare) || defined(qFuzzyIsNull)
#  define PK_GLOBAL_SCALARS_FROM_PKTEST
#endif

// qglobal.h:279-283 —— QT_COORD_TYPE 是嵌入式配置项，桌面与 Android 都不定义。
typedef double qreal;

#ifndef PK_GLOBAL_SCALARS_FROM_PKTEST
// qglobal.h:657-658。⚠ 条件是 `t >= 0` 而不是 `t > 0`：-0.0 >= 0 为真，所以
// qAbs(-0.0) **原样返回 -0.0**（signbit 仍是 1，1.0/qAbs(-0.0) == -inf）。
// 写成 `t > 0` 会把零号规范成 +0.0 —— 那是真实的行为差异，会经 1/x、atan2、
// copysign 扩散出去。实测真 Qt 5.15.7 确认，tests/test_global.cpp 用 signbit 钉住。
template <typename T>
constexpr inline T qAbs(const T &t) { return t >= 0 ? t : -t; }
#endif

// qglobal.h:660-663。**不是** std::round：负半值向 +∞ 取整（qRound(-0.5) == 0、
// qRound(-1.5) == -1），且 int(d + 0.5) 让 0.49999999999999994 进位到 1。
// float 重载不是摆设：它按 float 精度做加法，0.49999997f 会进位而提升到 double
// 后不会。qRound64 实测 0 调用点，不做。
constexpr inline int qRound(double d)
{ return d >= 0.0 ? int(d + 0.5) : int(d - double(int(d - 1)) + 0.5) + int(d - 1); }
constexpr inline int qRound(float d)
{ return d >= 0.0f ? int(d + 0.5f) : int(d - float(int(d - 1)) + 0.5f) + int(d - 1); }

// qglobal.h:670-677。返回 const T& 是 Qt 的签名，照抄——改成按值返回会给大对象
// 加拷贝，也会让 `const T &r = qMin(a, b)` 这类调用点的语义变化。
// 由此而来的陷阱：实参是临时量（字面量）时，返回的引用只在那条 full-expression
// 内有效，跨语句用就悬垂。tests 里对 qBound 的字面量调用都先拷进具名变量。
template <typename T>
constexpr inline const T &qMin(const T &a, const T &b) { return (a < b) ? a : b; }
template <typename T>
constexpr inline const T &qMax(const T &a, const T &b) { return (a < b) ? b : a; }
template <typename T>
constexpr inline const T &qBound(const T &min, const T &val, const T &max)
{ return qMax(min, qMin(max, val)); }

// qglobal.h:900-917。相对误差，右端取 qMin(|p1|, |p2|)：任何一侧是 0 时永远
// 不成立（两个方向都是 false）。double 的相对阈值 1e-12、float 的 1e-5。
//
// **公式住在 pkQt* 这组名字里，而不是 qFuzzy* 里** —— 这不是风格，是必需：
// `qFuzzyCompare`/`qFuzzyIsNull` 在「pk/test 那份垫片先进 TU」的路径上是
// **两个 #define**（→ pkFuzzyCompare / pkFuzzyIsNull），凡是写这两个名字的
// 函数体都会被预处理器当场改写。PkPointF/PkSizeF/PkRectF 的 operator== 照
// Qt 抄的正是这两个名字，一旦被改写，几何类型的相等语义就静默换成 pk/test
// 那套（阈值相同但零侧分支不同，且 pk/test 不在 R-03 的 locks 里、R-11 随时
// 可能动它）。几何类型内部一律走 pkQt*，宏改写不到，语义钉死在 Qt 上。
// 这条不是口头断言：tests/test_point.cpp 的 fuzzyEqualityIsMacroProof 与
// tests/coexist.h 的探针一起钉住。
constexpr inline bool pkQtFuzzyCompare(double p1, double p2)
{ return (qAbs(p1 - p2) * 1000000000000. <= qMin(qAbs(p1), qAbs(p2))); }
constexpr inline bool pkQtFuzzyCompare(float p1, float p2)
{ return (qAbs(p1 - p2) * 100000.f <= qMin(qAbs(p1), qAbs(p2))); }
constexpr inline bool pkQtFuzzyIsNull(double d) { return qAbs(d) <= 0.000000000001; }
constexpr inline bool pkQtFuzzyIsNull(float f) { return qAbs(f) <= 0.00001f; }

#ifndef PK_GLOBAL_SCALARS_FROM_PKTEST
// 对外的 Qt 名字只是转发，公式不重复第二遍（两份公式必然漂移）。
constexpr inline bool qFuzzyCompare(double p1, double p2) { return pkQtFuzzyCompare(p1, p2); }
constexpr inline bool qFuzzyCompare(float p1, float p2) { return pkQtFuzzyCompare(p1, p2); }
constexpr inline bool qFuzzyIsNull(double d) { return pkQtFuzzyIsNull(d); }
constexpr inline bool qFuzzyIsNull(float f) { return pkQtFuzzyIsNull(f); }
#endif

// ---------------------------------------------------------------------------
// qnamespace.h:1235-1239 —— `Qt::AspectRatioMode`，**逐字照抄**（枚举名与顺序
// 都不能动：调用点写的是 `Qt::KeepAspectRatio` 这个限定名，取值 0/1/2 还会经
// QSize::scaled 的 `mode == Qt::IgnoreAspectRatio` 比较进入行为）。
// 实测真 Qt 5.15.7：IgnoreAspectRatio=0 KeepAspectRatio=1
// KeepAspectRatioByExpanding=2，sizeof=4、底层类型无符号。
//
// **这是全项目唯一一个真 `namespace`**，与「全局 Pk 前缀、不引 namespace」那条
// 不冲突：那条针对的是我们自己的类型（compat 垫片靠 `#define QRect PkRect`，
// 而 Krita 里有 `class QRect;` 前置声明），而这个枚举在调用点上**本来就是
// `Qt::` 限定的**，不套 namespace 反而对不上。
//
// 为什么放 PkGlobal.h 而不是 compat/QtGlobal：PkSize.h 的成员签名要用它
// （`scaled(const PkSize &, Qt::AspectRatioMode)`），而几何头**不许依赖
// compat/**（对拍的 -I 里绝不能有 compat，否则两侧解析成同一个类型）。
// 放这里 compat/QtGlobal 与 compat/QSize 都能经 PkGlobal.h 拿到它，
// 与 Qt 的形态一致（qsize.h 自己 #include <QtCore/qnamespace.h>）。
//
// 实测用量（口径：R-03 文件集 3 325 个文件）：`Qt::AspectRatioMode` 这个类型名
// 22 次，`Qt::IgnoreAspectRatio` 12 次、`Qt::KeepAspectRatio` 18 次、
// `Qt::KeepAspectRatioByExpanding` 3 次 —— 都 > 0，所以不退化成"只留默认参数
// 所需的定义"。`qnamespace.h` 的其余几百个枚举一概不做（判据①）。
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// qnamespace.h:1386-1390 —— `Qt::Axis`，同样逐字照抄（XAxis=0 YAxis=1 ZAxis=2）。
//
// ⚠ **这一条与上面的 AspectRatioMode 不同：它的直接调用点实测是 0。**
//   口径：fork 全仓（含 tests/ 与 benchmarks/）
//   `git grep -o 'Qt::[XYZ]Axis' -- '*.cpp' '*.h' '*.cc' | wc -l` → **0**；
//   `git grep -n 'rotate([^)]*Qt::[XYZ]Axis'` → **0 处**。
//
// 那为什么还是做了：它不是一个独立交付项，而是 `PkTransform::rotate` 与
// `rotateRadians` **签名里的形参类型**（`rotate(qreal a, Qt::Axis axis = Qt::ZAxis)`）。
// 判据①「一项不多」拦的是「多实现一个 API」，而这里的两个选项是：
//   ① 照 Qt 的签名做 → 多一个 0 用量的枚举；
//   ② 把形参砍掉、只留 `rotate(qreal)` → 这是**签名偏离**，将来任何一处
//      `t.rotate(90, Qt::YAxis)` 会在替换时编不过，而"哪些会漏"在替换之前测不出来。
// 选 ①，理由与 R-03.md §「运算符：按 Qt 头文件全集实现」那条登记在案的范围偏离
// 完全同形（同样是"签名面不能按调用点 grep 归属"），**同样逐条登记进 README 请
// reviewer 判**。非 Z 轴那条路径本身也不是摆设：它走 `result * *this` 且把
// m_type 直接钉成 TxProject，与 Z 轴路径是两套算法，对拍里各有各的 tag。
// ---------------------------------------------------------------------------
namespace Qt {
enum AspectRatioMode {
    IgnoreAspectRatio,
    KeepAspectRatio,
    KeepAspectRatioByExpanding
};

enum Axis {
    XAxis,
    YAxis,
    ZAxis
};
}

// qnumeric.h:48 与 qnumeric.h:59。Qt 里这两个是 Q_CORE_EXPORT 的**非 inline**
// 函数（实现在 qnumeric_p.h 的 qt_is_nan / qt_inf，就是 std::isnan 与
// std::numeric_limits<double>::infinity()），照同样的形态放 PkGlobal.cpp：
// 改成 inline 会让它们跟着调用方的 -ffast-math 一起被优化掉，而 Qt 的不会。
// 实测用量 qIsNaN 19 次、qInf 1 次，实参都是 double，因此不做 float 重载
//（float 实参隐式提升到 double，取值一致）。
bool qIsNaN(double d);
double qInf();

#endif // PK_GEOMETRY_PKGLOBAL_H
