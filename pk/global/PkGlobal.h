#ifndef PK_GLOBAL_PKGLOBAL_H
#define PK_GLOBAL_PKGLOBAL_H

// std::isnan / std::numeric_limits 依赖。⚠ 在 coexist 探针 TU 里本头会被包进
// 匿名 namespace，这两行必须由该 TU 在 namespace 之外先 include（include guard
// 让这里的二次 include 空转），否则 std 会被卷进 (anonymous)::std。
#include <cmath>
#include <limits>

// ---------------------------------------------------------------------------
// R-18 的标量地基：qreal / 整数别名 / qAbs / qMin / qMax / qBound / qRound /
// qFuzzyCompare / qFuzzyIsNull / qFloor / qCeil / qNextPowerOfTwo / qIsNaN /
// qInf / qQNaN / Q_UNUSED / Q_ASSERT。
//
// **逐字照抄自真 Qt 5.15.7**（/mnt/ssd-disk/liyang/projects/krita-ci-env/_install/
// include/QtCore/qglobal.h、qmath.h、qalgorithms.h、qnumeric.h，
// QT_VERSION_STR "5.15.7"），来源行号标在各项上方。对齐口径与 R-03 相同：
// 与 Qt 的任何行为差异默认都是缺陷 —— 所以 Qt 那些看着像 bug 的地方也照抄
// （qRound 对负半值向 +∞ 取整、int(d+0.5) 让 0.49999999999999994 进位到 1、
// qAbs(-0.0) 返回 -0.0 而不是 +0.0），并在 tests/test_global.cpp 里逐条钉住。
//
// 本头是 R-18 线给整个剥离项目提供的**标量唯一权威**：R-11 的 pk/test 带了
// 一份标量子集，本头用「让位」机制与它共存，任意 include 顺序都不重定义。
// R-03 的 pk/geometry 原来自带一份标量子集，R-18 已折叠进本头——它的
// PkGlobal.h 只剩薄转发头，标量与 Qt 枚举都从这里来。机制全文见下方
// 「与 pk/test 的共存」。
//
// 全局作用域、无 namespace：与 R-03 同因 —— compat 垫片靠 `#define QRect PkRect`
// 这类改写工作，而 Krita 里有 `class QRect;` 前置声明，套 namespace 就废。
// 唯一例外是本头里的 `namespace Qt { enum … }`：那是 geometry 成员签名要用的
// 限定名（`Qt::AspectRatioMode` / `Qt::Axis`），不套 namespace 反而对不上。
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// 与 pk/test/compat/QtGlobal（R-11）的共存
//
// 两份标量源都可能落进同一个 TU：
//   · pk/test 的 compat/QtGlobal：qAbs（函数模板，条件 `t >= T(0)`）+ 两个
//     #define qFuzzyCompare→pkFuzzyCompare、qFuzzyIsNull→pkFuzzyIsNull。
//   · 本头：上面「交付面」的全部，用下方机制让位。
// （pk/geometry 原来自带一份标量子集，R-18 已折叠进本头——它的 PkGlobal.h
//   现在是薄转发头，不再有独立的 qAbs/qRound/qIsNaN 等实现，因此也没有
//   PK_GEOMETRY_PKGLOBAL_H 这条可探测痕迹了。）
//
// 让位机制（**逐项**，不是整段让位）：
//
//   ① 检测 pk/test 是否已落地：它唯一可探测的痕迹是宏 qFuzzyCompare /
//      qFuzzyIsNull（它的 qAbs 是普通函数模板，无宏痕迹）。
//   ② qAbs：pk/test 与本头都提供同名同签名的函数模板，重复定义是硬错误 →
//      只要 pk/test 已落地就让位。
//   ③ qFuzzyCompare/qFuzzyIsNull：pk/test 的 #define 已把函数名改写（本头若再
//      定义函数，函数体里的 qFuzzyCompare 会被当场改写坏）→ pk/test 已落地就
//      让位。
//   ④ qreal/整数别名/uchar/ushort/uint/ulong：typedef 重复声明同一类型合法，
//      无需让位，无条件定义。
//   ⑤ qRound/qMin/qMax/qBound/pkQtFuzzy*/qIsNaN/qInf/qFloor/qCeil/
//      qNextPowerOfTwo/qQNaN：pk/test 不提供这些名字，无条件定义。
//   ⑥ Q_UNUSED/Q_ASSERT：#if !defined 防重，无条件。
//
// **顺序无关为什么成立**：compat 垫片链（见 compat/QtGlobal）强制「pk/test →
// 本头」的落地顺序，本头检测时 pk/test 必已可见。真实调用点写
// `#include <QtGlobal>` 只命中两份 compat 中靠 -I 最近的一份，那份垫片自己把
// 另一份拉进来 —— 于是无论 -I 怎么排，本头总在最后落地、总能让位。两条
// include 路径各有一个 TU 在 tests/coexist_*.cpp 里编译并核对取值。
//
// 让位时的语义差（核对过，不构成行为差异）：
//   · pk/test 的 qAbs 写作 `t >= T(0)`、Qt 写作 `t >= 0`，对全部算术类型等价
//     （含 -0.0：`-0.0 >= T(0)` 与 `-0.0 >= 0` 都为真，两边都原样返回 -0.0）；
//   · pkFuzzyCompare 用 std::fabs/std::fmin，Qt 用 qAbs/qMin。只有实参含 NaN 时
//     fmin 与 qMin 的取值不同，而那种情况下两边的 <= 都被 NaN 拉成 false。
// 这条「语义等价」不是口头断言：tests/coexist.h 的探针在让位路径上取到的值
// 全部与真 Qt 一致，pk/test 漂离 Qt 时测试会变红。
// ---------------------------------------------------------------------------
#if defined(qFuzzyCompare) || defined(qFuzzyIsNull)
#  define PK_GLOBAL_SCALARS_FROM_PKTEST
#endif

// qglobal.h:279-283 —— QT_COORD_TYPE 是嵌入式配置项，桌面与 Android 都不定义。
typedef double qreal;

// qglobal.h:232-257。64 位在 Linux 上是 long long（Q_OS_WIN 非 GNU 才是 __int64）。
typedef signed char qint8;                  /* 8 bit signed */
typedef unsigned char quint8;               /* 8 bit unsigned */
typedef short qint16;                       /* 16 bit signed */
typedef unsigned short quint16;             /* 16 bit unsigned */
typedef int qint32;                         /* 32 bit signed */
typedef unsigned int quint32;               /* 32 bit unsigned */
typedef long long qint64;                   /* 64 bit signed */
typedef unsigned long long quint64;         /* 64 bit unsigned */
typedef qint64 qlonglong;
typedef quint64 qulonglong;

// qglobal.h:273-276。Qt 也提供 uchar/ushort/uint/ulong；ulong 在 Qt 里同样存在
//（qglobal.h:276，本头登记为「照抄 Qt，不是 C 语义补充」）。
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

// 让位给真 Qt（R-34，R-35 放宽守卫口径）：real Qt 的对应头已进 TU（各自 include
// guard 宏定义）时，本头里与 Qt 同名的一切（qAbs/qRound/qMin/qMax/qBound/qIsNull/
// qFuzzyCompare/qFuzzyIsNull/qFloor/qCeil/qNextPowerOfTwo/qIsNaN/qInf/qQNaN 与
// namespace Qt 枚举族）全部让位——libs/global 基线测试经 kis_debug.h 拉本头，与真
// Qt qglobal.h/qmath.h/qnumeric.h/qnamespace.h 重定义。
//
// ⚠ R-35 守卫口径（2026-08-21）：`!QT_CORE_LIB` 不够——主树编译行**全局**带
// -DQT_CORE_LIB，但 TU 不一定 include 真 Qt 头（S-04 的 KisColorimetryUtils.cpp 经
// PkVectorND.h 拉本头、无 Qt 头）。只看 QT_CORE_LIB 会让 pk 版让位、真 Qt 又不在场，
// qAbs/qMin/qRound/qIsNull 解析不到（pkQtFuzzy* 与 pk/geometry 内联代码编译失败）。
// 所以每组改用 `!QT_CORE_LIB || !<真 Qt 对应头的 include guard>`：对应头在场就让位
// （与 R-34 同），不在场（含 -DQT_CORE_LIB 无真 Qt 头）就由 pk 提供。各组的真 Qt
// 对应头：qglobal.h（QGLOBAL_H；qAbs/qRound/qMin/qMax/qBound/qIsNull/qFuzzy*）、
// qmath.h（QMATH_H；qFloor/qCeil）、qalgorithms.h（QALGORITHMS_H；qNextPowerOfTwo）、
// qnumeric.h（QNUMERIC_H；qIsNaN/qInf/qQNaN）、qnamespace.h（QNAMESPACE_H；namespace
// Qt 枚举族）。mixed TU 必须「Qt 头在前」，反序（pk 先、Qt 后）会让 Qt 头无条件
// 重定义——同 qFloor/qCeil 已登记的约定。
//
// typedef（qreal/qint8…）重复声明同一类型合法、Q_UNUSED/Q_ASSERT 自带 #if !defined
// 防重，均无需让位。pkQtFuzzyCompare/pkQtFuzzyIsNull 是 pk 自有名、真 Qt 没有，
// **不**让位（geometry 头内部调用它们），只依赖 qAbs/qMin——真 Qt qglobal.h 在场时
// 解析到真 Qt 的实现，不在场时由本头守卫内提供，两条路径语义等价（公式逐字同源，
// 见「语义等价」注释）。
#if !defined(QT_CORE_LIB) || !defined(QGLOBAL_H)
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
// 后不会。qRound64 实测 0 调用点（R-03 口径），不做。
constexpr inline int qRound(double d)
{ return d >= 0.0 ? int(d + 0.5) : int(d - double(int(d-1)) + 0.5) + int(d-1); }
constexpr inline int qRound(float d)
{ return d >= 0.0f ? int(d + 0.5f) : int(d - float(int(d-1)) + 0.5f) + int(d-1); }

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
#endif // !defined(QT_CORE_LIB) || !defined(QGLOBAL_H)

// qglobal.h:900-917。相对误差，右端取 qMin(|p1|, |p2|)：任何一侧是 0 时永远
// 不成立（两个方向都是 false）。double 的相对阈值 1e-12、float 的 1e-5。
//
// **公式住在 pkQt* 这组名字里，而不是 qFuzzy* 里** —— 这不是风格，是必需：
// `qFuzzyCompare`/`qFuzzyIsNull` 在「pk/test 那份垫片先进 TU」的路径上是
// **两个 #define**（→ pkFuzzyCompare / pkFuzzyIsNull），凡是写这两个名字的
// 函数体都会被预处理器当场改写。本头的 qFuzzyCompare/qFuzzyIsNull 只是转发，
// 公式只有这一份（pkQt*），geometry 经转发头命中的也是这份（R-18 折叠后不再
// 有第二份公式可漂移）。内部一律走 pkQt*，宏改写不到，语义钉死在 Qt 上。
constexpr inline bool pkQtFuzzyCompare(double p1, double p2)
{ return (qAbs(p1 - p2) * 1000000000000. <= qMin(qAbs(p1), qAbs(p2))); }
constexpr inline bool pkQtFuzzyCompare(float p1, float p2)
{ return (qAbs(p1 - p2) * 100000.f <= qMin(qAbs(p1), qAbs(p2))); }
constexpr inline bool pkQtFuzzyIsNull(double d) { return qAbs(d) <= 0.000000000001; }
constexpr inline bool pkQtFuzzyIsNull(float f) { return qAbs(f) <= 0.00001f; }

#if !defined(QT_CORE_LIB) || !defined(QGLOBAL_H)
// qglobal.h:930 —— `qIsNull(float)` = **精确零**比较（`f == 0.0f`），不是模糊
// 比较。与 `qFuzzyIsNull`（阈值 1e-5）是**两个名字、两套语义**：QVector2D/3D/4D
// 的 isNull() 与 toVector2DAffine/toVector3DAffine 用的是 `qIsNull`（精确零），
// normalized() 的"已是单位向量/零向量"判定用的是 `qFuzzyIsNull`（模糊）。R-21 T3
// 的对拍实锤这个区别：`1e-6f` 在 `qIsNull` 眼里是 false、在 `qFuzzyIsNull` 眼里
// 是 true（探针实测 `qIsNull(1e-6f)=0`、`qFuzzyIsNull(1e-6f)=1`），两侧对拍在
// isNull/toVector2DAffine 上分家——照 Qt 的精确零。本文件头「范围」注释里原来
// 把 qIsNull 标"不做"（当时实测调用点 0），R-21 T3 解出三个向量的真实调用点后
// 补上。不设 pkQt 前缀版：qIsNull 不被 pk/test 的宏改写（那边只 #define
// qFuzzyIsNull/qFuzzyCompare），没有公式漂移风险。
constexpr inline bool qIsNull(float f) { return f == 0.0f; }
constexpr inline bool qIsNull(double d) { return d == 0.0; }

// 对外的 Qt 名字只是转发，公式不重复第二遍（两份公式必然漂移）。
#if !defined(qFuzzyCompare) && !defined(qFuzzyIsNull)
constexpr inline bool qFuzzyCompare(double p1, double p2) { return pkQtFuzzyCompare(p1, p2); }
constexpr inline bool qFuzzyCompare(float p1, float p2) { return pkQtFuzzyCompare(p1, p2); }
constexpr inline bool qFuzzyIsNull(double d) { return pkQtFuzzyIsNull(d); }
constexpr inline bool qFuzzyIsNull(float f) { return pkQtFuzzyIsNull(f); }
#endif

#endif // !defined(QT_CORE_LIB) || !defined(QGLOBAL_H) —— qIsNull/qFuzzy* 段至此
#if !defined(QT_CORE_LIB) || !defined(QNAMESPACE_H)

// ---------------------------------------------------------------------------
// qnamespace.h:1235-1239（AspectRatioMode）、qnamespace.h:1386-1390（Axis），
// **逐字照抄**（枚举名与顺序都不能动：调用点写的是 `Qt::KeepAspectRatio` 这类
// 限定名，取值 0/1/2 还会经 QSize::scaled 的 `mode == Qt::IgnoreAspectRatio`
// 比较进入行为）。实测真 Qt 5.15.7：IgnoreAspectRatio=0 KeepAspectRatio=1
// KeepAspectRatioByExpanding=2，XAxis=0 YAxis=1 ZAxis=2，sizeof=4。
//
// **这是全项目仅有的两个真 `namespace` 之一**（另一个是 pk/namespace 的
// `PkNamespace.h`，两者分权共构完整 Qt 枚举集：本头提供 AspectRatioMode/Axis/
// SizeMode/FillRule/GlobalColor/TransformationMode，pk/namespace 提供其余枚举，
// 同一 TU 并集可见）。与「全局 Pk 前缀、不引 namespace」那条不冲突：那条针对的
// 是我们自己的类型（compat 垫片靠 `#define QRect PkRect`，而 Krita 里有
// `class QRect;` 前置声明），而这两个枚举在调用点上**本来就是 `Qt::` 限定的**，
// 不套 namespace 反而对不上。
//
// 为什么住 pk/global 而不是 pk/geometry 或 compat/：geometry 的 PkSize.h /
// PkTransform.h 成员签名要用它们（`scaled(const PkSize &, Qt::AspectRatioMode)`、
// `rotate(qreal, Qt::Axis)`），而几何头**不许依赖 compat/**（对拍的 -I 里绝不能
// 有 compat，否则两侧解析成同一个类型）。R-18 折叠后 geometry 经转发头命中这里，
// 与 Qt 的形态一致（qsize.h 自己 #include <QtCore/qnamespace.h>）。
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

// qnamespace.h:1185-1187 —— `Qt::SizeMode`，逐字照抄
enum SizeMode {
    AbsoluteSize,
    RelativeSize
};

// ---------------------------------------------------------------------------
// qnamespace.h:1352-1355（R-21 T2）—— `Qt::FillRule`，逐字照抄
// （OddEvenFill=0 WindingFill=1，普通枚举无显式取值、按声明顺序编号）。
//
// **取值经真 Qt 5.15.7 探针实测确认**（不是凭 Qt 文档记忆硬编）：
//   g++ -I$QT/include/QtCore probe.cpp -lQt5Core，打印
//   `(int)Qt::OddEvenFill` / `(int)Qt::WindingFill` / `sizeof(Qt::FillRule)`
//   → `OddEvenFill=0 WindingFill=1 sizeof=4`，与头文件声明顺序一致。
//
// 真实调用点 ≥15 处，全部经 `PkPolygonF::containsPoint(PkPointF, Qt::FillRule)`
// 落到这里（`kis_algebra_2d.cpp:74,260,1393,2446,2508`、
// `kis_cage_transform_worker.cpp:121,290`、
// `kis_grid_interpolation_tools.h:270,300,448`、
// `KisBezierGradientMesh.cpp:38`、`PerspectiveAssistant.cc:68,127,171`、
// `PerspectiveEllipseAssistant.cc:601`、`kis_free_transform_strategy.cpp:212`、
// `kis_perspective_transform_strategy.cpp:167`、`SvgTextTool.cpp:807,810,821`、
// `CutThroughShapeStrategy.cpp:142` 等）——判据①「一项不少」直接要求。
// `setFillRule`/`Qt::FillRule` 在 `QPainterPath`（`KoPathShape` 等）上的用量
// **不算在 R-21 内**——那是 R-22（`QPainterPath`）的事，这里只做
// `PkPolygonF::containsPoint` 这一个消费者。
//
// 放 PkGlobal.h 而不是 pk/geometry/PkPolygon.h：与 AspectRatioMode/Axis 同一条
// 理由（`namespace Qt` 只在这一处出现，几何头**不许依赖 compat/**）。
// ---------------------------------------------------------------------------
enum FillRule {
    OddEvenFill,
    WindingFill
};

// ---------------------------------------------------------------------------
// qnamespace.h:75-96 —— `Qt::GlobalColor`，**逐字照抄**（R-15 交接：`PkImage`
// 的 `fill(Qt::GlobalColor)` 需要这个类型，签名要用，理由与 AspectRatioMode/
// Axis 完全同构——几何头/`pk/image` 头都不许依赖 `compat/`，对拍要求两侧真的
// 分别 include 各自的头，混进 compat 会让对拍恒等）。
//
// 完整枚举序数照抄自真 Qt 5.15.7（`$QT/include/QtCore/qnamespace.h:75-96`）：
//   color0=0 color1=1 black=2 white=3 darkGray=4 gray=5 lightGray=6 red=7
//   green=8 blue=9 cyan=10 magenta=11 yellow=12 darkRed=13 darkGreen=14
//   darkBlue=15 darkCyan=16 darkMagenta=17 darkYellow=18 transparent=19
//
// **全部 20 个值都声明**（枚举常量声明成本趋近于零，缺一个会让任何
// `switch` 全覆盖 Qt 枚举值的调用点在替换时编不过——与 `pk/image` 里
// `QImage::Format` 全量声明同一个理由）。但 `PkImage::fill(GlobalColor)`
// 内部转换表**只保证实测有真实调用点的 5 个值精确**（`white black red gray
// transparent`，R-15 plan `docs/superpowers/plans/R-15.md` 的 grep 用量表：
// 10 个文件用到 `red`/`transparent`/`gray`/`white`）+ `black` 顺手做（枚举里
// 排在中间，不做反而要在转换表里挖洞）。这 6 个的精确 RGBA 已用真 Qt 探针
// 实测（同一份 plan 文档「GlobalColor 精确 RGBA」一节）：
//   white=(255,255,255,255) black=(0,0,0,255) red=(255,0,0,255)
//   gray=(160,160,164,255)  ← 不是 (128,128,128)！Qt 的经典坑，darkGray 才是
//   transparent=(0,0,0,0)   darkGray=(128,128,128,255)
// 其余 14 个值（`color0/color1/lightGray/blue/green/cyan/magenta/yellow/
// darkRed/darkGreen/darkBlue/darkCyan/darkMagenta/darkYellow`）真实调用点
// 为 0，`PkImage::fill()` 遇到它们可以不保证精确颜色（判据①：一项不多）。
// ---------------------------------------------------------------------------
enum GlobalColor {
    color0,
    color1,
    black,
    white,
    darkGray,
    gray,
    lightGray,
    red,
    green,
    blue,
    cyan,
    magenta,
    yellow,
    darkRed,
    darkGreen,
    darkBlue,
    darkCyan,
    darkMagenta,
    darkYellow,
    transparent
};

// ---------------------------------------------------------------------------
// qnamespace.h:1381-1384 —— `Qt::TransformationMode`，**逐字照抄**（R-15
// 交接：`PkImage::scaled()`/`transformed()` 的形参类型，序数 FastTransformation=0
// SmoothTransformation=1，真 Qt 5.15.7 实测确认）。两个值都有真实调用点
// （R-15 plan 用量表：`.scaled()` 5 文件 7 处、`.transformed()` 4-5 文件，
// Fast/Smooth 两个值都被用到），全量声明（只有两个值，没有"不做"的空间）。
// ---------------------------------------------------------------------------
enum TransformationMode {
    FastTransformation,
    SmoothTransformation
};
}
#endif // !defined(QT_CORE_LIB) || !defined(QNAMESPACE_H) —— namespace Qt 段至此（真 Qt qnamespace.h 在场则让位）

#if !defined(QT_CORE_LIB) || !defined(QMATH_H)
// qmath.h:68-76。语义是 int(floor(v))（向 -∞）与 int(ceil(v))（向 +∞）。
// ⚠ 逐字照抄 `int(floor(v))` 写不了 constexpr（std::floor 在 C++17 非 constexpr），
// 这里用等价公式 `int(v) - (v < int(v))` / `int(v) + (v > int(v))`（int(v) 是向
// 零截断，负半值差 1）。实测真 Qt 5.15.7：qFloor(-0.5)==-1、qFloor(-2.7)==-3、
// qCeil(2.3)==3、qCeil(-0.5)==0，tests/test_global.cpp 全部钉住。不实现
// `-int(-v)` 这种捷径——它对 2.3 给 2 而真 Qt 给 3，是行为差异。
//
// ⚠ 修复轮（R-18 task-2 review Important 1）：±inf/NaN 的 int(v) 越界是 UB，
// x86-64 cvttsd2si 恰好给 INT_MIN —— 真 Qt 的 int(floor(±inf))/int(ceil(±inf)) 也
// 是 INT_MIN，两侧一致。旧版在 -inf 上走 `pkTruncated-1` 回绕成 INT_MAX、在 +inf
// 上走 `pkTruncated+1` 成 INT_MIN+1，与 Qt 分家。这里对非有限值**原样返回 int(v)**
// （= INT_MIN）；有限输入永不进 guard，行为逐位不变。guard 用 v!=v / ±max 比较
// 而不是 std::isfinite —— C++17 下 std::isfinite 非 constexpr，比较式是。
constexpr inline int qFloor(qreal v)
{
    const int pkTruncated = int(v);
    if (v != v || v > std::numeric_limits<double>::max() || v < -std::numeric_limits<double>::max())
        return pkTruncated;   // 非有限值：原样截断（x86 → INT_MIN），同 Qt int(floor(±inf))
    return (v < pkTruncated) ? pkTruncated - 1 : pkTruncated;
}
constexpr inline int qCeil(qreal v)
{
    const int pkTruncated = int(v);
    if (v != v || v > std::numeric_limits<double>::max() || v < -std::numeric_limits<double>::max())
        return pkTruncated;   // 非有限值：原样截断（x86 → INT_MIN），同 Qt int(ceil(±inf))
    return (v > pkTruncated) ? pkTruncated + 1 : pkTruncated;
}
#endif // !defined(QT_CORE_LIB) || !defined(QMATH_H)
// ⚠ 让位守卫（R-34 终审）：qmath.h 不被 qglobal.h 拉（只有 <QtMath>/QtCore umbrella
// 拉它）。real-Qt 在场但没 include <QtMath> 的 TU 里，真 Qt 的 qFloor/qCeil 不可见，
// pk 版仍应可用——所以守卫是 `!QT_CORE_LIB || !QMATH_H`（QMATH_H 是真 Qt qmath.h 的
// include guard；mixed TU 按「Qt 头在前」顺序，Qt 先定义则 pk 让位、Qt 未定义则 pk
// 提供）。反序（pk 先、Qt 后）会让 Qt qmath.h 无条件重定义，同 README 登记的约定。

#if !defined(QT_CORE_LIB) || !defined(QALGORITHMS_H)
// qmath.h:247-258 的「非内置 clz」分支（逐字照抄）。语义：返回**严格大于** v 的
// 最小 2 的幂，v==0 → 1；v ≥ 2^31（含 0x80000000）时 OR 级联把低 31 位全填成 1、
// ++v 回绕到 0（实测真 Qt 5.15.7 确认，内置 clz 路径同样给 0）。所以 v=1024 → 2048
// 而不是 1024——名字叫 NextPowerOfTwo，不含等值。
constexpr inline quint32 qNextPowerOfTwo(quint32 v)
{
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    ++v;
    return v;
}
#endif // !defined(QT_CORE_LIB) || !defined(QALGORITHMS_H)
// qNextPowerOfTwo 在真 Qt qalgorithms.h（同样不被 qglobal.h 拉），守卫同 qFloor 一条。

// qnumeric.h:48-59。真 Qt 里这些是 Q_CORE_EXPORT 的非 inline 函数（实现在
// qnumeric_p.h，就是 std::isnan 与 std::numeric_limits）。R-03 的 geometry 侧
// 曾照此形态把定义放 PkGlobal.cpp；R-18 折叠后那份 .cpp 已无用（本头按 R-18
// 交付面做 constexpr inline）。std::isnan 在 libstdc++ 的 <cmath> 里按 constexpr
// 处理，下面编译与测试都过了。
// float 重载不是摆设：qIsNaN(float) 按 float 精度判，实参提升到 double 后取值
// 一致但语义不同，Krita 里 float 版有调用点。
#if !defined(QT_CORE_LIB) || !defined(QNUMERIC_H)  // 真 Qt qnumeric.h（qglobal.h:1303 拉它）在场则让位
constexpr inline bool qIsNaN(double d) { return std::isnan(d); }
constexpr inline bool qIsNaN(float f) { return std::isnan(f); }
constexpr inline double qInf() { return std::numeric_limits<double>::infinity(); }

// qnumeric.h:58。quiet NaN，std::numeric_limits<double>::quiet_NaN()。无冲突，无条件。
constexpr inline double qQNaN() { return std::numeric_limits<double>::quiet_NaN(); }

#endif // !defined(QT_CORE_LIB) || !defined(QNUMERIC_H)

// PkGlobal.cpp 实现：fprintf(stderr, "ASSERT: %s in file %s, line %d\n") + abort()。
void pk_qt_assert(const char *what, const char *file, int line);

// qglobal.h:117。
#if !defined(Q_UNUSED)
#  define Q_UNUSED(x) (void)x;
#endif

// qglobal.h:855-859。真 Qt 的条件是 `QT_NO_DEBUG && !QT_FORCE_ASSERTS`，本头按
// R-18 交付面用 `QT_NO_DEBUG || NDEBUG`（NDEBUG 是标准 release 记号）。Debug 构建
// 下 NDEBUG 未定义 → Q_ASSERT 激活，触发路径走 pk_qt_assert（SIGABRT）。
#if !defined(Q_ASSERT)
#  if defined(QT_NO_DEBUG) || defined(NDEBUG)
#    define Q_ASSERT(cond) ((void)0)
#  else
#    define Q_ASSERT(cond) ((cond) ? static_cast<void>(0) : pk_qt_assert(#cond, __FILE__, __LINE__))
#  endif
#endif

#endif // PK_GLOBAL_PKGLOBAL_H
