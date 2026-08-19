#include "PkMargins.h"

// ⚠ **这个系统头必须在 oracle/geometry_difftest.cpp 顶部的系统头区里也出现过**
// —— 理由与 PkRect.cpp / PkLine.cpp 顶部同一条纪律。PkMargins.h/.cpp 没有
// 任何 out-of-line 成员（qmargins.h 全部是 inline 的），所以本文件**不需要
// 被 oracle 的 `namespace pkoracle {}` #include 进去**——那个技巧只在类型
// 有编在别处的非 inline 成员时才必要（PkRect.cpp/PkTransform.cpp/PkLine.cpp
// 都有，本文件没有）。留着本 .cpp 只是为了给下面这批 static_assert 一个
// 落脚的 TU，与 PkPoint.cpp/PkSize.cpp/PkRect.cpp 的角色一致。
#include <type_traits>

// 布局：PkMargins 四个 int，PkMarginsF 四个 qreal，无虚表、无 padding。
static_assert(sizeof(PkMargins) == 4 * sizeof(int), "PkMargins 必须是四个 int");
static_assert(std::is_trivially_copyable<PkMargins>::value, "PkMargins 必须可平凡拷贝");
static_assert(std::is_standard_layout<PkMargins>::value, "PkMargins 必须是标准布局");

static_assert(sizeof(PkMarginsF) == 4 * sizeof(qreal), "PkMarginsF 必须是四个 qreal");
static_assert(std::is_trivially_copyable<PkMarginsF>::value, "PkMarginsF 必须可平凡拷贝");
static_assert(std::is_standard_layout<PkMarginsF>::value, "PkMarginsF 必须是标准布局");

// constexpr 面：默认构造与全部访问器必须能在常量表达式里跑。
static_assert(PkMargins().isNull(), "默认 PkMargins 必须 isNull");
static_assert(PkMargins(1, 2, 3, 4).left() == 1 && PkMargins(1, 2, 3, 4).bottom() == 4,
              "四个分量必须逐个对上，顺序是 left/top/right/bottom");
static_assert(PkMarginsF().isNull(), "默认 PkMarginsF 必须 isNull");

// ⚠ **PkMargins → PkMarginsF 必须是隐式的**（Qt 的 QMarginsF(const
// QMargins&) 非 explicit），反向**不存在**这样的构造（Qt 没有吃 QMarginsF
// 的 QMargins 构造，只有 toMargins()）。
static_assert(std::is_convertible<PkMargins, PkMarginsF>::value,
              "PkMargins → PkMarginsF 必须能隐式提升");
static_assert(!std::is_convertible<PkMarginsF, PkMargins>::value,
              "PkMarginsF → PkMargins 必须**不能**隐式转换 —— 只能走 toMargins()");

// relaxed constexpr（Q_DECL_RELAXED_CONSTEXPR → constexpr，README 偏离清单
// 第 5 条同型）：mutator 也要能在常量表达式里跑。
static_assert([] { PkMargins m(1, 2, 3, 4); m += 5; return m.left(); }() == 6,
              "operator+=(int) 必须四个分量各加");
static_assert([] { PkMargins m(4, 4, 4, 4); m *= 2; return m.right(); }() == 8,
              "operator*=(int) 必须四个分量各乘");

// 算术取值：与 README「与决策文档/实施计划的差异」登记的一条实测纠正配套
// ——真 Qt 5.15.7 的 QMargins **没有 operator|**（PkMargins.h 头部注释与
// R-21 T1 报告已经交代），这里反着钉一条「PkMargins 没有 operator|」的编译期
// 证据：若日后有人手滑给它加了 operator|，这条 static_assert 不会失败（它测
// 的是加法），但 PkMargins.h 头部的长注释与 T1 报告是唯一的书面记录。
static_assert(PkMargins(1, 2, 3, 4) + PkMargins(10, 10, 10, 10) == PkMargins(11, 12, 13, 14),
              "operator+(Margins,Margins) 必须逐分量相加");
static_assert(-PkMargins(1, 2, 3, 4) == PkMargins(-1, -2, -3, -4),
              "一元 operator- 必须逐分量取负");
