// geometry_difftest.cpp —— pk/geometry 与真 Qt5 的逐输入对拍。
//
// **这份是 R-03 Task 3..6（Size / Rect / RectF / Transform）要往里加节的骨架。**
// 抄自 `.exec/replacement/R-01.difftest.cpp`（QString ↔ PkString）。
// 骨架的六件事不要改，各族只往「逐 API 对拍」与「输入集」两节里加东西。
//
// ── 输出契约（run_oracle.sh 读这两种行，别的都是给人看的）────────────────
//     DIFF total=<N> mismatch=<M>      恰好一行，程序末尾打
//     DIFFTAG <api> <tag> <count>      一类差异一行
// **退出码必须是 0，即使 M>0** —— 已声明的偏离不算失败，退出码只表示"跑完没崩"。
//
// ── 为什么替代品要塞进 namespace pkoracle ──────────────────────────────
// PkGlobal.h 与 Qt 的 qglobal.h 在**同一个全局作用域**里定义了签名完全相同的
// qAbs / qRound / qMin / qMax / qBound / qFuzzyCompare / qFuzzyIsNull —— 两个头
// 直接进同一个 TU 是硬性的重定义错误，对拍根本编不出来。
// 解法：`namespace pkoracle { #include "PkPoint.h" }`。PkGlobal.h 与 PkPoint.h
// **一个 #include 都没有**（只互相包），所以整包干净地落进 pkoracle::，
// 里面的 qRound / qAbs 仍然是替代品自己那份实现，不是 Qt 的。
//   · 若哪天几何头开始 #include <cstring> 之类，必须把那个 include 提到本文件
//     顶部的系统头区，不能让它落进 pkoracle::（那会造出 pkoracle::std）。
//   · 这个写法顺带把「compat 垫片被拉进 -I」查得比 static_assert 还死：垫片一旦
//     命中，`#include <QPoint>` 会先把 PkPoint.h 的 include guard 点掉，
//     namespace 里那次 include 就整个空转，pkoracle::PkPointF 根本不存在 → 编译失败。
//     下面的 #error 与 static_assert 是同一件事的另外两道，三道都留着，成本为零。
//
// ── 跑出 mismatch=0 是警报，不是好消息 ─────────────────────────────────
// 但 R-03 与 R-01 有一处根本不同：PkString 是重写，**PkPoint 是逐字照抄 qpoint.h**，
// 所以"真实差异 = 0"才是正确结果，geometry.deviation 里也就没有一条真实偏离。
// 于是判别力不能再靠"看到差异"来证明，本文件用两件事顶上：
//   ① **canary**：三条故意不相等的比对，走的是与真实 API 完全相同的 rec()/比较
//      原语/tag 构造路径。它们必须出现在输出里 —— 一旦少一条，说明比较管道
//      （bit 比较、计数、tag）被写死或被优化没了，run_oracle.sh 直接 FAIL。
//   ② **注入自证**：往被测类型里塞真 bug，必须产生**未声明**的 tag。
//
// ── tag 的两条硬规则（违反了整件事白做，理由紧跟在每条下面）──────────
//   规则一：tag 必须由「触发这次差异的**输入形态**」参与构造，不能是「每个 API
//           一个字面量常量」。R-01 第一版那么写，注入三组真 bug 全部绿灯通过。
//   规则二：tag 的判定谓词**不能比 .deviation 里那句理由宽**。理由说「两侧都失败」，
//           谓词里就必须出现 !ok；理由说「退化矩形」，谓词就不能是「任何矩形」。
//   规则三：**每一个已实现的重载都要有自己的 rec()**，一个都不许合并、一个都不许漏。
//           Task 3 复评实测：cmp_sizef_scaled 少写了 `SF::scale(w,h)` 那一条 rec，
//           于是把 PkSizeF::scale(qreal,qreal,mode) 整个改坏（永远走
//           IgnoreAspectRatio）之后，**93 630 039 次比对一条都没红、run_oracle.sh
//           退出码 0 放行**。转发链「A 转发到 B、B 有 rec」不算覆盖 —— 坏掉的正
//           可能是 A 那一跳。合并也不行（`setWidth/setHeight` 挤一条 rec 时，
//           tag 说不清是哪个重载分的家）。
//           **落地方式**：每族写完对拍后列一张「已实现重载 vs 对应 rec()」对照表
//           （逐条对着头文件的声明数，不是对着记忆数），有一格空的就是漏了。
//           **已知残留（Task 2 的 Point 族，有意不动）**：`setX/setY`、`rx/ry`、
//           `F::setX/setY`、`F::rx/ry` 四条仍是两个重载挤一条 rec。它们不是覆盖
//           漏洞（两个 mutator 在同一次往返里都被调到，任一个坏掉 same_pt 就分家），
//           只是归因粗；不拆是为了让「Point 族 total=35 569 662」这条 Task 2 基线
//           保持逐字不变，好继续当"我没动到 Point"的自证。**新写的族一律按规则三
//           拆到底**（Size 族已拆）。

// ── 真 Qt 侧 + 系统头（都必须在 namespace 之外）────────────────────────
#include <QPoint>
#include <QSize>
#include <QRect>
#include <QTransform>
#include <QLine>
#include <QMargins>
#include <QPolygon>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QVector>
#include <QString>
#include <QtGlobal>
#include <QMessageLogContext>

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// 垫片一旦混进 -I，<QPoint> 会解析到 compat/QPoint，而那份是两个 #define。
// 两侧于是解析成同一个类型，跑出来必然零差异且看不出破绽。成本三行。
#if defined(QPoint) || defined(QPointF) || defined(QSize) || defined(QSizeF) \
    || defined(QRect) || defined(QRectF) || defined(QTransform) \
    || defined(QLine) || defined(QLineF) || defined(QMargins) || defined(QMarginsF) \
    || defined(QPolygon) || defined(QPolygonF)
#  error "对拍两侧解析成了同一个类型 —— -I 里混进了 pk/geometry/compat"
#endif

// ── 替代品侧 ───────────────────────────────────────────────────────────
namespace pkoracle {
#include "PkPoint.h"
// ⚠ **PkGlobal.cpp 也要进来**：`qIsNaN`/`qInf` 是 out-of-line 的（照 Qt 的
// 形态，qnumeric.h 只声明，本 Task 的 PkLineF::intersects 是第一个在对拍里
// 真正调用 qIsNaN 的地方——之前五族都没撞上这个符号）。libpkgeometry.a 里
// 那份定义的是 `::qIsNaN`，本 TU 需要的是 `pkoracle::qIsNaN`——两个不同的
// 符号，链不上。纪律同上：PkGlobal.cpp 的系统头（<cmath> 与 <limits>）必须
// 在上面的系统头区里已经出现过（两个都有）。
#include "PkGlobal.cpp"
#include "PkSize.h"
// ⚠ **PkSize.cpp 也要进来**：两个 scaled(const Pk*&, mode) 是 out-of-line 的
//（照 Qt 的形态，QSize::scaled 定义在 qsize.cpp 里）。libpkgeometry.a 里那份
// 定义的是 `::PkSize::scaled`，而本 TU 需要的是 `pkoracle::PkSize::scaled`
// —— 两个不同的符号，链不上（第一版就是这么炸的）。
// 纪律与 namespace 那条完全一样：**PkSize.cpp 里的系统头必须在上面的系统头区
// 里已经出现过**（它只 #include <type_traits>，上面有），否则会造出 pkoracle::std。
// 顺带好处：PkSize.cpp 里那批 static_assert 也在这个 TU 里编一遍。
#include "PkSize.cpp"
#include "PkRect.h"
// ⚠ 同理 **PkRect.cpp 也要进来**：normalized / operator| / operator& /
// contains ×2 / intersects 六个是 out-of-line 的（照 Qt 的形态，实现在 qrect.cpp
// 里）。libpkgeometry.a 里那份定义的是 `::PkRect::normalized`，本 TU 需要的是
// `pkoracle::PkRect::normalized` —— 两个不同的符号，链不上。
// 纪律同上：PkRect.cpp 只 #include <type_traits>（上面的系统头区已有）。
#include "PkRect.cpp"
#include "PkTransform.h"
// ⚠ 同理 **PkTransform.cpp 也要进来**：这一族几乎全是 out-of-line 的（照 Qt 的
// 形态，qtransform.h 只留了取值器与四个标量运算符，其余编在 libQt5Gui 里）。
// libpkgeometry.a 里那份定义的是 `::PkTransform::map`，本 TU 需要的是
// `pkoracle::PkTransform::map` —— 两个不同的符号，链不上。
// 纪律同上：PkTransform.cpp 只 #include <cmath> 与 <type_traits>，上面的系统头
// 区两个都有。顺带好处：它尾部那批 static_assert（枚举位标志、布局、noexcept 面）
// 也在这个 TU 里编一遍。
#include "PkTransform.cpp"
#include "PkLine.h"
// ⚠ 同理 **PkLine.cpp 也要进来**：fromPolar/length/angle/setAngle/angleTo/
// unitVector/intersects 七个是 out-of-line 的（照 Qt 的形态，qline.cpp 编在
// libQt5Core.so 里，本机没有那份源码，公式靠独立差分脚本逼出来，见
// PkLine.cpp 顶部注释）。libpkgeometry.a 里那份定义的是
// `::PkLineF::length`，本 TU 需要的是 `pkoracle::PkLineF::length`——两个
// 不同的符号，链不上。
// 纪律同上：PkLine.cpp 只 #include <cmath> 与 <type_traits>，上面的系统头
// 区两个都有。顺带好处：它尾部那批 static_assert（布局、隐式提升、
// IntersectType 取值）也在这个 TU 里编一遍。
#include "PkLine.cpp"
// PkMargins.h **不需要配 .cpp**：两个类全部是 inline 的（qmargins.h 本身
// 也全是 inline，没有 out-of-line 成员要从 libpkgeometry.a 里"抢"符号）。
#include "PkMargins.h"
// ⚠ **PkPolygon.h 自己 `#include "../container/PkVector.h"`**（R-21 T2：
// PkPolygon 继承 PkVector<PkPoint>），那条 include 是**引号相对路径**、按
// PkPolygon.h 自身的物理目录解析（pk/geometry/../container = pk/container），
// 与本 TU 的 -I 列表无关——所以 run_oracle.sh 的 INCS 不需要专门加
// pk/container。但 PkVector.h 这一整棵树用到的系统头（<algorithm> <cassert>
// <cstddef> <initializer_list> <iterator> <memory> <utility>，PkArrayData.h/
// PkArrayContainer.h/PkVector.h/PkList.h 各自的一部分）必须已经在本文件顶部
// 的系统头区出现过——纪律与 PkRect.cpp/PkTransform.cpp 那几条完全一样，漏一个
// 就会在 pkoracle:: 里首次看见那个头、造出 pkoracle::std。
#include "PkPolygon.h"
// ⚠ **PkPolygon.cpp 也要进来**：boundingRect / containsPoint / toPolygon /
// translate(const PkPointF&) / translated(const PkPointF&) /
// PkPolygonF(const PkRectF&) 六个是 out-of-line 的（照 Qt 的形态，qpolygon.cpp
// 编在 libQt5Gui.so 里，本机没有源码，取自上游同版本标签）。libpkgeometry.a
// 里那份定义的是 `::PkPolygonF::boundingRect`，本 TU 需要的是
// `pkoracle::PkPolygonF::boundingRect`——两个不同的符号，链不上。纪律同上：
// PkPolygon.cpp 只 #include <type_traits>（上面的系统头区已有）。
#include "PkPolygon.cpp"
// ⚠ 同理 **PkVectorND.cpp 也要进来**：length/normalized/normalize/dotProduct/
// crossProduct/normal/distanceTo*/toVector*Affine 这一批是 out-of-line 的（照
// Qt 的形态，qvectornd.cpp 编在 libQt5Gui.so 里，本机没有源码，float/double
// 精度不对称经反汇编 .so 实测确认，见 PkVectorND.h 文件头）。libpkgeometry.a
// 里那份定义的是 `::PkVector2D::length`，本 TU 需要的是
// `pkoracle::PkVector2D::length`——两个不同的符号，链不上。纪律同上：
// PkVectorND.cpp 只 #include <cmath>（上面的系统头区已有）。
#include "PkVectorND.cpp"
}

using PkPoint  = pkoracle::PkPoint;
using PkPointF = pkoracle::PkPointF;
using PkSize   = pkoracle::PkSize;
using PkSizeF  = pkoracle::PkSizeF;
using PkRect   = pkoracle::PkRect;
using PkRectF  = pkoracle::PkRectF;
using PkTransform = pkoracle::PkTransform;
using PkLine   = pkoracle::PkLine;
using PkLineF  = pkoracle::PkLineF;
using PkMargins  = pkoracle::PkMargins;
using PkMarginsF = pkoracle::PkMarginsF;
using PkPolygon  = pkoracle::PkPolygon;
using PkPolygonF = pkoracle::PkPolygonF;
using PkVector2D = pkoracle::PkVector2D;
using PkVector3D = pkoracle::PkVector3D;
using PkVector4D = pkoracle::PkVector4D;
// PkPolygon(const PkVector<PkPoint>&) 的真实调用点需要在 pkoracle:: 之外也
// 拼得出 `PkVector<T>` 这个名字——别名模板，不是新类型，等价于
// `pkoracle::PkVector<T>`。
template <typename T>
using PkVector = pkoracle::PkVector<T>;

static_assert(!std::is_same<QPoint,  PkPoint >::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
static_assert(!std::is_same<QPointF, PkPointF>::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
static_assert(sizeof(QPointF) == sizeof(PkPointF), "两侧布局不一致");
static_assert(!std::is_same<QSize,  PkSize >::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
static_assert(!std::is_same<QSizeF, PkSizeF>::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
static_assert(sizeof(QSizeF) == sizeof(PkSizeF), "两侧布局不一致");
// ⚠ 枚举也必须是两个不同的类型（替代品那份落在 pkoracle::Qt 里）。若哪天
// 替代品改成 `using Qt::AspectRatioMode = ::Qt::AspectRatioMode` 之类的转发，
// 下面这条会立刻炸 —— 那种写法等于把对拍的 mode 一侧接到真 Qt 上，白比。
static_assert(!std::is_same<Qt::AspectRatioMode,
                            pkoracle::Qt::AspectRatioMode>::value,
              "AspectRatioMode 两侧解析成了同一个类型");
static_assert((int)Qt::IgnoreAspectRatio == (int)pkoracle::Qt::IgnoreAspectRatio
              && (int)Qt::KeepAspectRatio == (int)pkoracle::Qt::KeepAspectRatio
              && (int)Qt::KeepAspectRatioByExpanding
                     == (int)pkoracle::Qt::KeepAspectRatioByExpanding,
              "AspectRatioMode 的枚举取值两侧不一致");
static_assert(!std::is_same<QRect, PkRect>::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
static_assert(sizeof(QRect) == sizeof(PkRect), "两侧布局不一致");
static_assert(!std::is_same<QRectF, PkRectF>::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
static_assert(sizeof(QRectF) == sizeof(PkRectF), "两侧布局不一致");
// 提升方向两侧必须一致：整数矩形能隐式变浮点矩形，反向不行。
// 若哪天替代品给 PkRect 加了吃 PkRectF 的构造，下面第二条会当场炸 ——
// 那种"多一档能力"的偏离对拍本身看不见（它只比取值）。
static_assert(std::is_convertible<QRect, QRectF>::value
              == std::is_convertible<PkRect, PkRectF>::value,
              "PkRect → PkRectF 的隐式提升与 Qt 不一致");
static_assert(std::is_convertible<QRectF, QRect>::value
              == std::is_convertible<PkRectF, PkRect>::value,
              "PkRectF → PkRect 的隐式转换与 Qt 不一致");
static_assert(!std::is_same<QTransform, PkTransform>::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
// ⚠ **这里没有 sizeof 相等那一条**（Point/Size/Rect 三族都有），不是漏了：
// Qt5 的 QTransform 尾部有一个恒为 nullptr 的 `Private *d`（Qt6 已删），
// 替代品不留它，于是 sizeof 是 80 而 Qt 是 88。它不经任何 API 露出来，
// 登记在 PkTransform.h 头部与 README 偏离清单。下面这条把"差的正好是一个指针"
// 钉住 —— 哪天 Qt 那侧的布局变了（或者我们不小心多塞了字段），它会响。
static_assert(sizeof(QTransform) == sizeof(PkTransform) + sizeof(void *),
              "两侧布局差的应当恰好是 Qt5 那个 Private *d");
// 枚举也必须是两个不同的类型，取值必须逐档相同（**位标志，不是 0..5**）。
static_assert(!std::is_same<QTransform::TransformationType,
                            PkTransform::TransformationType>::value,
              "TransformationType 两侧解析成了同一个类型");
static_assert((int)QTransform::TxNone      == (int)PkTransform::TxNone
              && (int)QTransform::TxTranslate == (int)PkTransform::TxTranslate
              && (int)QTransform::TxScale     == (int)PkTransform::TxScale
              && (int)QTransform::TxRotate    == (int)PkTransform::TxRotate
              && (int)QTransform::TxShear     == (int)PkTransform::TxShear
              && (int)QTransform::TxProject   == (int)PkTransform::TxProject,
              "TransformationType 的枚举取值两侧不一致");
static_assert(!std::is_same<Qt::Axis, pkoracle::Qt::Axis>::value,
              "Qt::Axis 两侧解析成了同一个类型");
static_assert((int)Qt::XAxis == (int)pkoracle::Qt::XAxis
              && (int)Qt::YAxis == (int)pkoracle::Qt::YAxis
              && (int)Qt::ZAxis == (int)pkoracle::Qt::ZAxis,
              "Qt::Axis 的枚举取值两侧不一致");

// ── R-21 T1：Line / Margins 两族的两侧类型自证 ─────────────────────────────
static_assert(!std::is_same<QLine, PkLine>::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
static_assert(sizeof(QLine) == sizeof(PkLine), "两侧布局不一致");
static_assert(!std::is_same<QLineF, PkLineF>::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
static_assert(sizeof(QLineF) == sizeof(PkLineF), "两侧布局不一致");
// 提升方向：整数线段能隐式变浮点线段，反向不行（PkLineF 没有吃 QLineF 的
// QLine 构造，只有下面这一条正向）。
static_assert(std::is_convertible<QLine, QLineF>::value
              == std::is_convertible<PkLine, PkLineF>::value,
              "PkLine → PkLineF 的隐式提升与 Qt 不一致");
// 枚举也必须是两个不同的类型，取值必须逐档相同。
static_assert(!std::is_same<QLineF::IntersectType, PkLineF::IntersectType>::value,
              "IntersectType 两侧解析成了同一个类型");
static_assert((int)QLineF::NoIntersection == (int)PkLineF::NoIntersection
              && (int)QLineF::BoundedIntersection == (int)PkLineF::BoundedIntersection
              && (int)QLineF::UnboundedIntersection == (int)PkLineF::UnboundedIntersection,
              "IntersectType 的枚举取值两侧不一致");

static_assert(!std::is_same<QMargins, PkMargins>::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
static_assert(sizeof(QMargins) == sizeof(PkMargins), "两侧布局不一致");
static_assert(!std::is_same<QMarginsF, PkMarginsF>::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
static_assert(sizeof(QMarginsF) == sizeof(PkMarginsF), "两侧布局不一致");
static_assert(std::is_convertible<QMargins, QMarginsF>::value
              == std::is_convertible<PkMargins, PkMarginsF>::value,
              "PkMargins → PkMarginsF 的隐式提升与 Qt 不一致");

// ── R-21 T2：Polygon 族的两侧类型自证 ───────────────────────────────────────
//
// **没有 sizeof 相等的 static_assert**（Point/Size/Rect/Line/Margins 五族都
// 有）：`PkPolygon`/`PkPolygonF` 继承 `PkVector<T>`（COW，一个 shared_ptr 大小），
// 而真 `QPolygon`/`QPolygonF` 继承 `QVector<T>`（Qt5 的 QVector 是三个指针
// /引用计数头的经典实现），两侧容器实现本就不同源，sizeof 不构成任何契约——
// 这与 R-02（容器族）自己的对齐口径一致，不是本族特有的松口。
static_assert(!std::is_same<QPolygon, PkPolygon>::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
static_assert(!std::is_same<QPolygonF, PkPolygonF>::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");

// ═══ 计数与记录 ════════════════════════════════════════════════════════════

static long g_total = 0, g_mismatch = 0;
static std::map<std::string, long> g_tags;   // "<api> <tag>" -> **分家**次数（分子）
// 「该 tag 的谓词命中过多少次比对」= **分母**。
//
// 为什么需要它：geometry.deviation 第三列的额度规矩写着「那个数字要能说清为什么
// 恰好是这么多，不能填个大概」。光有分子说不清 —— 「分家 12 799 次」既可能是
// 「喂了 12 800 次几乎全分家」，也可能是「喂了 200 万次里只有这么点分家」，
// 两者对偏离范围的含义天差地别。有了分母，每一行都能读成
// **「命中 N 次里分家 M 次」**，N 与 M 各自漂移都会被额度闸门抓住（N 由
// run_oracle.sh 打出来给人看，M 是机器双向比较的那一列）。
//
// 代价：每条 rec 多一次 map 查找（键与分子共用同一个字符串）。
static std::map<std::string, long> g_tag_seen;
static long g_printed = 0;

// 规则三的**机器闸门**（Task 4 新增）。规则三是「每个已实现的重载都要有自己的
// rec()」，Task 3 之前只能靠人手列对照表 —— 而 Task 3 复评实测过：漏一个重载，
// 93 630 039 次比对一条都不红、run_oracle.sh exit=0 放行。
//
// 做法：rec() 顺手把见过的 api 名收进这个集合，程序末尾按 `APISEEN <name>`
// 逐行打出来；仓库里放一份人维护的期望清单（oracle/api_seen.expected 与
// oracle/rect_api.map），run_oracle.sh 做三向核对：
//   ① APISEEN 集合 == api_seen.expected（多一条少一条都 FAIL）
//   ② PkRect.h 类体里的每一条声明都在 rect_api.map 里有一行（**加了重载却
//      没写 rec() 会在这里被抓**——这正是规则三原来漏掉的那一半）
//   ③ rect_api.map 里的每个标签都真的出现在 APISEEN 里
// 多打的 APISEEN 行不影响既有 stdout 契约：契约只规定 DIFF/DIFFTAG 两种行有意义。
static std::set<std::string> g_apis;

static void rec(const char *api, bool same, const std::string &tag,
                const std::string &in, const std::string &qs, const std::string &ps)
{
    ++g_total;
    g_apis.insert(api);
    const std::string key = std::string(api) + " " + tag;
    ++g_tag_seen[key];              // 分母：谓词命中就记，不管同不同
    if (same) return;
    ++g_mismatch;
    ++g_tags[key];                  // 分子：只记分家
    if (g_printed < 40) {           // 明细只打前 40 条，run_oracle.sh 不读它
        ++g_printed;
        std::printf("MISMATCH: %s [%s] in=%s qt=%s pk=%s\n",
                    api, tag.c_str(), in.c_str(), qs.c_str(), ps.c_str());
    }
}

// ═══ 比较原语 ══════════════════════════════════════════════════════════════

// **位精确**比较 double：`==` 会把 +0/-0 判等、把 NaN 判不等，两者都不是我们要的。
// 几何类型里零号的符号位是真的会传播出去的（一元 +/-、manhattanLength、
// dotProduct 都保号），用 `==` 比等于把这一整类差异永久豁免。
static bool same_double(double a, double b)
{
    std::uint64_t ba, bb;
    std::memcpy(&ba, &a, sizeof ba);
    std::memcpy(&bb, &b, sizeof bb);
    if (ba == bb) return true;
    // NaN 的位模式两侧可以不同（载荷不保证），都是 NaN 就算同
    return (a != a) && (b != b);
}

static std::string dstr(double d)
{
    std::uint64_t b; std::memcpy(&b, &d, sizeof b);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.17g(0x%016llx)", d, (unsigned long long)b);
    return buf;
}
static std::string istr(long long v) { return std::to_string(v); }
static std::string bstr(bool b) { return b ? "true" : "false"; }

static std::string qstr(const QPoint &p)   { return "(" + istr(p.x()) + "," + istr(p.y()) + ")"; }
static std::string qstr(const PkPoint &p)  { return "(" + istr(p.x()) + "," + istr(p.y()) + ")"; }
static std::string qstr(const QPointF &p)  { return "(" + dstr(p.x()) + "," + dstr(p.y()) + ")"; }
static std::string qstr(const PkPointF &p) { return "(" + dstr(p.x()) + "," + dstr(p.y()) + ")"; }

static bool same_pt(const QPoint &q, const PkPoint &p)
{ return q.x() == p.x() && q.y() == p.y(); }
static bool same_ptf(const QPointF &q, const PkPointF &p)
{ return same_double(q.x(), p.x()) && same_double(q.y(), p.y()); }

static std::string qstr(const QSize &s)   { return istr(s.width()) + "x" + istr(s.height()); }
static std::string qstr(const PkSize &s)  { return istr(s.width()) + "x" + istr(s.height()); }
static std::string qstr(const QSizeF &s)  { return dstr(s.width()) + "x" + dstr(s.height()); }
static std::string qstr(const PkSizeF &s) { return dstr(s.width()) + "x" + dstr(s.height()); }

static bool same_sz(const QSize &q, const PkSize &p)
{ return q.width() == p.width() && q.height() == p.height(); }
static bool same_szf(const QSizeF &q, const PkSizeF &p)
{ return same_double(q.width(), p.width()) && same_double(q.height(), p.height()); }

// ── Rect ─────────────────────────────────────────────────────────────────
// ⚠ 矩形一律按**四个内部坐标**打印与比较，不按 x/y/w/h：后者在退化矩形上是
// 多对一的（(0,0,0,0) 与 (5,5,0,0) 的 x/y/w/h 里 w/h 都是 0，但它们不相等），
// 用 x/y/w/h 比会把一整类差异静默豁免。
// 取值走 left/top/right/bottom 四条**各自独立的一行取值器**而不是 getCoords：
// 拿 getCoords 当比较手段的话，"getCoords 自己坏了"就检测不出来了。
static std::string qstr(const QRect &r)
{ return "[" + istr(r.left()) + "," + istr(r.top()) + ","
              + istr(r.right()) + "," + istr(r.bottom()) + "]"; }
static std::string qstr(const PkRect &r)
{ return "[" + istr(r.left()) + "," + istr(r.top()) + ","
              + istr(r.right()) + "," + istr(r.bottom()) + "]"; }

static bool same_rect(const QRect &q, const PkRect &p)
{
    return q.left() == p.left() && q.top() == p.top()
        && q.right() == p.right() && q.bottom() == p.bottom();
}

// 两侧都用 setCoords 摆坐标建矩形：只有这样才够得到 (l,t,w,h) 构造不出来的
// 形态（x2 == x1-1 的"宽恰为 0"、x2 < x1-1 的"需要交换"）。setCoords 自己
// 也有一条 rec()，坏了会单独现形。
static QRect mkQ(int a, int b, int c, int d) { QRect r; r.setCoords(a, b, c, d); return r; }
static PkRect mkP(int a, int b, int c, int d) { PkRect r; r.setCoords(a, b, c, d); return r; }

// ── RectF ────────────────────────────────────────────────────────────────
// ⚠ 浮点矩形一律按**四个内部字段 x/y/w/h** 打印与比较，不按 left/top/right/
// bottom：后者带一次加法（right() == xp + w），拿它当比较手段会让"加法本身
// 的舍入差"与"字段存错了"混成一件事，也会把 -0.0 这类只在字段里看得见的差异
// 抹掉（-0.0 + 0.0 == +0.0）。四条取值器 x()/y()/width()/height() 各自独立、
// 一行、无算术，正好当地基 —— 与整数版拿 left/top/right/bottom 当地基同理。
// 比较走 same_double（**位**比较），不是 `==`：`==` 会把 +0/-0 判等、把 NaN
// 判不等，而这两类正是 PkRectF 里真的会传播出去的（normalized 不翻正 -0.0、
// 三谓词在 NaN 上互不为补）。
static std::string qstr(const QRectF &r)
{ return "[" + dstr(r.x()) + "," + dstr(r.y()) + ","
              + dstr(r.width()) + "," + dstr(r.height()) + "]"; }
static std::string qstr(const PkRectF &r)
{ return "[" + dstr(r.x()) + "," + dstr(r.y()) + ","
              + dstr(r.width()) + "," + dstr(r.height()) + "]"; }

static bool same_rectf(const QRectF &q, const PkRectF &p)
{
    return same_double(q.x(), p.x()) && same_double(q.y(), p.y())
        && same_double(q.width(), p.width()) && same_double(q.height(), p.height());
}

// 两侧都用 (l,t,w,h) 构造建矩形。**与整数版不同，这里不需要 setCoords**：
// QRectF 内部存的就是 x/y/w/h，这个构造直接摆四个字段，够得到全部内部状态
//（整数版必须走 setCoords，因为 (l,t,w,h) 构造做了 +w-1，摸不到 x2==x1-1
// 那一档）。构造自己也各有一条 rec()，坏了会单独现形。
static QRectF mkQF(double x, double y, double w, double h) { return QRectF(x, y, w, h); }
static PkRectF mkPF(double x, double y, double w, double h) { return PkRectF(x, y, w, h); }

// ═══ tag 谓词：**全部由输入的形态算出来**，没有一个是字面量常量 ═══════════

static bool nonFinite(double d) { return std::isinf(d) || std::isnan(d); }
static bool signedZero(double d) { return d == 0.0 && std::signbit(d); }
static bool subnormal(double d)
{ return d != 0.0 && !nonFinite(d) && std::fabs(d) < DBL_MIN; }
// 落在 int 值域之外 → int(d) 是 UB，两侧靠同一条指令给同一个答案
static bool outOfIntRange(double d)
{ return nonFinite(d) || !(d >= -2147483648.0 && d <= 2147483647.0); }
// 小数部分恰好是 ±0.5 —— qRound 的取整方向在这里才有分歧（负半值向 +∞）
static bool halfBoundary(double d)
{
    if (nonFinite(d) || std::fabs(d) > 1e15) return false;
    const double f = std::fabs(d - std::floor(d));
    return f == 0.5;
}
// int(d + 0.5) 的经典边界：比 0.5 小一个 ulp 却因为加法舍入而进位
static bool nearHalfUlp(double d)
{
    if (nonFinite(d)) return false;
    const double f = std::fabs(d - std::floor(d));
    return f != 0.5 && std::fabs(f - 0.5) < 1e-15;
}
static bool intExtremum(int v) { return v == INT_MIN || v == INT_MAX; }
static bool addOverflows(int a, int b)
{ long long s = (long long)a + b; return s < INT_MIN || s > INT_MAX; }
static bool subOverflows(int a, int b)
{ long long s = (long long)a - b; return s < INT_MIN || s > INT_MAX; }
static bool mulOverflows(int a, int b)
{ long long s = (long long)a * b; return s < INT_MIN || s > INT_MAX; }
// |x| 本身就溢出（qAbs(INT_MIN) 回绕），或者两个绝对值相加溢出
static bool manhattanOverflows(int x, int y)
{
    if (x == INT_MIN || y == INT_MIN) return true;
    long long s = (long long)std::llabs((long long)x) + std::llabs((long long)y);
    return s > INT_MAX;
}

// ── shapeOf*：通用形态分流（**Task 3–6 照抄这个约定**）────────────────────
//
// **约定：一个 API 的 shape 必须由它「全部参与分量」算出，取其中最特殊的那种
// 形态命名 —— 不是只取第一对分量。** 所以签名是 initializer_list 而不是两个
// 标量：调用点被迫把参与的分量一个不落地列出来，漏一个是看得见的，
// 而 `shapeOfD(ax, bx)` 那种写法漏掉 ay/by 是看不见的。
//
// 为什么这条是硬要求（Task 2 复评实测）：`F::operator+` 的四个分量里只往
// shapeOfD 喂了 x 那一对，注入「只在 **y** 是 NaN 时出错」的缺陷后，8 036 次
// 差异被贴成 `F::operator+ finite` —— 与真实根因完全相反的标签。今天白名单是
// 空的所以不影响检出，可一旦有人往 .deviation 写下 `<api> finite` 这样一行，
// 「y 分量上的非有限缺陷」这一整片就被永久白名单化，**正是规则二要防的形状**。
// Rect（二元 8 个分量）/ Transform（二元 18 个分量）照抄会成倍恶化。
//
// 优先级从最能解释差异的往下排。新增形态往前插时想清楚它是不是比下面几条更
// 能解释根因 —— 排序就是"这次差异该归咎于谁"的判断。
static std::string shapeOfD(std::initializer_list<double> vs)
{
    for (double v : vs) if (nonFinite(v)) return "nonfinite";
    for (double v : vs) if (signedZero(v)) return "signed-zero";
    for (double v : vs) if (subnormal(v)) return "subnormal";
    for (double v : vs) if (v == 0.0) return "zero";
    for (double v : vs) if (std::fabs(v) > 1e300) return "huge";
    return "finite";
}

static std::string shapeOfI(std::initializer_list<int> vs)
{
    for (int v : vs) if (intExtremum(v)) return "int-extremum";
    for (int v : vs) if (v != 0) return "ordinary";
    return "origin";
}

// 同时吃整数与浮点分量的 API（跨类型运算）：两族各取最特殊形态，拼起来。
// 只取其中一族会把另一族的根因整片盖掉 —— 与上面那条是同一个道理。
static std::string shapeOfMixed(std::initializer_list<int> is,
                                std::initializer_list<double> ds)
{ return shapeOfI(is) + "+" + shapeOfD(ds); }

// ═══ Point 族：逐 API 对拍 ═════════════════════════════════════════════════
//
// 每个 rec() 的 tag 都由**这一次的实参**算出来。加新 API 时照这个形状写：
// 先想清楚"这个 API 会在什么输入形态上跟 Qt 分家"，把那个形态做成谓词，
// 让它参与 tag；想不出来就写 shapeOfD/shapeOfI 那种通用形态分流 —— 但**绝不能**
// 写成一个与输入无关的常量（那等于把这个 API 整个退出对拍）。

// 无输入的 API（默认构造），口径同 cmp_size_constants。只跑一次。
// **Task 4 修复轮补**：M-4 把 PkPoint.h 接进规则三的机器闸门之后，
// `PkPoint()` / `PkPointF()` 两条声明在 point_api.map 里找不到任何标签 ——
// 也就是说这两个默认构造**从来没被对拍比过**（Task 2 漏了，Size 族有、Point
// 族没有）。闸门刚上线就抓到了一个真实缺口，这两条 rec 是补上它。
static void cmp_point_constants()
{
    rec("defaultCtor", same_pt(QPoint(), PkPoint()), "no-input",
        "QPoint()", qstr(QPoint()), qstr(PkPoint()));
    rec("F::defaultCtor", same_ptf(QPointF(), PkPointF()), "no-input",
        "QPointF()", qstr(QPointF()), qstr(PkPointF()));
}

static void cmp_point_unary(int x, int y)
{
    const QPoint  q(x, y);
    const PkPoint p(x, y);
    const std::string in = "(" + istr(x) + "," + istr(y) + ")";
    const std::string sh = shapeOfI({x, y});

    rec("x", q.x() == p.x(), sh, in, istr(q.x()), istr(p.x()));
    rec("y", q.y() == p.y(), sh, in, istr(q.y()), istr(p.y()));
    rec("isNull", q.isNull() == p.isNull(), sh, in, bstr(q.isNull()), bstr(p.isNull()));

    // manhattanLength 唯一会分家的形态是溢出（qAbs(INT_MIN) 回绕、两绝对值相加溢出）
    rec("manhattanLength", q.manhattanLength() == p.manhattanLength(),
        manhattanOverflows(x, y) ? "int-overflow" : "in-range",
        in, istr(q.manhattanLength()), istr(p.manhattanLength()));

    rec("operator-unary", same_pt(-q, -p),
        (x == INT_MIN || y == INT_MIN) ? "int-min-negation" : sh,
        in, qstr(-q), qstr(-p));
    rec("operator+unary", same_pt(+q, +p), sh, in, qstr(+q), qstr(+p));

    // setX/setY 与 rx()/ry()：出参一起比（rx 返回的是**引用**，改了要看得见）
    { QPoint q2 = q; PkPoint p2 = p; q2.setX(y); p2.setX(y); q2.setY(x); p2.setY(x);
      rec("setX/setY", same_pt(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QPoint q2 = q; PkPoint p2 = p; q2.rx() += 1; p2.rx() += 1; q2.ry() -= 1; p2.ry() -= 1;
      rec("rx/ry", same_pt(q2, p2),
          (x == INT_MAX || y == INT_MIN) ? "int-overflow" : sh,
          in, qstr(q2), qstr(p2)); }
}

static void cmp_point_binary(int ax, int ay, int bx, int by)
{
    const QPoint  qa(ax, ay), qb(bx, by);
    const PkPoint pa(ax, ay), pb(bx, by);
    const std::string in = "(" + istr(ax) + "," + istr(ay) + ")|("
                         + istr(bx) + "," + istr(by) + ")";
    // ⚠ **四个分量都参与**，不是只取 x 那一对（约定见 shapeOfI 上方）。
    const std::string sh = shapeOfI({ax, ay, bx, by});

    rec("operator+", same_pt(qa + qb, pa + pb),
        (addOverflows(ax, bx) || addOverflows(ay, by)) ? "int-overflow" : sh,
        in, qstr(qa + qb), qstr(pa + pb));
    rec("operator-", same_pt(qa - qb, pa - pb),
        (subOverflows(ax, bx) || subOverflows(ay, by)) ? "int-overflow" : sh,
        in, qstr(qa - qb), qstr(pa - pb));
    rec("operator==", (qa == qb) == (pa == pb), sh,
        in, bstr(qa == qb), bstr(pa == pb));
    rec("operator!=", (qa != qb) == (pa != pb), sh,
        in, bstr(qa != qb), bstr(pa != pb));
    { QPoint q2 = qa; PkPoint p2 = pa; q2 += qb; p2 += pb;
      rec("operator+=", same_pt(q2, p2),
          (addOverflows(ax, bx) || addOverflows(ay, by)) ? "int-overflow" : sh,
          in, qstr(q2), qstr(p2)); }
    { QPoint q2 = qa; PkPoint p2 = pa; q2 -= qb; p2 -= pb;
      rec("operator-=", same_pt(q2, p2),
          (subOverflows(ax, bx) || subOverflows(ay, by)) ? "int-overflow" : sh,
          in, qstr(q2), qstr(p2)); }

    // dotProduct：静态成员，不防溢出（乘法与加法都可能溢出）
    rec("dotProduct", QPoint::dotProduct(qa, qb) == PkPoint::dotProduct(pa, pb),
        (mulOverflows(ax, bx) || mulOverflows(ay, by)
         || addOverflows((int)((long long)ax * bx), (int)((long long)ay * by)))
            ? "int-overflow" : sh,
        in, istr(QPoint::dotProduct(qa, qb)), istr(PkPoint::dotProduct(pa, pb)));
}

// QPoint 的缩放：**三个重载走三条不同的路**（float 与 double 用不同精度做
// `v*f + 0.5`，int 根本不取整），所以三条都要单独对拍。
static void cmp_point_scale_double(int x, int y, double f)
{
    const QPoint  q(x, y);
    const PkPoint p(x, y);
    const std::string in = "(" + istr(x) + "," + istr(y) + ")*" + dstr(f);
    // 分歧只可能出在"乘出来的实数落在哪"：半值边界（取整方向）、越出 int 值域
    //（int(d) 是 UB）、非有限。其余落 in-range。
    const double px = (double)x * f, py = (double)y * f;
    const char *tag = (nonFinite(px) || nonFinite(py))            ? "nonfinite-product"
                    : (outOfIntRange(px) || outOfIntRange(py))    ? "product-out-of-int-range"
                    : (halfBoundary(px) || halfBoundary(py))      ? "half-boundary"
                    : (nearHalfUlp(px) || nearHalfUlp(py))        ? "near-half-ulp"
                                                                  : "in-range";
    rec("operator*(double)", same_pt(q * f, p * f), tag, in, qstr(q * f), qstr(p * f));
    rec("operator*(double,rev)", same_pt(f * q, f * p), tag, in, qstr(f * q), qstr(f * p));
    { QPoint q2 = q; PkPoint p2 = p; q2 *= f; p2 *= f;
      rec("operator*=(double)", same_pt(q2, p2), tag, in, qstr(q2), qstr(p2)); }

    const double dx = (double)x / f, dy = (double)y / f;
    const char *dtag = (f == 0.0)                                 ? "divisor-zero"
                     : (nonFinite(dx) || nonFinite(dy))           ? "nonfinite-quotient"
                     : (outOfIntRange(dx) || outOfIntRange(dy))   ? "quotient-out-of-int-range"
                     : (halfBoundary(dx) || halfBoundary(dy))     ? "half-boundary"
                     : (nearHalfUlp(dx) || nearHalfUlp(dy))       ? "near-half-ulp"
                                                                  : "in-range";
    rec("operator/", same_pt(q / f, p / f), dtag, in, qstr(q / f), qstr(p / f));
    { QPoint q2 = q; PkPoint p2 = p; q2 /= f; p2 /= f;
      rec("operator/=", same_pt(q2, p2), dtag, in, qstr(q2), qstr(p2)); }
}

static void cmp_point_scale_float(int x, int y, float f)
{
    const QPoint  q(x, y);
    const PkPoint p(x, y);
    const std::string in = "(" + istr(x) + "," + istr(y) + ")*f" + dstr(f);
    // ⚠ 按 **float** 精度算，不是 double —— 这正是两个重载存在的理由
    const float px = (float)x * f, py = (float)y * f;
    const char *tag = (nonFinite(px) || nonFinite(py))         ? "nonfinite-product"
                    : (outOfIntRange(px) || outOfIntRange(py)) ? "product-out-of-int-range"
                    : (halfBoundary(px) || halfBoundary(py))   ? "half-boundary"
                                                               : "in-range";
    rec("operator*(float)", same_pt(q * f, p * f), tag, in, qstr(q * f), qstr(p * f));
    rec("operator*(float,rev)", same_pt(f * q, f * p), tag, in, qstr(f * q), qstr(f * p));
    { QPoint q2 = q; PkPoint p2 = p; q2 *= f; p2 *= f;
      rec("operator*=(float)", same_pt(q2, p2), tag, in, qstr(q2), qstr(p2)); }
}

static void cmp_point_scale_int(int x, int y, int f)
{
    const QPoint  q(x, y);
    const PkPoint p(x, y);
    const std::string in = "(" + istr(x) + "," + istr(y) + ")*i" + istr(f);
    const char *tag = (mulOverflows(x, f) || mulOverflows(y, f)) ? "int-overflow" : "in-range";
    rec("operator*(int)", same_pt(q * f, p * f), tag, in, qstr(q * f), qstr(p * f));
    rec("operator*(int,rev)", same_pt(f * q, f * p), tag, in, qstr(f * q), qstr(f * p));
    { QPoint q2 = q; PkPoint p2 = p; q2 *= f; p2 *= f;
      rec("operator*=(int)", same_pt(q2, p2), tag, in, qstr(q2), qstr(p2)); }
}

static void cmp_pointf_unary(double x, double y)
{
    const QPointF  q(x, y);
    const PkPointF p(x, y);
    const std::string in = "(" + dstr(x) + "," + dstr(y) + ")";
    const std::string sh = shapeOfD({x, y});

    rec("F::x", same_double(q.x(), p.x()), sh, in, dstr(q.x()), dstr(p.x()));
    rec("F::y", same_double(q.y(), p.y()), sh, in, dstr(q.y()), dstr(p.y()));

    // isNull 用 qIsNull（d == 0.0）：-0.0 算 null、次正规数不算。零号与次正规
    // 正是这条唯一会分家的地方，所以让它们参与 tag。
    rec("F::isNull", q.isNull() == p.isNull(),
        (signedZero(x) || signedZero(y)) ? "signed-zero"
        : (subnormal(x) || subnormal(y)) ? "subnormal" : sh,
        in, bstr(q.isNull()), bstr(p.isNull()));

    rec("F::manhattanLength", same_double(q.manhattanLength(), p.manhattanLength()),
        sh, in, dstr(q.manhattanLength()), dstr(p.manhattanLength()));

    rec("F::operator-unary", same_ptf(-q, -p), sh, in, qstr(-q), qstr(-p));
    rec("F::operator+unary", same_ptf(+q, +p), sh, in, qstr(+q), qstr(+p));

    // toPoint 走 qRound。分歧形态：半值边界（取整方向）、越出 int 值域（UB）、
    // 差一个 ulp 的半值（int(d+0.5) 的进位）。
    rec("F::toPoint", same_pt(q.toPoint(), p.toPoint()),
        (outOfIntRange(x) || outOfIntRange(y)) ? "out-of-int-range"
        : (halfBoundary(x) || halfBoundary(y)) ? "half-boundary"
        : (nearHalfUlp(x) || nearHalfUlp(y))   ? "near-half-ulp"
        : (subnormal(x) || subnormal(y))       ? "subnormal" : "in-range",
        in, qstr(q.toPoint()), qstr(p.toPoint()));

    { QPointF q2 = q; PkPointF p2 = p; q2.setX(y); p2.setX(y); q2.setY(x); p2.setY(x);
      rec("F::setX/setY", same_ptf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QPointF q2 = q; PkPointF p2 = p; q2.rx() += 1.0; p2.rx() += 1.0;
      q2.ry() -= 1.0; p2.ry() -= 1.0;
      rec("F::rx/ry", same_ptf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
}

static void cmp_pointf_binary(double ax, double ay, double bx, double by)
{
    const QPointF  qa(ax, ay), qb(bx, by);
    const PkPointF pa(ax, ay), pb(bx, by);
    const std::string in = "(" + dstr(ax) + "," + dstr(ay) + ")|("
                         + dstr(bx) + "," + dstr(by) + ")";
    // ⚠ **四个分量都参与**，不是只取 x 那一对（约定见 shapeOfD 上方；只喂 x 时
    // 「只在 y 上出错」的缺陷会被贴成 finite —— 复评实测抓到 8 036 次错标）。
    const std::string sh = shapeOfD({ax, ay, bx, by});

    rec("F::operator+", same_ptf(qa + qb, pa + pb), sh, in, qstr(qa + qb), qstr(pa + pb));
    rec("F::operator-", same_ptf(qa - qb, pa - pb), sh, in, qstr(qa - qb), qstr(pa - pb));

    // ⚠ QPointF::operator== 是**模糊比较**，逐分量二选一：任一侧恰好是 0 走
    // 绝对阈值 fuzzyIsNull，否则走相对阈值 fuzzyCompare。两条分支上的分歧根因
    // 完全不同，所以走了哪条分支必须参与 tag —— 否则一条分支的偏离会把另一条
    // 一起罩住（规则二踩过的坑）。
    const bool zeroBranchX = (ax == 0.0 || bx == 0.0);
    const bool zeroBranchY = (ay == 0.0 || by == 0.0);
    const std::string eqtag =
        (nonFinite(ax) || nonFinite(bx) || nonFinite(ay) || nonFinite(by)) ? "nonfinite"
      : (zeroBranchX && zeroBranchY)                                       ? "zero-branch-both"
      : (zeroBranchX || zeroBranchY)                                       ? "zero-branch-mixed"
      : (subnormal(ax) || subnormal(bx) || subnormal(ay) || subnormal(by)) ? "subnormal"
                                                                           : "relative-branch";
    rec("F::operator==", (qa == qb) == (pa == pb), eqtag, in, bstr(qa == qb), bstr(pa == pb));
    rec("F::operator!=", (qa != qb) == (pa != pb), eqtag, in, bstr(qa != qb), bstr(pa != pb));

    { QPointF q2 = qa; PkPointF p2 = pa; q2 += qb; p2 += pb;
      rec("F::operator+=", same_ptf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QPointF q2 = qa; PkPointF p2 = pa; q2 -= qb; p2 -= pb;
      rec("F::operator-=", same_ptf(q2, p2), sh, in, qstr(q2), qstr(p2)); }

    rec("F::dotProduct",
        same_double(QPointF::dotProduct(qa, qb), PkPointF::dotProduct(pa, pb)),
        (nonFinite(ax) || nonFinite(ay) || nonFinite(bx) || nonFinite(by)) ? "nonfinite"
        : (signedZero(ax) || signedZero(ay) || signedZero(bx) || signedZero(by)) ? "signed-zero"
        : (subnormal(ax) || subnormal(ay) || subnormal(bx) || subnormal(by)) ? "subnormal"
        : (std::fabs(ax) > 1e150 || std::fabs(ay) > 1e150
           || std::fabs(bx) > 1e150 || std::fabs(by) > 1e150) ? "overflow-prone" : "finite",
        in, dstr(QPointF::dotProduct(qa, qb)), dstr(PkPointF::dotProduct(pa, pb)));
}

static void cmp_pointf_scale(double x, double y, double c)
{
    const QPointF  q(x, y);
    const PkPointF p(x, y);
    const std::string in = "(" + dstr(x) + "," + dstr(y) + ")*" + dstr(c);
    // 参与分量是 x、y 与标量 c 三个，一个都不能漏（约定见 shapeOfD 上方）。
    const std::string sh = shapeOfD({x, y, c});

    rec("F::operator*", same_ptf(q * c, p * c), sh, in, qstr(q * c), qstr(p * c));
    rec("F::operator*(rev)", same_ptf(c * q, c * p), sh, in, qstr(c * q), qstr(c * p));
    rec("F::operator/", same_ptf(q / c, p / c),
        (c == 0.0) ? "divisor-zero" : sh, in, qstr(q / c), qstr(p / c));
    { QPointF q2 = q; PkPointF p2 = p; q2 *= c; p2 *= c;
      rec("F::operator*=", same_ptf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QPointF q2 = q; PkPointF p2 = p; q2 /= c; p2 /= c;
      rec("F::operator/=", same_ptf(q2, p2), (c == 0.0) ? "divisor-zero" : sh,
          in, qstr(q2), qstr(p2)); }
}

// PkPoint → PkPointF 的隐式提升（Krita 里大量调用点靠这条），以及混合运算。
static void cmp_promotion(int x, int y, double cx, double cy)
{
    const QPoint  qi(x, y);
    const PkPoint pi(x, y);
    const QPointF  qf = qi;      // 隐式
    const PkPointF pf = pi;
    const std::string in = "(" + istr(x) + "," + istr(y) + ")+("
                         + dstr(cx) + "," + dstr(cy) + ")";
    // fromPoint / roundTrip 只有 (x,y) 参与；mixedAdd 四个分量都参与，所以它
    // 用跨族的 shapeOfMixed —— 只喂 (cx,cy) 会把 int 侧的根因整片盖掉。
    const std::string sh = shapeOfI({x, y});
    rec("F::fromPoint", same_ptf(qf, pf), sh, in, qstr(qf), qstr(pf));
    rec("F::mixedAdd", same_ptf(QPointF(cx, cy) + qi, PkPointF(cx, cy) + pi),
        shapeOfMixed({x, y}, {cx, cy}), in,
        qstr(QPointF(cx, cy) + qi), qstr(PkPointF(cx, cy) + pi));
    // 往返：提升再 toPoint 必须回到原点（除非 int 值域边界上的 double 表示丢精度）
    rec("F::roundTrip", same_pt(qf.toPoint(), pf.toPoint()), sh, in,
        qstr(qf.toPoint()), qstr(pf.toPoint()));
}

// ═══ Size 族：逐 API 对拍 ══════════════════════════════════════════════════
//
// 与 Point 族的三处结构性不同，直接决定了这一节的 tag 怎么设计：
//   ① **三条谓词的分界线互不相同**（isNull 只认 0、isEmpty 的门槛是 `<1`／浮点
//      `<=0.`、isValid 是 `>=0`）。所以 0 / 1 / 负 是三个独立的根因，
//      不能压成 shapeOf* 里的一档 —— 各谓词用各自的边界谓词做 tag。
//   ② **scaled/scale 多了一个 mode 参数**，而三种 mode 走的是三条不同的分支
//      （Ignore 直接返回目标；Keep/Expand 只差比较方向）。mode 是输入的一部分，
//      必须参与 tag（规则一），否则一种 mode 上的偏离会把另外两种一起罩住。
//   ③ 整数版 scaled 内部有 **qint64 中间量**再窄回 int，浮点版没有。
//      "窄化会不会回绕"是整数版独有的根因，单独一档。
static const Qt::AspectRatioMode kQtModes[3] = {
    Qt::IgnoreAspectRatio, Qt::KeepAspectRatio, Qt::KeepAspectRatioByExpanding };
static const pkoracle::Qt::AspectRatioMode kPkModes[3] = {
    pkoracle::Qt::IgnoreAspectRatio, pkoracle::Qt::KeepAspectRatio,
    pkoracle::Qt::KeepAspectRatioByExpanding };
static const char *kModeName[3] = { "ignore", "keep", "expand" };

// Size 版的通用形态分流。与 shapeOfI/shapeOfD 分开：Point 那两个把 0 与负数
// 归成同一档（"ordinary"），而尺寸的语义里这两者是完全不同的东西。
static std::string shapeOfSizeI(std::initializer_list<int> vs)
{
    for (int v : vs) if (intExtremum(v)) return "int-extremum";
    for (int v : vs) if (v < 0) return "negative-dim";
    for (int v : vs) if (v == 0) return "zero-dim";
    for (int v : vs) if (v == 1) return "unit-dim";      // isEmpty 的 `<1` 边界
    return "ordinary";
}

static std::string shapeOfSizeD(std::initializer_list<double> vs)
{
    for (double v : vs) if (nonFinite(v)) return "nonfinite";
    for (double v : vs) if (signedZero(v)) return "signed-zero";
    for (double v : vs) if (subnormal(v)) return "subnormal";
    for (double v : vs) if (v == 0.0) return "zero-dim";
    for (double v : vs) if (v < 0.0) return "negative-dim";
    for (double v : vs) if (std::fabs(v) > 1e300) return "huge";
    return "finite";
}

// 无输入的 API（默认构造）：**只有一种输入形态**，所以 tag 退化成常量是这条
// 规则的边界情形而不是违反 —— 规则一禁的是"有输入却不让输入参与 tag"。
// 只跑一次。
static void cmp_size_constants()
{
    rec("S::defaultCtor", same_sz(QSize(), PkSize()), "no-input",
        "QSize()", qstr(QSize()), qstr(PkSize()));
    rec("SF::defaultCtor", same_szf(QSizeF(), PkSizeF()), "no-input",
        "QSizeF()", qstr(QSizeF()), qstr(PkSizeF()));
    rec("S::defaultPredicates",
        QSize().isNull() == PkSize().isNull()
        && QSize().isEmpty() == PkSize().isEmpty()
        && QSize().isValid() == PkSize().isValid(),
        "no-input", "QSize()",
        bstr(QSize().isNull()) + bstr(QSize().isEmpty()) + bstr(QSize().isValid()),
        bstr(PkSize().isNull()) + bstr(PkSize().isEmpty()) + bstr(PkSize().isValid()));
    rec("SF::defaultPredicates",
        QSizeF().isNull() == PkSizeF().isNull()
        && QSizeF().isEmpty() == PkSizeF().isEmpty()
        && QSizeF().isValid() == PkSizeF().isValid(),
        "no-input", "QSizeF()",
        bstr(QSizeF().isNull()) + bstr(QSizeF().isEmpty()) + bstr(QSizeF().isValid()),
        bstr(PkSizeF().isNull()) + bstr(PkSizeF().isEmpty()) + bstr(PkSizeF().isValid()));
}

static void cmp_size_unary(int w, int h)
{
    const QSize  q(w, h);
    const PkSize p(w, h);
    const std::string in = istr(w) + "x" + istr(h);
    const std::string sh = shapeOfSizeI({w, h});
    const bool extremum = intExtremum(w) || intExtremum(h);

    rec("S::width", q.width() == p.width(), sh, in, istr(q.width()), istr(p.width()));
    rec("S::height", q.height() == p.height(), sh, in, istr(q.height()), istr(p.height()));

    // 三条谓词的边界各不相同，各用各的（把它们压成同一个 tag 就等于允许
    // 「isValid 抄了 isEmpty 的门槛」这类偏离藏在同一片白名单下）。
    rec("S::isNull", q.isNull() == p.isNull(),
        extremum ? std::string("int-extremum") : (w == 0 || h == 0) ? std::string("zero-dim") : sh,
        in, bstr(q.isNull()), bstr(p.isNull()));
    rec("S::isEmpty", q.isEmpty() == p.isEmpty(),
        extremum ? std::string("int-extremum")
                 : (w <= 1 || h <= 1) ? std::string("empty-boundary") : sh,
        in, bstr(q.isEmpty()), bstr(p.isEmpty()));
    rec("S::isValid", q.isValid() == p.isValid(),
        extremum ? std::string("int-extremum")
                 : (w <= 0 || h <= 0) ? std::string("valid-boundary") : sh,
        in, bstr(q.isValid()), bstr(p.isValid()));

    // 规则三：一个重载一条 rec（原来 setWidth 与 setHeight 挤在一条里，
    // rwidth/rheight 同样 —— 拆开之后哪个重载分的家在 DIFFTAG 里直接看得见）。
    { QSize q2 = q; PkSize p2 = p; q2.setWidth(h); p2.setWidth(h);
      rec("S::setWidth", same_sz(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QSize q2 = q; PkSize p2 = p; q2.setHeight(w); p2.setHeight(w);
      rec("S::setHeight", same_sz(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QSize q2 = q; PkSize p2 = p; q2.rwidth() += 1; p2.rwidth() += 1;
      rec("S::rwidth", same_sz(q2, p2),
          (w == INT_MAX) ? std::string("int-overflow") : sh,
          in, qstr(q2), qstr(p2)); }
    { QSize q2 = q; PkSize p2 = p; q2.rheight() -= 1; p2.rheight() -= 1;
      rec("S::rheight", same_sz(q2, p2),
          (h == INT_MIN) ? std::string("int-overflow") : sh,
          in, qstr(q2), qstr(p2)); }
}

static void cmp_size_binary(int aw, int ah, int bw, int bh)
{
    const QSize  qa(aw, ah), qb(bw, bh);
    const PkSize pa(aw, ah), pb(bw, bh);
    const std::string in = istr(aw) + "x" + istr(ah) + "|" + istr(bw) + "x" + istr(bh);
    // ⚠ **四个分量都参与**（约定见 shapeOfI 上方那段）。
    const std::string sh = shapeOfSizeI({aw, ah, bw, bh});

    rec("S::operator+", same_sz(qa + qb, pa + pb),
        (addOverflows(aw, bw) || addOverflows(ah, bh)) ? std::string("int-overflow") : sh,
        in, qstr(qa + qb), qstr(pa + pb));
    rec("S::operator-", same_sz(qa - qb, pa - pb),
        (subOverflows(aw, bw) || subOverflows(ah, bh)) ? std::string("int-overflow") : sh,
        in, qstr(qa - qb), qstr(pa - pb));
    rec("S::operator==", (qa == qb) == (pa == pb), sh, in, bstr(qa == qb), bstr(pa == pb));
    rec("S::operator!=", (qa != qb) == (pa != pb), sh, in, bstr(qa != qb), bstr(pa != pb));
    { QSize q2 = qa; PkSize p2 = pa; q2 += qb; p2 += pb;
      rec("S::operator+=", same_sz(q2, p2),
          (addOverflows(aw, bw) || addOverflows(ah, bh)) ? std::string("int-overflow") : sh,
          in, qstr(q2), qstr(p2)); }
    { QSize q2 = qa; PkSize p2 = pa; q2 -= qb; p2 -= pb;
      rec("S::operator-=", same_sz(q2, p2),
          (subOverflows(aw, bw) || subOverflows(ah, bh)) ? std::string("int-overflow") : sh,
          in, qstr(q2), qstr(p2)); }
    // expandedTo 是 qMax 逐分量：唯一会分家的形态是"两侧相等/一侧更大"的选择，
    // 由分量形态解释即可。
    rec("S::expandedTo", same_sz(qa.expandedTo(qb), pa.expandedTo(pb)), sh,
        in, qstr(qa.expandedTo(qb)), qstr(pa.expandedTo(pb)));
}

// QSize 的缩放：只有 qreal 一个重载（不像 QPoint 有 float/double/int 三个），
// 但除法那两条在 Qt 里带 Q_ASSERT —— 对拍用 -DQT_NO_DEBUG 编（= Krita 发布构建
// 的形态，理由见 run_oracle.sh 与 README 偏离清单），断言整条编译掉。
static void cmp_size_scale(int w, int h, double c)
{
    const QSize  q(w, h);
    const PkSize p(w, h);
    const std::string in = istr(w) + "x" + istr(h) + "*" + dstr(c);
    const double pw = (double)w * c, ph = (double)h * c;
    const std::string mtag =
          (nonFinite(pw) || nonFinite(ph))         ? "nonfinite-product"
        : (outOfIntRange(pw) || outOfIntRange(ph)) ? "product-out-of-int-range"
        : (halfBoundary(pw) || halfBoundary(ph))   ? "half-boundary"
        : (nearHalfUlp(pw) || nearHalfUlp(ph))     ? "near-half-ulp"
                                                    : "in-range";
    rec("S::operator*", same_sz(q * c, p * c), mtag, in, qstr(q * c), qstr(p * c));
    rec("S::operator*(rev)", same_sz(c * q, c * p), mtag, in, qstr(c * q), qstr(c * p));
    { QSize q2 = q; PkSize p2 = p; q2 *= c; p2 *= c;
      rec("S::operator*=", same_sz(q2, p2), mtag, in, qstr(q2), qstr(p2)); }

    const double dw = (double)w / c, dh = (double)h / c;
    const std::string dtag =
          (c == 0.0)                               ? "divisor-zero"
        : (nonFinite(dw) || nonFinite(dh))         ? "nonfinite-quotient"
        : (outOfIntRange(dw) || outOfIntRange(dh)) ? "quotient-out-of-int-range"
        : (halfBoundary(dw) || halfBoundary(dh))   ? "half-boundary"
        : (nearHalfUlp(dw) || nearHalfUlp(dh))     ? "near-half-ulp"
                                                    : "in-range";
    rec("S::operator/", same_sz(q / c, p / c), dtag, in, qstr(q / c), qstr(p / c));
    { QSize q2 = q; PkSize p2 = p; q2 /= c; p2 /= c;
      rec("S::operator/=", same_sz(q2, p2), dtag, in, qstr(q2), qstr(p2)); }
}

// scaled/scale：**mode 参与 tag**，且分支形态（退化源 / qint64 窄化 / 负分量）
// 各自成一档 —— 这三条分支在 Qt 里走的是完全不同的代码，混成一个 tag 等于
// 把三片行为一起白名单化（规则二）。
static void cmp_size_scaled(int sw, int sh, int tw, int th, int mi)
{
    const QSize  qs(sw, sh), qt(tw, th);
    const PkSize ps(sw, sh), pt(tw, th);
    const std::string in = istr(sw) + "x" + istr(sh) + "->" + istr(tw) + "x" + istr(th);

    // qint64 中间量是否越出 int 值域（越出就要靠窄化回绕，是独立的根因）
    bool narrows = false;
    if (sw != 0 && sh != 0) {
        const long long rw = (long long)th * (long long)sw / (long long)sh;
        const long long rh = (long long)tw * (long long)sh / (long long)sw;
        narrows = rw < INT_MIN || rw > INT_MAX || rh < INT_MIN || rh > INT_MAX;
    }
    const std::string branch =
          (mi == 0)                  ? "ignore-returns-target"
        : (sw == 0 || sh == 0)       ? "degenerate-source"
        : narrows                    ? "int64-narrowing"
        : (sw < 0 || sh < 0 || tw < 0 || th < 0) ? "negative-dim"
        : (intExtremum(sw) || intExtremum(sh) || intExtremum(tw) || intExtremum(th))
                                     ? "int-extremum"
                                     : "ordinary";
    const std::string tag = std::string(kModeName[mi]) + "/" + branch;

    rec("S::scaled(size)", same_sz(qs.scaled(qt, kQtModes[mi]), ps.scaled(pt, kPkModes[mi])),
        tag, in, qstr(qs.scaled(qt, kQtModes[mi])), qstr(ps.scaled(pt, kPkModes[mi])));
    rec("S::scaled(w,h)",
        same_sz(qs.scaled(tw, th, kQtModes[mi]), ps.scaled(tw, th, kPkModes[mi])),
        tag, in, qstr(qs.scaled(tw, th, kQtModes[mi])), qstr(ps.scaled(tw, th, kPkModes[mi])));
    { QSize q2 = qs; PkSize p2 = ps;
      q2.scale(qt, kQtModes[mi]); p2.scale(pt, kPkModes[mi]);
      rec("S::scale(size)", same_sz(q2, p2), tag, in, qstr(q2), qstr(p2)); }
    { QSize q2 = qs; PkSize p2 = ps;
      q2.scale(tw, th, kQtModes[mi]); p2.scale(tw, th, kPkModes[mi]);
      rec("S::scale(w,h)", same_sz(q2, p2), tag, in, qstr(q2), qstr(p2)); }
}

static void cmp_sizef_unary(double w, double h)
{
    const QSizeF  q(w, h);
    const PkSizeF p(w, h);
    const std::string in = dstr(w) + "x" + dstr(h);
    const std::string sh = shapeOfSizeD({w, h});

    rec("SF::width", same_double(q.width(), p.width()), sh, in, dstr(q.width()), dstr(p.width()));
    rec("SF::height", same_double(q.height(), p.height()), sh, in, dstr(q.height()), dstr(p.height()));

    // isNull 用 qIsNull（== 0.0）：-0.0 算 null、次正规不算 —— 与 isEmpty 的
    // `<= 0.`、isValid 的 `>= 0.` 三条门槛互不相同，各用各的 tag。
    rec("SF::isNull", q.isNull() == p.isNull(),
        (signedZero(w) || signedZero(h)) ? std::string("signed-zero")
        : (subnormal(w) || subnormal(h)) ? std::string("subnormal")
        : (w == 0.0 || h == 0.0)         ? std::string("zero-dim") : sh,
        in, bstr(q.isNull()), bstr(p.isNull()));
    rec("SF::isEmpty", q.isEmpty() == p.isEmpty(),
        (std::isnan(w) || std::isnan(h)) ? std::string("nan-dim")
        : (w <= 0.0 || h <= 0.0)         ? std::string("empty-boundary") : sh,
        in, bstr(q.isEmpty()), bstr(p.isEmpty()));
    rec("SF::isValid", q.isValid() == p.isValid(),
        (std::isnan(w) || std::isnan(h)) ? std::string("nan-dim")
        : (w <= 0.0 || h <= 0.0)         ? std::string("valid-boundary") : sh,
        in, bstr(q.isValid()), bstr(p.isValid()));

    // toSize 走 qRound：半值方向、越出 int 值域、差一个 ulp 的半值是三个根因。
    rec("SF::toSize", same_sz(q.toSize(), p.toSize()),
        (outOfIntRange(w) || outOfIntRange(h)) ? std::string("out-of-int-range")
        : (halfBoundary(w) || halfBoundary(h)) ? std::string("half-boundary")
        : (nearHalfUlp(w) || nearHalfUlp(h))   ? std::string("near-half-ulp")
        : (subnormal(w) || subnormal(h))       ? std::string("subnormal")
                                                : std::string("in-range"),
        in, qstr(q.toSize()), qstr(p.toSize()));

    // 规则三：一个重载一条 rec（拆分理由同整数版）。
    { QSizeF q2 = q; PkSizeF p2 = p; q2.setWidth(h); p2.setWidth(h);
      rec("SF::setWidth", same_szf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QSizeF q2 = q; PkSizeF p2 = p; q2.setHeight(w); p2.setHeight(w);
      rec("SF::setHeight", same_szf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QSizeF q2 = q; PkSizeF p2 = p; q2.rwidth() += 1.0; p2.rwidth() += 1.0;
      rec("SF::rwidth", same_szf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QSizeF q2 = q; PkSizeF p2 = p; q2.rheight() -= 1.0; p2.rheight() -= 1.0;
      rec("SF::rheight", same_szf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
}

static void cmp_sizef_binary(double aw, double ah, double bw, double bh)
{
    const QSizeF  qa(aw, ah), qb(bw, bh);
    const PkSizeF pa(aw, ah), pb(bw, bh);
    const std::string in = dstr(aw) + "x" + dstr(ah) + "|" + dstr(bw) + "x" + dstr(bh);
    const std::string sh = shapeOfSizeD({aw, ah, bw, bh});

    rec("SF::operator+", same_szf(qa + qb, pa + pb), sh, in, qstr(qa + qb), qstr(pa + pb));
    rec("SF::operator-", same_szf(qa - qb, pa - pb), sh, in, qstr(qa - qb), qstr(pa - pb));

    // ⚠ QSizeF::operator== 是**两个分量各一次 qFuzzyCompare，没有零分支**
    //（与 QPointF 不同，那边任一侧为 0 时改走 fuzzyIsNull）。qFuzzyCompare 的
    // 右端取 qMin(|a|,|b|)，所以"恰好一侧是 0"与"两侧都是 0"是两个截然不同的
    // 结局（前者恒 false、后者恒 true），必须分成两个 tag。
    const bool zeroW = (aw == 0.0 || bw == 0.0), bothZeroW = (aw == 0.0 && bw == 0.0);
    const bool zeroH = (ah == 0.0 || bh == 0.0), bothZeroH = (ah == 0.0 && bh == 0.0);
    const std::string eqtag =
          (nonFinite(aw) || nonFinite(bw) || nonFinite(ah) || nonFinite(bh)) ? "nonfinite"
        : ((zeroW && !bothZeroW) || (zeroH && !bothZeroH))                   ? "one-side-zero"
        : (bothZeroW || bothZeroH)                                           ? "both-zero"
        : (subnormal(aw) || subnormal(bw) || subnormal(ah) || subnormal(bh)) ? "subnormal"
                                                                             : "relative-branch";
    rec("SF::operator==", (qa == qb) == (pa == pb), eqtag, in, bstr(qa == qb), bstr(pa == pb));
    rec("SF::operator!=", (qa != qb) == (pa != pb), eqtag, in, bstr(qa != qb), bstr(pa != pb));

    { QSizeF q2 = qa; PkSizeF p2 = pa; q2 += qb; p2 += pb;
      rec("SF::operator+=", same_szf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QSizeF q2 = qa; PkSizeF p2 = pa; q2 -= qb; p2 -= pb;
      rec("SF::operator-=", same_szf(q2, p2), sh, in, qstr(q2), qstr(p2)); }

    // expandedTo 是 qMax（`(a<b)?b:a`），**NaN 上不可交换、零号上保号** ——
    // 这两类正是它唯一会分家的地方，让它们参与 tag。
    rec("SF::expandedTo", same_szf(qa.expandedTo(qb), pa.expandedTo(pb)),
        (std::isnan(aw) || std::isnan(bw) || std::isnan(ah) || std::isnan(bh))
            ? std::string("nan-dim")
        : (signedZero(aw) || signedZero(bw) || signedZero(ah) || signedZero(bh))
            ? std::string("signed-zero") : sh,
        in, qstr(qa.expandedTo(qb)), qstr(pa.expandedTo(pb)));
}

static void cmp_sizef_scale(double w, double h, double c)
{
    const QSizeF  q(w, h);
    const PkSizeF p(w, h);
    const std::string in = dstr(w) + "x" + dstr(h) + "*" + dstr(c);
    const std::string sh = shapeOfSizeD({w, h, c});

    rec("SF::operator*", same_szf(q * c, p * c), sh, in, qstr(q * c), qstr(p * c));
    rec("SF::operator*(rev)", same_szf(c * q, c * p), sh, in, qstr(c * q), qstr(c * p));
    { QSizeF q2 = q; PkSizeF p2 = p; q2 *= c; p2 *= c;
      rec("SF::operator*=", same_szf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    rec("SF::operator/", same_szf(q / c, p / c),
        (c == 0.0) ? std::string("divisor-zero") : sh, in, qstr(q / c), qstr(p / c));
    { QSizeF q2 = q; PkSizeF p2 = p; q2 /= c; p2 /= c;
      rec("SF::operator/=", same_szf(q2, p2),
          (c == 0.0) ? std::string("divisor-zero") : sh, in, qstr(q2), qstr(p2)); }
}

static void cmp_sizef_scaled(double sw, double sh, double tw, double th, int mi)
{
    const QSizeF  qs(sw, sh), qt(tw, th);
    const PkSizeF ps(sw, sh), pt(tw, th);
    const std::string in = dstr(sw) + "x" + dstr(sh) + "->" + dstr(tw) + "x" + dstr(th);
    // 浮点版的分支条件是 qIsNull（== 0.0，含 -0.0），**次正规不走那条**；
    // 而 NaN 会让 `rw <= s.wd` 恒假从而自动落到 else 分支 —— 两者根因不同。
    const std::string branch =
          (mi == 0)                          ? "ignore-returns-target"
        : (sw == 0.0 || sh == 0.0)           ? "degenerate-source"
        : (std::isnan(sw) || std::isnan(sh) || std::isnan(tw) || std::isnan(th))
                                             ? "nan-dim"
        : (nonFinite(sw) || nonFinite(sh) || nonFinite(tw) || nonFinite(th))
                                             ? "infinite-dim"
        : (subnormal(sw) || subnormal(sh) || subnormal(tw) || subnormal(th))
                                             ? "subnormal"
        : (sw < 0.0 || sh < 0.0 || tw < 0.0 || th < 0.0) ? "negative-dim"
                                             : "ordinary";
    const std::string tag = std::string(kModeName[mi]) + "/" + branch;

    rec("SF::scaled(size)", same_szf(qs.scaled(qt, kQtModes[mi]), ps.scaled(pt, kPkModes[mi])),
        tag, in, qstr(qs.scaled(qt, kQtModes[mi])), qstr(ps.scaled(pt, kPkModes[mi])));
    rec("SF::scaled(w,h)",
        same_szf(qs.scaled(tw, th, kQtModes[mi]), ps.scaled(tw, th, kPkModes[mi])),
        tag, in, qstr(qs.scaled(tw, th, kQtModes[mi])), qstr(ps.scaled(tw, th, kPkModes[mi])));
    { QSizeF q2 = qs; PkSizeF p2 = ps;
      q2.scale(qt, kQtModes[mi]); p2.scale(pt, kPkModes[mi]);
      rec("SF::scale(size)", same_szf(q2, p2), tag, in, qstr(q2), qstr(p2)); }
    // ⚠ 规则三：**这一条曾经漏掉**（整数版四个重载齐全、浮点版只有三个）。
    // 复评把 PkSizeF::scale(qreal,qreal,mode) 整个改坏，对拍一条都没红。
    { QSizeF q2 = qs; PkSizeF p2 = ps;
      q2.scale(tw, th, kQtModes[mi]); p2.scale(tw, th, kPkModes[mi]);
      rec("SF::scale(w,h)", same_szf(q2, p2), tag, in, qstr(q2), qstr(p2)); }
}

// PkSize → PkSizeF 的隐式提升（Task 4/5 的 PkRectF 靠这条），以及往返。
static void cmp_size_promotion(int w, int h, double fw, double fh)
{
    const QSizeF  qf = QSize(w, h);      // 隐式
    const PkSizeF pf = PkSize(w, h);
    const std::string in = istr(w) + "x" + istr(h) + "+" + dstr(fw) + "x" + dstr(fh);
    const std::string sh = shapeOfSizeI({w, h});

    rec("SF::fromSize", same_szf(qf, pf), sh, in, qstr(qf), qstr(pf));
    // 往返：提升再 toSize 必须回到原值（int 全值域内都是精确的）
    rec("SF::roundTrip", same_sz(qf.toSize(), pf.toSize()), sh, in,
        qstr(qf.toSize()), qstr(pf.toSize()));
    // 混合二元：浮点尺寸与**隐式提升上来的**整数尺寸做 expandedTo。
    // 两族分量都参与 tag（约定见 shapeOfMixed 上方）。
    rec("SF::mixedExpandedTo",
        same_szf(QSizeF(fw, fh).expandedTo(QSize(w, h)),
                 PkSizeF(fw, fh).expandedTo(PkSize(w, h))),
        shapeOfSizeI({w, h}) + "+" + shapeOfSizeD({fw, fh}), in,
        qstr(QSizeF(fw, fh).expandedTo(QSize(w, h))),
        qstr(PkSizeF(fw, fh).expandedTo(PkSize(w, h))));
    rec("SF::mixedEquality",
        (QSizeF(fw, fh) == QSizeF(QSize(w, h))) == (PkSizeF(fw, fh) == PkSizeF(PkSize(w, h))),
        shapeOfSizeI({w, h}) + "+" + shapeOfSizeD({fw, fh}), in,
        bstr(QSizeF(fw, fh) == QSizeF(QSize(w, h))),
        bstr(PkSizeF(fw, fh) == PkSizeF(PkSize(w, h))));
}

// ═══ Rect 族（整数）：逐 API 对拍 ══════════════════════════════════════════
//
// 与 Point/Size 两族的四处结构性不同，直接决定了这一节的 tag 怎么设计：
//   ① **内部是四个边界坐标**，(l,t,w,h) 构造够不到一大片形态
//      （x2 == x1-1 的"宽恰为 0"只能由 setCoords 摆出来），所以输入集是
//      **坐标四元组**，不是 (x,y,w,h) 四元组。
//   ② **退化分三档而不是两档**：x2 == x1-1（宽 0，normalized **不**交换）、
//      x2 < x1-1（宽为负，交换）、x2 == x1（宽 1）。这三条线是 normalized /
//      contains / operator& 的真实分支边界，压成一档等于把它们一起白名单化。
//   ③ **双目 API 在 null 上不对称**（operator| 的两条 return 有先后），所以
//      双目 tag 用 `shapeA ^ shapeB` 这种**有序对**，不能照抄 Point/Size 那条
//      「把两侧分量合起来取最特殊的一个」的约定 —— 那条会把
//      「a 为 null」与「b 为 null」压成同一个 tag，恰好盖住这个 API 唯一的怪处。
//      有序对比"取最特殊"**更窄**，方向上是安全的（规则二）。
//   ④ 双目 API 还额外带一个**相对位置**分量（disjoint / a-covers-b / …）。
//      它由 qtInterval() 用纯 long long 算术算出来，**不调用任何一侧的实现**，
//      所以仍然是"由输入形态构造"的 tag（规则一）。
//
// 规则三（一个重载一条 rec）在这一族是硬要求，且有机器闸门：
// 见 g_apis / APISEEN 与 oracle/rect_api.map。

// 跨距是否越出 int。⚠ **两个量都要查**：`x2 - x1` 这个中间量自己就可能溢出，
// 而最终的 `x2 - x1 + 1` 反而回到值域里 —— 例如 x1=1、x2=INT_MIN 时
// x2-x1 = -2147483649（越界），+1 之后是 INT_MIN（在界内）。
// 只查后者会漏掉一整片跨距溢出的输入，让它们混进 `extremum` / `normal` 两档 ——
// 那正是规则二要防的形状：标签比事实宽一格，两类根因不同的输入压成同一个 tag。
// （它现在**只用来给 tag 分档**，不再有任何豁免作用：曾经基于它的 spanUB()
// 白名单随 Task 4 修复轮删除，见下面 pairTag 的长注释。）
static bool spanOverflows(int lo, int hi)
{
    const long long d = (long long)hi - lo;
    if (d < INT_MIN || d > INT_MAX) return true;
    const long long s = d + 1;
    return s < INT_MIN || s > INT_MAX;
}

// 单个轴的形态。**六档**，顺序就是"这次差异该归咎于谁"的判断：
//   span-overflow → 跨距本身溢出（只有极值坐标才到得了）
//   zero          → x2 == x1-1，宽恰为 0：normalized **不**交换的边界、
//                   isNull 的构成条件、contains(point) 上区间为空
//   negative      → x2 < x1-1，宽为负：要交换的那一侧
//   unit          → x2 == x1，宽 1
//   extremum      → 坐标里有 INT_MIN/INT_MAX 但跨距没溢出
//   normal        → 其余
static std::string axisShape(int lo, int hi)
{
    if (spanOverflows(lo, hi)) return "span-overflow";
    if (hi == lo - 1) return "zero";
    if (hi < lo - 1) return "negative";
    if (hi == lo) return "unit";
    if (intExtremum(lo) || intExtremum(hi)) return "extremum";
    return "normal";
}

static std::string shapeOfRect(int x1, int y1, int x2, int y2)
{ return axisShape(x1, x2) + "-" + axisShape(y1, y2); }

// 按 **Qt 的翻正规则**（`x2 - x1 + 1 < 0` 时交换）算出一个轴上的闭区间。
// 纯 long long 算术，**不调用两侧任何一个实现** —— 所以拿它做 tag 不构成
// "用被测物给自己贴标签"。
static void qtInterval(int lo, int hi, long long &l, long long &r)
{
    if ((long long)hi - lo + 1 < 0) { l = hi; r = lo; } else { l = lo; r = hi; }
}

static std::string axisRel(int alo, int ahi, int blo, int bhi)
{
    long long l1, r1, l2, r2;
    qtInterval(alo, ahi, l1, r1);
    qtInterval(blo, bhi, l2, r2);
    if (l1 > r2 || l2 > r1) return "disjoint";
    if (l1 == l2 && r1 == r2) return "same";
    if (l2 >= l1 && r2 <= r1) return "a-covers-b";
    if (l1 >= l2 && r1 <= r2) return "b-covers-a";
    return "partial";
}

// 双目 tag。**全部输入一律走细粒度 `shape/<形态对>/<相对位置>`，没有豁免档。**
//
// ⚠ 前缀从 `defined/` 改名成 `shape/`：跨距溢出的输入两侧**仍然都是 UB**
// （只是现在两侧同旗标，UB 的取值恰好一致），叫 "defined" 等于让标签比事实
// 宽一格 —— 正是方法论规则二要防的形状。`shape/` 只声称"这是按输入形态贴的"。
//
// 曾经这里有一个 `spanUB()` 分支，把"任一侧跨距越出 int"的输入整片塌成单个
// tag `span-overflow-ub` 并在 geometry.deviation 里按 9 个 API 各声明一行。
// 那套机制随 Task 4 修复轮**整个删掉**，原因是它解决的问题已经从根上没了：
//
//   那 704 589 条差异全部来自**旗标不对等** —— operator| / operator& /
//   contains(QRect) / intersects 的实现编在 libQt5Core.so 里，对拍 TU 的
//   `-fwrapv` 到不了那侧，于是同一条 `x2 - x1 + 1 < 0` 判据两侧取值不同。
//   run_oracle.sh 去掉 `-fwrapv` 之后两侧同旗标，mismatch 直接回到 3（只剩
//   canary）—— **不需要豁免任何输入**。
//
// 删掉它的收益是可量的：那一档原本覆盖 **3 286 575 次比对（占 total 的 2.62%，
// 实测计数）**，这些比对现在全都是真比对，任何差异都会是**新 tag** 而 FAIL。
// （历史记录：修复前的报告写的 704 589 是**差异**次数，不是被覆盖的**比对**
// 次数，把盲区规模低估了 4.7 倍 —— 这正是"豁免档必须量化"的理由。）
static std::string pairTag(int ax1, int ay1, int ax2, int ay2,
                           int bx1, int by1, int bx2, int by2)
{
    return "shape/" + shapeOfRect(ax1, ay1, ax2, ay2)
         + "^" + shapeOfRect(bx1, by1, bx2, by2)
         + "/" + axisRel(ax1, ax2, bx1, bx2) + "," + axisRel(ay1, ay2, by1, by2);
}

// normalized 专用：直接把"这个轴交不交换"做成 tag。四档里 boundary-zero 与
// swap 只差一格，正是 `x2 < x1-1` 写成 `x2 < x1` 时会挪动的那条线。
static std::string swapAxis(int lo, int hi)
{
    if (hi == lo - 1) return "boundary-zero";
    if (hi < lo - 1) return "swap";
    if (hi == lo) return "unit";
    return "keep";
}

// contains(point) 专用：点相对于**点版翻正规则**（`x2 < x1 - 1`）给出的区间
// 落在哪。at-lo / at-hi 两档就是差一边界，below/above 是外侧。
static std::string edgeTag(int lo, int hi, int p)
{
    long long l, r;
    if ((long long)hi < (long long)lo - 1) { l = hi; r = lo; } else { l = lo; r = hi; }
    if (l > r) return "empty-interval";
    if (p < l) return "below";
    if (p == l) return "at-lo";
    if (p == r) return "at-hi";
    if (p > r) return "above";
    return "inside";
}

static std::string argShape(std::initializer_list<int> vs)
{
    for (int v : vs) if (intExtremum(v)) return "int-extremum";
    for (int v : vs) if (v == 0) return "zero";
    for (int v : vs) if (v < 0) return "negative";
    return "positive";
}

static std::string rin(int a, int b, int c, int d)
{ return "[" + istr(a) + "," + istr(b) + "," + istr(c) + "," + istr(d) + "]"; }

// 无输入的 API（默认构造）：只有一种输入形态，tag 退化成常量是规则一的
// 边界情形而不是违反（与 cmp_size_constants 同一个道理）。只跑一次。
static void cmp_rect_constants()
{
    rec("R::defaultCtor", same_rect(QRect(), PkRect()), "no-input", "QRect()",
        qstr(QRect()), qstr(PkRect()));
}

// 三个带参构造。**三条各自一个 rec**（规则三）：它们做的事不一样 ——
// (l,t,w,h) 与 (topLeft,size) 做 +w-1，(topLeft,bottomRight) 直接摆坐标。
static void cmp_rect_ctor(int a, int b, int c, int d)
{
    const std::string in = rin(a, b, c, d);
    // 分歧只可能出在"+w-1 有没有溢出"与"宽高是 0 / 负"这两类形态上
    const bool ov = (long long)a + c - 1 > INT_MAX || (long long)a + c - 1 < INT_MIN
                 || (long long)b + d - 1 > INT_MAX || (long long)b + d - 1 < INT_MIN;
    const std::string sh =
          ov                                              ? "ctor-span-overflow"
        : (intExtremum(a) || intExtremum(b)
           || intExtremum(c) || intExtremum(d))            ? "int-extremum"
        : (c == 0 || d == 0)                               ? "zero-dim"
        : (c < 0 || d < 0)                                 ? "negative-dim"
                                                           : "ordinary";

    rec("R::ctorLTWH", same_rect(QRect(a, b, c, d), PkRect(a, b, c, d)), sh, in,
        qstr(QRect(a, b, c, d)), qstr(PkRect(a, b, c, d)));
    rec("R::ctorPoints",
        same_rect(QRect(QPoint(a, b), QPoint(c, d)), PkRect(PkPoint(a, b), PkPoint(c, d))),
        shapeOfRect(a, b, c, d), in,
        qstr(QRect(QPoint(a, b), QPoint(c, d))), qstr(PkRect(PkPoint(a, b), PkPoint(c, d))));
    rec("R::ctorPointSize",
        same_rect(QRect(QPoint(a, b), QSize(c, d)), PkRect(PkPoint(a, b), PkSize(c, d))),
        sh, in,
        qstr(QRect(QPoint(a, b), QSize(c, d))), qstr(PkRect(PkPoint(a, b), PkSize(c, d))));
}

// 一元取值器 + normalized。输入是**坐标四元组**。
static void cmp_rect_unary(int x1, int y1, int x2, int y2)
{
    const QRect  q = mkQ(x1, y1, x2, y2);
    const PkRect p = mkP(x1, y1, x2, y2);
    const std::string in = rin(x1, y1, x2, y2);
    const std::string sh = shapeOfRect(x1, y1, x2, y2);

    rec("R::left", q.left() == p.left(), sh, in, istr(q.left()), istr(p.left()));
    rec("R::top", q.top() == p.top(), sh, in, istr(q.top()), istr(p.top()));
    rec("R::right", q.right() == p.right(), sh, in, istr(q.right()), istr(p.right()));
    rec("R::bottom", q.bottom() == p.bottom(), sh, in, istr(q.bottom()), istr(p.bottom()));
    rec("R::x", q.x() == p.x(), sh, in, istr(q.x()), istr(p.x()));
    rec("R::y", q.y() == p.y(), sh, in, istr(q.y()), istr(p.y()));

    // 差一：width/height 是 x2-x1+1，在极值坐标上溢出（两侧都是 UB，对拍靠
    // "两侧同旗标"比同取值，不靠 -fwrapv 钉死 —— 见 run_oracle.sh 的旗标注释）
    rec("R::width", q.width() == p.width(), sh, in, istr(q.width()), istr(p.width()));
    rec("R::height", q.height() == p.height(), sh, in, istr(q.height()), istr(p.height()));
    rec("R::size", same_sz(q.size(), p.size()), sh, in, qstr(q.size()), qstr(p.size()));

    // 三条谓词的分界线各不相同，**各用各的 tag**（把它们压成一个 sh 就等于
    // 允许"isValid 抄了 isEmpty 的门槛"这类偏离藏在同一片白名单下）。
    rec("R::isNull", q.isNull() == p.isNull(),
        (x2 == x1 - 1 || y2 == y1 - 1) ? std::string("null-boundary") : sh,
        in, bstr(q.isNull()), bstr(p.isNull()));
    rec("R::isEmpty", q.isEmpty() == p.isEmpty(),
        (x1 > x2 || y1 > y2) ? std::string("empty-side") : std::string("nonempty-side"),
        in, bstr(q.isEmpty()), bstr(p.isEmpty()));
    rec("R::isValid", q.isValid() == p.isValid(),
        (x1 <= x2 && y1 <= y2) ? std::string("valid-side") : std::string("invalid-side"),
        in, bstr(q.isValid()), bstr(p.isValid()));

    rec("R::topLeft", same_pt(q.topLeft(), p.topLeft()), sh, in,
        qstr(q.topLeft()), qstr(p.topLeft()));
    rec("R::topRight", same_pt(q.topRight(), p.topRight()), sh, in,
        qstr(q.topRight()), qstr(p.topRight()));
    rec("R::bottomLeft", same_pt(q.bottomLeft(), p.bottomLeft()), sh, in,
        qstr(q.bottomLeft()), qstr(p.bottomLeft()));
    rec("R::bottomRight", same_pt(q.bottomRight(), p.bottomRight()), sh, in,
        qstr(q.bottomRight()), qstr(p.bottomRight()));

    // center 唯一会分家的形态是"x1+x2 越出 int"——那正是 qint64 中间量存在的理由
    const bool sumOv = (long long)x1 + x2 > INT_MAX || (long long)x1 + x2 < INT_MIN
                    || (long long)y1 + y2 > INT_MAX || (long long)y1 + y2 < INT_MIN;
    rec("R::center", same_pt(q.center(), p.center()),
        sumOv ? std::string("sum-out-of-int-range") : sh,
        in, qstr(q.center()), qstr(p.center()));

    // normalized 用专门的交换 tag（形态就是"这个轴交不交换"）
    rec("R::normalized", same_rect(q.normalized(), p.normalized()),
        "x-" + swapAxis(x1, x2) + "/y-" + swapAxis(y1, y2),
        in, qstr(q.normalized()), qstr(p.normalized()));

    // getRect / getCoords：四个出参逐个比（**两条各自一个 rec**，它们差一个 ±1）
    { int a1, b1, c1, d1, a2, b2, c2, d2;
      q.getRect(&a1, &b1, &c1, &d1); p.getRect(&a2, &b2, &c2, &d2);
      rec("R::getRect", a1 == a2 && b1 == b2 && c1 == c2 && d1 == d2, sh, in,
          rin(a1, b1, c1, d1), rin(a2, b2, c2, d2)); }
    { int a1, b1, c1, d1, a2, b2, c2, d2;
      q.getCoords(&a1, &b1, &c1, &d1); p.getCoords(&a2, &b2, &c2, &d2);
      rec("R::getCoords", a1 == a2 && b1 == b2 && c1 == c2 && d1 == d2, sh, in,
          rin(a1, b1, c1, d1), rin(a2, b2, c2, d2)); }
}

// 全部带标量/点/尺寸参数的修改器。**一个重载一条 rec**（规则三）：
// setLeft 与 setX 写的是同一个字段但是两个 API，moveTo(int,int) 与
// moveTo(PkPoint) 是两个重载，translate/translated 同理 —— 合并的话
// "坏掉的是哪一跳"在 DIFFTAG 里就看不见了。
static void cmp_rect_mutate(int x1, int y1, int x2, int y2, int a, int b)
{
    const QRect  q0 = mkQ(x1, y1, x2, y2);
    const PkRect p0 = mkP(x1, y1, x2, y2);
    const std::string in = rin(x1, y1, x2, y2) + "+(" + istr(a) + "," + istr(b) + ")";
    const std::string sh = shapeOfRect(x1, y1, x2, y2) + "^" + argShape({a, b});

// 变参：`r.setRect(a, b, a, b)` 里的逗号会被预处理器当成实参分隔符，
// 写成 (label, expr) 那一版编不过。
#define PK_MUT2(label, ...)                                                   \
    do { QRect q2 = q0; PkRect p2 = p0;                                       \
         { QRect &r = q2; __VA_ARGS__; }                                      \
         { PkRect &r = p2; __VA_ARGS__; }                                     \
         rec(label, same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); } while (0)

    // ⚠ 这个宏把**同一段源码**分别在 QRect 与 PkRect 上跑一遍。它不是"少打字"：
    // 两侧写成两段代码时，改了一侧忘改另一侧就变成"对拍在比两件不同的事"，
    // 而那种错是静默的（Task 3 复评抓到过同型问题的另一种形态）。
    PK_MUT2("R::setLeft", r.setLeft(a));
    PK_MUT2("R::setTop", r.setTop(a));
    PK_MUT2("R::setRight", r.setRight(a));
    PK_MUT2("R::setBottom", r.setBottom(a));
    PK_MUT2("R::setX", r.setX(a));
    PK_MUT2("R::setY", r.setY(a));
    PK_MUT2("R::setWidth", r.setWidth(a));
    PK_MUT2("R::setHeight", r.setHeight(a));
    PK_MUT2("R::moveLeft", r.moveLeft(a));
    PK_MUT2("R::moveTop", r.moveTop(a));
    PK_MUT2("R::moveToXY", r.moveTo(a, b));
    PK_MUT2("R::setRect", r.setRect(a, b, a, b));
    PK_MUT2("R::setCoords", r.setCoords(a, b, a, b));
    PK_MUT2("R::translateXY", r.translate(a, b));
    PK_MUT2("R::translatedXY", r = r.translated(a, b));

    // 吃 QPoint / QSize 的那几个：两侧的实参类型不同，宏套不进来，逐条写。
    { QRect q2 = q0; PkRect p2 = p0; q2.setTopLeft(QPoint(a, b)); p2.setTopLeft(PkPoint(a, b));
      rec("R::setTopLeft", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QRect q2 = q0; PkRect p2 = p0;
      q2.setBottomRight(QPoint(a, b)); p2.setBottomRight(PkPoint(a, b));
      rec("R::setBottomRight", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QRect q2 = q0; PkRect p2 = p0;
      q2.moveTopLeft(QPoint(a, b)); p2.moveTopLeft(PkPoint(a, b));
      rec("R::moveTopLeft", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QRect q2 = q0; PkRect p2 = p0;
      q2.moveCenter(QPoint(a, b)); p2.moveCenter(PkPoint(a, b));
      rec("R::moveCenter", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QRect q2 = q0; PkRect p2 = p0; q2.moveTo(QPoint(a, b)); p2.moveTo(PkPoint(a, b));
      rec("R::moveToPoint", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QRect q2 = q0; PkRect p2 = p0; q2.translate(QPoint(a, b)); p2.translate(PkPoint(a, b));
      rec("R::translatePoint", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { const QRect q2 = q0.translated(QPoint(a, b));
      const PkRect p2 = p0.translated(PkPoint(a, b));
      rec("R::translatedPoint", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QRect q2 = q0; PkRect p2 = p0; q2.setSize(QSize(a, b)); p2.setSize(PkSize(a, b));
      rec("R::setSize", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }

#undef PK_MUT2
}

// adjust / adjusted：四个增量，单独一层输入。
static void cmp_rect_adjust(int x1, int y1, int x2, int y2, int d1, int d2, int d3, int d4)
{
    const QRect  q0 = mkQ(x1, y1, x2, y2);
    const PkRect p0 = mkP(x1, y1, x2, y2);
    const std::string in = rin(x1, y1, x2, y2) + "+" + rin(d1, d2, d3, d4);
    // 四个增量全参与形态判断（约定见 shapeOfD 上方那段）。溢出单独一档：
    // adjust 是裸的 int 加法，极值坐标上会回绕。
    const bool ov = (long long)x1 + d1 > INT_MAX || (long long)x1 + d1 < INT_MIN
                 || (long long)y1 + d2 > INT_MAX || (long long)y1 + d2 < INT_MIN
                 || (long long)x2 + d3 > INT_MAX || (long long)x2 + d3 < INT_MIN
                 || (long long)y2 + d4 > INT_MAX || (long long)y2 + d4 < INT_MIN;
    const std::string sh = (ov ? std::string("int-overflow")
                               : shapeOfRect(x1, y1, x2, y2))
                         + "^" + argShape({d1, d2, d3, d4});

    { QRect q2 = q0; PkRect p2 = p0; q2.adjust(d1, d2, d3, d4); p2.adjust(d1, d2, d3, d4);
      rec("R::adjust", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { const QRect q2 = q0.adjusted(d1, d2, d3, d4);
      const PkRect p2 = p0.adjusted(d1, d2, d3, d4);
      rec("R::adjusted", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }
}

// 双目：| & |= &= united intersected intersects contains(rect) == !=
static void cmp_rect_binary(int ax1, int ay1, int ax2, int ay2,
                            int bx1, int by1, int bx2, int by2)
{
    const QRect  qa = mkQ(ax1, ay1, ax2, ay2), qb = mkQ(bx1, by1, bx2, by2);
    const PkRect pa = mkP(ax1, ay1, ax2, ay2), pb = mkP(bx1, by1, bx2, by2);
    const std::string in = rin(ax1, ay1, ax2, ay2) + "|" + rin(bx1, by1, bx2, by2);
    const std::string tag = pairTag(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2);

    rec("R::operator|", same_rect(qa | qb, pa | pb), tag, in,
        qstr(qa | qb), qstr(pa | pb));
    rec("R::operator&", same_rect(qa & qb, pa & pb), tag, in,
        qstr(qa & qb), qstr(pa & pb));
    // ⚠ united / intersected 在 Qt 里是 `*this | r` / `*this & r` 的转发，
    // **仍然各写一条 rec**：规则三说的正是"转发链上坏掉的可能是转发那一跳"。
    rec("R::united", same_rect(qa.united(qb), pa.united(pb)), tag, in,
        qstr(qa.united(qb)), qstr(pa.united(pb)));
    rec("R::intersected", same_rect(qa.intersected(qb), pa.intersected(pb)), tag, in,
        qstr(qa.intersected(qb)), qstr(pa.intersected(pb)));
    { QRect q2 = qa; PkRect p2 = pa; q2 |= qb; p2 |= pb;
      rec("R::operator|=", same_rect(q2, p2), tag, in, qstr(q2), qstr(p2)); }
    { QRect q2 = qa; PkRect p2 = pa; q2 &= qb; p2 &= pb;
      rec("R::operator&=", same_rect(q2, p2), tag, in, qstr(q2), qstr(p2)); }

    rec("R::intersects", qa.intersects(qb) == pa.intersects(pb), tag, in,
        bstr(qa.intersects(qb)), bstr(pa.intersects(pb)));
    // contains(rect) 的两个 proper 取值走的是两条不同分支（`<` vs `<=`），
    // 各一条 rec，且 proper 参与 tag。
    rec("R::containsRect", qa.contains(qb) == pa.contains(pb), tag + "/proper-false", in,
        bstr(qa.contains(qb)), bstr(pa.contains(pb)));
    rec("R::containsRectProper", qa.contains(qb, true) == pa.contains(pb, true),
        tag + "/proper-true", in,
        bstr(qa.contains(qb, true)), bstr(pa.contains(pb, true)));

    // == / != 与相对位置无关，用形态对 + "坐标是否逐个相等"
    const std::string eqtag = shapeOfRect(ax1, ay1, ax2, ay2) + "^"
                            + shapeOfRect(bx1, by1, bx2, by2) + "/"
                            + ((ax1 == bx1 && ay1 == by1 && ax2 == bx2 && ay2 == by2)
                               ? "same-coords" : "differ");
    rec("R::operator==", (qa == qb) == (pa == pb), eqtag, in,
        bstr(qa == qb), bstr(pa == pb));
    rec("R::operator!=", (qa != qb) == (pa != pb), eqtag, in,
        bstr(qa != qb), bstr(pa != pb));
}

// contains(point) 的三个重载 + proper。四条各一个 rec（规则三）：
// contains(int,int) 与 contains(int,int,bool) 是两个独立的重载，
// 都转发到 contains(QPoint,bool)，但**转发那一跳本身可能坏**。
static void cmp_rect_contains_point(int x1, int y1, int x2, int y2, int px, int py)
{
    const QRect  q = mkQ(x1, y1, x2, y2);
    const PkRect p = mkP(x1, y1, x2, y2);
    const std::string in = rin(x1, y1, x2, y2) + "@(" + istr(px) + "," + istr(py) + ")";
    const std::string tag = "x-" + edgeTag(x1, x2, px) + "/y-" + edgeTag(y1, y2, py);

    rec("R::containsPoint", q.contains(QPoint(px, py)) == p.contains(PkPoint(px, py)),
        tag + "/proper-false", in,
        bstr(q.contains(QPoint(px, py))), bstr(p.contains(PkPoint(px, py))));
    rec("R::containsPointProper",
        q.contains(QPoint(px, py), true) == p.contains(PkPoint(px, py), true),
        tag + "/proper-true", in,
        bstr(q.contains(QPoint(px, py), true)), bstr(p.contains(PkPoint(px, py), true)));
    rec("R::containsXY", q.contains(px, py) == p.contains(px, py), tag + "/proper-false", in,
        bstr(q.contains(px, py)), bstr(p.contains(px, py)));
    rec("R::containsXYProper",
        q.contains(px, py, true) == p.contains(px, py, true), tag + "/proper-true", in,
        bstr(q.contains(px, py, true)), bstr(p.contains(px, py, true)));
}

// ═══ RectF 族：tag 谓词 ════════════════════════════════════════════════════
//
// **整套骨架、语料生成器、tag 约定全部复用 Rect 族的**（shapeOf* 的
// initializer_list 约定、`shape/` 前缀、双向交叉的输入形状、规则三一个重载一条
// rec）。这一节只补**浮点矩形特有的谓词差异** —— 整数版的 axisShape 说的是
// "跨距是 -1 / < -1 / 0 / 极值"，浮点版没有跨距这回事，说的是
// "位置或尺寸是 nan / ±0.0/ 次正规 / 恰好 0 / 负 / 极大"。
//
// 一个轴的形态由**位置与尺寸两个分量**算出（约定见 shapeOfD 上方那段：
// 一个 API 的 shape 必须由它全部参与分量算出，取最特殊的那种）。优先级从最能
// 解释差异的往下排：非有限 > 符号零 > 次正规 > 尺寸恰好 0（三谓词与 &/contains
// 的判空分界）> 尺寸为负（normalized / 翻正分支的分界）> 极大 > 普通。
static std::string axisShapeF(double pos, double dim)
{
    if (nonFinite(pos) || nonFinite(dim)) return "nonfinite";
    if (signedZero(pos) || signedZero(dim)) return "signed-zero";
    if (subnormal(pos) || subnormal(dim)) return "subnormal";
    if (dim == 0.0) return "zero-dim";
    if (dim < 0.0) return "negative-dim";
    if (std::fabs(pos) > 1e300 || std::fabs(dim) > 1e300) return "huge";
    return "normal";
}

static std::string shapeOfRectF(double x, double y, double w, double h)
{ return axisShapeF(x, w) + "-" + axisShapeF(y, h); }

// 按 **Qt 自己的翻正规则**（`dim < 0` 时摊成 [pos+dim, pos]）算出一个轴上的
// 闭区间。纯 double 算术，**不调用两侧任何一个实现** —— 所以拿它做 tag 不构成
// "用被测物给自己贴标签"。⚠ 判据是 `< 0` 不是 `<= 0`：-0.0 走的是正宽那一支，
// 这正是 normalized/operator& 里那条线。
static void qtIntervalF(double pos, double dim, double &l, double &r)
{ if (dim < 0) { l = pos + dim; r = pos; } else { l = pos; r = pos + dim; } }

static std::string axisRelF(double ap, double ad, double bp, double bd)
{
    double l1, r1, l2, r2;
    qtIntervalF(ap, ad, l1, r1);
    qtIntervalF(bp, bd, l2, r2);
    // NaN 单开一档：`<`/`>=`/`==` 在 NaN 上全为假，Qt 的每一条提前返回都不会
    // 触发，于是含 NaN 的输入一路走到底。把它们混进 disjoint/partial 会让标签
    // 说的事比事实宽（规则二）。
    if (std::isnan(l1) || std::isnan(r1) || std::isnan(l2) || std::isnan(r2))
        return "nan-axis";
    // Qt 的 operator& / contains / intersects 在**任一轴 l == r** 时提前返回
    //（不是 isNull()）—— 这一档要单独看得见。
    if (l1 == r1 || l2 == r2) return "degenerate-axis";
    if (l1 >= r2 || l2 >= r1) return "disjoint";
    if (l1 == l2 && r1 == r2) return "same";
    if (l2 >= l1 && r2 <= r1) return "a-covers-b";
    if (l1 >= l2 && r1 <= r2) return "b-covers-a";
    return "partial";
}

// 双目 tag。形状与整数版的 pairTag 逐字同构，只是换了浮点的形态谓词。
static std::string pairTagF(double ax, double ay, double aw, double ah,
                            double bx, double by, double bw, double bh)
{
    return "shape/" + shapeOfRectF(ax, ay, aw, ah)
         + "^" + shapeOfRectF(bx, by, bw, bh)
         + "/" + axisRelF(ax, aw, bx, bw) + "," + axisRelF(ay, ah, by, bh);
}

// normalized 专用：直接把"这个轴交不交换"做成 tag。boundary-zero 与 swap 只差
// 一格，正是 `dim < 0` 写成 `dim <= 0` 时会挪动的那条线；signed-zero 单开一档
// 是因为 -0.0 **不**交换而且宽度的符号位要原样留着。
static std::string swapAxisF(double dim)
{
    if (std::isnan(dim)) return "nan";
    if (signedZero(dim)) return "signed-zero";
    if (dim == 0.0) return "boundary-zero";
    if (dim < 0.0) return "swap";
    return "keep";
}

// contains(point) 专用：点相对于翻正后区间落在哪。at-lo / at-hi 两档就是
// **闭区间**的两条边（PkRectF 含边界、PkRect 因差一不含，两族在这里相反）。
static std::string edgeTagF(double pos, double dim, double p)
{
    double l, r;
    qtIntervalF(pos, dim, l, r);
    if (std::isnan(l) || std::isnan(r) || std::isnan(p)) return "nan";
    if (l == r) return "degenerate-axis";
    if (p < l) return "below";
    if (p == l) return "at-lo";
    if (p == r) return "at-hi";
    if (p > r) return "above";
    return "inside";
}

// toRect / toAlignedRect 专用：取整只看**四条边**（qRound 在 x/y/x+w/y+h 上，
// floor/ceil 同），所以 tag 也只由四条边算。
// ⚠ out-of-int-range 一档是**两侧都 UB**（浮点→int 越界）：-fwrapv 管不着它，
// 我们靠的是"两侧在本机编成同一条 cvttsd2si"。标签写实，不叫 "defined"。
static std::string roundShapeF(double x, double y, double w, double h)
{
    const double edges[4] = { x, y, x + w, y + h };
    for (double e : edges) if (nonFinite(e)) return "nonfinite-edge";
    for (double e : edges) if (outOfIntRange(e)) return "out-of-int-range";
    for (double e : edges) if (halfBoundary(e)) return "half-boundary";
    for (double e : edges) if (nearHalfUlp(e)) return "near-half-ulp";
    for (double e : edges) if (signedZero(e)) return "signed-zero-edge";
    // 边界恰为整数 —— ceil 不进位的那一档，toAlignedRect 最在意的一格
    for (double e : edges) if (e == std::floor(e)) return "exact-integer-edge";
    return "fractional";
}

// 修改器实参的形态（与整数版 argShape 同构）
static std::string argShapeF(std::initializer_list<double> vs)
{
    for (double v : vs) if (nonFinite(v)) return "nonfinite";
    for (double v : vs) if (signedZero(v)) return "signed-zero";
    for (double v : vs) if (subnormal(v)) return "subnormal";
    for (double v : vs) if (v == 0.0) return "zero";
    for (double v : vs) if (std::fabs(v) > 1e300) return "huge";
    for (double v : vs) if (v < 0.0) return "negative";
    return "positive";
}

static std::string rfin(double a, double b, double c, double d)
{ return "[" + dstr(a) + "," + dstr(b) + "," + dstr(c) + "," + dstr(d) + "]"; }

// ═══ RectF 族：逐 API 对拍 ════════════════════════════════════════════════

// 无输入的 API（默认构造）：口径同 cmp_rect_constants。只跑一次。
static void cmp_rectf_constants()
{
    rec("RF::defaultCtor", same_rectf(QRectF(), PkRectF()), "no-input", "QRectF()",
        qstr(QRectF()), qstr(PkRectF()));
}

// 三个吃浮点的构造。**三条各自一个 rec**（规则三）：只有 (topLeft,bottomRight)
// 做减法，另外两个直接摆字段 —— 做的事不一样。
static void cmp_rectf_ctor(double a, double b, double c, double d)
{
    const std::string in = rfin(a, b, c, d);
    const std::string sh = shapeOfRectF(a, b, c, d);
    // (topLeft,bottomRight) 的宽高是**减出来**的，形态要按减法后的量判
    const std::string shp = shapeOfRectF(a, b, c - a, d - b);

    rec("RF::ctorLTWH", same_rectf(QRectF(a, b, c, d), PkRectF(a, b, c, d)), sh, in,
        qstr(QRectF(a, b, c, d)), qstr(PkRectF(a, b, c, d)));
    rec("RF::ctorPoints",
        same_rectf(QRectF(QPointF(a, b), QPointF(c, d)),
                   PkRectF(PkPointF(a, b), PkPointF(c, d))),
        shp, in,
        qstr(QRectF(QPointF(a, b), QPointF(c, d))),
        qstr(PkRectF(PkPointF(a, b), PkPointF(c, d))));
    rec("RF::ctorPointSize",
        same_rectf(QRectF(QPointF(a, b), QSizeF(c, d)),
                   PkRectF(PkPointF(a, b), PkSizeF(c, d))),
        sh, in,
        qstr(QRectF(QPointF(a, b), QSizeF(c, d))),
        qstr(PkRectF(PkPointF(a, b), PkSizeF(c, d))));
}

// PkRect → PkRectF 的隐式提升。输入是**整数矩形的内部坐标**（走 setCoords，
// 与 cmp_rect_* 同一套语料），这样才够得到 x2 == x1-1 那些整数侧的退化形态。
static void cmp_rectf_from_rect(int x1, int y1, int x2, int y2)
{
    const QRect  q = mkQ(x1, y1, x2, y2);
    const PkRect p = mkP(x1, y1, x2, y2);
    rec("RF::ctorFromRect", same_rectf(QRectF(q), PkRectF(p)),
        shapeOfRect(x1, y1, x2, y2), rin(x1, y1, x2, y2),
        qstr(QRectF(q)), qstr(PkRectF(p)));
}

// 一元取值器 + normalized + 两个取整。输入是**四个字段** (x,y,w,h)。
static void cmp_rectf_unary(double x, double y, double w, double h)
{
    const QRectF  q = mkQF(x, y, w, h);
    const PkRectF p = mkPF(x, y, w, h);
    const std::string in = rfin(x, y, w, h);
    const std::string sh = shapeOfRectF(x, y, w, h);

    rec("RF::left", same_double(q.left(), p.left()), sh, in,
        dstr(q.left()), dstr(p.left()));
    rec("RF::top", same_double(q.top(), p.top()), sh, in, dstr(q.top()), dstr(p.top()));
    // ⚠ right/bottom 带一次加法，且**没有差一** —— 注入把它写成 xp+w-1 时
    // 这两条（且只有这两条）会红，tag 落在普通形态上，一眼看得出不是特值问题。
    rec("RF::right", same_double(q.right(), p.right()), sh, in,
        dstr(q.right()), dstr(p.right()));
    rec("RF::bottom", same_double(q.bottom(), p.bottom()), sh, in,
        dstr(q.bottom()), dstr(p.bottom()));
    rec("RF::x", same_double(q.x(), p.x()), sh, in, dstr(q.x()), dstr(p.x()));
    rec("RF::y", same_double(q.y(), p.y()), sh, in, dstr(q.y()), dstr(p.y()));
    rec("RF::width", same_double(q.width(), p.width()), sh, in,
        dstr(q.width()), dstr(p.width()));
    rec("RF::height", same_double(q.height(), p.height()), sh, in,
        dstr(q.height()), dstr(p.height()));
    rec("RF::size", same_szf(q.size(), p.size()), sh, in, qstr(q.size()), qstr(p.size()));

    // 三条谓词的分界线各不相同，**各用各的 tag**。⚠ 浮点版的分界与整数版全不同：
    // isNull 看 `w==0 && h==0`（-0.0 也算）、isEmpty 看 `<=0`、isValid 看 `>0`，
    // 而三者在 NaN 上**同时为假**，所以 nan 一档要单独看得见。
    rec("RF::isNull", q.isNull() == p.isNull(),
        (w == 0.0 && h == 0.0) ? std::string("null-side") : sh,
        in, bstr(q.isNull()), bstr(p.isNull()));
    rec("RF::isEmpty", q.isEmpty() == p.isEmpty(),
        (std::isnan(w) || std::isnan(h)) ? std::string("nan-side")
        : (w <= 0.0 || h <= 0.0) ? std::string("empty-side")
                                 : std::string("nonempty-side"),
        in, bstr(q.isEmpty()), bstr(p.isEmpty()));
    rec("RF::isValid", q.isValid() == p.isValid(),
        (std::isnan(w) || std::isnan(h)) ? std::string("nan-side")
        : (w > 0.0 && h > 0.0) ? std::string("valid-side")
                               : std::string("invalid-side"),
        in, bstr(q.isValid()), bstr(p.isValid()));

    rec("RF::topLeft", same_ptf(q.topLeft(), p.topLeft()), sh, in,
        qstr(q.topLeft()), qstr(p.topLeft()));
    rec("RF::topRight", same_ptf(q.topRight(), p.topRight()), sh, in,
        qstr(q.topRight()), qstr(p.topRight()));
    rec("RF::bottomLeft", same_ptf(q.bottomLeft(), p.bottomLeft()), sh, in,
        qstr(q.bottomLeft()), qstr(p.bottomLeft()));
    rec("RF::bottomRight", same_ptf(q.bottomRight(), p.bottomRight()), sh, in,
        qstr(q.bottomRight()), qstr(p.bottomRight()));

    // center 是 `xp + w/2`（先除后加）—— 写成 (left+right)/2 会在极大值上溢出
    // 到 inf，所以 huge 一档要看得见。
    rec("RF::center", same_ptf(q.center(), p.center()), sh, in,
        qstr(q.center()), qstr(p.center()));

    rec("RF::normalized", same_rectf(q.normalized(), p.normalized()),
        "x-" + swapAxisF(w) + "/y-" + swapAxisF(h),
        in, qstr(q.normalized()), qstr(p.normalized()));

    { double a1, b1, c1, d1, a2, b2, c2, d2;
      q.getRect(&a1, &b1, &c1, &d1); p.getRect(&a2, &b2, &c2, &d2);
      rec("RF::getRect", same_double(a1, a2) && same_double(b1, b2)
                      && same_double(c1, c2) && same_double(d1, d2), sh, in,
          rfin(a1, b1, c1, d1), rfin(a2, b2, c2, d2)); }
    { double a1, b1, c1, d1, a2, b2, c2, d2;
      q.getCoords(&a1, &b1, &c1, &d1); p.getCoords(&a2, &b2, &c2, &d2);
      rec("RF::getCoords", same_double(a1, a2) && same_double(b1, b2)
                        && same_double(c1, c2) && same_double(d1, d2), sh, in,
          rfin(a1, b1, c1, d1), rfin(a2, b2, c2, d2)); }

    // ⚠ 两个取整**各一条 rec 且共用同一个 roundShapeF**：它们在同一批输入上
    // 系统性地给不同答案，把它们并成一条会让"toAlignedRect 抄成 toRect"这类
    // 注入无法归因（注入实验 C 组正是这个）。
    const std::string rsh = roundShapeF(x, y, w, h);
    rec("RF::toRect", same_rect(q.toRect(), p.toRect()), rsh, in,
        qstr(q.toRect()), qstr(p.toRect()));
    rec("RF::toAlignedRect", same_rect(q.toAlignedRect(), p.toAlignedRect()), rsh, in,
        qstr(q.toAlignedRect()), qstr(p.toAlignedRect()));
}

// 全部带标量/点/尺寸参数的修改器。**一个重载一条 rec**（规则三）。
static void cmp_rectf_mutate(double x, double y, double w, double h, double a, double b)
{
    const QRectF  q0 = mkQF(x, y, w, h);
    const PkRectF p0 = mkPF(x, y, w, h);
    const std::string in = rfin(x, y, w, h) + "+(" + dstr(a) + "," + dstr(b) + ")";
    const std::string sh = shapeOfRectF(x, y, w, h) + "^" + argShapeF({a, b});

// 与整数版的 PK_MUT2 同一个理由：把**同一段源码**分别在 QRectF 与 PkRectF 上跑，
// 两侧写成两段代码时"改了一侧忘改另一侧"是静默的。
#define PK_MUTF2(label, ...)                                                  \
    do { QRectF q2 = q0; PkRectF p2 = p0;                                     \
         { QRectF &r = q2; __VA_ARGS__; }                                     \
         { PkRectF &r = p2; __VA_ARGS__; }                                    \
         rec(label, same_rectf(q2, p2), sh, in, qstr(q2), qstr(p2)); } while (0)

    // ⚠ set* 一族在浮点版里**保对边、改宽高**（整数版只摆一个坐标）——
    // setLeft 还走 `diff` 中间量，舍入与 `w = w + xp - pos` 不等价。
    PK_MUTF2("RF::setLeft", r.setLeft(a));
    PK_MUTF2("RF::setTop", r.setTop(a));
    PK_MUTF2("RF::setRight", r.setRight(a));
    PK_MUTF2("RF::setBottom", r.setBottom(a));
    PK_MUTF2("RF::setX", r.setX(a));
    PK_MUTF2("RF::setY", r.setY(a));
    PK_MUTF2("RF::setWidth", r.setWidth(a));
    PK_MUTF2("RF::setHeight", r.setHeight(a));
    PK_MUTF2("RF::moveLeft", r.moveLeft(a));
    PK_MUTF2("RF::moveTop", r.moveTop(a));
    PK_MUTF2("RF::moveToXY", r.moveTo(a, b));
    PK_MUTF2("RF::setRect", r.setRect(a, b, a, b));
    PK_MUTF2("RF::setCoords", r.setCoords(a, b, a, b));
    PK_MUTF2("RF::translateXY", r.translate(a, b));
    PK_MUTF2("RF::translatedXY", r = r.translated(a, b));

    // 吃 QPointF / QSizeF 的那几个：两侧的实参类型不同，宏套不进来，逐条写。
    { QRectF q2 = q0; PkRectF p2 = p0;
      q2.setTopLeft(QPointF(a, b)); p2.setTopLeft(PkPointF(a, b));
      rec("RF::setTopLeft", same_rectf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QRectF q2 = q0; PkRectF p2 = p0;
      q2.setBottomRight(QPointF(a, b)); p2.setBottomRight(PkPointF(a, b));
      rec("RF::setBottomRight", same_rectf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QRectF q2 = q0; PkRectF p2 = p0;
      q2.moveTopLeft(QPointF(a, b)); p2.moveTopLeft(PkPointF(a, b));
      rec("RF::moveTopLeft", same_rectf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QRectF q2 = q0; PkRectF p2 = p0;
      q2.moveCenter(QPointF(a, b)); p2.moveCenter(PkPointF(a, b));
      rec("RF::moveCenter", same_rectf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QRectF q2 = q0; PkRectF p2 = p0;
      q2.moveTo(QPointF(a, b)); p2.moveTo(PkPointF(a, b));
      rec("RF::moveToPoint", same_rectf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QRectF q2 = q0; PkRectF p2 = p0;
      q2.translate(QPointF(a, b)); p2.translate(PkPointF(a, b));
      rec("RF::translatePoint", same_rectf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { const QRectF q2 = q0.translated(QPointF(a, b));
      const PkRectF p2 = p0.translated(PkPointF(a, b));
      rec("RF::translatedPoint", same_rectf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QRectF q2 = q0; PkRectF p2 = p0;
      q2.setSize(QSizeF(a, b)); p2.setSize(PkSizeF(a, b));
      rec("RF::setSize", same_rectf(q2, p2), sh, in, qstr(q2), qstr(p2)); }

#undef PK_MUTF2
}

// adjust / adjusted：四个增量，单独一层输入。
// ⚠ 浮点版的宽高增量是 **xp2 - xp1**（两个增量之差），整数版是四个坐标各加各的。
static void cmp_rectf_adjust(double x, double y, double w, double h,
                             double d1, double d2, double d3, double d4)
{
    const QRectF  q0 = mkQF(x, y, w, h);
    const PkRectF p0 = mkPF(x, y, w, h);
    const std::string in = rfin(x, y, w, h) + "+" + rfin(d1, d2, d3, d4);
    const std::string sh = shapeOfRectF(x, y, w, h) + "^" + argShapeF({d1, d2, d3, d4});

    { QRectF q2 = q0; PkRectF p2 = p0;
      q2.adjust(d1, d2, d3, d4); p2.adjust(d1, d2, d3, d4);
      rec("RF::adjust", same_rectf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { const QRectF q2 = q0.adjusted(d1, d2, d3, d4);
      const PkRectF p2 = p0.adjusted(d1, d2, d3, d4);
      rec("RF::adjusted", same_rectf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
}

// 双目：| & |= &= united intersected intersects contains(rect) == !=
static void cmp_rectf_binary(double ax, double ay, double aw, double ah,
                             double bx, double by, double bw, double bh)
{
    const QRectF  qa = mkQF(ax, ay, aw, ah), qb = mkQF(bx, by, bw, bh);
    const PkRectF pa = mkPF(ax, ay, aw, ah), pb = mkPF(bx, by, bw, bh);
    const std::string in = rfin(ax, ay, aw, ah) + "|" + rfin(bx, by, bw, bh);
    const std::string tag = pairTagF(ax, ay, aw, ah, bx, by, bw, bh);

    rec("RF::operator|", same_rectf(qa | qb, pa | pb), tag, in,
        qstr(qa | qb), qstr(pa | pb));
    rec("RF::operator&", same_rectf(qa & qb, pa & pb), tag, in,
        qstr(qa & qb), qstr(pa & pb));
    // united / intersected 是 | / & 的转发，**仍然各写一条 rec**（规则三：
    // 转发链上坏掉的可能正是转发那一跳）。
    rec("RF::united", same_rectf(qa.united(qb), pa.united(pb)), tag, in,
        qstr(qa.united(qb)), qstr(pa.united(pb)));
    rec("RF::intersected", same_rectf(qa.intersected(qb), pa.intersected(pb)), tag, in,
        qstr(qa.intersected(qb)), qstr(pa.intersected(pb)));
    { QRectF q2 = qa; PkRectF p2 = pa; q2 |= qb; p2 |= pb;
      rec("RF::operator|=", same_rectf(q2, p2), tag, in, qstr(q2), qstr(p2)); }
    { QRectF q2 = qa; PkRectF p2 = pa; q2 &= qb; p2 &= pb;
      rec("RF::operator&=", same_rectf(q2, p2), tag, in, qstr(q2), qstr(p2)); }

    rec("RF::intersects", qa.intersects(qb) == pa.intersects(pb), tag, in,
        bstr(qa.intersects(qb)), bstr(pa.intersects(pb)));
    // ⚠ contains(rect) **没有 proper 参数**（整数版有两条 rec，这里只有一条）。
    // 它与 intersects 只差排除条件，抄串了正好在 "partial" 那一档上分家。
    rec("RF::containsRect", qa.contains(qb) == pa.contains(pb), tag, in,
        bstr(qa.contains(qb)), bstr(pa.contains(pb)));

    // == / != 是**模糊比较**，与相对位置无关：tag 用形态对 + 逐分量的 fuzzy 关系
    //（**自己算**，不调用两侧任何一个 operator==，否则等于用被测物给自己贴标签）。
    auto fuzzyEq = [](double u, double v) {
        const double au = std::fabs(u), av = std::fabs(v);
        return std::fabs(u - v) * 1000000000000. <= (au < av ? au : av);
    };
    const int nEq = int(fuzzyEq(ax, bx)) + int(fuzzyEq(ay, by))
                  + int(fuzzyEq(aw, bw)) + int(fuzzyEq(ah, bh));
    const std::string eqtag = shapeOfRectF(ax, ay, aw, ah) + "^"
                            + shapeOfRectF(bx, by, bw, bh) + "/fuzzy-"
                            + std::to_string(nEq) + "of4";
    rec("RF::operator==", (qa == qb) == (pa == pb), eqtag, in,
        bstr(qa == qb), bstr(pa == pb));
    rec("RF::operator!=", (qa != qb) == (pa != pb), eqtag, in,
        bstr(qa != qb), bstr(pa != pb));
}

// contains(point) 的两个重载。**各一条 rec**（规则三）：contains(qreal,qreal)
// 转发到 contains(PkPointF)，而**转发那一跳本身可能坏**。
static void cmp_rectf_contains_point(double x, double y, double w, double h,
                                     double px, double py)
{
    const QRectF  q = mkQF(x, y, w, h);
    const PkRectF p = mkPF(x, y, w, h);
    const std::string in = rfin(x, y, w, h) + "@(" + dstr(px) + "," + dstr(py) + ")";
    const std::string tag = "x-" + edgeTagF(x, w, px) + "/y-" + edgeTagF(y, h, py);

    rec("RF::containsPoint", q.contains(QPointF(px, py)) == p.contains(PkPointF(px, py)),
        tag, in,
        bstr(q.contains(QPointF(px, py))), bstr(p.contains(PkPointF(px, py))));
    rec("RF::containsXY", q.contains(px, py) == p.contains(px, py), tag, in,
        bstr(q.contains(px, py)), bstr(p.contains(px, py)));
}

// ═══ Transform 族：比较原语与 tag 谓词 ════════════════════════════════════
//
// ⚠ **矩阵一律按九个分量按位比较与打印**，不按 type()/isIdentity() 那些派生量：
// 派生量是多对一的（无穷多个矩阵同为 TxScale），用它们比会把一整类差异静默豁免。
// 九个取值器各自是一行、无算术，正好当地基 —— 与 Point/Rect 三族同一条口径。
//
// ⚠ **type() 单独有一条 rec**，而且它是**有状态**的（见 PkTransform.h 的
// 「惰性缓存」）：同一个矩阵、问过与没问过 type()，后续答案不同。所以
//   · 凡是"只想比这一个 API"的 rec，两侧各自**新建**一对对象，避免上一条 rec
//     留下的缓存状态串到下一条；
//   · 缓存本身由 cmp_tf_cache() 专门喂序列去比。
// 两条合起来才既比得干净、又不放过缓存语义。

static std::string qstr(const QTransform &t)
{
    return "{" + dstr(t.m11()) + "," + dstr(t.m12()) + "," + dstr(t.m13()) + ";"
               + dstr(t.m21()) + "," + dstr(t.m22()) + "," + dstr(t.m23()) + ";"
               + dstr(t.m31()) + "," + dstr(t.m32()) + "," + dstr(t.m33()) + "}";
}
static std::string qstr(const PkTransform &t)
{
    return "{" + dstr(t.m11()) + "," + dstr(t.m12()) + "," + dstr(t.m13()) + ";"
               + dstr(t.m21()) + "," + dstr(t.m22()) + "," + dstr(t.m23()) + ";"
               + dstr(t.m31()) + "," + dstr(t.m32()) + "," + dstr(t.m33()) + "}";
}

static bool same_tf(const QTransform &q, const PkTransform &p)
{
    return same_double(q.m11(), p.m11()) && same_double(q.m12(), p.m12())
        && same_double(q.m13(), p.m13()) && same_double(q.m21(), p.m21())
        && same_double(q.m22(), p.m22()) && same_double(q.m23(), p.m23())
        && same_double(q.m31(), p.m31()) && same_double(q.m32(), p.m32())
        && same_double(q.m33(), p.m33());
}

// 九参构造：**两侧唯一够得到全部九个分量的入口**（六参构造摸不到 m13/m23/m33）。
static QTransform mkQT(const double m[9])
{ return QTransform(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]); }
static PkTransform mkPT(const double m[9])
{ return PkTransform(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]); }

static std::string tfin(const double m[9])
{
    std::string s = "{";
    for (int i = 0; i < 9; ++i) { s += dstr(m[i]); if (i != 8) s += ","; }
    return s + "}";
}

// ── tag 谓词：**全部从输入的九个分量算出来**，一个都不问被测对象 ────────────
//
// ⚠ 关键纪律：不能写 `q.type() == QTransform::TxProject` 这种 —— 那是拿**被测
// 的一侧**去构造 tag，被测对象坏掉时 tag 会跟着一起坏（差异被贴到别的桶里，
// 或者干脆两侧 tag 不同而没人发现）。下面几条都是拿原始 double 重算的。

// qglobal.h 的 qFuzzyIsNull(double)：|d| <= 1e-12。type() 的四道门槛都用它。
static bool tfFuzzyIsNull(double d) { return std::fabs(d) <= 0.000000000001; }

// Qt 的 qMin 是 `(a < b) ? a : b`，**不是** std::fmin —— NaN 上两者取值不同，
// 而 needsPerspectiveClipping 正好可能吃到 NaN。照抄 qMin 的形状。
static double tfMin(double a, double b) { return (a < b) ? a : b; }

// 三阶行列式，展开顺序照抄 qtransform.h:250-254（浮点加减不结合，换写法换取值）。
static double tfDet(const double m[9])
{
    return m[0] * (m[8] * m[4] - m[7] * m[5]) -
        m[3] * (m[8] * m[1] - m[7] * m[2]) + m[6] * (m[5] * m[1] - m[4] * m[2]);
}

// 「**新构造**的九参矩阵的 type() 是不是 TxProject」——
// 这条只对 m_dirty == TxProject 的对象成立（九参构造与 setMatrix 之后），
// 下面凡是用它的地方喂的都是这种对象。用在别处会算错，别抄走。
static bool tfFreshIsProject(const double m[9])
{ return !tfFuzzyIsNull(m[2]) || !tfFuzzyIsNull(m[5]) || !tfFuzzyIsNull(m[8] - 1); }

// qtransform.cpp:1934-1940 的 needsPerspectiveClipping，就地重算。
// ⚠ 传进来的是 **QRectF 口径的 left/right/top/bottom**（left=x, right=x+w），
// 整数矩形那一侧也是先提升成 QRectF 再算的，所以口径一致。
static bool tfNeedsClip(const double m[9], double l, double r, double t, double b)
{
    const double wx = tfMin(m[2] * l, m[2] * r);
    const double wy = tfMin(m[5] * t, m[5] * b);
    return wx + wy + m[8] < 0.000001;
}

// 结构形态：投影 vs 仿射，用**精确**判据（m13/m23 非 0 或 m33 != 1）。
// 与上面 tfFreshIsProject 的模糊判据是两条线：这条给通用 tag 用（宽一点没关系，
// 它不为任何偏离背书），那条只给 persp-clip 那一条偏离用（必须与 Qt 的判据同宽）。
static std::string shapeOfTf(const double m[9])
{
    const bool proj = (m[2] != 0.0 || m[5] != 0.0 || m[8] != 1.0);
    return std::string(proj ? "proj" : "aff") + "-"
        + shapeOfD({m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]});
}

// inverted 专用：三条路径的门槛互不相同，tag 要能把它们分开。
static std::string shapeOfInv(const double m[9])
{
    const double det = tfDet(m);
    std::string sing;
    if (det != det)                      sing = "det-nan";
    else if (det == 0.0)                 sing = "singular";
    else if (std::fabs(det) < 1e-12)     sing = "near-singular";
    else                                 sing = "regular";
    return shapeOfTf(m) + "/" + sing;
}

// rotate 的直角特判是本族最尖锐的一条，所以「角度是不是 Qt 特判的那五个之一」
// 必须参与 tag 构造（规则一）。
// ⚠ 谓词写的是 Qt 源码里那五个字面量（90/-270/270/-90/180），**不是**「90 的
// 整数倍」：-180 与 360 不在特判里，把它们算进来就比理由宽了。
static bool tfSpecialAngle(double a)
{ return a == 90. || a == -270. || a == 270. || a == -90. || a == 180.; }

// ═══ Transform 族：逐 API 对拍 ════════════════════════════════════════════

// 无输入的 API（默认构造），口径同 cmp_rect_constants。只跑一次。
static void cmp_tf_constants()
{
    rec("T::defaultCtor", same_tf(QTransform(), PkTransform()), "no-input",
        "QTransform()", qstr(QTransform()), qstr(PkTransform()));
}

// 两个公开构造各一条 rec（规则三）。**它们不是一件事**：九参摸得到 m13/m23/m33
// 且把 m_dirty 播成 TxProject，六参摸不到、播成 TxShear。
static void cmp_tf_ctor(const double m[9])
{
    const std::string in = tfin(m);
    const std::string sh = shapeOfTf(m);

    rec("T::ctor9", same_tf(mkQT(m), mkPT(m)), sh, in, qstr(mkQT(m)), qstr(mkPT(m)));

    // 六参构造取 (m11,m12,m21,m22,dx,dy)，也就是 m[0],m[1],m[3],m[4],m[6],m[7]。
    const QTransform q6(m[0], m[1], m[3], m[4], m[6], m[7]);
    const PkTransform p6(m[0], m[1], m[3], m[4], m[6], m[7]);
    rec("T::ctor6", same_tf(q6, p6), sh, in, qstr(q6), qstr(p6));
}

// 九个取值器 + dx/dy + determinant + type + isAffine + isIdentity +
// transposed + reset + setMatrix + inverted。
// ⚠ **每条 rec 各自新建一对对象**：type()/isAffine()/isIdentity() 都会改写缓存，
// 共用一个对象的话上一条的副作用会串到下一条，差异归因就乱了。
static void cmp_tf_unary(const double m[9])
{
    const std::string in = tfin(m);
    const std::string sh = shapeOfTf(m);

    {
        const QTransform q = mkQT(m); const PkTransform p = mkPT(m);
        rec("T::m11", same_double(q.m11(), p.m11()), sh, in, dstr(q.m11()), dstr(p.m11()));
        rec("T::m12", same_double(q.m12(), p.m12()), sh, in, dstr(q.m12()), dstr(p.m12()));
        rec("T::m13", same_double(q.m13(), p.m13()), sh, in, dstr(q.m13()), dstr(p.m13()));
        rec("T::m21", same_double(q.m21(), p.m21()), sh, in, dstr(q.m21()), dstr(p.m21()));
        rec("T::m22", same_double(q.m22(), p.m22()), sh, in, dstr(q.m22()), dstr(p.m22()));
        rec("T::m23", same_double(q.m23(), p.m23()), sh, in, dstr(q.m23()), dstr(p.m23()));
        rec("T::m31", same_double(q.m31(), p.m31()), sh, in, dstr(q.m31()), dstr(p.m31()));
        rec("T::m32", same_double(q.m32(), p.m32()), sh, in, dstr(q.m32()), dstr(p.m32()));
        rec("T::m33", same_double(q.m33(), p.m33()), sh, in, dstr(q.m33()), dstr(p.m33()));
        rec("T::dx", same_double(q.dx(), p.dx()), sh, in, dstr(q.dx()), dstr(p.dx()));
        rec("T::dy", same_double(q.dy(), p.dy()), sh, in, dstr(q.dy()), dstr(p.dy()));
        rec("T::determinant", same_double(q.determinant(), p.determinant()), sh, in,
            dstr(q.determinant()), dstr(p.determinant()));
    }
    {
        const QTransform q = mkQT(m); const PkTransform p = mkPT(m);
        rec("T::type", (int)q.type() == (int)p.type(), sh, in,
            istr((int)q.type()), istr((int)p.type()));
    }
    {
        const QTransform q = mkQT(m); const PkTransform p = mkPT(m);
        rec("T::isAffine", q.isAffine() == p.isAffine(), sh, in,
            bstr(q.isAffine()), bstr(p.isAffine()));
    }
    {
        const QTransform q = mkQT(m); const PkTransform p = mkPT(m);
        rec("T::isIdentity", q.isIdentity() == p.isIdentity(), sh, in,
            bstr(q.isIdentity()), bstr(p.isIdentity()));
    }
    {
        const QTransform q = mkQT(m); const PkTransform p = mkPT(m);
        rec("T::transposed", same_tf(q.transposed(), p.transposed()), sh, in,
            qstr(q.transposed()), qstr(p.transposed()));
    }
    {
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        q.reset(); p.reset();
        // 取值必然是单位阵；这条 rec 真正要抓的是"reset 有没有把缓存也清掉"，
        // 所以连 type() 一起比。
        rec("T::reset", same_tf(q, p) && (int)q.type() == (int)p.type(), sh, in,
            qstr(q) + "|" + istr((int)q.type()), qstr(p) + "|" + istr((int)p.type()));
    }
    {
        QTransform q; PkTransform p;
        q.setMatrix(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
        p.setMatrix(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
        rec("T::setMatrix", same_tf(q, p) && (int)q.type() == (int)p.type(), sh, in,
            qstr(q) + "|" + istr((int)q.type()), qstr(p) + "|" + istr((int)p.type()));
    }
    {
        // ⚠ 规则二的靶子：**出参、结果矩阵、结果档位三样一起比**。
        // 只比 `invertible` 标志位的话，"成功路径上矩阵整体乘 2"这类缺陷全绿
        //（Step 8 第三组注入专门验这一条）；只比矩阵不比档位的话，
        // "失败时把源档位拷过去了"这类缺陷看不见。
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        bool qok = false, pok = false;
        const QTransform qi = q.inverted(&qok);
        const PkTransform pi = p.inverted(&pok);
        const bool same = (qok == pok) && same_tf(qi, pi)
                       && (int)qi.type() == (int)pi.type();
        rec("T::inverted", same, shapeOfInv(m), in,
            bstr(qok) + qstr(qi) + "|" + istr((int)qi.type()),
            bstr(pok) + qstr(pi) + "|" + istr((int)pi.type()));
    }
    {
        // 同一条声明的另一个用法：不传出参（默认实参 nullptr）。
        // 单独一条 label，挂在同一条声明上（transform_api.map 里两个标签）。
        const QTransform qi = mkQT(m).inverted();
        const PkTransform pi = mkPT(m).inverted();
        rec("T::inverted(noflag)", same_tf(qi, pi), shapeOfInv(m), in,
            qstr(qi), qstr(pi));
    }
}

// ── I-1：四个标量运算符各自的"制造过期"路径 ──────────────────────────────
//
// 四个对 m_dirty 的写法互不相同，所以"档位会不会过期、什么时候过期"在四个上
// 各不相同，**必须四个都喂序列** —— 只喂 `*=` 一条会漏掉另外三条过期路径：
//   · `*=` 条件抬升到 TxScale（`if (m_dirty < TxScale)`）—— 原矩阵档位高于
//     TxScale 时就会留下过期值
//   · `/=` 转发给 `*=`，但多一道 `div == 0` 的提前返回
//   · `+=` / `-=` **无条件**把 m_dirty 钉成 TxProject —— 于是它们**永远不会**
//     留下过期值（下一次 type() 必定全量重算）。这一条与上面两条正好相反，
//     不喂就证明不了"我们抄的是无条件赋值而不是条件抬升"
// 四个各自还有提前返回（num==1 / div==0 / num==0），提前返回时 m_dirty
// **一个字都不动** —— 那是第三种档位演化，tag 里必须看得见。
static const char *kTfOpName[4] = { "mul", "div", "add", "sub" };

static void tfApplyQ(QTransform &q, double s, int op)
{
    switch (op) {
    case 0: q *= s; break;
    case 1: q /= s; break;
    case 2: q += s; break;
    default: q -= s; break;
    }
}
static void tfApplyP(PkTransform &p, double s, int op)
{
    switch (op) {
    case 0: p *= s; break;
    case 1: p /= s; break;
    case 2: p += s; break;
    default: p -= s; break;
    }
}
// 这一次是不是提前返回（= m_dirty 一个字不动）。判据照抄四个运算符各自的第一行。
static bool tfOpIsNoop(double s, int op)
{
    if (op == 0) return s == 1.;
    if (op == 1) return s == 0;
    return s == 0;
}

// 惰性缓存专用：喂**序列**而不是单个矩阵。
// 序列 A：type() → *= 0.5 → type()   （档位可能过期）
// 序列 B：           *= 0.5 → type()   （全量重算）
// 两条必须逐输入一致，且 A 与 B 在同一批输入上**本来就该给不同答案** ——
// 那正是"缓存是可观测语义"的形状。
static void cmp_tf_cache(const double m[9], double s, int op)
{
    const std::string in = tfin(m) + "#" + kTfOpName[op] + dstr(s);
    // tag 由输入形态构造（规则一）：矩阵形态 × 哪个运算符 × 这一次是不是提前返回。
    // 后两个都是从实参算出来的，不是常量。
    const std::string sh = shapeOfTf(m) + "/" + kTfOpName[op] + "/"
        + (tfOpIsNoop(s, op) ? "no-op" : "applied");

    // ── 分支一：**中间问过 type()**（把 m_dirty 清零、m_type 钉住），
    //           于是接下来的写入可能留下过期值
    {
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        (void)q.type(); (void)p.type();
        tfApplyQ(q, s, op); tfApplyP(p, s, op);
        rec("T::type(stale)", (int)q.type() == (int)p.type(), sh, in,
            istr((int)q.type()), istr((int)p.type()));
    }
    // ── 分支二：**中间不问**，m_dirty 仍是构造时的 TxProject，走全量重算。
    //           两条分支在同一批输入上**本来就该给不同答案** —— 那正是
    //           "缓存是可观测语义"的形状；缺了这一条，分支一自己证明不了什么。
    {
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        tfApplyQ(q, s, op); tfApplyP(p, s, op);
        rec("T::type(fresh)", (int)q.type() == (int)p.type(), sh, in,
            istr((int)q.type()), istr((int)p.type()));
    }
    {
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        (void)q.type(); (void)p.type();
        tfApplyQ(q, s, op); tfApplyP(p, s, op);
        rec("T::isIdentity(stale)", q.isIdentity() == p.isIdentity(), sh, in,
            bstr(q.isIdentity()), bstr(p.isIdentity()));
    }
    {
        // 过期档位会传染到 map：这一条比的是**取值**，走的是被过期档位选中的分支。
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        (void)q.type(); (void)p.type();
        tfApplyQ(q, s, op); tfApplyP(p, s, op);
        rec("T::map(stale)", same_ptf(q.map(QPointF(3, 4)), p.map(PkPointF(3, 4))), sh, in,
            qstr(q.map(QPointF(3, 4))), qstr(p.map(PkPointF(3, 4))));
    }
    {
        // 拷贝要把缓存状态一起带走（Qt 手写的 operator= 逐字段拷 m_type/m_dirty，
        // 我们靠编译器生成的拷贝 —— 这条 rec 就是那个等价性的判据）。
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        (void)q.type(); (void)p.type();
        tfApplyQ(q, s, op); tfApplyP(p, s, op);
        const QTransform qc = q; const PkTransform pc = p;
        rec("T::copy(stale)", (int)qc.type() == (int)pc.type(), sh, in,
            istr((int)qc.type()), istr((int)pc.type()));
    }
}

// ── I-1：旋转往返序列（brief 点名的 type/after-roundtrip）───────────────────
//
// `rotate(a)` 之后 `rotate(-a)`：直角特判下矩阵**精确**回到起点，于是 type()
// 也回到起点；走 sin/cos 的角度上回不到精确起点。两条分支（中间问过 type() /
// 不问）都要比 —— 中间那一问会把 m_type 钉在 TxRotate 上，而 rotate 的
// `if (m_dirty < TxRotate)` 是条件抬升，两者叠起来才是真实的调用形态。
static void cmp_tf_roundtrip(const double m[9], double ang)
{
    const std::string in = tfin(m) + "@rt" + dstr(ang);
    const std::string sh = shapeOfTf(m) + "/"
        + (tfSpecialAngle(ang) ? "right-angle"
           : ang == 0. ? std::string("zero-angle") : shapeOfD({ang}))
        + "/after-roundtrip";

    {
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        q.rotate(ang); q.rotate(-ang);
        p.rotate(ang); p.rotate(-ang);
        rec("T::rotate(roundtrip)", same_tf(q, p) && (int)q.type() == (int)p.type(), sh, in,
            qstr(q) + "|" + istr((int)q.type()), qstr(p) + "|" + istr((int)p.type()));
    }
    {
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        q.rotate(ang); (void)q.type(); q.rotate(-ang);
        p.rotate(ang); (void)p.type(); p.rotate(-ang);
        rec("T::rotate(roundtrip-asked)", same_tf(q, p) && (int)q.type() == (int)p.type(),
            sh, in, qstr(q) + "|" + istr((int)q.type()),
            qstr(p) + "|" + istr((int)p.type()));
    }
}

// ── I-1：连续 mutator 链 ───────────────────────────────────────────────────
//
// translate → scale → shear → rotate 四步连着走，每一步都按 inline_type() 分档
// 选公式，而档位又被上一步的 m_dirty 写法决定 —— **档位错一步，后面三步的公式
// 全错**。两条分支：每步之后都问一次 type()（把缓存钉住）/ 全程不问（一路带着
// 脏标志）。单点调用证明不了这个。
static void cmp_tf_chain(const double m[9], double a)
{
    const std::string in = tfin(m) + "@chain" + dstr(a);
    const std::string sh = shapeOfTf(m) + "/" + shapeOfD({a}) + "/chain";

    {
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        q.translate(a, a); q.scale(a, a); q.shear(a, a); q.rotate(a);
        p.translate(a, a); p.scale(a, a); p.shear(a, a); p.rotate(a);
        rec("T::type(chain)", same_tf(q, p) && (int)q.type() == (int)p.type(), sh, in,
            qstr(q) + "|" + istr((int)q.type()), qstr(p) + "|" + istr((int)p.type()));
    }
    {
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        q.translate(a, a); (void)q.type(); q.scale(a, a); (void)q.type();
        q.shear(a, a); (void)q.type(); q.rotate(a);
        p.translate(a, a); (void)p.type(); p.scale(a, a); (void)p.type();
        p.shear(a, a); (void)p.type(); p.rotate(a);
        rec("T::type(chain-asked)", same_tf(q, p) && (int)q.type() == (int)p.type(), sh, in,
            qstr(q) + "|" + istr((int)q.type()), qstr(p) + "|" + istr((int)p.type()));
    }
}

// translate / scale / shear：三条各自一条 rec（规则三），各自新建一对对象。
// 结果连 type() 一起比 —— 这三个的末尾都是**条件抬升** `if (m_dirty < TxN)`，
// 写成无条件赋值时矩阵取值不变、只有档位会变，不比档位就抓不到。
static void cmp_tf_mutate(const double m[9], double a, double b)
{
    const std::string in = tfin(m) + "(" + dstr(a) + "," + dstr(b) + ")";
    const std::string sh = shapeOfTf(m) + "/" + shapeOfD({a, b});

    {
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        q.translate(a, b); p.translate(a, b);
        rec("T::translate", same_tf(q, p) && (int)q.type() == (int)p.type(), sh, in,
            qstr(q) + "|" + istr((int)q.type()), qstr(p) + "|" + istr((int)p.type()));
    }
    {
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        q.scale(a, b); p.scale(a, b);
        rec("T::scale", same_tf(q, p) && (int)q.type() == (int)p.type(), sh, in,
            qstr(q) + "|" + istr((int)q.type()), qstr(p) + "|" + istr((int)p.type()));
    }
    {
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        q.shear(a, b); p.shear(a, b);
        rec("T::shear", same_tf(q, p) && (int)q.type() == (int)p.type(), sh, in,
            qstr(q) + "|" + istr((int)q.type()), qstr(p) + "|" + istr((int)p.type()));
    }
}

// rotate / rotateRadians。**直角特判**是本族最尖锐的一条（tfSpecialAngle
// 定义在上面的 tag 谓词一节）—— 去掉特判之后差异全部落在 right-angle 这个桶里，
// 一眼看得出根因。
static void cmp_tf_rotate(const double m[9], double ang)
{
    const std::string in = tfin(m) + "@" + dstr(ang);
    const std::string sh = shapeOfTf(m) + "/"
        + (tfSpecialAngle(ang) ? "right-angle"
           : ang == 0. ? std::string("zero-angle") : shapeOfD({ang}));

    {
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        q.rotate(ang); p.rotate(ang);
        rec("T::rotate", same_tf(q, p) && (int)q.type() == (int)p.type(), sh, in,
            qstr(q) + "|" + istr((int)q.type()), qstr(p) + "|" + istr((int)p.type()));
    }
    {
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        q.rotateRadians(ang); p.rotateRadians(ang);
        rec("T::rotateRadians", same_tf(q, p) && (int)q.type() == (int)p.type(), sh, in,
            qstr(q) + "|" + istr((int)q.type()), qstr(p) + "|" + istr((int)p.type()));
    }
    // 非 Z 轴：同一条声明的另一条路径（造 result 再 result * *this），
    // 单独一个 label 挂在同一条声明上。
    {
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        q.rotate(ang, Qt::YAxis); p.rotate(ang, pkoracle::Qt::YAxis);
        rec("T::rotate(axis)", same_tf(q, p) && (int)q.type() == (int)p.type(),
            sh + "/y", in, qstr(q) + "|" + istr((int)q.type()),
            qstr(p) + "|" + istr((int)p.type()));
    }
    {
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        q.rotate(ang, Qt::XAxis); p.rotate(ang, pkoracle::Qt::XAxis);
        rec("T::rotate(axis)", same_tf(q, p) && (int)q.type() == (int)p.type(),
            sh + "/x", in, qstr(q) + "|" + istr((int)q.type()),
            qstr(p) + "|" + istr((int)p.type()));
    }
    {
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        q.rotateRadians(ang, Qt::YAxis); p.rotateRadians(ang, pkoracle::Qt::YAxis);
        rec("T::rotateRadians(axis)", same_tf(q, p) && (int)q.type() == (int)p.type(),
            sh + "/y", in, qstr(q) + "|" + istr((int)q.type()),
            qstr(p) + "|" + istr((int)p.type()));
    }
}

// 四个静态/自由的标量运算符，各自一条 rec（规则三）。
// ⚠ 四个对 m_dirty 的写法互不相同（*= 条件抬升、/= 转发、+= 与 -= 无条件钉死），
// 所以四条都连 type() 一起比。
static void cmp_tf_scalar(const double m[9], double s)
{
    const std::string in = tfin(m) + "#" + dstr(s);
    const std::string sh = shapeOfTf(m) + "/" + shapeOfD({s});

    {
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        q *= s; p *= s;
        rec("T::operator*=(qreal)", same_tf(q, p) && (int)q.type() == (int)p.type(), sh, in,
            qstr(q) + "|" + istr((int)q.type()), qstr(p) + "|" + istr((int)p.type()));
    }
    {
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        q /= s; p /= s;
        rec("T::operator/=(qreal)", same_tf(q, p) && (int)q.type() == (int)p.type(), sh, in,
            qstr(q) + "|" + istr((int)q.type()), qstr(p) + "|" + istr((int)p.type()));
    }
    {
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        q += s; p += s;
        rec("T::operator+=(qreal)", same_tf(q, p) && (int)q.type() == (int)p.type(), sh, in,
            qstr(q) + "|" + istr((int)q.type()), qstr(p) + "|" + istr((int)p.type()));
    }
    {
        QTransform q = mkQT(m); PkTransform p = mkPT(m);
        q -= s; p -= s;
        rec("T::operator-=(qreal)", same_tf(q, p) && (int)q.type() == (int)p.type(), sh, in,
            qstr(q) + "|" + istr((int)q.type()), qstr(p) + "|" + istr((int)p.type()));
    }
    // 四个自由函数（头文件里的 operator* / / / + / - (T, qreal)）。它们不在类体里，
    // 规则三的机器闸门看不见它们，但一样各给一条 rec —— 转发链坏在"拷一份"那一跳
    // 时复合版是好的、自由版是坏的。
    {
        rec("T::free*(qreal)", same_tf(mkQT(m) * s, mkPT(m) * s), sh, in,
            qstr(mkQT(m) * s), qstr(mkPT(m) * s));
        rec("T::free/(qreal)", same_tf(mkQT(m) / s, mkPT(m) / s), sh, in,
            qstr(mkQT(m) / s), qstr(mkPT(m) / s));
        rec("T::free+(qreal)", same_tf(mkQT(m) + s, mkPT(m) + s), sh, in,
            qstr(mkQT(m) + s), qstr(mkPT(m) + s));
        rec("T::free-(qreal)", same_tf(mkQT(m) - s, mkPT(m) - s), sh, in,
            qstr(mkQT(m) - s), qstr(mkPT(m) - s));
    }
}

// 前向声明：Line 族的 qstr/same_linef 定义在本文件更下方（Line 族小节），
// 但 T::map(LineF)（R-21 T1 新增）要在 Transform 族的 cmp_tf_map 里用它们
// ——C++ 没有跨越普通函数定义的隐式前向查找，不声明会在此处报「no known
// conversion」。只声明这三个签名，不改变下方 Line 族小节里的定义。
static std::string qstr(const QLineF &l);
static std::string qstr(const PkLineF &l);
static bool same_linef(const QLineF &q, const PkLineF &p);

// 四个 map 重载。**分两族**：map(点) 不夹持 w、map(指针出参) 夹持到 1e-6，
// 所以 tag 里「这次是不是投影阵」必须参与构造。
static void cmp_tf_map(const double m[9], double x, double y)
{
    const QTransform q = mkQT(m);
    const PkTransform p = mkPT(m);
    const std::string in = tfin(m) + "@(" + dstr(x) + "," + dstr(y) + ")";
    // 分母 w 的取值是这一族唯一会分家的形态，所以它参与 tag：
    // 大于夹持阈值 / 落进夹持区（含负与 0）/ 非有限。
    const double w = m[2] * x + m[5] * y + m[8];
    const std::string wtag = (w != w) ? "w-nan"
        : std::isinf(w) ? "w-inf"
        : (w < 0.000001) ? "w-clipped" : "w-plain";
    const std::string sh = shapeOfTf(m) + "/" + wtag + "/" + shapeOfD({x, y});

    rec("T::map(PkPointF)", same_ptf(q.map(QPointF(x, y)), p.map(PkPointF(x, y))), sh, in,
        qstr(q.map(QPointF(x, y))), qstr(p.map(PkPointF(x, y))));

    // T::map(const PkLineF&) —— 真 Qt qtransform.cpp 就是
    // `QLineF(map(l.p1()), map(l.p2()))`：两个端点各自走上面已经测过的
    // map(PkPointF)，不是新算法。这里用 (x,y)/(y,x) 拼一条线，两个端点仍在
    // 上面 nPt×nPt 的 token 组合范围内，覆盖度与 map(PkPointF) 同源。
    // R-21 T1 交付 PkLineF 后新增此重载，须补 rec() 才满足规则三。
    {
        const QLineF ql(QPointF(x, y), QPointF(y, x));
        const PkLineF pl(PkPointF(x, y), PkPointF(y, x));
        rec("T::map(LineF)", same_linef(q.map(ql), p.map(pl)), sh, in,
            qstr(q.map(ql)), qstr(p.map(pl)));
        rec("T::freeLineF*", same_linef(ql * q, pl * p), sh, in,
            qstr(ql * q), qstr(pl * p));
    }

    {
        qreal qtx = 0, qty = 0, ptx = 0, pty = 0;
        q.map(x, y, &qtx, &qty);
        p.map(x, y, &ptx, &pty);
        rec("T::map(qreal*)", same_double(qtx, ptx) && same_double(qty, pty), sh, in,
            "(" + dstr(qtx) + "," + dstr(qty) + ")",
            "(" + dstr(ptx) + "," + dstr(pty) + ")");
    }
    // 自由函数 operator*(QPointF, QTransform) 只是转发，单独一条 rec。
    rec("T::freePointF*", same_ptf(QPointF(x, y) * q, PkPointF(x, y) * p), sh, in,
        qstr(QPointF(x, y) * q), qstr(PkPointF(x, y) * p));
}

// 整数版的两个 map 重载（走 qRound，落进 int 值域之外是另一类 UB，
// 与 Point 族同一条口径：两侧运行期同一条指令，编译期折叠不参与）。
static void cmp_tf_map_int(const double m[9], int x, int y)
{
    const QTransform q = mkQT(m);
    const PkTransform p = mkPT(m);
    const std::string in = tfin(m) + "@i(" + istr(x) + "," + istr(y) + ")";
    const double w = m[2] * x + m[5] * y + m[8];
    const std::string wtag = (w != w) ? "w-nan"
        : std::isinf(w) ? "w-inf"
        : (w < 0.000001) ? "w-clipped" : "w-plain";
    const std::string sh = shapeOfTf(m) + "/" + wtag + "/" + shapeOfI({x, y});

    rec("T::map(PkPoint)", same_pt(q.map(QPoint(x, y)), p.map(PkPoint(x, y))), sh, in,
        qstr(q.map(QPoint(x, y))), qstr(p.map(PkPoint(x, y))));

    {
        int qtx = 0, qty = 0, ptx = 0, pty = 0;
        q.map(x, y, &qtx, &qty);
        p.map(x, y, &ptx, &pty);
        rec("T::map(int*)", qtx == ptx && qty == pty, sh, in,
            "(" + istr(qtx) + "," + istr(qty) + ")",
            "(" + istr(ptx) + "," + istr(pty) + ")");
    }
    rec("T::freePoint*", same_pt(QPoint(x, y) * q, PkPoint(x, y) * p), sh, in,
        qstr(QPoint(x, y) * q), qstr(PkPoint(x, y) * p));
}

// 两个 mapRect 重载。
// ⚠ **本族唯一一处已声明的行为偏离住在这里**：Qt 在
// 「type()==TxProject 且 needsPerspectiveClipping(rect)」时走 QPainterPath，
// 我们落回四角包围盒。tag 的谓词就是这条判据本身（tfFreshIsProject 用的是
// 与 Qt 的 type() 逐字相同的模糊门槛，tfNeedsClip 是 qtransform.cpp:1934-1940
// 的就地重算），**一个限定词都不宽**。
static void cmp_tf_maprect(const double m[9], double x, double y, double w, double h)
{
    const QTransform q = mkQT(m);
    const PkTransform p = mkPT(m);
    const std::string in = tfin(m) + "R(" + dstr(x) + "," + dstr(y) + ","
                         + dstr(w) + "," + dstr(h) + ")";
    const bool clip = tfFreshIsProject(m) && tfNeedsClip(m, x, x + w, y, y + h);
    // ⚠ **矩形那四个分量也要参与**（约定见 shapeOfD 上方那段：一个 API 的 shape
    // 必须由它全部参与分量算出）。persp-clip 这一支只贴矩阵形态的话，
    // "缺陷只在某种矩形上触发"就会被贴成与根因无关的标签。
    const std::string base = shapeOfTf(m) + "/" + shapeOfD({x, y, w, h});
    const std::string sh = clip ? ("persp-clip/" + base) : base;

    rec("T::mapRect(PkRectF)",
        same_rectf(q.mapRect(QRectF(x, y, w, h)), p.mapRect(PkRectF(x, y, w, h))),
        sh, in, qstr(q.mapRect(QRectF(x, y, w, h))), qstr(p.mapRect(PkRectF(x, y, w, h))));
}

static void cmp_tf_maprect_int(const double m[9], int x1, int y1, int x2, int y2)
{
    const QTransform q = mkQT(m);
    const PkTransform p = mkPT(m);
    const QRect qr = mkQ(x1, y1, x2, y2);
    const PkRect pr = mkP(x1, y1, x2, y2);
    const std::string in = tfin(m) + "Ri" + rin(x1, y1, x2, y2);
    // 整数矩形提升成 QRectF 时用的是 x/y/w/h（left=x, right=x+w），
    // 与 needsPerspectiveClipping 那一侧口径一致。
    const bool clip = tfFreshIsProject(m)
        && tfNeedsClip(m, qr.x(), (double)qr.x() + qr.width(),
                       qr.y(), (double)qr.y() + qr.height());
    // ⚠ 矩形那四个分量也要参与，理由同浮点版。
    const std::string base = shapeOfTf(m) + "/" + shapeOfRect(x1, y1, x2, y2);
    const std::string sh = clip ? ("persp-clip/" + base) : base;

    rec("T::mapRect(PkRect)", same_rect(q.mapRect(qr), p.mapRect(pr)), sh, in,
        qstr(q.mapRect(qr)), qstr(p.mapRect(pr)));
}

// 二元：乘法与相等。
static void cmp_tf_binary(const double a[9], const double b[9])
{
    const std::string in = tfin(a) + "x" + tfin(b);
    // ⚠ **十八个分量全部参与**形态判定（约定见 shapeOfD 上方那段）——
    // 只取第一个矩阵会把"缺陷只在第二个矩阵的某个分量上触发"贴成相反的标签。
    const std::string sh = shapeOfTf(a) + "x" + shapeOfTf(b);

    {
        const QTransform q = mkQT(a) * mkQT(b);
        const PkTransform p = mkPT(a) * mkPT(b);
        rec("T::operator*", same_tf(q, p) && (int)q.type() == (int)p.type(), sh, in,
            qstr(q) + "|" + istr((int)q.type()), qstr(p) + "|" + istr((int)p.type()));
    }
    {
        QTransform q = mkQT(a); PkTransform p = mkPT(a);
        q *= mkQT(b); p *= mkPT(b);
        rec("T::operator*=", same_tf(q, p) && (int)q.type() == (int)p.type(), sh, in,
            qstr(q) + "|" + istr((int)q.type()), qstr(p) + "|" + istr((int)p.type()));
    }
    rec("T::operator==", (mkQT(a) == mkQT(b)) == (mkPT(a) == mkPT(b)), sh, in,
        bstr(mkQT(a) == mkQT(b)), bstr(mkPT(a) == mkPT(b)));
    rec("T::operator!=", (mkQT(a) != mkQT(b)) == (mkPT(a) != mkPT(b)), sh, in,
        bstr(mkQT(a) != mkQT(b)), bstr(mkPT(a) != mkPT(b)));
    // 自由函数 qFuzzyCompare(T,T)：不在类体里，规则三的闸门看不见它，
    // 但它是 operator== 的模糊对照物，坏了会静默 —— 给一条自己的 rec。
    rec("T::freeFuzzyCompare",
        qFuzzyCompare(mkQT(a), mkQT(b)) == qFuzzyCompare(mkPT(a), mkPT(b)), sh, in,
        bstr(qFuzzyCompare(mkQT(a), mkQT(b))), bstr(qFuzzyCompare(mkPT(a), mkPT(b))));
}

// 两个静态工厂：**它们不走构造那条重算路径**，直接钉档位，所以连 type() 一起比。
static void cmp_tf_static(double a, double b)
{
    const std::string in = "(" + dstr(a) + "," + dstr(b) + ")";
    const std::string sh = shapeOfD({a, b});
    {
        const QTransform q = QTransform::fromTranslate(a, b);
        const PkTransform p = PkTransform::fromTranslate(a, b);
        rec("T::fromTranslate", same_tf(q, p) && (int)q.type() == (int)p.type(), sh, in,
            qstr(q) + "|" + istr((int)q.type()), qstr(p) + "|" + istr((int)p.type()));
    }
    {
        const QTransform q = QTransform::fromScale(a, b);
        const PkTransform p = PkTransform::fromScale(a, b);
        rec("T::fromScale", same_tf(q, p) && (int)q.type() == (int)p.type(), sh, in,
            qstr(q) + "|" + istr((int)q.type()), qstr(p) + "|" + istr((int)p.type()));
    }
}

// ═══ Line 族：比较原语 + tag 谓词 ═══════════════════════════════════════════
//
// **规模声明**（R-21 plan.md「问 3」）：Line/Margins 两族不复刻 R-03 的亿级
// 组合爆炸，改为"手挑对抗用例 + 有界组合"，每族目标 ≥ 10⁴ 次比对。
//
// tag 设计复用顶部已经定义的 shapeOfD（nonfinite/signed-zero/subnormal/
// zero/huge/finite），四个坐标一起喂；额外加一档 "degenerate-zero-length"
// ——length()/angle()/setLength()/unitVector() 等好几个 API 在"两端点重合"
// 这个输入形态上各自走特殊分支（unitVector 除以 0 得 NaN、setLength 是
// 空操作、angle 打出 -0），这一档必须单独可见，不能被 shapeOfD 的
// "zero"/"finite" 两档吞掉（那两档只看单个分量是不是 0，看不出"两点重合"
// 这个组合形态）。
static std::string shapeOfLine(double x1, double y1, double x2, double y2)
{
    std::string base = shapeOfD({x1, y1, x2, y2});
    if (base != "finite") return base;
    if (x1 == x2 && y1 == y2) return "degenerate-zero-length";
    return "finite";
}

static std::string qstr(const QLine &l)
{ return "[" + istr(l.x1()) + "," + istr(l.y1()) + "->" + istr(l.x2()) + "," + istr(l.y2()) + "]"; }
static std::string qstr(const PkLine &l)
{ return "[" + istr(l.p1().x()) + "," + istr(l.p1().y()) + "->"
             + istr(l.p2().x()) + "," + istr(l.p2().y()) + "]"; }
static bool same_line(const QLine &q, const PkLine &p)
{
    return q.x1() == p.p1().x() && q.y1() == p.p1().y()
        && q.x2() == p.p2().x() && q.y2() == p.p2().y();
}

static std::string qstr(const QLineF &l)
{ return "[" + dstr(l.x1()) + "," + dstr(l.y1()) + "->" + dstr(l.x2()) + "," + dstr(l.y2()) + "]"; }
static std::string qstr(const PkLineF &l)
{ return "[" + dstr(l.x1()) + "," + dstr(l.y1()) + "->" + dstr(l.x2()) + "," + dstr(l.y2()) + "]"; }
static bool same_linef(const QLineF &q, const PkLineF &p)
{
    return same_double(q.x1(), p.x1()) && same_double(q.y1(), p.y1())
        && same_double(q.x2(), p.x2()) && same_double(q.y2(), p.y2());
}

// ═══ Line 族：逐 API 对拍 ═══════════════════════════════════════════════════
//
// **PkLine 只有 5 条声明**（默认构造 / (int,int,int,int) / (PkPoint,PkPoint) /
// p1() / p2()，见 PkLine.h 头部注释——保留范围内 QLine 唯一的真实调用点只是
// "构造后立即隐式转 QLineF"）。cmp_line_ctors 一次性覆盖这 5 条。
static void cmp_line_ctors(int x1, int y1, int x2, int y2)
{
    const QLine  q(x1, y1, x2, y2);
    const PkLine p(x1, y1, x2, y2);
    const std::string in = istr(x1) + "," + istr(y1) + "->" + istr(x2) + "," + istr(y2);
    const std::string sh = shapeOfI({x1, y1, x2, y2});

    rec("L::ctorInts", same_line(q, p), sh, in, qstr(q), qstr(p));

    const QLine  q2(QPoint(x1, y1), QPoint(x2, y2));
    const PkLine p2(PkPoint(x1, y1), PkPoint(x2, y2));
    rec("L::ctorPoints", same_line(q2, p2), sh, in, qstr(q2), qstr(p2));

    rec("L::p1", q.p1() == QPoint(x1, y1) && p.p1() == PkPoint(x1, y1), sh, in,
        qstr(QLine(q.p1(), q.p1())), qstr(PkLine(p.p1(), p.p1())));
    rec("L::p2", q.p2() == QPoint(x2, y2) && p.p2() == PkPoint(x2, y2), sh, in,
        qstr(QLine(q.p2(), q.p2())), qstr(PkLine(p.p2(), p.p2())));
}

// 无输入的 API（默认构造）：只有一种输入形态，只跑一次（与 Point/Size 族
// 的 cmp_*_constants 同一个约定，见规则一注释里"边界情形"那句）。
static void cmp_line_constants()
{
    rec("L::defaultCtor", same_line(QLine(), PkLine()), "no-input",
        "QLine()", qstr(QLine()), qstr(PkLine()));
    rec("LF::defaultCtor", same_linef(QLineF(), PkLineF()), "no-input",
        "QLineF()", qstr(QLineF()), qstr(PkLineF()));
}

// 真实调用点唯一的形态：`QLineF l = QLine(...)`，构造后立即隐式转。
static void cmp_line_promotion(int x1, int y1, int x2, int y2)
{
    const QLineF  q = QLine(x1, y1, x2, y2);
    const PkLineF p = PkLine(x1, y1, x2, y2);
    const std::string in = istr(x1) + "," + istr(y1) + "->" + istr(x2) + "," + istr(y2);
    rec("LF::ctorFromLine", same_linef(q, p), shapeOfI({x1, y1, x2, y2}), in, qstr(q), qstr(p));
}

static void cmp_linef_ctors(double x1, double y1, double x2, double y2)
{
    const QLineF  q(x1, y1, x2, y2);
    const PkLineF p(x1, y1, x2, y2);
    const std::string in = dstr(x1) + "," + dstr(y1) + "->" + dstr(x2) + "," + dstr(y2);
    const std::string sh = shapeOfLine(x1, y1, x2, y2);
    rec("LF::ctorReals", same_linef(q, p), sh, in, qstr(q), qstr(p));

    const QLineF  q2(QPointF(x1, y1), QPointF(x2, y2));
    const PkLineF p2(PkPointF(x1, y1), PkPointF(x2, y2));
    rec("LF::ctorPoints", same_linef(q2, p2), sh, in, qstr(q2), qstr(p2));
}

// 一元 API 的大头：isNull / p1 / p2 / x1 / y1 / x2 / y2 / dx / dy / length /
// angle / unitVector / normalVector / center / setP1 / setP2。
static void cmp_linef_unary(double x1, double y1, double x2, double y2)
{
    const QLineF  q(x1, y1, x2, y2);
    const PkLineF p(x1, y1, x2, y2);
    const std::string in = dstr(x1) + "," + dstr(y1) + "->" + dstr(x2) + "," + dstr(y2);
    const std::string sh = shapeOfLine(x1, y1, x2, y2);

    rec("LF::isNull", q.isNull() == p.isNull(), sh, in, bstr(q.isNull()), bstr(p.isNull()));
    rec("LF::p1", same_ptf(q.p1(), p.p1()), sh, in, qstr(q.p1()), qstr(p.p1()));
    rec("LF::p2", same_ptf(q.p2(), p.p2()), sh, in, qstr(q.p2()), qstr(p.p2()));
    rec("LF::x1", same_double(q.x1(), p.x1()), sh, in, dstr(q.x1()), dstr(p.x1()));
    rec("LF::y1", same_double(q.y1(), p.y1()), sh, in, dstr(q.y1()), dstr(p.y1()));
    rec("LF::x2", same_double(q.x2(), p.x2()), sh, in, dstr(q.x2()), dstr(p.x2()));
    rec("LF::y2", same_double(q.y2(), p.y2()), sh, in, dstr(q.y2()), dstr(p.y2()));
    rec("LF::dx", same_double(q.dx(), p.dx()), sh, in, dstr(q.dx()), dstr(p.dx()));
    rec("LF::dy", same_double(q.dy(), p.dy()), sh, in, dstr(q.dy()), dstr(p.dy()));
    rec("LF::length", same_double(q.length(), p.length()), sh, in, dstr(q.length()), dstr(p.length()));
    rec("LF::angle", same_double(q.angle(), p.angle()), sh, in, dstr(q.angle()), dstr(p.angle()));
    rec("LF::unitVector", same_linef(q.unitVector(), p.unitVector()), sh, in,
        qstr(q.unitVector()), qstr(p.unitVector()));
    rec("LF::normalVector", same_linef(q.normalVector(), p.normalVector()), sh, in,
        qstr(q.normalVector()), qstr(p.normalVector()));
    rec("LF::center", same_ptf(q.center(), p.center()), sh, in, qstr(q.center()), qstr(p.center()));

    { QLineF q2 = q; PkLineF p2 = p;
      q2.setP1(QPointF(y1, x1)); p2.setP1(PkPointF(y1, x1));
      rec("LF::setP1", same_linef(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QLineF q2 = q; PkLineF p2 = p;
      q2.setP2(QPointF(y2, x2)); p2.setP2(PkPointF(y2, x2));
      rec("LF::setP2", same_linef(q2, p2), sh, in, qstr(q2), qstr(p2)); }
}

static void cmp_linef_translate(double x1, double y1, double x2, double y2, double tx, double ty)
{
    const QLineF  q(x1, y1, x2, y2);
    const PkLineF p(x1, y1, x2, y2);
    const std::string in = dstr(x1) + "," + dstr(y1) + "->" + dstr(x2) + "," + dstr(y2)
                          + " +(" + dstr(tx) + "," + dstr(ty) + ")";
    const std::string sh = shapeOfD({x1, y1, x2, y2, tx, ty});

    { QLineF q2 = q; PkLineF p2 = p; q2.translate(tx, ty); p2.translate(tx, ty);
      rec("LF::translateXY", same_linef(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QLineF q2 = q; PkLineF p2 = p;
      q2.translate(QPointF(tx, ty)); p2.translate(PkPointF(tx, ty));
      rec("LF::translatePoint", same_linef(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    rec("LF::translatedXY", same_linef(q.translated(tx, ty), p.translated(tx, ty)), sh, in,
        qstr(q.translated(tx, ty)), qstr(p.translated(tx, ty)));
    rec("LF::translatedPoint",
        same_linef(q.translated(QPointF(tx, ty)), p.translated(PkPointF(tx, ty))), sh, in,
        qstr(q.translated(QPointF(tx, ty))), qstr(p.translated(PkPointF(tx, ty))));
}

// pointAt：t 不夹持，专门喂 t<0 / t>1 的外插形态（tag 里带 t 的"越界"信息）。
static void cmp_linef_pointat(double x1, double y1, double x2, double y2, double t)
{
    const QLineF  q(x1, y1, x2, y2);
    const PkLineF p(x1, y1, x2, y2);
    const std::string in = dstr(x1) + "," + dstr(y1) + "->" + dstr(x2) + "," + dstr(y2)
                          + " t=" + dstr(t);
    std::string sh = shapeOfD({x1, y1, x2, y2, t});
    if (sh == "finite") {
        sh = (t < 0.0) ? "t-below-0" : (t > 1.0) ? "t-above-1" : "t-in-range";
    }
    rec("LF::pointAt", same_ptf(q.pointAt(t), p.pointAt(t)), sh, in,
        qstr(q.pointAt(t)), qstr(p.pointAt(t)));
}

static void cmp_linef_setlength(double x1, double y1, double x2, double y2, double len)
{
    QLineF q(x1, y1, x2, y2); PkLineF p(x1, y1, x2, y2);
    const std::string in = dstr(x1) + "," + dstr(y1) + "->" + dstr(x2) + "," + dstr(y2)
                          + " len=" + dstr(len);
    std::string sh = shapeOfLine(x1, y1, x2, y2);
    if (sh == "finite" && !nonFinite(len)) sh = "finite";
    q.setLength(len); p.setLength(len);
    rec("LF::setLength", same_linef(q, p), sh, in, qstr(q), qstr(p));
}

static void cmp_linef_setangle(double x1, double y1, double x2, double y2, double ang)
{
    QLineF q(x1, y1, x2, y2); PkLineF p(x1, y1, x2, y2);
    const std::string in = dstr(x1) + "," + dstr(y1) + "->" + dstr(x2) + "," + dstr(y2)
                          + " ang=" + dstr(ang);
    const std::string sh = shapeOfLine(x1, y1, x2, y2);
    q.setAngle(ang); p.setAngle(ang);
    rec("LF::setAngle", same_linef(q, p), sh, in, qstr(q), qstr(p));
}

static void cmp_linef_angleto(double ax1, double ay1, double ax2, double ay2,
                              double bx1, double by1, double bx2, double by2)
{
    const QLineF qa(ax1, ay1, ax2, ay2), qb(bx1, by1, bx2, by2);
    const PkLineF pa(ax1, ay1, ax2, ay2), pb(bx1, by1, bx2, by2);
    const std::string in = dstr(ax1) + "," + dstr(ay1) + "->" + dstr(ax2) + "," + dstr(ay2)
                          + " | " + dstr(bx1) + "," + dstr(by1) + "->" + dstr(bx2) + "," + dstr(by2);
    std::string sh = shapeOfLine(ax1, ay1, ax2, ay2);
    const std::string shB = shapeOfLine(bx1, by1, bx2, by2);
    if (shB == "degenerate-zero-length" && sh != "degenerate-zero-length") sh = shB;
    rec("LF::angleTo", same_double(qa.angleTo(qb), pa.angleTo(pb)), sh, in,
        dstr(qa.angleTo(qb)), dstr(pa.angleTo(pb)));
}

// intersects：一个声明、三个标签（type 判定 / 交点坐标 / nullptr 实参）。
// **NoIntersection 分支下不比较交点**：真 Qt 在 denominator==0 时提前返回，
// 根本不写 *intersectionPoint（out 参数留着调用方传进来的原值），比它是比
// 未定义的东西。
static void cmp_linef_intersects(double ax1, double ay1, double ax2, double ay2,
                                 double bx1, double by1, double bx2, double by2)
{
    const QLineF qa(ax1, ay1, ax2, ay2), qb(bx1, by1, bx2, by2);
    const PkLineF pa(ax1, ay1, ax2, ay2), pb(bx1, by1, bx2, by2);
    const std::string in = dstr(ax1) + "," + dstr(ay1) + "->" + dstr(ax2) + "," + dstr(ay2)
                          + " x " + dstr(bx1) + "," + dstr(by1) + "->" + dstr(bx2) + "," + dstr(by2);
    const std::string sh = shapeOfD({ax1, ay1, ax2, ay2, bx1, by1, bx2, by2});

    QPointF qip(-999, -999);
    PkPointF pip(-999, -999);
    const int qt = (int)qa.intersects(qb, &qip);
    const int pt = (int)pa.intersects(pb, &pip);
    rec("LF::intersects", qt == pt, sh, in, istr(qt), istr(pt));
    if (qt != 0 && pt != 0) {
        rec("LF::intersectsPoint", same_ptf(qip, pip),
            sh + (qt == 1 ? "/bounded" : "/unbounded"), in, qstr(qip), qstr(pip));
    }
}

// nullptr 实参：只需要证明"不传交点也能正常判定 type"，手挑几组固定输入，
// 不需要跟主对拍一样做组合爆炸——这条本身不是行为分叉点，是接口形状点。
static void cmp_linef_intersects_nullarg()
{
    struct { double a[4], b[4]; } kCases[] = {
        {{0, 0, 10, 10}, {0, 10, 10, 0}},   // bounded
        {{0, 0, 1, 0}, {5, 5, 10, 10}},     // unbounded
        {{0, 0, 1, 0}, {5, 5, 6, 5}},       // none（平行）
    };
    for (const auto &c : kCases) {
        const QLineF qa(c.a[0], c.a[1], c.a[2], c.a[3]), qb(c.b[0], c.b[1], c.b[2], c.b[3]);
        const PkLineF pa(c.a[0], c.a[1], c.a[2], c.a[3]), pb(c.b[0], c.b[1], c.b[2], c.b[3]);
        const int qt = (int)qa.intersects(qb, nullptr);
        const int pt = (int)pa.intersects(pb, nullptr);
        rec("LF::intersectsNullArg", qt == pt, "no-input", qstr(qa) + " x " + qstr(qb),
            istr(qt), istr(pt));
    }
}

static void cmp_linef_frompolar(double len, double ang)
{
    const QLineF  q = QLineF::fromPolar(len, ang);
    const PkLineF p = PkLineF::fromPolar(len, ang);
    const std::string in = "len=" + dstr(len) + " ang=" + dstr(ang);
    rec("LF::fromPolar", same_linef(q, p), shapeOfD({len, ang}), in, qstr(q), qstr(p));
}

static void cmp_linef_binary(double ax1, double ay1, double ax2, double ay2,
                             double bx1, double by1, double bx2, double by2)
{
    const QLineF qa(ax1, ay1, ax2, ay2), qb(bx1, by1, bx2, by2);
    const PkLineF pa(ax1, ay1, ax2, ay2), pb(bx1, by1, bx2, by2);
    const std::string in = dstr(ax1) + "," + dstr(ay1) + "->" + dstr(ax2) + "," + dstr(ay2)
                          + " | " + dstr(bx1) + "," + dstr(by1) + "->" + dstr(bx2) + "," + dstr(by2);
    const std::string sh = shapeOfD({ax1, ay1, ax2, ay2, bx1, by1, bx2, by2});
    rec("LF::operatorEq", (qa == qb) == (pa == pb), sh, in, bstr(qa == qb), bstr(pa == pb));
    rec("LF::operatorNe", (qa != qb) == (pa != pb), sh, in, bstr(qa != qb), bstr(pa != pb));
}

// ═══ Margins 族：比较原语 ═══════════════════════════════════════════════════

static std::string qstr(const QMargins &m)
{ return "(" + istr(m.left()) + "," + istr(m.top()) + "," + istr(m.right()) + "," + istr(m.bottom()) + ")"; }
static std::string qstr(const PkMargins &m)
{ return "(" + istr(m.left()) + "," + istr(m.top()) + "," + istr(m.right()) + "," + istr(m.bottom()) + ")"; }
static bool same_margins(const QMargins &q, const PkMargins &p)
{
    return q.left() == p.left() && q.top() == p.top()
        && q.right() == p.right() && q.bottom() == p.bottom();
}

static std::string qstr(const QMarginsF &m)
{ return "(" + dstr(m.left()) + "," + dstr(m.top()) + "," + dstr(m.right()) + "," + dstr(m.bottom()) + ")"; }
static std::string qstr(const PkMarginsF &m)
{ return "(" + dstr(m.left()) + "," + dstr(m.top()) + "," + dstr(m.right()) + "," + dstr(m.bottom()) + ")"; }
static bool same_marginsf(const QMarginsF &q, const PkMarginsF &p)
{
    return same_double(q.left(), p.left()) && same_double(q.top(), p.top())
        && same_double(q.right(), p.right()) && same_double(q.bottom(), p.bottom());
}

// ═══ Margins 族：逐 API 对拍 ═════════════════════════════════════════════════
//
// **实测调用点为 0**（QMargins 全仓保留范围内唯一命中是本目录自己的注释，
// 原消费方 `libkdcraw/rnuminput.cpp` 已被 D-02-a 删除）。任务定义要求仍然
// 实现——它挡着 PkRect/PkRectF 的四个互操作成员，登记见 PkMargins.h 头部。
// 对拍照样跑满，理由与 PkRect 的构造函数/运算符按 Qt 头文件全集实现同一条：
// 没有实测输入可选，就把 qmargins.h 的取值域（含极值、0、负数）都覆盖到。

static void cmp_margins_ctor(int l, int t, int r, int b)
{
    const QMargins  q(l, t, r, b);
    const PkMargins p(l, t, r, b);
    const std::string in = istr(l) + "," + istr(t) + "," + istr(r) + "," + istr(b);
    const std::string sh = shapeOfI({l, t, r, b});

    rec("M::ctorLTRB", same_margins(q, p), sh, in, qstr(q), qstr(p));
    rec("M::isNull", q.isNull() == p.isNull(), sh, in, bstr(q.isNull()), bstr(p.isNull()));
    rec("M::left", q.left() == p.left(), sh, in, istr(q.left()), istr(p.left()));
    rec("M::top", q.top() == p.top(), sh, in, istr(q.top()), istr(p.top()));
    rec("M::right", q.right() == p.right(), sh, in, istr(q.right()), istr(p.right()));
    rec("M::bottom", q.bottom() == p.bottom(), sh, in, istr(q.bottom()), istr(p.bottom()));

    { QMargins q2 = q; PkMargins p2 = p; q2.setLeft(t); p2.setLeft(t);
      rec("M::setLeft", same_margins(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QMargins q2 = q; PkMargins p2 = p; q2.setTop(r); p2.setTop(r);
      rec("M::setTop", same_margins(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QMargins q2 = q; PkMargins p2 = p; q2.setRight(b); p2.setRight(b);
      rec("M::setRight", same_margins(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QMargins q2 = q; PkMargins p2 = p; q2.setBottom(l); p2.setBottom(l);
      rec("M::setBottom", same_margins(q2, p2), sh, in, qstr(q2), qstr(p2)); }

    rec("M::operatorPlus", same_margins(+q, +p), sh, in, qstr(+q), qstr(+p));
    rec("M::operatorUnaryMinus", same_margins(-q, -p), sh, in, qstr(-q), qstr(-p));
}

static void cmp_margins_constants()
{
    rec("M::defaultCtor", same_margins(QMargins(), PkMargins()), "no-input",
        "QMargins()", qstr(QMargins()), qstr(PkMargins()));
    rec("MF::defaultCtor", same_marginsf(QMarginsF(), PkMarginsF()), "no-input",
        "QMarginsF()", qstr(QMarginsF()), qstr(PkMarginsF()));
}

static void cmp_margins_binary(int al, int at, int ar, int ab, int bl, int bt, int br, int bb)
{
    const QMargins  qa(al, at, ar, ab), qb(bl, bt, br, bb);
    const PkMargins pa(al, at, ar, ab), pb(bl, bt, br, bb);
    const std::string in = istr(al) + "," + istr(at) + "," + istr(ar) + "," + istr(ab)
                          + " | " + istr(bl) + "," + istr(bt) + "," + istr(br) + "," + istr(bb);
    const std::string sh0 = shapeOfI({al, at, ar, ab, bl, bt, br, bb});
    const bool addOv = addOverflows(al, bl) || addOverflows(at, bt)
                     || addOverflows(ar, br) || addOverflows(ab, bb);
    const bool subOv = subOverflows(al, bl) || subOverflows(at, bt)
                     || subOverflows(ar, br) || subOverflows(ab, bb);

    rec("M::operatorPlusMargins", same_margins(qa + qb, pa + pb),
        addOv ? std::string("int-overflow") : sh0, in, qstr(qa + qb), qstr(pa + pb));
    rec("M::operatorMinusMargins", same_margins(qa - qb, pa - pb),
        subOv ? std::string("int-overflow") : sh0, in, qstr(qa - qb), qstr(pa - pb));
    rec("M::operatorEq", (qa == qb) == (pa == pb), sh0, in, bstr(qa == qb), bstr(pa == pb));
    rec("M::operatorNe", (qa != qb) == (pa != pb), sh0, in, bstr(qa != qb), bstr(pa != pb));

    { QMargins q2 = qa; PkMargins p2 = pa; q2 += qb; p2 += pb;
      rec("M::operatorPlusEqMargins", same_margins(q2, p2),
          addOv ? std::string("int-overflow") : sh0, in, qstr(q2), qstr(p2)); }
    { QMargins q2 = qa; PkMargins p2 = pa; q2 -= qb; p2 -= pb;
      rec("M::operatorMinusEqMargins", same_margins(q2, p2),
          subOv ? std::string("int-overflow") : sh0, in, qstr(q2), qstr(p2)); }
}

static void cmp_margins_scale(int l, int t, int r, int b, int factorI, double factorD)
{
    const QMargins  q(l, t, r, b);
    const PkMargins p(l, t, r, b);
    const std::string in = istr(l) + "," + istr(t) + "," + istr(r) + "," + istr(b)
                          + " *i=" + istr(factorI) + " *d=" + dstr(factorD);
    const std::string shI = shapeOfI({l, t, r, b, factorI});

    rec("M::operatorPlusInt", same_margins(q + factorI, p + factorI), shI, in,
        qstr(q + factorI), qstr(p + factorI));
    rec("M::operatorPlusIntRev", same_margins(factorI + q, factorI + p), shI, in,
        qstr(factorI + q), qstr(factorI + p));
    rec("M::operatorMinusInt", same_margins(q - factorI, p - factorI), shI, in,
        qstr(q - factorI), qstr(p - factorI));
    rec("M::operatorMulInt", same_margins(q * factorI, p * factorI), shI, in,
        qstr(q * factorI), qstr(p * factorI));
    rec("M::operatorMulIntRev", same_margins(factorI * q, factorI * p), shI, in,
        qstr(factorI * q), qstr(factorI * p));
    { QMargins q2 = q; PkMargins p2 = p; q2 += factorI; p2 += factorI;
      rec("M::operatorPlusEqInt", same_margins(q2, p2), shI, in, qstr(q2), qstr(p2)); }
    { QMargins q2 = q; PkMargins p2 = p; q2 -= factorI; p2 -= factorI;
      rec("M::operatorMinusEqInt", same_margins(q2, p2), shI, in, qstr(q2), qstr(p2)); }
    { QMargins q2 = q; PkMargins p2 = p; q2 *= factorI; p2 *= factorI;
      rec("M::operatorMulEqInt", same_margins(q2, p2), shI, in, qstr(q2), qstr(p2)); }
    // `factorI == -1` 与任一分量 `INT_MIN` 同时出现时，`INT_MIN / -1` 是
    // x86 `idiv` 的硬件级溢出——两侧（真 Qt 与 PkMargins）都是裸 `int`
    // 除法，会**同样**触发 SIGFPE 把整个对拍进程打死，不是"两侧算出不同
    // 结果"那种能比较的分歧。跳过这一组合，与 add/sub/mul 溢出（那些不
    // 会硬件陷入，只是 UB 静默回绕，仍然照常比较、按 shI 打 tag）不是
    // 同一类问题，不能套用同一处置。
    const bool divTrap = (factorI == -1)
        && (l == INT_MIN || t == INT_MIN || r == INT_MIN || b == INT_MIN);
    if (factorI != 0 && !divTrap) {
        rec("M::operatorDivInt", same_margins(q / factorI, p / factorI), shI, in,
            qstr(q / factorI), qstr(p / factorI));
        QMargins q2 = q; PkMargins p2 = p; q2 /= factorI; p2 /= factorI;
        rec("M::operatorDivEqInt", same_margins(q2, p2), shI, in, qstr(q2), qstr(p2));
    }

    // qreal 标量：结果按 qRound 取整，tag 里带上"半值取整方向"这个根因。
    std::string shD = shapeOfD({(double)l, (double)t, (double)r, (double)b, factorD});
    rec("M::operatorMulQreal", same_margins(q * factorD, p * factorD), shD, in,
        qstr(q * factorD), qstr(p * factorD));
    rec("M::operatorMulQrealRev", same_margins(factorD * q, factorD * p), shD, in,
        qstr(factorD * q), qstr(factorD * p));
    { QMargins q2 = q; PkMargins p2 = p; q2 *= factorD; p2 *= factorD;
      rec("M::operatorMulEqQreal", same_margins(q2, p2), shD, in, qstr(q2), qstr(p2)); }
    if (factorD != 0.0 && !nonFinite(factorD)) {
        rec("M::operatorDivQreal", same_margins(q / factorD, p / factorD), shD, in,
            qstr(q / factorD), qstr(p / factorD));
        QMargins q2 = q; PkMargins p2 = p; q2 /= factorD; p2 /= factorD;
        rec("M::operatorDivEqQreal", same_margins(q2, p2), shD, in, qstr(q2), qstr(p2));
    }
}

static void cmp_marginsf_ctor(double l, double t, double r, double b)
{
    const QMarginsF  q(l, t, r, b);
    const PkMarginsF p(l, t, r, b);
    const std::string in = dstr(l) + "," + dstr(t) + "," + dstr(r) + "," + dstr(b);
    const std::string sh = shapeOfD({l, t, r, b});

    rec("MF::ctorLTRB", same_marginsf(q, p), sh, in, qstr(q), qstr(p));
    rec("MF::isNull", q.isNull() == p.isNull(), sh, in, bstr(q.isNull()), bstr(p.isNull()));
    rec("MF::left", same_double(q.left(), p.left()), sh, in, dstr(q.left()), dstr(p.left()));
    rec("MF::top", same_double(q.top(), p.top()), sh, in, dstr(q.top()), dstr(p.top()));
    rec("MF::right", same_double(q.right(), p.right()), sh, in, dstr(q.right()), dstr(p.right()));
    rec("MF::bottom", same_double(q.bottom(), p.bottom()), sh, in, dstr(q.bottom()), dstr(p.bottom()));

    { QMarginsF q2 = q; PkMarginsF p2 = p; q2.setLeft(t); p2.setLeft(t);
      rec("MF::setLeft", same_marginsf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QMarginsF q2 = q; PkMarginsF p2 = p; q2.setTop(r); p2.setTop(r);
      rec("MF::setTop", same_marginsf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QMarginsF q2 = q; PkMarginsF p2 = p; q2.setRight(b); p2.setRight(b);
      rec("MF::setRight", same_marginsf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QMarginsF q2 = q; PkMarginsF p2 = p; q2.setBottom(l); p2.setBottom(l);
      rec("MF::setBottom", same_marginsf(q2, p2), sh, in, qstr(q2), qstr(p2)); }

    rec("MF::toMargins", same_margins(q.toMargins(), p.toMargins()),
        (outOfIntRange(l) || outOfIntRange(t) || outOfIntRange(r) || outOfIntRange(b))
            ? std::string("out-of-int-range") : sh,
        in, qstr(q.toMargins()), qstr(p.toMargins()));

    rec("MF::operatorPlus", same_marginsf(+q, +p), sh, in, qstr(+q), qstr(+p));
    rec("MF::operatorUnaryMinus", same_marginsf(-q, -p), sh, in, qstr(-q), qstr(-p));

    // PkMargins → PkMarginsF 隐式提升（真实互操作靠它：PkRectF::marginsAdded
    // 吃 PkMarginsF，调用点传 PkMargins 全靠这条编过）。
    // ⚠ l 可能是 NaN/inf（本函数其余断言都要吃这批输入），llround 对它们是
    // UB，先夹到有限范围再转 int。
    const double lc = nonFinite(l) ? 0.0 : std::max(-1e9, std::min(1e9, l));
    const int il = (int)std::llround(lc);
    const QMarginsF  qf = QMargins(il, il, il, il);
    const PkMarginsF pf = PkMargins(il, il, il, il);
    rec("MF::ctorFromMargins", same_marginsf(qf, pf), shapeOfI({il}), istr(il), qstr(qf), qstr(pf));
}

static void cmp_marginsf_binary(double al, double at, double ar, double ab,
                                double bl, double bt, double br, double bb)
{
    const QMarginsF  qa(al, at, ar, ab), qb(bl, bt, br, bb);
    const PkMarginsF pa(al, at, ar, ab), pb(bl, bt, br, bb);
    const std::string in = dstr(al) + "," + dstr(at) + "," + dstr(ar) + "," + dstr(ab)
                          + " | " + dstr(bl) + "," + dstr(bt) + "," + dstr(br) + "," + dstr(bb);
    const std::string sh = shapeOfD({al, at, ar, ab, bl, bt, br, bb});

    rec("MF::operatorPlusMargins", same_marginsf(qa + qb, pa + pb), sh, in,
        qstr(qa + qb), qstr(pa + pb));
    rec("MF::operatorMinusMargins", same_marginsf(qa - qb, pa - pb), sh, in,
        qstr(qa - qb), qstr(pa - pb));
    rec("MF::operatorEq", (qa == qb) == (pa == pb), sh, in, bstr(qa == qb), bstr(pa == pb));
    rec("MF::operatorNe", (qa != qb) == (pa != pb), sh, in, bstr(qa != qb), bstr(pa != pb));

    { QMarginsF q2 = qa; PkMarginsF p2 = pa; q2 += qb; p2 += pb;
      rec("MF::operatorPlusEqMargins", same_marginsf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QMarginsF q2 = qa; PkMarginsF p2 = pa; q2 -= qb; p2 -= pb;
      rec("MF::operatorMinusEqMargins", same_marginsf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
}

static void cmp_marginsf_scale(double l, double t, double r, double b, double factor)
{
    const QMarginsF  q(l, t, r, b);
    const PkMarginsF p(l, t, r, b);
    const std::string in = dstr(l) + "," + dstr(t) + "," + dstr(r) + "," + dstr(b)
                          + " *=" + dstr(factor);
    const std::string sh = shapeOfD({l, t, r, b, factor});

    rec("MF::operatorPlusQreal", same_marginsf(q + factor, p + factor), sh, in,
        qstr(q + factor), qstr(p + factor));
    rec("MF::operatorPlusQrealRev", same_marginsf(factor + q, factor + p), sh, in,
        qstr(factor + q), qstr(factor + p));
    rec("MF::operatorMinusQreal", same_marginsf(q - factor, p - factor), sh, in,
        qstr(q - factor), qstr(p - factor));
    rec("MF::operatorMulQreal", same_marginsf(q * factor, p * factor), sh, in,
        qstr(q * factor), qstr(p * factor));
    rec("MF::operatorMulQrealRev", same_marginsf(factor * q, factor * p), sh, in,
        qstr(factor * q), qstr(factor * p));

    { QMarginsF q2 = q; PkMarginsF p2 = p; q2 += factor; p2 += factor;
      rec("MF::operatorPlusEqQreal", same_marginsf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QMarginsF q2 = q; PkMarginsF p2 = p; q2 -= factor; p2 -= factor;
      rec("MF::operatorMinusEqQreal", same_marginsf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QMarginsF q2 = q; PkMarginsF p2 = p; q2 *= factor; p2 *= factor;
      rec("MF::operatorMulEqQreal", same_marginsf(q2, p2), sh, in, qstr(q2), qstr(p2)); }

    if (factor != 0.0 && !nonFinite(factor)) {
        rec("MF::operatorDivQreal", same_marginsf(q / factor, p / factor), sh, in,
            qstr(q / factor), qstr(p / factor));
        QMarginsF q2 = q; PkMarginsF p2 = p; q2 /= factor; p2 /= factor;
        rec("MF::operatorDivEqQreal", same_marginsf(q2, p2), sh, in, qstr(q2), qstr(p2));
    }
}

// ═══ PkRect / PkRectF 的四个互操作成员（R-21 T1 解锁）════════════════════════

static void cmp_rect_margins(int x1, int y1, int x2, int y2, int ml, int mt, int mr, int mb)
{
    QRect qr; qr.setCoords(x1, y1, x2, y2);
    PkRect pr; pr.setCoords(x1, y1, x2, y2);
    const QMargins  qm(ml, mt, mr, mb);
    const PkMargins pm(ml, mt, mr, mb);
    const std::string in = qstr(qr) + " +margins" + qstr(qm);
    const std::string sh = shapeOfI({x1, y1, x2, y2, ml, mt, mr, mb});

    rec("R::marginsAdded", same_rect(qr.marginsAdded(qm), pr.marginsAdded(pm)), sh, in,
        qstr(qr.marginsAdded(qm)), qstr(pr.marginsAdded(pm)));
    rec("R::marginsRemoved", same_rect(qr.marginsRemoved(qm), pr.marginsRemoved(pm)), sh, in,
        qstr(qr.marginsRemoved(qm)), qstr(pr.marginsRemoved(pm)));

    { QRect q2 = qr; PkRect p2 = pr; q2 += qm; p2 += pm;
      rec("R::operatorPlusEqMargins", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QRect q2 = qr; PkRect p2 = pr; q2 -= qm; p2 -= pm;
      rec("R::operatorMinusEqMargins", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }

    rec("R::operatorPlusMargins", same_rect(qr + qm, pr + pm), sh, in,
        qstr(qr + qm), qstr(pr + pm));
    rec("R::operatorPlusMarginsRev", same_rect(qm + qr, pm + pr), sh, in,
        qstr(qm + qr), qstr(pm + pr));
    rec("R::operatorMinusMargins", same_rect(qr - qm, pr - pm), sh, in,
        qstr(qr - qm), qstr(pr - pm));
}

static void cmp_rectf_margins(double x1, double y1, double x2, double y2,
                              double ml, double mt, double mr, double mb)
{
    const QRectF  qr(x1, y1, x2 - x1, y2 - y1);
    const PkRectF pr(x1, y1, x2 - x1, y2 - y1);
    const QMarginsF  qm(ml, mt, mr, mb);
    const PkMarginsF pm(ml, mt, mr, mb);
    const std::string in = qstr(qr) + " +margins" + qstr(qm);
    const std::string sh = shapeOfD({x1, y1, x2, y2, ml, mt, mr, mb});

    rec("RF::marginsAdded", same_rectf(qr.marginsAdded(qm), pr.marginsAdded(pm)), sh, in,
        qstr(qr.marginsAdded(qm)), qstr(pr.marginsAdded(pm)));
    rec("RF::marginsRemoved", same_rectf(qr.marginsRemoved(qm), pr.marginsRemoved(pm)), sh, in,
        qstr(qr.marginsRemoved(qm)), qstr(pr.marginsRemoved(pm)));

    { QRectF q2 = qr; PkRectF p2 = pr; q2 += qm; p2 += pm;
      rec("RF::operatorPlusEqMargins", same_rectf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QRectF q2 = qr; PkRectF p2 = pr; q2 -= qm; p2 -= pm;
      rec("RF::operatorMinusEqMargins", same_rectf(q2, p2), sh, in, qstr(q2), qstr(p2)); }

    rec("RF::operatorPlusMargins", same_rectf(qr + qm, pr + pm), sh, in,
        qstr(qr + qm), qstr(pr + pm));
    rec("RF::operatorPlusMarginsRev", same_rectf(qm + qr, pm + pr), sh, in,
        qstr(qm + qr), qstr(pm + pr));
    rec("RF::operatorMinusMargins", same_rectf(qr - qm, pr - pm), sh, in,
        qstr(qr - qm), qstr(pr - pm));
}

// ═══ Polygon 族：比较原语 + tag 谓词（R-21 T2）════════════════════════════
//
// **规模声明**（R-21 plan.md「问 3」，与 Line/Margins 两族同一条口径）：不复刻
// R-03 的亿级组合爆炸，改为"手挑对抗用例 + 有界组合"，目标 ≥ 10⁴ 次比对。
// PkPolygon（int，只做构造+迭代，见 PkPolygon.h 头部）与 PkPolygonF
// （containsPoint/boundingRect/translate/translated/isClosed/toPolygon/构造，
// 外加 PkTransform 新解开的 map(PkPolygonF)/squareToQuad/quadToSquare）
// 共用这一节。

static std::string qstr(const QPolygon &poly)
{
    std::string s = "{";
    for (int i = 0; i < poly.size(); ++i) { if (i) s += ";"; s += qstr(poly.at(i)); }
    return s + "}";
}
static std::string qstr(const PkPolygon &poly)
{
    std::string s = "{";
    for (int i = 0; i < poly.size(); ++i) { if (i) s += ";"; s += qstr(poly.at(i)); }
    return s + "}";
}
static bool same_polygon(const QPolygon &q, const PkPolygon &p)
{
    if (q.size() != p.size()) return false;
    for (int i = 0; i < q.size(); ++i) if (!same_pt(q.at(i), p.at(i))) return false;
    return true;
}

static std::string qstr(const QPolygonF &poly)
{
    std::string s = "{";
    for (int i = 0; i < poly.size(); ++i) { if (i) s += ";"; s += qstr(poly.at(i)); }
    return s + "}";
}
static std::string qstr(const PkPolygonF &poly)
{
    std::string s = "{";
    for (int i = 0; i < poly.size(); ++i) { if (i) s += ";"; s += qstr(poly.at(i)); }
    return s + "}";
}
static bool same_polygonf(const QPolygonF &q, const PkPolygonF &p)
{
    if (q.size() != p.size()) return false;
    for (int i = 0; i < q.size(); ++i) if (!same_ptf(q.at(i), p.at(i))) return false;
    return true;
}

// shapeOfD 的 vector 版：签名要求 std::initializer_list，而这里参与形态判定
// 的分量个数是运行期才知道的（多边形点数可变）。逻辑与 shapeOfD **逐字相同**
// （同一份优先级），只是换了容器类型——不能直接拿两个指针"拼"出
// initializer_list（那个构造函数是私有的，只有编译器能造），所以另开一份。
static std::string shapeOfVec(const std::vector<double> &vs)
{
    for (double v : vs) if (nonFinite(v)) return "nonfinite";
    for (double v : vs) if (signedZero(v)) return "signed-zero";
    for (double v : vs) if (subnormal(v)) return "subnormal";
    for (double v : vs) if (v == 0.0) return "zero";
    for (double v : vs) if (std::fabs(v) > 1e300) return "huge";
    return "finite";
}

// ═══ Polygon 族：逐 API 对拍 ═══════════════════════════════════════════════

// PkPolygon 只有 3 条声明：默认构造（无输入）+ 两个从 PkVector<PkPoint> 转的
// 构造（同一份点集分别走左值/右值路径）。
static void cmp_polygon_constants()
{
    rec("PG::defaultCtor", same_polygon(QPolygon(), PkPolygon()), "no-input",
        "QPolygon()", qstr(QPolygon()), qstr(PkPolygon()));
}

static void cmp_polygon_ctors(const int (*pts)[2], int n)
{
    QVector<QPoint> qv;
    PkVector<PkPoint> pv;
    std::string in = "n=" + istr(n);
    std::string sh = "ordinary";
    for (int i = 0; i < n; ++i) {
        qv << QPoint(pts[i][0], pts[i][1]);
        pv << PkPoint(pts[i][0], pts[i][1]);
        if (intExtremum(pts[i][0]) || intExtremum(pts[i][1])) sh = "int-extremum";
    }
    if (n == 0) sh = "empty";

    const QPolygon q(qv);
    const PkPolygon p(pv);
    rec("PG::ctorFromVector", same_polygon(q, p), sh, in, qstr(q), qstr(p));

    QVector<QPoint> qv2 = qv;
    PkVector<PkPoint> pv2 = pv;
    const QPolygon q2(std::move(qv2));
    const PkPolygon p2(std::move(pv2));
    rec("PG::ctorFromVectorMove", same_polygon(q2, p2), sh, in, qstr(q2), qstr(p2));
}

// PkPolygonF：defaultCtor / ctorSize —— 两条都是"无输入形态由参数摆出来"，
// 挂在同一个 no-input/ordinary tag 下即可（构造本身没有分支可言）。
static void cmp_polygonf_constants(int size)
{
    if (size == 0) {
        rec("PGF::defaultCtor", same_polygonf(QPolygonF(), PkPolygonF()), "no-input",
            "QPolygonF()", qstr(QPolygonF()), qstr(PkPolygonF()));
    }
    const QPolygonF q(size);
    const PkPolygonF p(size);
    rec("PGF::ctorSize", same_polygonf(q, p), size == 0 ? "zero" : "ordinary",
        "size=" + istr(size), qstr(q), qstr(p));
}

static void cmp_polygonf_ctors(const double (*pts)[2], int n)
{
    QVector<QPointF> qv;
    PkVector<PkPointF> pv;
    std::string in = "n=" + istr(n);
    std::vector<double> flatv;
    for (int i = 0; i < n; ++i) {
        qv << QPointF(pts[i][0], pts[i][1]);
        pv << PkPointF(pts[i][0], pts[i][1]);
        flatv.push_back(pts[i][0]);
        flatv.push_back(pts[i][1]);
    }
    std::string sh = n == 0 ? "empty" : shapeOfVec(flatv);

    const QPolygonF q(qv);
    const PkPolygonF p(pv);
    rec("PGF::ctorFromVector", same_polygonf(q, p), sh, in, qstr(q), qstr(p));

    QVector<QPointF> qv2 = qv;
    PkVector<PkPointF> pv2 = pv;
    const QPolygonF q2(std::move(qv2));
    const PkPolygonF p2(std::move(pv2));
    rec("PGF::ctorFromVectorMove", same_polygonf(q2, p2), sh, in, qstr(q2), qstr(p2));
}

static void cmp_polygonf_ctor_rect(double x, double y, double w, double h)
{
    const std::string in = dstr(x) + "," + dstr(y) + "," + dstr(w) + "," + dstr(h);
    const std::string sh = shapeOfD({x, y, w, h});
    const QPolygonF q((QRectF(x, y, w, h)));
    const PkPolygonF p((PkRectF(x, y, w, h)));
    rec("PGF::ctorFromRect", same_polygonf(q, p), sh, in, qstr(q), qstr(p));
}

static void cmp_polygonf_translate(const double (*pts)[2], int n, double tx, double ty)
{
    QVector<QPointF> qv; PkVector<PkPointF> pv;
    for (int i = 0; i < n; ++i) { qv << QPointF(pts[i][0], pts[i][1]); pv << PkPointF(pts[i][0], pts[i][1]); }
    const QPolygonF base(qv);
    const PkPolygonF pbase(pv);
    const std::string in = "n=" + istr(n) + " t=(" + dstr(tx) + "," + dstr(ty) + ")";
    const std::string sh = shapeOfD({tx, ty});

    { QPolygonF q = base; PkPolygonF p = pbase; q.translate(tx, ty); p.translate(tx, ty);
      rec("PGF::translateXY", same_polygonf(q, p), sh, in, qstr(q), qstr(p)); }
    { QPolygonF q = base; PkPolygonF p = pbase;
      q.translate(QPointF(tx, ty)); p.translate(PkPointF(tx, ty));
      rec("PGF::translatePoint", same_polygonf(q, p), sh, in, qstr(q), qstr(p)); }
    rec("PGF::translatedXY", same_polygonf(base.translated(tx, ty), pbase.translated(tx, ty)),
        sh, in, qstr(base.translated(tx, ty)), qstr(pbase.translated(tx, ty)));
    rec("PGF::translatedPoint",
        same_polygonf(base.translated(QPointF(tx, ty)), pbase.translated(PkPointF(tx, ty))),
        sh, in, qstr(base.translated(QPointF(tx, ty))), qstr(pbase.translated(PkPointF(tx, ty))));
}

// isClosed / boundingRect / toPolygon：一元 API，同一份点集三个都测。
static void cmp_polygonf_unary(const double (*pts)[2], int n)
{
    QVector<QPointF> qv; PkVector<PkPointF> pv;
    for (int i = 0; i < n; ++i) { qv << QPointF(pts[i][0], pts[i][1]); pv << PkPointF(pts[i][0], pts[i][1]); }
    const QPolygonF q(qv);
    const PkPolygonF p(pv);
    const std::string in = "n=" + istr(n);
    const std::string sh = n == 0 ? "empty" : "ordinary";

    rec("PGF::isClosed", q.isClosed() == p.isClosed(), sh, in,
        bstr(q.isClosed()), bstr(p.isClosed()));

    const QRectF qbr = q.boundingRect();
    const PkRectF pbr = p.boundingRect();
    rec("PGF::boundingRect", same_rectf(qbr, pbr), sh, in, qstr(qbr), qstr(pbr));

    const QPolygon qip = q.toPolygon();
    const PkPolygon pip = p.toPolygon();
    rec("PGF::toPolygon", same_polygon(qip, pip), sh, in, qstr(qip), qstr(pip));
}

// containsPoint：**两条标签**（同一条声明，OddEvenFill / WindingFill 是同一个
// 形参的两个取值，不是两个重载——按规则三合成一条 rec 反而看不出哪种规则
// 分了家，所以拆两个 api 标签，map 文件里用 `;` 挂在同一条声明下）。
static void cmp_polygonf_containspoint(const double (*pts)[2], int n, double qx, double qy)
{
    QVector<QPointF> qv; PkVector<PkPointF> pv;
    for (int i = 0; i < n; ++i) { qv << QPointF(pts[i][0], pts[i][1]); pv << PkPointF(pts[i][0], pts[i][1]); }
    const QPolygonF q(qv);
    const PkPolygonF p(pv);
    const std::string in = "n=" + istr(n) + " pt=(" + dstr(qx) + "," + dstr(qy) + ")";
    const std::string sh = n == 0 ? "empty" : shapeOfD({qx, qy});

    const bool qo = q.containsPoint(QPointF(qx, qy), Qt::OddEvenFill);
    const bool po = p.containsPoint(PkPointF(qx, qy), pkoracle::Qt::OddEvenFill);
    rec("PGF::containsPointOddEven", qo == po, sh, in, bstr(qo), bstr(po));

    const bool qw = q.containsPoint(QPointF(qx, qy), Qt::WindingFill);
    const bool pw = p.containsPoint(PkPointF(qx, qy), pkoracle::Qt::WindingFill);
    rec("PGF::containsPointWinding", qw == pw, sh, in, bstr(qw), bstr(pw));
}

// ── PkTransform::map(const PkPolygonF&) ────────────────────────────────────
//
// tag 谓词复刻 map(PointF)/mapRect 那批 tag 里已经验证过的分档逻辑：按
// TransformationType 分三档（<=TxTranslate 走快路径 / <TxProject 走一般仿射 /
// >=TxProject 是本类与 Qt 的登记在案偏离，见 PkTransform.cpp 里
// map(const PkPolygonF&) 上方那段）。
static void cmp_transform_map_polygonf(const double m[9], const double (*pts)[2], int n)
{
    const QTransform  q(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
    const PkTransform p(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);

    QVector<QPointF> qv; PkVector<PkPointF> pv;
    for (int i = 0; i < n; ++i) { qv << QPointF(pts[i][0], pts[i][1]); pv << PkPointF(pts[i][0], pts[i][1]); }
    const QPolygonF qpoly(qv);
    const PkPolygonF ppoly(pv);

    std::string in = "m=(";
    for (int i = 0; i < 9; ++i) { in += dstr(m[i]); if (i != 8) in += ","; }
    in += ") n=" + istr(n);

    // TxProject 分支是登记在案的偏离，tag 必须点名它——规则二要求谓词不能
    // 比 geometry.deviation 里的理由宽：理由说"TxProject 且非退化多边形的
    // 逐点无裁剪透视"，谓词就是这三个条件的合取（q.type() 直接问真 Qt 那侧，
    // 不是猜本类会给什么档位——两侧对同一份分量的 type() 分档在 Point/
    // Transform 两族已经钉过一致，这里不重复验证那件事，只借它分类）。
    std::string sh;
    if (n == 0) {
        sh = "empty";
    } else if ((int)q.type() >= (int)QTransform::TxProject) {
        sh = "txproject-deviation";
    } else if ((int)q.type() <= (int)QTransform::TxTranslate) {
        sh = "translate-fastpath";
    } else {
        sh = "affine-general";
    }

    const QPolygonF qm = q.map(qpoly);
    const PkPolygonF pm = p.map(ppoly);
    rec("T::map(PolygonF)", same_polygonf(qm, pm), sh, in, qstr(qm), qstr(pm));
}

// ── PkTransform::squareToQuad / quadToSquare ───────────────────────────────
static void cmp_transform_square_quad(const double (*quad)[2], int n)
{
    QVector<QPointF> qv; PkVector<PkPointF> pv;
    for (int i = 0; i < n; ++i) { qv << QPointF(quad[i][0], quad[i][1]); pv << PkPointF(quad[i][0], quad[i][1]); }
    const QPolygonF qq(qv);
    const PkPolygonF pq(pv);

    std::string in = "n=" + istr(n) + " {";
    for (int i = 0; i < n; ++i) { if (i) in += ";"; in += "(" + dstr(quad[i][0]) + "," + dstr(quad[i][1]) + ")"; }
    in += "}";
    const std::string sh = n != 4 ? "wrong-count" : "ordinary";

    QTransform qt;
    PkTransform pt;
    const bool qok = QTransform::squareToQuad(qq, qt);
    const bool pok = PkTransform::squareToQuad(pq, pt);
    bool okSame = qok == pok;
    bool matSame = !qok || (same_double(qt.m11(), pt.m11()) && same_double(qt.m12(), pt.m12())
        && same_double(qt.m13(), pt.m13()) && same_double(qt.m21(), pt.m21())
        && same_double(qt.m22(), pt.m22()) && same_double(qt.m23(), pt.m23())
        && same_double(qt.m31(), pt.m31()) && same_double(qt.m32(), pt.m32())
        && same_double(qt.m33(), pt.m33()));
    rec("T::squareToQuad", okSame && matSame, sh, in,
        bstr(qok) + " " + (qok ? qstr(qt.map(QPointF(0.5, 0.5))) : "-"),
        bstr(pok) + " " + (pok ? qstr(pt.map(PkPointF(0.5, 0.5))) : "-"));

    QTransform qt2;
    PkTransform pt2;
    const bool qok2 = QTransform::quadToSquare(qq, qt2);
    const bool pok2 = PkTransform::quadToSquare(pq, pt2);
    bool okSame2 = qok2 == pok2;
    // quadToSquare 的判据不比九个分量（inverted() 的浮点舍入路径已经在
    // Transform 族自己的 invertedXxx 那批 rec() 里钉过），比"往返"：quad 的
    // 每个顶点各自经结果矩阵映射，应当落回单位正方形对应角——这是
    // quadToSquare 存在的定义式语义，也是真实调用点（KisScreentoneGenerator*
    // 系列）真正依赖的东西。
    bool roundtripSame = true;
    if (qok2 && pok2) {
        for (int i = 0; i < n; ++i) {
            const QPointF qr = qt2.map(qq.at(i));
            const PkPointF pr = pt2.map(pq.at(i));
            if (!same_ptf(qr, pr)) { roundtripSame = false; break; }
        }
    }
    rec("T::quadToSquare", okSame2 && roundtripSame, sh, in, bstr(qok2), bstr(pok2));
}

// ═══ VectorND 族：比较原语 + tag 谓词（R-21 T3）═════════════════════════════
//
// 三个 N 维 float 向量族（PkVector2D/3D/4D）共用一套对拍骨架：分量存取、算术
// 运算符（inline，头文件逐字照抄）、length/lengthSquared/normalized/dotProduct
// （out-of-line，qvectornd.cpp 逐字照抄）。**float 精度不对称**（length double
// 累加、lengthSquared float 累加、dotProduct float 累加）见 PkVectorND.h 文件头，
// 这条是反汇编真 libQt5Gui.so 实测钉死的，对拍两侧会自然在极端量级上分家或
// 不分家，正好验证这条不对称被照抄对。
//
// 比较原语走**位比较**（same_float 用 memcpy 位比较，不是 ==），理由与
// same_double 那条一致：== 会把 +0/-0 判等、把 NaN 判不等，而 float 向量在
// 这些极端值上正是要钉住的地方。

static std::string fstr(float f)
{
    std::uint32_t b; std::memcpy(&b, &f, sizeof b);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.9g(0x%08x)", (double)f, (unsigned int)b);
    return buf;
}

// 位比较：float 的 ±0 与 NaN 都要分得开（与 same_double 同一条理由）。
static bool same_float(float a, float b)
{
    std::uint32_t ba, bb;
    std::memcpy(&ba, &a, sizeof ba);
    std::memcpy(&bb, &b, sizeof bb);
    if (ba == bb) return true;
    // NaN 的位模式两侧可以不同（载荷不保证），都是 NaN 就算同
    return (a != a) && (b != b);
}

static std::string qstr(const QVector2D &v)
{ return "(" + fstr(v.x()) + "," + fstr(v.y()) + ")"; }
static std::string qstr(const PkVector2D &v)
{ return "(" + fstr(v.x()) + "," + fstr(v.y()) + ")"; }
static std::string qstr(const QVector3D &v)
{ return "(" + fstr(v.x()) + "," + fstr(v.y()) + "," + fstr(v.z()) + ")"; }
static std::string qstr(const PkVector3D &v)
{ return "(" + fstr(v.x()) + "," + fstr(v.y()) + "," + fstr(v.z()) + ")"; }
static std::string qstr(const QVector4D &v)
{ return "(" + fstr(v.x()) + "," + fstr(v.y()) + "," + fstr(v.z()) + "," + fstr(v.w()) + ")"; }
static std::string qstr(const PkVector4D &v)
{ return "(" + fstr(v.x()) + "," + fstr(v.y()) + "," + fstr(v.z()) + "," + fstr(v.w()) + ")"; }

static bool same_v2(const QVector2D &q, const PkVector2D &p)
{ return same_float(q.x(), p.x()) && same_float(q.y(), p.y()); }
static bool same_v3(const QVector3D &q, const PkVector3D &p)
{ return same_float(q.x(), p.x()) && same_float(q.y(), p.y()) && same_float(q.z(), p.z()); }
static bool same_v4(const QVector4D &q, const PkVector4D &p)
{ return same_float(q.x(), p.x()) && same_float(q.y(), p.y()) && same_float(q.z(), p.z()) && same_float(q.w(), p.w()); }

// float 向量的形态 tag：非有限 / ±0 / 次正规 / 巨大 / 有限。与 shapeOfD 同构，
// 只是输入是 float（喂 double 版本会把 float 特有的次正规边界抹平）。
static std::string shapeOfF(std::initializer_list<float> vs)
{
    for (float v : vs) {
        if (std::isnan(v)) return "nonfinite";
        if (std::isinf(v)) return "nonfinite";
    }
    for (float v : vs) {
        if (v == 0.0f && std::signbit(v)) return "signed-zero";
    }
    for (float v : vs) {
        if (std::fpclassify(v) == FP_SUBNORMAL) return "subnormal";
    }
    for (float v : vs) {
        if (v == 0.0f) return "zero";
    }
    for (float v : vs) {
        if (std::fabs(v) >= 1e19f) return "huge";
    }
    return "finite";
}

// ═══ VectorND 族：逐 API 对拍 ═══════════════════════════════════════════════
//
// 三个类各 40+ 条声明全部要有自己的 rec()（规则三闸门，见 run_oracle.sh）。
// 构造函数是 fixture 不是被测项（构造错了 x/y 当场变红），映射到 x;y 那批
// 标签；每个成员/运算符各一条 rec。inline 的成员在头文件里逐字照抄、out-of-line
// 的编在 .cpp 里，但两侧都要在这里逐输入比对。

static void cmp_vec2d(float x, float y, float ax, float ay, float f)
{
    const QVector2D q(x, y), qa(ax, ay);
    const PkVector2D p(x, y), pa(ax, ay);
    const std::string in = fstr(x) + "," + fstr(y) + "|" + fstr(ax) + "," + fstr(ay) + "|f=" + fstr(f);
    const std::string sh = shapeOfF({x, y, ax, ay, f});

    // 默认构造（零向量）。
    {
        const QVector2D qz; const PkVector2D pz;
        rec("V2::defaultCtor", same_v2(qz, pz), sh, in, qstr(qz), qstr(pz));
    }
    rec("V2::isNull", q.isNull() == p.isNull(), sh, in, bstr(q.isNull()), bstr(p.isNull()));
    rec("V2::x", same_float(q.x(), p.x()), sh, in, fstr(q.x()), fstr(p.x()));
    rec("V2::y", same_float(q.y(), p.y()), sh, in, fstr(q.y()), fstr(p.y()));
    {
        QVector2D qs = q; PkVector2D ps = p;
        qs.setX(ax); ps.setX(ax);
        rec("V2::setX/setY", same_v2(qs, ps), sh, in, qstr(qs), qstr(ps));
        QVector2D qs2 = q; PkVector2D ps2 = p;
        qs2.setY(ay); ps2.setY(ay);
        rec("V2::setX/setY", same_v2(qs2, ps2), sh, in, qstr(qs2), qstr(ps2));
    }
    {
        QVector2D qm = q; PkVector2D pm = p;
        rec("V2::operator[]", same_float(qm[0], pm[0]) && same_float(qm[1], pm[1]), sh, in,
            fstr(qm[0]) + "," + fstr(qm[1]), fstr(pm[0]) + "," + fstr(pm[1]));
        qm[0] = ax; pm[0] = ax;   // 非 const 下标可写
        rec("V2::operator[]", same_v2(qm, pm), sh, in, qstr(qm), qstr(pm));
        const QVector2D qc = q; const PkVector2D pc = p;
        rec("V2::operator[] const", same_float(qc[0], pc[0]) && same_float(qc[1], pc[1]), sh, in,
            fstr(qc[0]) + "," + fstr(qc[1]), fstr(pc[0]) + "," + fstr(pc[1]));
    }
    rec("V2::length", same_float(q.length(), p.length()), sh, in, fstr(q.length()), fstr(p.length()));
    rec("V2::lengthSquared", same_float(q.lengthSquared(), p.lengthSquared()), sh, in,
        fstr(q.lengthSquared()), fstr(p.lengthSquared()));
    rec("V2::normalized", same_v2(q.normalized(), p.normalized()), sh, in,
        qstr(q.normalized()), qstr(p.normalized()));
    {
        QVector2D qn = q; PkVector2D pn = p;
        qn.normalize(); pn.normalize();
        rec("V2::normalize", same_v2(qn, pn), sh, in, qstr(qn), qstr(pn));
    }
    rec("V2::distanceToPoint", same_float(q.distanceToPoint(qa), p.distanceToPoint(pa)), sh, in,
        fstr(q.distanceToPoint(qa)), fstr(p.distanceToPoint(pa)));
    rec("V2::distanceToLine", same_float(q.distanceToLine(qa, QVector2D(1, 0)),
          p.distanceToLine(pa, PkVector2D(1, 0))), sh, in,
        fstr(q.distanceToLine(qa, QVector2D(1, 0))), fstr(p.distanceToLine(pa, PkVector2D(1, 0))));
    {
        QVector2D qc = q; PkVector2D pc = p; qc += qa; pc += pa;
        rec("V2::operator+=", same_v2(qc, pc), sh, in, qstr(qc), qstr(pc));
    }
    {
        QVector2D qc = q; PkVector2D pc = p; qc -= qa; pc -= pa;
        rec("V2::operator-=", same_v2(qc, pc), sh, in, qstr(qc), qstr(pc));
    }
    {
        QVector2D qc = q; PkVector2D pc = p; qc *= f; pc *= f;
        rec("V2::operator*=(float)", same_v2(qc, pc), sh, in, qstr(qc), qstr(pc));
    }
    {
        QVector2D qc = q; PkVector2D pc = p; qc *= qa; pc *= pa;
        rec("V2::operator*=(v)", same_v2(qc, pc), sh, in, qstr(qc), qstr(pc));
    }
    {
        QVector2D qc = q; PkVector2D pc = p; qc /= f; pc /= f;
        rec("V2::operator/=(float)", same_v2(qc, pc), sh, in, qstr(qc), qstr(pc));
    }
    {
        QVector2D qc = q; PkVector2D pc = p; qc /= qa; pc /= pa;
        rec("V2::operator/=(v)", same_v2(qc, pc), sh, in, qstr(qc), qstr(pc));
    }
    rec("V2::dotProduct", same_float(QVector2D::dotProduct(q, qa), PkVector2D::dotProduct(p, pa)), sh, in,
        fstr(QVector2D::dotProduct(q, qa)), fstr(PkVector2D::dotProduct(p, pa)));
    rec("V2::operator==", (q == qa) == (p == pa), sh, in, bstr(q == qa), bstr(p == pa));
    rec("V2::operator!=", (q != qa) == (p != pa), sh, in, bstr(q != qa), bstr(p != pa));
    rec("V2::operator+", same_v2(q + qa, p + pa), sh, in, qstr(q + qa), qstr(p + pa));
    rec("V2::operator-(v)", same_v2(q - qa, p - pa), sh, in, qstr(q - qa), qstr(p - pa));
    rec("V2::operator*(f,rev)", same_v2(f * q, f * p), sh, in, qstr(f * q), qstr(f * p));
    rec("V2::operator*(f)", same_v2(q * f, p * f), sh, in, qstr(q * f), qstr(p * f));
    rec("V2::operator*(v)", same_v2(q * qa, p * pa), sh, in, qstr(q * qa), qstr(p * pa));
    rec("V2::operator-unary", same_v2(-q, -p), sh, in, qstr(-q), qstr(-p));
    rec("V2::operator/(f)", same_v2(q / f, p / f), sh, in, qstr(q / f), qstr(p / f));
    rec("V2::operator/(v)", same_v2(q / qa, p / pa), sh, in, qstr(q / qa), qstr(p / pa));
    rec("V2::qFuzzyCompare", qFuzzyCompare(q, qa) == qFuzzyCompare(p, pa), sh, in,
        bstr(qFuzzyCompare(q, qa)), bstr(qFuzzyCompare(p, pa)));
    rec("V2::toVector3D", same_v3(q.toVector3D(), p.toVector3D()), sh, in,
        qstr(q.toVector3D()), qstr(p.toVector3D()));
    rec("V2::toVector4D", same_v4(q.toVector4D(), p.toVector4D()), sh, in,
        qstr(q.toVector4D()), qstr(p.toVector4D()));
    rec("V2::toPoint", q.toPoint().x() == p.toPoint().x() && q.toPoint().y() == p.toPoint().y(), sh, in,
        istr(q.toPoint().x()) + "," + istr(q.toPoint().y()), istr(p.toPoint().x()) + "," + istr(p.toPoint().y()));
    rec("V2::toPointF", same_ptf(q.toPointF(), p.toPointF()), sh, in, qstr(q.toPointF()), qstr(p.toPointF()));
}

static void cmp_vec3d(float x, float y, float z, float ax, float ay, float az, float f)
{
    const QVector3D q(x, y, z), qa(ax, ay, az);
    const PkVector3D p(x, y, z), pa(ax, ay, az);
    const std::string in = fstr(x) + "," + fstr(y) + "," + fstr(z) + "|"
                         + fstr(ax) + "," + fstr(ay) + "," + fstr(az) + "|f=" + fstr(f);
    const std::string sh = shapeOfF({x, y, z, ax, ay, az, f});

    {
        const QVector3D qz; const PkVector3D pz;
        rec("V3::defaultCtor", same_v3(qz, pz), sh, in, qstr(qz), qstr(pz));
    }
    rec("V3::isNull", q.isNull() == p.isNull(), sh, in, bstr(q.isNull()), bstr(p.isNull()));
    rec("V3::x", same_float(q.x(), p.x()), sh, in, fstr(q.x()), fstr(p.x()));
    rec("V3::y", same_float(q.y(), p.y()), sh, in, fstr(q.y()), fstr(p.y()));
    rec("V3::z", same_float(q.z(), p.z()), sh, in, fstr(q.z()), fstr(p.z()));
    {
        QVector3D qs = q; PkVector3D ps = p;
        qs.setX(ax); ps.setX(ax);
        rec("V3::setX/setY/setZ", same_v3(qs, ps), sh, in, qstr(qs), qstr(ps));
        QVector3D qs2 = q; PkVector3D ps2 = p;
        qs2.setY(ay); ps2.setY(ay);
        rec("V3::setX/setY/setZ", same_v3(qs2, ps2), sh, in, qstr(qs2), qstr(ps2));
        QVector3D qs3 = q; PkVector3D ps3 = p;
        qs3.setZ(az); ps3.setZ(az);
        rec("V3::setX/setY/setZ", same_v3(qs3, ps3), sh, in, qstr(qs3), qstr(ps3));
    }
    {
        QVector3D qm = q; PkVector3D pm = p;
        rec("V3::operator[]", same_float(qm[0], pm[0]) && same_float(qm[2], pm[2]), sh, in,
            fstr(qm[0]) + "," + fstr(qm[2]), fstr(pm[0]) + "," + fstr(pm[2]));
        qm[0] = ax; pm[0] = ax;
        rec("V3::operator[]", same_v3(qm, pm), sh, in, qstr(qm), qstr(pm));
        const QVector3D qc = q; const PkVector3D pc = p;
        rec("V3::operator[] const", same_float(qc[0], pc[0]) && same_float(qc[2], pc[2]), sh, in,
            fstr(qc[0]) + "," + fstr(qc[2]), fstr(pc[0]) + "," + fstr(pc[2]));
    }
    rec("V3::length", same_float(q.length(), p.length()), sh, in, fstr(q.length()), fstr(p.length()));
    rec("V3::lengthSquared", same_float(q.lengthSquared(), p.lengthSquared()), sh, in,
        fstr(q.lengthSquared()), fstr(p.lengthSquared()));
    rec("V3::normalized", same_v3(q.normalized(), p.normalized()), sh, in,
        qstr(q.normalized()), qstr(p.normalized()));
    {
        QVector3D qn = q; PkVector3D pn = p;
        qn.normalize(); pn.normalize();
        rec("V3::normalize", same_v3(qn, pn), sh, in, qstr(qn), qstr(pn));
    }
    {
        QVector3D qc = q; PkVector3D pc = p; qc += qa; pc += pa;
        rec("V3::operator+=", same_v3(qc, pc), sh, in, qstr(qc), qstr(pc));
    }
    {
        QVector3D qc = q; PkVector3D pc = p; qc -= qa; pc -= pa;
        rec("V3::operator-=", same_v3(qc, pc), sh, in, qstr(qc), qstr(pc));
    }
    {
        QVector3D qc = q; PkVector3D pc = p; qc *= f; pc *= f;
        rec("V3::operator*=(float)", same_v3(qc, pc), sh, in, qstr(qc), qstr(pc));
    }
    {
        QVector3D qc = q; PkVector3D pc = p; qc *= qa; pc *= pa;
        rec("V3::operator*=(v)", same_v3(qc, pc), sh, in, qstr(qc), qstr(pc));
    }
    {
        QVector3D qc = q; PkVector3D pc = p; qc /= f; pc /= f;
        rec("V3::operator/=(float)", same_v3(qc, pc), sh, in, qstr(qc), qstr(pc));
    }
    {
        QVector3D qc = q; PkVector3D pc = p; qc /= qa; pc /= pa;
        rec("V3::operator/=(v)", same_v3(qc, pc), sh, in, qstr(qc), qstr(pc));
    }
    rec("V3::dotProduct", same_float(QVector3D::dotProduct(q, qa), PkVector3D::dotProduct(p, pa)), sh, in,
        fstr(QVector3D::dotProduct(q, qa)), fstr(PkVector3D::dotProduct(p, pa)));
    rec("V3::crossProduct", same_v3(QVector3D::crossProduct(q, qa), PkVector3D::crossProduct(p, pa)), sh, in,
        qstr(QVector3D::crossProduct(q, qa)), qstr(PkVector3D::crossProduct(p, pa)));
    rec("V3::normal(2)", same_v3(QVector3D::normal(q, qa), PkVector3D::normal(p, pa)), sh, in,
        qstr(QVector3D::normal(q, qa)), qstr(PkVector3D::normal(p, pa)));
    rec("V3::normal(3)", same_v3(QVector3D::normal(q, qa, QVector3D(0, 0, 1)),
          PkVector3D::normal(p, pa, PkVector3D(0, 0, 1))), sh, in,
        qstr(QVector3D::normal(q, qa, QVector3D(0, 0, 1))), qstr(PkVector3D::normal(p, pa, PkVector3D(0, 0, 1))));
    rec("V3::distanceToPoint", same_float(q.distanceToPoint(qa), p.distanceToPoint(pa)), sh, in,
        fstr(q.distanceToPoint(qa)), fstr(p.distanceToPoint(pa)));
    rec("V3::distanceToPlane(2)", same_float(q.distanceToPlane(qa, QVector3D(0, 0, 1)),
          p.distanceToPlane(pa, PkVector3D(0, 0, 1))), sh, in,
        fstr(q.distanceToPlane(qa, QVector3D(0, 0, 1))), fstr(p.distanceToPlane(pa, PkVector3D(0, 0, 1))));
    rec("V3::distanceToPlane(3)", same_float(q.distanceToPlane(qa, QVector3D(1, 0, 0), QVector3D(0, 1, 0)),
          p.distanceToPlane(pa, PkVector3D(1, 0, 0), PkVector3D(0, 1, 0))), sh, in,
        fstr(q.distanceToPlane(qa, QVector3D(1, 0, 0), QVector3D(0, 1, 0))),
        fstr(p.distanceToPlane(pa, PkVector3D(1, 0, 0), PkVector3D(0, 1, 0))));
    rec("V3::distanceToLine", same_float(q.distanceToLine(qa, QVector3D(1, 0, 0)),
          p.distanceToLine(pa, PkVector3D(1, 0, 0))), sh, in,
        fstr(q.distanceToLine(qa, QVector3D(1, 0, 0))), fstr(p.distanceToLine(pa, PkVector3D(1, 0, 0))));
    rec("V3::operator==", (q == qa) == (p == pa), sh, in, bstr(q == qa), bstr(p == pa));
    rec("V3::operator!=", (q != qa) == (p != pa), sh, in, bstr(q != qa), bstr(p != pa));
    rec("V3::operator+", same_v3(q + qa, p + pa), sh, in, qstr(q + qa), qstr(p + pa));
    rec("V3::operator-(v)", same_v3(q - qa, p - pa), sh, in, qstr(q - qa), qstr(p - pa));
    rec("V3::operator*(f,rev)", same_v3(f * q, f * p), sh, in, qstr(f * q), qstr(f * p));
    rec("V3::operator*(f)", same_v3(q * f, p * f), sh, in, qstr(q * f), qstr(p * f));
    rec("V3::operator*(v)", same_v3(q * qa, p * pa), sh, in, qstr(q * qa), qstr(p * pa));
    rec("V3::operator-unary", same_v3(-q, -p), sh, in, qstr(-q), qstr(-p));
    rec("V3::operator/(f)", same_v3(q / f, p / f), sh, in, qstr(q / f), qstr(p / f));
    rec("V3::operator/(v)", same_v3(q / qa, p / pa), sh, in, qstr(q / qa), qstr(p / pa));
    rec("V3::qFuzzyCompare", qFuzzyCompare(q, qa) == qFuzzyCompare(p, pa), sh, in,
        bstr(qFuzzyCompare(q, qa)), bstr(qFuzzyCompare(p, pa)));
    rec("V3::toVector2D", same_v2(q.toVector2D(), p.toVector2D()), sh, in,
        qstr(q.toVector2D()), qstr(p.toVector2D()));
    rec("V3::toVector4D", same_v4(q.toVector4D(), p.toVector4D()), sh, in,
        qstr(q.toVector4D()), qstr(p.toVector4D()));
    rec("V3::toPoint", q.toPoint().x() == p.toPoint().x() && q.toPoint().y() == p.toPoint().y(), sh, in,
        istr(q.toPoint().x()) + "," + istr(q.toPoint().y()), istr(p.toPoint().x()) + "," + istr(p.toPoint().y()));
    rec("V3::toPointF", same_ptf(q.toPointF(), p.toPointF()), sh, in, qstr(q.toPointF()), qstr(p.toPointF()));
}

static void cmp_vec4d(float x, float y, float z, float w, float ax, float ay, float az, float aw)
{
    const QVector4D q(x, y, z, w), qa(ax, ay, az, aw);
    const PkVector4D p(x, y, z, w), pa(ax, ay, az, aw);
    const std::string in = fstr(x) + "," + fstr(y) + "," + fstr(z) + "," + fstr(w) + "|"
                         + fstr(ax) + "," + fstr(ay) + "," + fstr(az) + "," + fstr(aw);
    const std::string sh = shapeOfF({x, y, z, w, ax, ay, az, aw});

    {
        const QVector4D qz; const PkVector4D pz;
        rec("V4::defaultCtor", same_v4(qz, pz), sh, in, qstr(qz), qstr(pz));
    }
    rec("V4::isNull", q.isNull() == p.isNull(), sh, in, bstr(q.isNull()), bstr(p.isNull()));
    rec("V4::x", same_float(q.x(), p.x()), sh, in, fstr(q.x()), fstr(p.x()));
    rec("V4::y", same_float(q.y(), p.y()), sh, in, fstr(q.y()), fstr(p.y()));
    rec("V4::z", same_float(q.z(), p.z()), sh, in, fstr(q.z()), fstr(p.z()));
    rec("V4::w", same_float(q.w(), p.w()), sh, in, fstr(q.w()), fstr(p.w()));
    {
        QVector4D qs = q; PkVector4D ps = p;
        qs.setX(ax); ps.setX(ax);
        rec("V4::setX/setY/setZ/setW", same_v4(qs, ps), sh, in, qstr(qs), qstr(ps));
        QVector4D qs2 = q; PkVector4D ps2 = p;
        qs2.setW(aw); ps2.setW(aw);
        rec("V4::setX/setY/setZ/setW", same_v4(qs2, ps2), sh, in, qstr(qs2), qstr(ps2));
    }
    {
        QVector4D qm = q; PkVector4D pm = p;
        rec("V4::operator[]", same_float(qm[0], pm[0]) && same_float(qm[3], pm[3]), sh, in,
            fstr(qm[0]) + "," + fstr(qm[3]), fstr(pm[0]) + "," + fstr(pm[3]));
        qm[0] = ax; pm[0] = ax;
        rec("V4::operator[]", same_v4(qm, pm), sh, in, qstr(qm), qstr(pm));
        const QVector4D qc = q; const PkVector4D pc = p;
        rec("V4::operator[] const", same_float(qc[0], pc[0]) && same_float(qc[3], pc[3]), sh, in,
            fstr(qc[0]) + "," + fstr(qc[3]), fstr(pc[0]) + "," + fstr(pc[3]));
    }
    rec("V4::length", same_float(q.length(), p.length()), sh, in, fstr(q.length()), fstr(p.length()));
    rec("V4::lengthSquared", same_float(q.lengthSquared(), p.lengthSquared()), sh, in,
        fstr(q.lengthSquared()), fstr(p.lengthSquared()));
    rec("V4::normalized", same_v4(q.normalized(), p.normalized()), sh, in,
        qstr(q.normalized()), qstr(p.normalized()));
    {
        QVector4D qn = q; PkVector4D pn = p;
        qn.normalize(); pn.normalize();
        rec("V4::normalize", same_v4(qn, pn), sh, in, qstr(qn), qstr(pn));
    }
    {
        QVector4D qc = q; PkVector4D pc = p; qc += qa; pc += pa;
        rec("V4::operator+=", same_v4(qc, pc), sh, in, qstr(qc), qstr(pc));
    }
    {
        QVector4D qc = q; PkVector4D pc = p; qc -= qa; pc -= pa;
        rec("V4::operator-=", same_v4(qc, pc), sh, in, qstr(qc), qstr(pc));
    }
    {
        QVector4D qc = q; PkVector4D pc = p; qc *= w; pc *= w;
        rec("V4::operator*=(float)", same_v4(qc, pc), sh, in, qstr(qc), qstr(pc));
    }
    {
        QVector4D qc = q; PkVector4D pc = p; qc *= qa; pc *= pa;
        rec("V4::operator*=(v)", same_v4(qc, pc), sh, in, qstr(qc), qstr(pc));
    }
    {
        QVector4D qc = q; PkVector4D pc = p; qc /= w; pc /= w;
        rec("V4::operator/=(float)", same_v4(qc, pc), sh, in, qstr(qc), qstr(pc));
    }
    {
        QVector4D qc = q; PkVector4D pc = p; qc /= qa; pc /= pa;
        rec("V4::operator/=(v)", same_v4(qc, pc), sh, in, qstr(qc), qstr(pc));
    }
    rec("V4::dotProduct", same_float(QVector4D::dotProduct(q, qa), PkVector4D::dotProduct(p, pa)), sh, in,
        fstr(QVector4D::dotProduct(q, qa)), fstr(PkVector4D::dotProduct(p, pa)));
    rec("V4::operator==", (q == qa) == (p == pa), sh, in, bstr(q == qa), bstr(p == pa));
    rec("V4::operator!=", (q != qa) == (p != pa), sh, in, bstr(q != qa), bstr(p != pa));
    rec("V4::operator+", same_v4(q + qa, p + pa), sh, in, qstr(q + qa), qstr(p + pa));
    rec("V4::operator-(v)", same_v4(q - qa, p - pa), sh, in, qstr(q - qa), qstr(p - pa));
    rec("V4::operator*(f,rev)", same_v4(w * q, w * p), sh, in, qstr(w * q), qstr(w * p));
    rec("V4::operator*(f)", same_v4(q * w, p * w), sh, in, qstr(q * w), qstr(p * w));
    rec("V4::operator*(v)", same_v4(q * qa, p * pa), sh, in, qstr(q * qa), qstr(p * pa));
    rec("V4::operator-unary", same_v4(-q, -p), sh, in, qstr(-q), qstr(-p));
    rec("V4::operator/(f)", same_v4(q / w, p / w), sh, in, qstr(q / w), qstr(p / w));
    rec("V4::operator/(v)", same_v4(q / qa, p / pa), sh, in, qstr(q / qa), qstr(p / pa));
    rec("V4::qFuzzyCompare", qFuzzyCompare(q, qa) == qFuzzyCompare(p, pa), sh, in,
        bstr(qFuzzyCompare(q, qa)), bstr(qFuzzyCompare(p, pa)));
    rec("V4::toVector2D", same_v2(q.toVector2D(), p.toVector2D()), sh, in,
        qstr(q.toVector2D()), qstr(p.toVector2D()));
    rec("V4::toVector2DAffine", same_v2(q.toVector2DAffine(), p.toVector2DAffine()), sh, in,
        qstr(q.toVector2DAffine()), qstr(p.toVector2DAffine()));
    rec("V4::toVector3D", same_v3(q.toVector3D(), p.toVector3D()), sh, in,
        qstr(q.toVector3D()), qstr(p.toVector3D()));
    rec("V4::toVector3DAffine", same_v3(q.toVector3DAffine(), p.toVector3DAffine()), sh, in,
        qstr(q.toVector3DAffine()), qstr(p.toVector3DAffine()));
    rec("V4::toPoint", q.toPoint().x() == p.toPoint().x() && q.toPoint().y() == p.toPoint().y(), sh, in,
        istr(q.toPoint().x()) + "," + istr(q.toPoint().y()), istr(p.toPoint().x()) + "," + istr(p.toPoint().y()));
    rec("V4::toPointF", same_ptf(q.toPointF(), p.toPointF()), sh, in, qstr(q.toPointF()), qstr(p.toPointF()));
}


// ═══ canary：证明比较管道是活的 ════════════════════════════════════════════
//
// 走的是与真实 API 完全相同的 rec() 与比较原语。三条都必须出现在 DIFFTAG 里，
// 少一条就说明对应的那段管道死了：
//   int-value               —— 整数比较与计数还活着
//   double-bits-signed-zero —— same_double 真的是**位**比较，没退化成 `==`
//                              （`==` 会把 +0.0 与 -0.0 判等，这条就消失）
//   nan-vs-number           —— NaN 侧的判等分支没有把一切都判成"同"
static void run_canaries()
{
    const double nan_ = std::nan("");
    rec("canary", same_pt(QPoint(1, 2), PkPoint(1, 3)), "int-value",
        "deliberate", qstr(QPoint(1, 2)), qstr(PkPoint(1, 3)));
    rec("canary", same_double(0.0, -0.0), "double-bits-signed-zero",
        "deliberate", dstr(0.0), dstr(-0.0));
    rec("canary", same_double(nan_, 1.0), "nan-vs-number",
        "deliberate", dstr(nan_), dstr(1.0));
    // 反向自证：这两条**必须**判成"同"，否则 same_double 过严（NaN 两侧判不等
    // 会让整份对拍在任何含 NaN 的输入上刷假差异）。它们不产生 tag。
    rec("canary-negative", same_double(nan_, nan_), "nan-vs-nan",
        "deliberate", dstr(nan_), dstr(nan_));
    rec("canary-negative", same_double(1.5, 1.5), "identical",
        "deliberate", dstr(1.5), dstr(1.5));
}

// ═══ 输入集 ════════════════════════════════════════════════════════════════

// ── Group 1：手挑对抗用例（钉住已知形态；覆盖度的主力是 Group 2）──────────
static const double kHandD[] = {
    // 零号两侧
    0.0, -0.0,
    // qRound 的取整方向：半值
    0.5, -0.5, 1.5, -1.5, 2.5, -2.5, 3.5, -3.5,
    // int(d+0.5) 的经典边界
    0.49999999999999994, -0.49999999999999994, 0.5000000000000001,
    // fuzzy 阈值两侧
    1e-12, -1e-12, 1e-13, 1e-11, 1e-300, 2e-12,
    // 次正规 / 极小 / 极大
    5e-324, -5e-324, 1e-323, 2.2250738585072014e-308, 1e308, -1e308, 1.7976931348623157e308,
    // 特值
    INFINITY, -INFINITY, NAN,
    // int 值域边界（toPoint 的 UB 分界）
    2147483647.0, 2147483648.0, -2147483648.0, -2147483649.0, 4294967296.0,
    // 普通值
    1.0, -1.0, 2.0, -2.0, 0.25, -0.25, 3.0, -3.0, 1.0 + 1e-13, 1.0 + 1e-11,
};
static const int kHandI[] = {
    0, 1, -1, 2, -2, 3, -3, 5, -5, 7, -7,
    46340, -46340, 46341, -46341,          // 平方接近 INT_MAX 的分界
    1000000, -1000000, 65536, -65536,
    INT_MAX, INT_MIN, INT_MAX - 1, INT_MIN + 1, INT_MAX / 2, INT_MIN / 2,
};

// ── Group 2：token 全组合（覆盖度的主力）──────────────────────────────────
// Point 是二元的：21 个标量 token → 21² = 441 个点 → 两点组合 441² = 194 481 次
// 比对/API。带标量参数的 API 做三层（点的两层 + 参数一层）：四层再乘参数会到
// 千万量级、跑几分钟（R-01 踩过）。
static const double kTokD[] = {
    0.0, -0.0, 0.5, -0.5, 1.0, -1.0, 1.5, -1.5, 2.5, -2.5, 3.0, -3.0,
    0.49999999999999994, 1e-12, 1e-13, 1e-300, 5e-324, 1e308, -1e308,
    INFINITY, NAN,
};
static const int kTokI[] = {
    0, 1, -1, 2, -2, 3, -3, 5, -5, 7, -7, 46341, -46341,
    1000000, -1000000, 65536, INT_MAX, INT_MIN, INT_MAX - 1, INT_MIN + 1,
};
// 缩放参数（第三层）
static const double kFacD[] = {
    0.0, -0.0, 0.5, -0.5, 1.0, -1.0, 2.0, -2.0, 3.0, 0.25,
    0.49999999999999994, 1e-300, 1e308, INFINITY, -INFINITY, NAN,
    1.5, -1.5, 2147483648.0, 1e-12,
};
static const float kFacF[] = {
    0.0f, -0.0f, 0.5f, -0.5f, 1.0f, -1.0f, 2.0f, -2.0f, 3.0f, 0.25f,
    0.49999997f,                       // ⚠ 按 float 精度会进位，提升到 double 不会
    1e-30f, 1e30f, INFINITY, -INFINITY, NAN, 1.5f, -1.5f, 2147483648.0f, 1e-12f,
};
static const int kFacI[] = { 0, 1, -1, 2, -2, 3, 65536, -65536, INT_MAX, INT_MIN };

// ── Group 3：Rect 族的坐标 token ──────────────────────────────────────────
//
// 矩形是**四元**的（一元 API 4 个分量、双目 API 8 个），44⁴ / 44⁸ 那种做满显然
// 做不了，所以按 README 那条约定做「密集小域 + 极值域 + 手挑集」三层交叉，
// 保证**每个分量位上都取得到极值与手挑值**（不是索引轮转）。
//
// 密集小域：6 个 token → 6⁴ = 1 296 个矩形。双目做满 1 296² ≈ 168 万组。
// 这批值覆盖了 x2 相对 x1 的全部关键位置（-1 = 宽 0、-2 = 宽 -1 要交换、
// 0 = 宽 1、+1/+2/+3 = 正常宽）。
static const int kRectTok6[] = { -2, -1, 0, 1, 2, 3 };
// 极值域：7 个 token → 7⁴ = 2 401 个矩形。只跑一元/修改器，双目太大。
static const int kRectTokX[] = { INT_MIN, INT_MIN + 1, -1, 0, 1, INT_MAX - 1, INT_MAX };
// 极值域的双目版：5 个 token → 5⁴ = 625 个矩形，双目做满 625² ≈ 39 万组。
static const int kRectTokE[] = { INT_MIN, -1, 0, 1, INT_MAX };
// 修改器/点的实参 token（含极值，让 setLeft(INT_MIN) 这类走到）
static const int kRectArg[] = { -2, -1, 0, 1, INT_MIN, INT_MAX };
// adjust 的增量：计划点名的 {-2,-1,0,1,2}⁴ = 625 组
static const int kAdjD[] = { -2, -1, 0, 1, 2 };
// adjust 的矩形底座（4⁴ = 256，再乘 625 已经是 16 万组）
static const int kRectTok4[] = { -1, 0, 1, 2 };

// 手挑对抗矩形，**以内部坐标给出**（(l,t,w,h) 构造够不到其中一大半形态）。
// 每一条都对应一个已实测的语义分界，注释写的就是它守着哪一条。
static const int kHandRect[][4] = {
    {  0,  0, -1, -1 },                       // QRect()：null，| 的偏心分支
    {  5,  5,  4,  4 },                       // (5,5,0,0)：null 但不在原点
    {  0,  0,  0,  0 },                       // (0,0,1,1)：最小的非空矩形
    {  0,  0,  9,  9 },                       // (0,0,10,10)：差一的标准样本
    {  0,  0, -2, -2 },                       // (0,0,-1,-1)：empty 非 null，要交换
    {  5,  5,  1,  1 },                       // (5,5,-3,-3)：负宽高且不在原点
    { 20, 20, 24, 24 },                       // 与上面几个完全不相交
    {  0,  0, -1,  0 },                       // 宽恰为 0：normalized **不**交换的边界
    {  0,  0, -2,  0 },                       // 宽 -1：交换边界的另一侧
    {  0,  0, -3,  0 },                       // 宽 -2
    {  0,  0,  9, -1 },                       // (0,0,10,0)：一边退化
    { INT_MIN, INT_MIN, INT_MIN, INT_MIN },   // (INT_MIN,INT_MIN,1,1)
    {  0,  0, INT_MAX - 1, INT_MAX - 1 },     // (0,0,INT_MAX,INT_MAX)
    {  0,  0, INT_MAX, INT_MAX },             // 跨距溢出
    { INT_MIN, INT_MIN, INT_MAX, INT_MAX },   // 跨距溢出到 0，center 靠 qint64
    { INT_MAX, INT_MAX, INT_MIN, INT_MIN },   // 反向极值：要交换且溢出
};

// ── Group 4：RectF 族的分量 token ────────────────────────────────────────
//
// **形状照抄 Group 3**（密集小域做满 + 特值域做满 + 手挑集与两个域双向交叉），
// 换的只是 token 的内容：整数矩形关心的是"跨距是 -1 / < -1 / 极值"，浮点矩形
// 关心的是"尺寸恰好 0 / 负 / ±0.0 / 次正规 / nan / inf / 半值边界"。
// 四个字段共用同一套 token（位置与尺寸轮流取它们），这样每个分量位上都取得到
// 每一个值 —— 与 Group 3 的理由完全相同。
//
// 密集小域：6 个 → 6⁴ = 1 296 个矩形，双目做满 1 296² ≈ 168 万组。
// 这批值覆盖了尺寸相对 0 的全部关键位置（-1 = 负宽要翻正、-0.0 = 不翻正但符号
// 位要留住、0.0 = 三谓词与 & 的判空分界、0.5 = 整数版够不到的小正宽、
// 2.0 = 普通、inf = 非有限）。
static const double kRectFTok6[] = { -1.0, -0.0, 0.0, 0.5, 2.0, INFINITY };
// 特值域：5 个 → 5⁴ = 625 个矩形，双目做满 625² ≈ 39 万组。
// NaN 单独放这里而不是塞进 kRectFTok6：混进密集域会让 168 万组里绝大多数都带
// NaN（四个字段任一是 NaN 就整片走同一条路），把普通形态的覆盖度挤掉。
static const double kRectFTok5[] = { NAN, -INFINITY, 1e308, 5e-324, 1.0 };
// 一元/构造/修改器的底座域：10 个 → 10⁴ = 10 000 个矩形。一元 API 只有 4 个
// 分量，做得起更大的域，正好把 toRect/toAlignedRect 最在意的**半值**塞进来。
static const double kRectFTokU[] = {
    -1.5, -1.0, -0.5, -0.0, 0.0, 0.5, 1.0, 2.5, INFINITY, NAN,
};
// 修改器/点的实参 token（含特值，让 setLeft(inf)、moveTo(nan) 这类走到）
static const double kRectFArg[] = { -1.5, -0.0, 0.0, 0.5, INFINITY, NAN };
// adjust 的增量：5⁴ = 625 组
static const double kAdjDF[] = { -1.0, -0.0, 0.0, 0.5, NAN };
// adjust 的矩形底座（4⁴ = 256，再乘 625 已经是 16 万组）
static const double kRectFTok4[] = { -1.0, 0.0, 0.5, 2.0 };

// 手挑对抗矩形，以 **(x, y, w, h) 四个字段**给出。每一条都对应一个已实测的
// 语义分界，注释写的就是它守着哪一条（真 Qt 5.15.7 实测）。
static const double kHandRectF[][4] = {
    {  0,    0,    0,    0    },   // QRectF()：isNull，| 的偏心分支
    {  5,    5,    0,    0    },   // null 但不在原点
    {  0,    0,   -0.0, -0.0  },   // ⚠ -0.0 也算 isNull（`-0.0 == 0.` 为真）
    {  0,    0,   -0.0,  1    },   // 只有一个轴是 -0.0：不翻正、符号位要留住
    {  0,    0,   10,   10    },   // 差一的标准样本（right 必须是 10 不是 9）
    {  0,    0,   -1,   -1    },   // empty 非 null，两个轴都要翻正
    {  5,    5,   -3,   -3    },   // 负宽高且不在原点：翻正后是 (2,2,3,3)
    { 20,   20,    5,    5    },   // 与上面几个完全不相交
    {  0,    0,    0,   10    },   // 一个轴恰好退化成线：& / contains 的提前返回
    {  0,    0,    0.5,  0.5  },   // ⚠ 整数矩形够不到的"小正宽"：isEmpty 必须为假
    {  0,    0,    5e-324, 5e-324 }, // 次正规宽高：isValid 仍是真
    { -1.5, -1.5,  1,    1    },   // toRect / toAlignedRect 分家的标准样本
    { -0.5, -0.5,  1,    1    },   // 半值边界（负半值向 +∞ 取整）
    {  0.5,  0.5,  1,    1    },
    {  2.5,  2.5,  1,    1    },
    {  0.49999999999999994, 0, 1, 1 }, // ⚠ qRound 进位 + xp+w 恰好舍入成 1.5
    {  1e308, 0,  -1e308, 1   },   // 翻正后仍是有限量
    { -1e308, 0,   1e308, 1   },   // right() 溢出到 inf
    {  0,    0,    NAN,  1    },   // 三谓词在 NaN 上互不为补
    {  0,    0,    INFINITY, INFINITY }, // isValid 为真的非有限矩形
    { -INFINITY, -INFINITY, INFINITY, INFINITY }, // right() = nan
    {  1e10,  0,   1,    1    },   // 取整越出 int 值域（两侧都是 UB）
};


// ── Group 5：Transform 族的输入 ──────────────────────────────────────────
//
// 九个分量做不了全组合（7⁹ = 4 千万个矩阵，再乘点集直接爆），所以照计划
// R-03.md §Task6 那条：**每次只让 3 个分量变化、其余固定成几组基底**。
// 三件事一起构成覆盖：
//   ① 基底集 kTfBase —— 十个"有名字的形态"（单位/平移/缩放/负缩放/直角旋转/
//      斜旋转/切变/投影/奇异/含特值），每一个都是真实调用点上会出现的东西；
//   ② 槽位分组 kTfSlots —— 把九个分量切成三组各三个，每组在基底上做满 token³。
//      三组合起来**每个分量位都被扫过**，不会有某个分量永远等于基底值；
//   ③ 手挑集 kTfHand —— 基底扫不到的对抗形态（全 nan、全 inf、m33=0、
//      刚好落在 qFuzzyIsNull 门槛两侧的 1e-12/1e-13 等）。
// ⚠ 手挑集同时充当**二元 API 的 a 侧**，与扫描集做双向交叉 ——
// 与 Rect/RectF 两族「手挑值必须能出现在每一个分量位上」是同一条约定。

// 计划点名的七元 token。
static const double kTfTok[] = { -2.0, -1.0, -0.5, 0.0, 0.5, 1.0, 2.0 };
// 重活用的缩减 token（5 个），理由：重活那几个 API 的 rec 代价是 Point 族的
// 十几倍（每条要打九个分量），全用 7³ 会把单次运行拖到分钟级。
static const double kTfTokS[] = { -2.0, -0.5, 0.0, 1.0, 2.0 };
// 特值：塞进单个分量位，逐位扫过九个位置。
static const double kTfSpecial[] = { -0.0, 5e-324, 1e308, INFINITY, -INFINITY, NAN,
                                     1e-12, 1e-13, 1.0 + 1e-13 };

// 顺序：m11 m12 m13 m21 m22 m23 m31 m32 m33
static const double kTfBase[][9] = {
    { 1, 0, 0,   0, 1, 0,   0, 0, 1 },          // 单位
    { 1, 0, 0,   0, 1, 0,   5, 7, 1 },          // 纯平移
    { 2, 0, 0,   0, 3, 0,   0, 0, 1 },          // 纯缩放
    { -2, 0, 0,  0, 3, 0,   0, 0, 1 },          // 负缩放
    { 0, 1, 0,  -1, 0, 0,   0, 0, 1 },          // rotate(90) 的精确结果
    { 0.70710678118654757, 0.70710678118654746, 0,
      -0.70710678118654746, 0.70710678118654757, 0, 0, 0, 1 },   // rotate(45)
    { 1, 2, 0,   1, 1, 0,   0, 0, 1 },          // 切变
    { 1, 0, 0.5, 0, 1, 0.25, 0, 0, 1 },         // 投影
    { 1, 2, 3,   2, 4, 6,   3, 6, 9 },          // 奇异（三行成比例，det = 0）
    { 1, 0, -1,  0, 1, 0,   0, 0, 1 },          // 需要透视裁剪的投影（w = -x+1）
};

// 九个分量切成三组，三组合起来盖满九个位置。
static const int kTfSlots[][3] = { { 0, 4, 6 }, { 1, 3, 2 }, { 5, 8, 7 } };

static const double kTfHand[][9] = {
    { 1, 0, 0,   0, 1, 0,   0, 0, 1 },
    { 0, 0, 0,   0, 0, 0,   0, 0, 0 },          // 全零（det = 0，m33 = 0）
    { 1, 0, 0,   0, 1, 0,   0, 0, 0 },          // m33 = 0 —— 投影且 w 恒为 0
    { 1, 0, 0,   0, 1, 0,   0, 0, 3 },          // 只有 m33 != 1
    { 1, 0, 1e-13, 0, 1, 0,  0, 0, 1 },         // m13 恰在 qFuzzyIsNull 门槛内
    { 1, 0, 1e-12, 0, 1, 0,  0, 0, 1 },         // m13 恰在门槛上（<= 1e-12 为真）
    { 1, 0, 1e-11, 0, 1, 0,  0, 0, 1 },         // m13 越过门槛
    { 1 + 1e-13, 0, 0, 0, 1, 0, 0, 0, 1 },      // m11 - 1 落在门槛内
    { 2, 1, 0,  -1, 2, 0,   0, 0, 1 },          // dot = 0 -> TxRotate
    { 2, 1, 0,   2, 1, 0,   0, 0, 1 },          // dot = 5 -> TxShear
    { 1, 2, 0,   2, 4, 0,   0, 0, 1 },          // 二阶式为 0 的切变（affine 奇异）
    { 1, 1, 0,   1, 1 + 1e-16, 0, 0, 0, 1 },    // 二阶式**精确**为 0（1+1e-16 == 1）
    { 1e-300, 0, 0, 0, 1, 0, 0, 0, 1 },         // 缩放轴 qFuzzyIsNull 但非 0
    { NAN, 0, 0,  0, 1, 0,   0, 0, 1 },
    { 0, 0, 0,   0, 0, 0,   0, 0, NAN },
    { NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN },
    { INFINITY, 0, 0, 0, 1, 0, 0, 0, 1 },
    { INFINITY, INFINITY, INFINITY, INFINITY, INFINITY, INFINITY,
      INFINITY, INFINITY, INFINITY },
    { -INFINITY, 0, 0, 0, -INFINITY, 0, 0, 0, 1 },
    { -0.0, -0.0, -0.0, -0.0, -0.0, -0.0, -0.0, -0.0, -0.0 },
    { 1, 0, 0,   0, 1, 0,  -0.0, -0.0, 1 },     // 平移分量是 -0.0
    { 5e-324, 0, 0, 0, 5e-324, 0, 0, 0, 1 },    // 次正规
    { 1e308, 0, 0, 0, 1e308, 0, 0, 0, 1 },      // 相乘会溢出到 inf
    { 1, 0, -1,  0, 1, -1,  0, 0, 1 },          // 两个方向都要裁剪
    { 1, 0, 0.01, 0, 1, 0,  0, 0, 1 },          // 投影但**不**需要裁剪
    { 1, 2, 3,   4, 5, 6,   7, 8, 9 },          // 计划里反复出现的那个奇异阵
    { 9, 8, 7,   6, 5, 4,   3, 2, 1 },
    { 2, 3, 0,   5, 7, 0,  11, 13, 1 },         // 探针 §I 的非对称阵
};

// map 的点 token（浮点与整数各一套）。
static const double kTfPt[] = { -2.0, -1.0, -0.0, 0.0, 1.0, 2.0, INFINITY, NAN };
static const int kTfPtI[] = { INT_MIN, -2, -1, 0, 1, 2, INT_MAX };
// translate/scale/shear 的实参、以及 fromTranslate/fromScale 的实参。
static const double kTfArg[] = { -2.0, -1.0, -0.0, 0.0, 1.0, 2.0, NAN };
// 标量运算符的实参（1 与 0 是三条提前返回的分界）。
static const double kTfSca[] = { -2.0, -1.0, -0.0, 0.0, 0.5, 1.0, 2.0, 3.0,
                                 INFINITY, NAN, 1e-300, 1e308 };
// rotate 的角度：五个特判值、它们的邻居、以及非特判的常见角。
static const double kTfAng[] = { 0.0, -0.0, 90.0, -90.0, 180.0, -180.0, 270.0, -270.0,
                                 89.999999999999986, 90.000000000000014, 360.0, 45.0,
                                 30.0, 1.0, -1.0, 1e-300, 1e308, INFINITY, NAN,
                                 0.017453292519943295, 3.141592653589793 };
// mapRect 的浮点矩形（x,y,w,h）与整数矩形（内部四坐标）。
static const double kTfRectF[][4] = {
    { 0, 0, 10, 10 }, { 0, 0, 0, 0 }, { 0, 0, -1, -1 }, { -5, -5, 10, 10 },
    { 0.5, 0.5, 1.5, 1.5 }, { 0, 0, 1e308, 1e308 }, { 0, 0, INFINITY, INFINITY },
    { NAN, 0, 10, 10 }, { 0, 0, NAN, 10 }, { -0.0, -0.0, -0.0, -0.0 },
    { 1, 2, 3, 4 }, { 0, 0, 5e-324, 5e-324 },
};
static const int kTfRectI[][4] = {
    { 0, 0, 9, 9 }, { 0, 0, -1, -1 }, { 5, 5, 4, 4 }, { 0, 0, 0, 0 },
    { -5, -5, 5, 5 }, { INT_MIN, INT_MIN, INT_MAX, INT_MAX },
    { INT_MAX, INT_MAX, INT_MIN, INT_MIN }, { 0, 0, -2, 0 },
    { 1, 2, 3, 4 }, { -1, -1, 1, 1 },
};

template <typename T, std::size_t N>
static constexpr int countOf(const T (&)[N]) { return (int)N; }

int main()
{
    // 把 Qt 的运行期警告吞掉：本对拍**故意**大量喂退化输入，Qt 在某些路径上会
    // 往 stderr 刷警告。run_oracle.sh 用 2>&1 收输出，不吞的话失败时那几行
    // tail 只剩噪音。抄这份骨架时保留这一段（Rect/Transform 那几节真会触发）。
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &) {});

    run_canaries();

    const int nHandD = countOf(kHandD), nHandI = countOf(kHandI);
    const int nTokD = countOf(kTokD), nTokI = countOf(kTokI);
    const int nFacD = countOf(kFacD), nFacF = countOf(kFacF), nFacI = countOf(kFacI);

    // ── Group 1：手挑对抗用例的**全组合**（不是索引轮转）──────────────────
    //
    // ⚠ **Task 3–6 照抄这个形状。** 第一版写的是 `x = kHand[i], y = kHand[(i+1)%n]`
    // 这种索引轮转，44 个手挑值只产生 44 个点而不是 44² 个 —— 后果是
    // **kHand 里那批 kTok 没有的值永远无法同时出现在 x 和 y 上**
    //（0.5000000000000001、1e-323、2.2250738585072014e-308、2147483647/8.0、
    //  -2147483649.0、4294967296.0、1.0+1e-13、2e-12、±0.25、±3.5 …）。
    // 复评实测：注入一个只在 `x == y == 2147483648.0`（手挑独有值）时触发的
    // toPoint 缺陷，对拍**全绿 exit=0**，连跑两遍一致。旁证：manhattanLength 用
    // fabs 的真缺陷全场只抓到 1 次，落在 (-0.0,-0.0) —— 纯粹因为 -0.0 恰好也在
    // kTok 里；若它只在手挑集里，那个真缺陷会全绿溜过。
    //
    // Point 是二元的（一元 API 2 个分量、二元 API 4 个），所以这里直接做满：
    // 一元 44²（double）/ 25²（int），二元 44⁴ / 25⁴ —— **数字以 kHandD/kHandI
    // 的实际元素个数为准**（countOf 算的，44 与 25）。**分量更多时不要退回轮转**
    // —— Rect 的二元 API 有 8 个分量、44⁸ 显然做不了，那就至少做
    //「手挑 × token」的交叉（手挑值必须能出现在每一个分量位上），
    // 绝不能让某个分量位永远取不到手挑值。Size 族的 scaled 就是这么做的
    //（4 个分量 + mode，见下面 Size 那几段）。
    for (int i = 0; i < nHandD; ++i)
        for (int j = 0; j < nHandD; ++j)
            cmp_pointf_unary(kHandD[i], kHandD[j]);
    for (int i = 0; i < nHandD; ++i)
        for (int j = 0; j < nHandD; ++j)
            for (int k = 0; k < nHandD; ++k)
                for (int l = 0; l < nHandD; ++l)
                    cmp_pointf_binary(kHandD[i], kHandD[j], kHandD[k], kHandD[l]);
    for (int i = 0; i < nHandD; ++i)
        for (int j = 0; j < nHandD; ++j)
            for (int f = 0; f < nFacD; ++f)
                cmp_pointf_scale(kHandD[i], kHandD[j], kFacD[f]);

    cmp_point_constants();
    for (int i = 0; i < nHandI; ++i)
        for (int j = 0; j < nHandI; ++j)
            cmp_point_unary(kHandI[i], kHandI[j]);
    for (int i = 0; i < nHandI; ++i)
        for (int j = 0; j < nHandI; ++j)
            for (int k = 0; k < nHandI; ++k)
                for (int l = 0; l < nHandI; ++l)
                    cmp_point_binary(kHandI[i], kHandI[j], kHandI[k], kHandI[l]);
    for (int i = 0; i < nHandI; ++i)
        for (int j = 0; j < nHandI; ++j) {
            for (int f = 0; f < nFacD; ++f) cmp_point_scale_double(kHandI[i], kHandI[j], kFacD[f]);
            for (int f = 0; f < nFacF; ++f) cmp_point_scale_float(kHandI[i], kHandI[j], kFacF[f]);
            for (int f = 0; f < nFacI; ++f) cmp_point_scale_int(kHandI[i], kHandI[j], kFacI[f]);
        }
    // 跨类型：整数点全组合 × 浮点分量全组合。
    for (int i = 0; i < nHandI; ++i)
        for (int j = 0; j < nHandI; ++j)
            for (int a = 0; a < nHandD; ++a)
                for (int b = 0; b < nHandD; ++b)
                    cmp_promotion(kHandI[i], kHandI[j], kHandD[a], kHandD[b]);

    // ── Group 2a：一元 API 走 token² 个点 ──
    for (int a = 0; a < nTokD; ++a)
        for (int b = 0; b < nTokD; ++b)
            cmp_pointf_unary(kTokD[a], kTokD[b]);
    for (int a = 0; a < nTokI; ++a)
        for (int b = 0; b < nTokI; ++b)
            cmp_point_unary(kTokI[a], kTokI[b]);

    // ── Group 2b：二元 API 走 (token²)² 组两点组合 ──
    for (int a = 0; a < nTokD; ++a)
        for (int b = 0; b < nTokD; ++b)
            for (int c = 0; c < nTokD; ++c)
                for (int d = 0; d < nTokD; ++d)
                    cmp_pointf_binary(kTokD[a], kTokD[b], kTokD[c], kTokD[d]);
    for (int a = 0; a < nTokI; ++a)
        for (int b = 0; b < nTokI; ++b)
            for (int c = 0; c < nTokI; ++c)
                for (int d = 0; d < nTokI; ++d)
                    cmp_point_binary(kTokI[a], kTokI[b], kTokI[c], kTokI[d]);

    // ── Group 2c：带标量参数的 API 做三层（点两层 + 参数一层）──
    for (int a = 0; a < nTokD; ++a)
        for (int b = 0; b < nTokD; ++b)
            for (int f = 0; f < nFacD; ++f)
                cmp_pointf_scale(kTokD[a], kTokD[b], kFacD[f]);
    for (int a = 0; a < nTokI; ++a)
        for (int b = 0; b < nTokI; ++b) {
            for (int f = 0; f < nFacD; ++f) cmp_point_scale_double(kTokI[a], kTokI[b], kFacD[f]);
            for (int f = 0; f < nFacF; ++f) cmp_point_scale_float(kTokI[a], kTokI[b], kFacF[f]);
            for (int f = 0; f < nFacI; ++f) cmp_point_scale_int(kTokI[a], kTokI[b], kFacI[f]);
        }

    // ── Group 2d：跨类型（提升 + 混合运算）三层 ──
    for (int a = 0; a < nTokI; ++a)
        for (int b = 0; b < nTokI; ++b)
            for (int d = 0; d < nTokD; ++d)
                cmp_promotion(kTokI[a], kTokI[b], kTokD[d], kTokD[(d + 3) % nTokD]);

    // ═══ Size 族 ═══════════════════════════════════════════════════════════
    // 输入集与 Point 族**共用**（kHandI/kTokI/kHandD/kTokD/kFacD）：尺寸的
    // 关键边界（0、±1、INT_MIN/MAX、±0.0、nan、inf、次正规）它们都覆盖到了，
    // 另起一套只会多一份要维护的东西。
    cmp_size_constants();

    // 一元：手挑² + token²（与 Point 同形）
    for (int i = 0; i < nHandI; ++i)
        for (int j = 0; j < nHandI; ++j)
            cmp_size_unary(kHandI[i], kHandI[j]);
    for (int a = 0; a < nTokI; ++a)
        for (int b = 0; b < nTokI; ++b)
            cmp_size_unary(kTokI[a], kTokI[b]);
    for (int i = 0; i < nHandD; ++i)
        for (int j = 0; j < nHandD; ++j)
            cmp_sizef_unary(kHandD[i], kHandD[j]);
    for (int a = 0; a < nTokD; ++a)
        for (int b = 0; b < nTokD; ++b)
            cmp_sizef_unary(kTokD[a], kTokD[b]);

    // 二元：手挑⁴ + token⁴（做满，与 Point 同形）
    for (int i = 0; i < nHandI; ++i)
        for (int j = 0; j < nHandI; ++j)
            for (int k = 0; k < nHandI; ++k)
                for (int l = 0; l < nHandI; ++l)
                    cmp_size_binary(kHandI[i], kHandI[j], kHandI[k], kHandI[l]);
    for (int a = 0; a < nTokI; ++a)
        for (int b = 0; b < nTokI; ++b)
            for (int c = 0; c < nTokI; ++c)
                for (int d = 0; d < nTokI; ++d)
                    cmp_size_binary(kTokI[a], kTokI[b], kTokI[c], kTokI[d]);
    for (int i = 0; i < nHandD; ++i)
        for (int j = 0; j < nHandD; ++j)
            for (int k = 0; k < nHandD; ++k)
                for (int l = 0; l < nHandD; ++l)
                    cmp_sizef_binary(kHandD[i], kHandD[j], kHandD[k], kHandD[l]);
    for (int a = 0; a < nTokD; ++a)
        for (int b = 0; b < nTokD; ++b)
            for (int c = 0; c < nTokD; ++c)
                for (int d = 0; d < nTokD; ++d)
                    cmp_sizef_binary(kTokD[a], kTokD[b], kTokD[c], kTokD[d]);

    // 带标量参数的缩放：尺寸两层 + 参数一层（与 Point 同形）
    for (int i = 0; i < nHandI; ++i)
        for (int j = 0; j < nHandI; ++j)
            for (int f = 0; f < nFacD; ++f)
                cmp_size_scale(kHandI[i], kHandI[j], kFacD[f]);
    for (int a = 0; a < nTokI; ++a)
        for (int b = 0; b < nTokI; ++b)
            for (int f = 0; f < nFacD; ++f)
                cmp_size_scale(kTokI[a], kTokI[b], kFacD[f]);
    for (int i = 0; i < nHandD; ++i)
        for (int j = 0; j < nHandD; ++j)
            for (int f = 0; f < nFacD; ++f)
                cmp_sizef_scale(kHandD[i], kHandD[j], kFacD[f]);
    for (int a = 0; a < nTokD; ++a)
        for (int b = 0; b < nTokD; ++b)
            for (int f = 0; f < nFacD; ++f)
                cmp_sizef_scale(kTokD[a], kTokD[b], kFacD[f]);

    // ── scaled/scale：4 个分量 + mode，做不了 25⁴×25⁴ ─────────────────────
    // 按 README 那条约定做**「手挑 × token」双向交叉**：源取手挑对、目标取
    // token 对，再反过来一遍 —— 于是**每个分量位上都取得到手挑值**，
    // 而不是像索引轮转那样让某些手挑值永远进不了某个位置。
    // 规模：int 25²×20²×2×3 模式 = 1 500 000 组；double 44²×21²×2×3 = 5 122 656 组。
    for (int mi = 0; mi < 3; ++mi) {
        for (int i = 0; i < nHandI; ++i)
            for (int j = 0; j < nHandI; ++j)
                for (int a = 0; a < nTokI; ++a)
                    for (int b = 0; b < nTokI; ++b) {
                        cmp_size_scaled(kHandI[i], kHandI[j], kTokI[a], kTokI[b], mi);
                        cmp_size_scaled(kTokI[a], kTokI[b], kHandI[i], kHandI[j], mi);
                    }
        for (int i = 0; i < nHandD; ++i)
            for (int j = 0; j < nHandD; ++j)
                for (int a = 0; a < nTokD; ++a)
                    for (int b = 0; b < nTokD; ++b) {
                        cmp_sizef_scaled(kHandD[i], kHandD[j], kTokD[a], kTokD[b], mi);
                        cmp_sizef_scaled(kTokD[a], kTokD[b], kHandD[i], kHandD[j], mi);
                    }
    }

    // 跨类型：整数尺寸全组合 × 浮点分量全组合（与 Point 的 cmp_promotion 同形）
    for (int i = 0; i < nHandI; ++i)
        for (int j = 0; j < nHandI; ++j)
            for (int a = 0; a < nHandD; ++a)
                for (int b = 0; b < nHandD; ++b)
                    cmp_size_promotion(kHandI[i], kHandI[j], kHandD[a], kHandD[b]);
    for (int a = 0; a < nTokI; ++a)
        for (int b = 0; b < nTokI; ++b)
            for (int d = 0; d < nTokD; ++d)
                cmp_size_promotion(kTokI[a], kTokI[b], kTokD[d], kTokD[(d + 3) % nTokD]);

    // ═══ Rect 族（整数）═══════════════════════════════════════════════════
    const int nR6 = countOf(kRectTok6), nRX = countOf(kRectTokX);
    const int nRE = countOf(kRectTokE), nR4 = countOf(kRectTok4);
    const int nArg = countOf(kRectArg), nAdj = countOf(kAdjD);
    const int nHR = countOf(kHandRect);

    cmp_rect_constants();

    // 构造函数：8 个 token 四层做满（计划点名的那 8 个）
    {
        static const int kCtorTok[] = { -2, -1, 0, 1, 2, 3, INT_MIN, INT_MAX };
        const int n = countOf(kCtorTok);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                for (int k = 0; k < n; ++k)
                    for (int l = 0; l < n; ++l)
                        cmp_rect_ctor(kCtorTok[i], kCtorTok[j], kCtorTok[k], kCtorTok[l]);
    }

    // ── 一元：密集域 1 296 + 极值域 2 401 + 手挑 16 ──
    for (int i = 0; i < nR6; ++i)
        for (int j = 0; j < nR6; ++j)
            for (int k = 0; k < nR6; ++k)
                for (int l = 0; l < nR6; ++l)
                    cmp_rect_unary(kRectTok6[i], kRectTok6[j], kRectTok6[k], kRectTok6[l]);
    for (int i = 0; i < nRX; ++i)
        for (int j = 0; j < nRX; ++j)
            for (int k = 0; k < nRX; ++k)
                for (int l = 0; l < nRX; ++l)
                    cmp_rect_unary(kRectTokX[i], kRectTokX[j], kRectTokX[k], kRectTokX[l]);
    for (int h = 0; h < nHR; ++h)
        cmp_rect_unary(kHandRect[h][0], kHandRect[h][1], kHandRect[h][2], kHandRect[h][3]);

    // ── 修改器：矩形三层 × 实参 6² ──
    for (int i = 0; i < nR6; ++i)
        for (int j = 0; j < nR6; ++j)
            for (int k = 0; k < nR6; ++k)
                for (int l = 0; l < nR6; ++l)
                    for (int a = 0; a < nArg; ++a)
                        for (int b = 0; b < nArg; ++b)
                            cmp_rect_mutate(kRectTok6[i], kRectTok6[j], kRectTok6[k],
                                            kRectTok6[l], kRectArg[a], kRectArg[b]);
    for (int i = 0; i < nRE; ++i)
        for (int j = 0; j < nRE; ++j)
            for (int k = 0; k < nRE; ++k)
                for (int l = 0; l < nRE; ++l)
                    for (int a = 0; a < nArg; ++a)
                        for (int b = 0; b < nArg; ++b)
                            cmp_rect_mutate(kRectTokE[i], kRectTokE[j], kRectTokE[k],
                                            kRectTokE[l], kRectArg[a], kRectArg[b]);
    for (int h = 0; h < nHR; ++h)
        for (int a = 0; a < nArg; ++a)
            for (int b = 0; b < nArg; ++b)
                cmp_rect_mutate(kHandRect[h][0], kHandRect[h][1], kHandRect[h][2],
                                kHandRect[h][3], kRectArg[a], kRectArg[b]);

    // ── adjust：矩形 4⁴ / 手挑 × 增量 5⁴ ──
    for (int i = 0; i < nR4; ++i)
        for (int j = 0; j < nR4; ++j)
            for (int k = 0; k < nR4; ++k)
                for (int l = 0; l < nR4; ++l)
                    for (int a = 0; a < nAdj; ++a)
                        for (int b = 0; b < nAdj; ++b)
                            for (int c = 0; c < nAdj; ++c)
                                for (int d = 0; d < nAdj; ++d)
                                    cmp_rect_adjust(kRectTok4[i], kRectTok4[j], kRectTok4[k],
                                                    kRectTok4[l], kAdjD[a], kAdjD[b],
                                                    kAdjD[c], kAdjD[d]);
    for (int h = 0; h < nHR; ++h)
        for (int a = 0; a < nAdj; ++a)
            for (int b = 0; b < nAdj; ++b)
                for (int c = 0; c < nAdj; ++c)
                    for (int d = 0; d < nAdj; ++d)
                        cmp_rect_adjust(kHandRect[h][0], kHandRect[h][1], kHandRect[h][2],
                                        kHandRect[h][3], kAdjD[a], kAdjD[b], kAdjD[c], kAdjD[d]);

    // ── contains(point)：点坐标由**矩形自己的边界**导出（l-1/l/l+1/r-1/r/r+1），
    // 这样差一边界在每个矩形上都真的被踩到；固定 token 集做不到这一点。
    {
        auto pointsAround = [](int lo, int hi, long long out[6]) {
            out[0] = (long long)lo - 1; out[1] = lo; out[2] = (long long)lo + 1;
            out[3] = (long long)hi - 1; out[4] = hi; out[5] = (long long)hi + 1;
        };
        auto runContains = [&](int x1, int y1, int x2, int y2) {
            long long xs[6], ys[6];
            pointsAround(x1, x2, xs);
            pointsAround(y1, y2, ys);
            for (int a = 0; a < 6; ++a)
                for (int b = 0; b < 6; ++b) {
                    // 导出值可能越出 int（lo-1 在 INT_MIN 上）——夹回值域，
                    // 越界的那一格由极值 token 自己覆盖。
                    if (xs[a] < INT_MIN || xs[a] > INT_MAX) continue;
                    if (ys[b] < INT_MIN || ys[b] > INT_MAX) continue;
                    cmp_rect_contains_point(x1, y1, x2, y2, (int)xs[a], (int)ys[b]);
                }
        };
        for (int i = 0; i < nR6; ++i)
            for (int j = 0; j < nR6; ++j)
                for (int k = 0; k < nR6; ++k)
                    for (int l = 0; l < nR6; ++l)
                        runContains(kRectTok6[i], kRectTok6[j], kRectTok6[k], kRectTok6[l]);
        for (int i = 0; i < nRX; ++i)
            for (int j = 0; j < nRX; ++j)
                for (int k = 0; k < nRX; ++k)
                    for (int l = 0; l < nRX; ++l)
                        runContains(kRectTokX[i], kRectTokX[j], kRectTokX[k], kRectTokX[l]);
        for (int h = 0; h < nHR; ++h)
            runContains(kHandRect[h][0], kHandRect[h][1], kHandRect[h][2], kHandRect[h][3]);
    }

    // ── 双目：密集域做满 1 296²，极值域做满 625²，再加手挑 × 两个域的
    // **双向**交叉（手挑值必须能出现在 a 侧和 b 侧的每一个分量位上）──
    for (int i = 0; i < nR6; ++i)
        for (int j = 0; j < nR6; ++j)
            for (int k = 0; k < nR6; ++k)
                for (int l = 0; l < nR6; ++l)
                    for (int m = 0; m < nR6; ++m)
                        for (int n = 0; n < nR6; ++n)
                            for (int o = 0; o < nR6; ++o)
                                for (int p = 0; p < nR6; ++p)
                                    cmp_rect_binary(kRectTok6[i], kRectTok6[j], kRectTok6[k],
                                                    kRectTok6[l], kRectTok6[m], kRectTok6[n],
                                                    kRectTok6[o], kRectTok6[p]);
    for (int i = 0; i < nRE; ++i)
        for (int j = 0; j < nRE; ++j)
            for (int k = 0; k < nRE; ++k)
                for (int l = 0; l < nRE; ++l)
                    for (int m = 0; m < nRE; ++m)
                        for (int n = 0; n < nRE; ++n)
                            for (int o = 0; o < nRE; ++o)
                                for (int p = 0; p < nRE; ++p)
                                    cmp_rect_binary(kRectTokE[i], kRectTokE[j], kRectTokE[k],
                                                    kRectTokE[l], kRectTokE[m], kRectTokE[n],
                                                    kRectTokE[o], kRectTokE[p]);
    for (int h = 0; h < nHR; ++h) {
        for (int i = 0; i < nR6; ++i)
            for (int j = 0; j < nR6; ++j)
                for (int k = 0; k < nR6; ++k)
                    for (int l = 0; l < nR6; ++l) {
                        cmp_rect_binary(kHandRect[h][0], kHandRect[h][1], kHandRect[h][2],
                                        kHandRect[h][3],
                                        kRectTok6[i], kRectTok6[j], kRectTok6[k], kRectTok6[l]);
                        cmp_rect_binary(kRectTok6[i], kRectTok6[j], kRectTok6[k], kRectTok6[l],
                                        kHandRect[h][0], kHandRect[h][1], kHandRect[h][2],
                                        kHandRect[h][3]);
                    }
        for (int i = 0; i < nRX; ++i)
            for (int j = 0; j < nRX; ++j)
                for (int k = 0; k < nRX; ++k)
                    for (int l = 0; l < nRX; ++l) {
                        cmp_rect_binary(kHandRect[h][0], kHandRect[h][1], kHandRect[h][2],
                                        kHandRect[h][3],
                                        kRectTokX[i], kRectTokX[j], kRectTokX[k], kRectTokX[l]);
                        cmp_rect_binary(kRectTokX[i], kRectTokX[j], kRectTokX[k], kRectTokX[l],
                                        kHandRect[h][0], kHandRect[h][1], kHandRect[h][2],
                                        kHandRect[h][3]);
                    }
        for (int g = 0; g < nHR; ++g)
            cmp_rect_binary(kHandRect[h][0], kHandRect[h][1], kHandRect[h][2], kHandRect[h][3],
                            kHandRect[g][0], kHandRect[g][1], kHandRect[g][2], kHandRect[g][3]);
    }

    // ═══ RectF 族（浮点）══════════════════════════════════════════════════
    // 形状与上面的 Rect 族逐段对应，只换 token（理由见 Group 4 上方那段）。
    const int nF6 = countOf(kRectFTok6), nF5 = countOf(kRectFTok5);
    const int nFU = countOf(kRectFTokU), nF4 = countOf(kRectFTok4);
    const int nFArg = countOf(kRectFArg), nAdjF = countOf(kAdjDF);
    const int nHRF = countOf(kHandRectF);

    cmp_rectf_constants();

    // 构造函数：底座域 10⁴ 做满 + 手挑集
    for (int i = 0; i < nFU; ++i)
        for (int j = 0; j < nFU; ++j)
            for (int k = 0; k < nFU; ++k)
                for (int l = 0; l < nFU; ++l)
                    cmp_rectf_ctor(kRectFTokU[i], kRectFTokU[j],
                                   kRectFTokU[k], kRectFTokU[l]);
    for (int h = 0; h < nHRF; ++h)
        cmp_rectf_ctor(kHandRectF[h][0], kHandRectF[h][1],
                       kHandRectF[h][2], kHandRectF[h][3]);

    // PkRect → PkRectF 的提升：输入沿用**整数**语料（那边的退化形态才是重点）
    for (int i = 0; i < nR6; ++i)
        for (int j = 0; j < nR6; ++j)
            for (int k = 0; k < nR6; ++k)
                for (int l = 0; l < nR6; ++l)
                    cmp_rectf_from_rect(kRectTok6[i], kRectTok6[j],
                                        kRectTok6[k], kRectTok6[l]);
    for (int i = 0; i < nRX; ++i)
        for (int j = 0; j < nRX; ++j)
            for (int k = 0; k < nRX; ++k)
                for (int l = 0; l < nRX; ++l)
                    cmp_rectf_from_rect(kRectTokX[i], kRectTokX[j],
                                        kRectTokX[k], kRectTokX[l]);
    for (int h = 0; h < nHR; ++h)
        cmp_rectf_from_rect(kHandRect[h][0], kHandRect[h][1],
                            kHandRect[h][2], kHandRect[h][3]);

    // ── 一元：底座域 10 000 + 特值域 625 + 手挑 ──
    for (int i = 0; i < nFU; ++i)
        for (int j = 0; j < nFU; ++j)
            for (int k = 0; k < nFU; ++k)
                for (int l = 0; l < nFU; ++l)
                    cmp_rectf_unary(kRectFTokU[i], kRectFTokU[j],
                                    kRectFTokU[k], kRectFTokU[l]);
    for (int i = 0; i < nF5; ++i)
        for (int j = 0; j < nF5; ++j)
            for (int k = 0; k < nF5; ++k)
                for (int l = 0; l < nF5; ++l)
                    cmp_rectf_unary(kRectFTok5[i], kRectFTok5[j],
                                    kRectFTok5[k], kRectFTok5[l]);
    for (int h = 0; h < nHRF; ++h)
        cmp_rectf_unary(kHandRectF[h][0], kHandRectF[h][1],
                        kHandRectF[h][2], kHandRectF[h][3]);

    // ── 修改器：矩形 6⁴ × 实参 6²，再加特值域与手挑 ──
    for (int i = 0; i < nF6; ++i)
        for (int j = 0; j < nF6; ++j)
            for (int k = 0; k < nF6; ++k)
                for (int l = 0; l < nF6; ++l)
                    for (int a = 0; a < nFArg; ++a)
                        for (int b = 0; b < nFArg; ++b)
                            cmp_rectf_mutate(kRectFTok6[i], kRectFTok6[j], kRectFTok6[k],
                                             kRectFTok6[l], kRectFArg[a], kRectFArg[b]);
    for (int i = 0; i < nF5; ++i)
        for (int j = 0; j < nF5; ++j)
            for (int k = 0; k < nF5; ++k)
                for (int l = 0; l < nF5; ++l)
                    for (int a = 0; a < nFArg; ++a)
                        for (int b = 0; b < nFArg; ++b)
                            cmp_rectf_mutate(kRectFTok5[i], kRectFTok5[j], kRectFTok5[k],
                                             kRectFTok5[l], kRectFArg[a], kRectFArg[b]);
    for (int h = 0; h < nHRF; ++h)
        for (int a = 0; a < nFArg; ++a)
            for (int b = 0; b < nFArg; ++b)
                cmp_rectf_mutate(kHandRectF[h][0], kHandRectF[h][1], kHandRectF[h][2],
                                 kHandRectF[h][3], kRectFArg[a], kRectFArg[b]);

    // ── adjust：矩形 4⁴ / 手挑 × 增量 5⁴ ──
    for (int i = 0; i < nF4; ++i)
        for (int j = 0; j < nF4; ++j)
            for (int k = 0; k < nF4; ++k)
                for (int l = 0; l < nF4; ++l)
                    for (int a = 0; a < nAdjF; ++a)
                        for (int b = 0; b < nAdjF; ++b)
                            for (int c = 0; c < nAdjF; ++c)
                                for (int d = 0; d < nAdjF; ++d)
                                    cmp_rectf_adjust(kRectFTok4[i], kRectFTok4[j],
                                                     kRectFTok4[k], kRectFTok4[l],
                                                     kAdjDF[a], kAdjDF[b],
                                                     kAdjDF[c], kAdjDF[d]);
    for (int h = 0; h < nHRF; ++h)
        for (int a = 0; a < nAdjF; ++a)
            for (int b = 0; b < nAdjF; ++b)
                for (int c = 0; c < nAdjF; ++c)
                    for (int d = 0; d < nAdjF; ++d)
                        cmp_rectf_adjust(kHandRectF[h][0], kHandRectF[h][1],
                                         kHandRectF[h][2], kHandRectF[h][3],
                                         kAdjDF[a], kAdjDF[b], kAdjDF[c], kAdjDF[d]);

    // ── contains(point)：点坐标由**矩形自己翻正后的区间**导出（l-0.5 / l / 中点
    // / r / r+0.5），外加 NaN —— 这样闭区间的两条边在每个矩形上都真的被踩到；
    // 固定 token 集做不到这一点。形状照抄整数版的 pointsAround。
    {
        auto runContainsF = [](double x, double y, double w, double h) {
            double lx, rx, ly, ry;
            qtIntervalF(x, w, lx, rx);
            qtIntervalF(y, h, ly, ry);
            const double xs[6] = { lx - 0.5, lx, (lx + rx) / 2, rx, rx + 0.5, NAN };
            const double ys[6] = { ly - 0.5, ly, (ly + ry) / 2, ry, ry + 0.5, NAN };
            for (int a = 0; a < 6; ++a)
                for (int b = 0; b < 6; ++b)
                    cmp_rectf_contains_point(x, y, w, h, xs[a], ys[b]);
        };
        for (int i = 0; i < nF6; ++i)
            for (int j = 0; j < nF6; ++j)
                for (int k = 0; k < nF6; ++k)
                    for (int l = 0; l < nF6; ++l)
                        runContainsF(kRectFTok6[i], kRectFTok6[j],
                                     kRectFTok6[k], kRectFTok6[l]);
        for (int i = 0; i < nF5; ++i)
            for (int j = 0; j < nF5; ++j)
                for (int k = 0; k < nF5; ++k)
                    for (int l = 0; l < nF5; ++l)
                        runContainsF(kRectFTok5[i], kRectFTok5[j],
                                     kRectFTok5[k], kRectFTok5[l]);
        for (int h = 0; h < nHRF; ++h)
            runContainsF(kHandRectF[h][0], kHandRectF[h][1],
                         kHandRectF[h][2], kHandRectF[h][3]);
    }

    // ── 双目：密集域做满 1 296²，特值域做满 625²，再加手挑 × 两个域的
    // **双向**交叉（手挑值必须能出现在 a 侧和 b 侧的每一个分量位上）──
    for (int i = 0; i < nF6; ++i)
        for (int j = 0; j < nF6; ++j)
            for (int k = 0; k < nF6; ++k)
                for (int l = 0; l < nF6; ++l)
                    for (int m = 0; m < nF6; ++m)
                        for (int n = 0; n < nF6; ++n)
                            for (int o = 0; o < nF6; ++o)
                                for (int p = 0; p < nF6; ++p)
                                    cmp_rectf_binary(kRectFTok6[i], kRectFTok6[j],
                                                     kRectFTok6[k], kRectFTok6[l],
                                                     kRectFTok6[m], kRectFTok6[n],
                                                     kRectFTok6[o], kRectFTok6[p]);
    for (int i = 0; i < nF5; ++i)
        for (int j = 0; j < nF5; ++j)
            for (int k = 0; k < nF5; ++k)
                for (int l = 0; l < nF5; ++l)
                    for (int m = 0; m < nF5; ++m)
                        for (int n = 0; n < nF5; ++n)
                            for (int o = 0; o < nF5; ++o)
                                for (int p = 0; p < nF5; ++p)
                                    cmp_rectf_binary(kRectFTok5[i], kRectFTok5[j],
                                                     kRectFTok5[k], kRectFTok5[l],
                                                     kRectFTok5[m], kRectFTok5[n],
                                                     kRectFTok5[o], kRectFTok5[p]);
    for (int h = 0; h < nHRF; ++h) {
        for (int i = 0; i < nF6; ++i)
            for (int j = 0; j < nF6; ++j)
                for (int k = 0; k < nF6; ++k)
                    for (int l = 0; l < nF6; ++l) {
                        cmp_rectf_binary(kHandRectF[h][0], kHandRectF[h][1],
                                         kHandRectF[h][2], kHandRectF[h][3],
                                         kRectFTok6[i], kRectFTok6[j],
                                         kRectFTok6[k], kRectFTok6[l]);
                        cmp_rectf_binary(kRectFTok6[i], kRectFTok6[j],
                                         kRectFTok6[k], kRectFTok6[l],
                                         kHandRectF[h][0], kHandRectF[h][1],
                                         kHandRectF[h][2], kHandRectF[h][3]);
                    }
        for (int i = 0; i < nF5; ++i)
            for (int j = 0; j < nF5; ++j)
                for (int k = 0; k < nF5; ++k)
                    for (int l = 0; l < nF5; ++l) {
                        cmp_rectf_binary(kHandRectF[h][0], kHandRectF[h][1],
                                         kHandRectF[h][2], kHandRectF[h][3],
                                         kRectFTok5[i], kRectFTok5[j],
                                         kRectFTok5[k], kRectFTok5[l]);
                        cmp_rectf_binary(kRectFTok5[i], kRectFTok5[j],
                                         kRectFTok5[k], kRectFTok5[l],
                                         kHandRectF[h][0], kHandRectF[h][1],
                                         kHandRectF[h][2], kHandRectF[h][3]);
                    }
        for (int g = 0; g < nHRF; ++g)
            cmp_rectf_binary(kHandRectF[h][0], kHandRectF[h][1],
                             kHandRectF[h][2], kHandRectF[h][3],
                             kHandRectF[g][0], kHandRectF[g][1],
                             kHandRectF[g][2], kHandRectF[g][3]);
    }


    // ═══ Transform 族 ══════════════════════════════════════════════════════
    //
    // 三层输入（说明见 Group 5 上方）：
    //   · 全量扫描 sweepFull —— 10 基底 × 3 槽位组 × 7³ = 10 290 个矩阵，
    //     喂给两个构造与全部一元 API（每个矩阵 22 条 rec）
    //   · 缩减扫描 sweepWork —— 10 × 3 × 5³ + 手挑 28 = 3 778 个矩阵，
    //     喂给重活（map / mapRect / mutate / rotate / scalar / cache / binary）。
    //     缩减不是偷懒：这一族每条 rec 要打九个分量，代价是 Point 族的十几倍，
    //     全量喂重活会把单次运行拖到分钟级而覆盖的形态并不多
    //   · 特值逐位扫 —— 九个分量位 × 9 个特值 × 10 基底，专打 nan/inf/±0/门槛值
    {
        const int nTfTok = countOf(kTfTok), nTfTokS = countOf(kTfTokS);
        const int nTfBase = countOf(kTfBase), nTfSlot = countOf(kTfSlots);
        const int nTfHand = countOf(kTfHand), nTfSpec = countOf(kTfSpecial);

        std::vector<double> sweepFull, sweepWork;
        double m[9];
        for (int b = 0; b < nTfBase; ++b) {
            for (int sl = 0; sl < nTfSlot; ++sl) {
                for (int i = 0; i < nTfTok; ++i)
                    for (int j = 0; j < nTfTok; ++j)
                        for (int k = 0; k < nTfTok; ++k) {
                            for (int c = 0; c < 9; ++c) m[c] = kTfBase[b][c];
                            m[kTfSlots[sl][0]] = kTfTok[i];
                            m[kTfSlots[sl][1]] = kTfTok[j];
                            m[kTfSlots[sl][2]] = kTfTok[k];
                            sweepFull.insert(sweepFull.end(), m, m + 9);
                        }
                for (int i = 0; i < nTfTokS; ++i)
                    for (int j = 0; j < nTfTokS; ++j)
                        for (int k = 0; k < nTfTokS; ++k) {
                            for (int c = 0; c < 9; ++c) m[c] = kTfBase[b][c];
                            m[kTfSlots[sl][0]] = kTfTokS[i];
                            m[kTfSlots[sl][1]] = kTfTokS[j];
                            m[kTfSlots[sl][2]] = kTfTokS[k];
                            sweepWork.insert(sweepWork.end(), m, m + 9);
                        }
            }
        }
        // 特值逐位扫：九个分量位一个都不落（只改一位，其余留基底值）。
        for (int b = 0; b < nTfBase; ++b)
            for (int pos = 0; pos < 9; ++pos)
                for (int v = 0; v < nTfSpec; ++v) {
                    for (int c = 0; c < 9; ++c) m[c] = kTfBase[b][c];
                    m[pos] = kTfSpecial[v];
                    sweepFull.insert(sweepFull.end(), m, m + 9);
                }
        for (int h = 0; h < nTfHand; ++h) {
            sweepFull.insert(sweepFull.end(), kTfHand[h], kTfHand[h] + 9);
            sweepWork.insert(sweepWork.end(), kTfHand[h], kTfHand[h] + 9);
        }
        const int nFull = (int)(sweepFull.size() / 9);
        const int nWork = (int)(sweepWork.size() / 9);

        cmp_tf_constants();

        for (int i = 0; i < nFull; ++i) {
            const double *mm = &sweepFull[9 * i];
            cmp_tf_ctor(mm);
            cmp_tf_unary(mm);
        }

        const int nPt = countOf(kTfPt), nPtI = countOf(kTfPtI);
        const int nArg = countOf(kTfArg), nSca = countOf(kTfSca), nAng = countOf(kTfAng);
        const int nRF = countOf(kTfRectF), nRI = countOf(kTfRectI);

        for (int i = 0; i < nWork; ++i) {
            const double *mm = &sweepWork[9 * i];

            for (int a = 0; a < nPt; ++a)
                for (int b = 0; b < nPt; ++b)
                    cmp_tf_map(mm, kTfPt[a], kTfPt[b]);
            for (int a = 0; a < nPtI; ++a)
                for (int b = 0; b < nPtI; ++b)
                    cmp_tf_map_int(mm, kTfPtI[a], kTfPtI[b]);

            for (int a = 0; a < nArg; ++a)
                for (int b = 0; b < nArg; ++b)
                    cmp_tf_mutate(mm, kTfArg[a], kTfArg[b]);

            for (int a = 0; a < nAng; ++a)
                cmp_tf_rotate(mm, kTfAng[a]);

            for (int a = 0; a < nSca; ++a) {
                cmp_tf_scalar(mm, kTfSca[a]);
                // I-1：四个标量运算符各自的"制造过期"路径都要喂
                for (int op = 0; op < 4; ++op)
                    cmp_tf_cache(mm, kTfSca[a], op);
            }

            // I-1：旋转往返（type/after-roundtrip）与连续 mutator 链
            for (int a = 0; a < nAng; ++a)
                cmp_tf_roundtrip(mm, kTfAng[a]);
            for (int a = 0; a < nArg; ++a)
                cmp_tf_chain(mm, kTfArg[a]);

            for (int r = 0; r < nRF; ++r)
                cmp_tf_maprect(mm, kTfRectF[r][0], kTfRectF[r][1],
                               kTfRectF[r][2], kTfRectF[r][3]);
            for (int r = 0; r < nRI; ++r)
                cmp_tf_maprect_int(mm, kTfRectI[r][0], kTfRectI[r][1],
                                   kTfRectI[r][2], kTfRectI[r][3]);
        }

        // 二元：手挑集 × 缩减扫描集，**双向**（手挑值必须能出现在 a 侧和 b 侧）。
        for (int h = 0; h < nTfHand; ++h)
            for (int i = 0; i < nWork; ++i) {
                cmp_tf_binary(kTfHand[h], &sweepWork[9 * i]);
                cmp_tf_binary(&sweepWork[9 * i], kTfHand[h]);
            }
        // 手挑 × 手挑做满（28² = 784 组）。
        for (int h = 0; h < nTfHand; ++h)
            for (int g = 0; g < nTfHand; ++g)
                cmp_tf_binary(kTfHand[h], kTfHand[g]);

        // 两个静态工厂：实参全组合（含 nan/inf/±0）。
        {
            static const double kFactory[] = { -2.0, -1.0, -0.0, 0.0, 1.0, 2.0,
                                               INFINITY, -INFINITY, NAN, 1e308, 5e-324 };
            const int n = countOf(kFactory);
            for (int a = 0; a < n; ++a)
                for (int b = 0; b < n; ++b)
                    cmp_tf_static(kFactory[a], kFactory[b]);
        }
    }

    // ═══ Line 族 ═══════════════════════════════════════════════════════════
    //
    // 规模（R-21 plan.md「问 3」）：手挑对抗用例 + 有界组合，不复刻 R-03 的
    // 亿级组合爆炸。token 集特意比 Point/Rect 族的小（8 个而不是 21/44 个）
    // ——本族的 out-of-line 成员（length/angle/setAngle/angleTo/unitVector/
    // intersects/fromPolar）已经用独立差分脚本跑过 591 119 + 50 100 次零
    // mismatch 的验证（PkLine.cpp 顶部注释），这里的对拍任务是把同一批公式
    // 接进机器闸门与"已声明偏离"框架，不是重新做一遍穷举搜索。
    {
        static const double kLineHand[][4] = {
            {  0,  0,   0,  0 },      // 退化：两点重合，length()/setLength()/
                                       // unitVector() 的特殊分支
            {  0,  0,   1,  0 },      // 单位右（angle() == 0）
            {  0,  0,   0,  1 },      // 单位下（Qt y 朝下，angle() == 270）
            {  0,  0,  -1,  0 },      // 单位左（angle() == 180）
            {  0,  0,   0, -1 },      // 单位上（angle() == 90）
            {  0,  0,   1,  1 },      // 对角线（angle() == 315）
            {  0,  0,   3,  4 },      // 3-4-5 直角三角形，length() == 5 精确
            {  5,  5,   5,  5 },      // 退化，不在原点
            { -5, -5,   5,  5 },      // 跨原点的长对角线
            {  1,  2,   4,  6 },      // 非对称普通线段
            {  0,  0, 1e308,  0 },    // 巨大：length()/unitVector() 的溢出边界
            {  0,  0, 1e-300, 0 },    // 极小：非退化但接近 0
            { -0.0, -0.0, 0.0, 0.0 }, // 负零：isNull() 的零分支
            {  0,  0, NAN,  0 },      // NaN：intersects()/isNull() 的 NaN 分支
            {  0,  0, INFINITY, 0 },  // 无穷：length()/angle() 的无穷分支
            {  2,  3,   2,  3 },      // 退化，不在原点（与 {5,5,5,5} 分属两个
                                       // 象限，交叉用于 intersects/angleTo）
        };
        const int nLH = countOf(kLineHand);
        static const double kLineTok[] = { 0.0, -0.0, 0.5, -0.5, 1.0, -1.0, 1e308, NAN };
        const int nLT = countOf(kLineTok);
        static const double kLineParam[] = {
            0.0, -0.0, 0.5, -0.5, 1.0, -1.0, 2.0, -2.0, 3.0, 0.25,
            1e-300, 1e300, INFINITY, -INFINITY, NAN, 90.0, 180.0, 270.0, 360.0, -90.0,
        };
        const int nLP = countOf(kLineParam);

        cmp_line_constants();

        // PkLine：int 手挑 + 有界组合。
        static const int kLineIntHand[] = {
            0, 1, -1, 5, -5, INT_MAX, INT_MIN, INT_MAX - 1, INT_MIN + 1,
        };
        const int nLI = countOf(kLineIntHand);
        for (int i = 0; i < nLI; ++i)
            for (int j = 0; j < nLI; ++j)
                for (int k = 0; k < nLI; ++k)
                    for (int l = 0; l < nLI; ++l) {
                        cmp_line_ctors(kLineIntHand[i], kLineIntHand[j],
                                      kLineIntHand[k], kLineIntHand[l]);
                        cmp_line_promotion(kLineIntHand[i], kLineIntHand[j],
                                           kLineIntHand[k], kLineIntHand[l]);
                    }

        // PkLineF 一元：手挑集 + token 全组合（8⁴ = 4096，每组合 ~15 条
        // rec()，单这一段已经 ≥ 6 万次比对，远超 10⁴ 的目标）。
        for (int h = 0; h < nLH; ++h) {
            cmp_linef_ctors(kLineHand[h][0], kLineHand[h][1], kLineHand[h][2], kLineHand[h][3]);
            cmp_linef_unary(kLineHand[h][0], kLineHand[h][1], kLineHand[h][2], kLineHand[h][3]);
        }
        for (int i = 0; i < nLT; ++i)
            for (int j = 0; j < nLT; ++j)
                for (int k = 0; k < nLT; ++k)
                    for (int l = 0; l < nLT; ++l) {
                        cmp_linef_ctors(kLineTok[i], kLineTok[j], kLineTok[k], kLineTok[l]);
                        cmp_linef_unary(kLineTok[i], kLineTok[j], kLineTok[k], kLineTok[l]);
                    }

        // 带参数的一元 API：手挑线 × 手挑参数集。
        for (int h = 0; h < nLH; ++h)
            for (int a = 0; a < nLP; ++a) {
                cmp_linef_translate(kLineHand[h][0], kLineHand[h][1],
                                    kLineHand[h][2], kLineHand[h][3], kLineParam[a], -kLineParam[a]);
                cmp_linef_pointat(kLineHand[h][0], kLineHand[h][1],
                                  kLineHand[h][2], kLineHand[h][3], kLineParam[a]);
                cmp_linef_setlength(kLineHand[h][0], kLineHand[h][1],
                                    kLineHand[h][2], kLineHand[h][3], kLineParam[a]);
                cmp_linef_setangle(kLineHand[h][0], kLineHand[h][1],
                                   kLineHand[h][2], kLineHand[h][3], kLineParam[a]);
            }
        for (int a = 0; a < nLP; ++a)
            for (int b = 0; b < nLP; ++b)
                cmp_linef_frompolar(kLineParam[a], kLineParam[b]);

        // 二元 API：手挑 × 手挑做满（16² = 256）。
        for (int h = 0; h < nLH; ++h)
            for (int g = 0; g < nLH; ++g) {
                cmp_linef_angleto(kLineHand[h][0], kLineHand[h][1], kLineHand[h][2], kLineHand[h][3],
                                  kLineHand[g][0], kLineHand[g][1], kLineHand[g][2], kLineHand[g][3]);
                cmp_linef_intersects(kLineHand[h][0], kLineHand[h][1], kLineHand[h][2], kLineHand[h][3],
                                     kLineHand[g][0], kLineHand[g][1], kLineHand[g][2], kLineHand[g][3]);
                cmp_linef_binary(kLineHand[h][0], kLineHand[h][1], kLineHand[h][2], kLineHand[h][3],
                                 kLineHand[g][0], kLineHand[g][1], kLineHand[g][2], kLineHand[g][3]);
            }
        // 二元 API 再加 token 交叉（4⁴ 对 4⁴ 太大，改用 token 子集 × 手挑，
        // 双向，保证 token 值在 a 侧和 b 侧都出现得到）。
        static const double kLineTok4[] = { -1.0, 0.0, 0.5, 1.0 };
        const int nLT4 = countOf(kLineTok4);
        for (int i = 0; i < nLT4; ++i)
            for (int j = 0; j < nLT4; ++j)
                for (int k = 0; k < nLT4; ++k)
                    for (int l = 0; l < nLT4; ++l)
                        for (int h = 0; h < nLH; ++h) {
                            cmp_linef_intersects(kLineTok4[i], kLineTok4[j], kLineTok4[k], kLineTok4[l],
                                                 kLineHand[h][0], kLineHand[h][1],
                                                 kLineHand[h][2], kLineHand[h][3]);
                            cmp_linef_angleto(kLineTok4[i], kLineTok4[j], kLineTok4[k], kLineTok4[l],
                                              kLineHand[h][0], kLineHand[h][1],
                                              kLineHand[h][2], kLineHand[h][3]);
                        }

        cmp_linef_intersects_nullarg();
    }

    // ═══ Margins 族 ════════════════════════════════════════════════════════
    {
        cmp_margins_constants();

        static const int kMarginsIntHand[] = {
            0, 1, -1, 2, -2, 5, -5, INT_MAX, INT_MIN, INT_MAX - 1, INT_MIN + 1,
        };
        const int nMI = countOf(kMarginsIntHand);
        for (int i = 0; i < nMI; ++i)
            for (int j = 0; j < nMI; ++j)
                for (int k = 0; k < nMI; ++k)
                    for (int l = 0; l < nMI; ++l)
                        cmp_margins_ctor(kMarginsIntHand[i], kMarginsIntHand[j],
                                         kMarginsIntHand[k], kMarginsIntHand[l]);

        // ⚠ **不做 8 层嵌套的全组合**（7⁸ ≈ 576 万，会把单次运行拖到不合理的
        // 时长）。改成「一层跑主分量、其余三个分量按 token 集循环移位取值」
        // ——四个分量各自都能取到 token 集里的每一个值（移位保证），只是不再
        // 保证"任意四元组合"都被枚举到，这与 kHandRect 系列"手挑×手挑做满"
        // 的精神一致：token 值必须在每个分量位上出现，不必是笛卡尔积全集。
        static const int kMarginsIntTok[] = { -2, -1, 0, 1, 2, INT_MAX, INT_MIN };
        const int nMIT = countOf(kMarginsIntTok);
        for (int i = 0; i < nMIT; ++i)
            for (int j = 0; j < nMIT; ++j)
                cmp_margins_binary(
                    kMarginsIntTok[i], kMarginsIntTok[(i + 1) % nMIT],
                    kMarginsIntTok[(i + 2) % nMIT], kMarginsIntTok[(i + 3) % nMIT],
                    kMarginsIntTok[j], kMarginsIntTok[(j + 1) % nMIT],
                    kMarginsIntTok[(j + 2) % nMIT], kMarginsIntTok[(j + 3) % nMIT]);

        static const double kMarginsFacD[] = {
            0.0, -0.0, 0.5, -0.5, 1.0, -1.0, 1.5, -1.5, 2.0, -2.0, 2147483648.0, 1e-12,
        };
        const int nMFD = countOf(kMarginsFacD);
        for (int i = 0; i < nMI; ++i)
            for (int f = 0; f < countOf(kFacI); ++f)
                for (int g = 0; g < nMFD; ++g)
                    cmp_margins_scale(kMarginsIntHand[i], kMarginsIntHand[(i + 1) % nMI],
                                      kMarginsIntHand[(i + 2) % nMI], kMarginsIntHand[(i + 3) % nMI],
                                      kFacI[f], kMarginsFacD[g]);

        static const double kMarginsFHand[] = {
            0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 1e300, 1e-300, INFINITY, -INFINITY, NAN, 100.0,
        };
        const int nMFH = countOf(kMarginsFHand);
        for (int i = 0; i < nMFH; ++i)
            for (int j = 0; j < nMFH; ++j)
                for (int k = 0; k < nMFH; ++k)
                    for (int l = 0; l < nMFH; ++l)
                        cmp_marginsf_ctor(kMarginsFHand[i], kMarginsFHand[j],
                                          kMarginsFHand[k], kMarginsFHand[l]);

        for (int i = 0; i < nMFH; ++i)
            for (int j = 0; j < nMFH; ++j)
                cmp_marginsf_binary(kMarginsFHand[i], kMarginsFHand[(i + 1) % nMFH],
                                    kMarginsFHand[(i + 2) % nMFH], kMarginsFHand[(i + 3) % nMFH],
                                    kMarginsFHand[j], kMarginsFHand[(j + 1) % nMFH],
                                    kMarginsFHand[(j + 2) % nMFH], kMarginsFHand[(j + 3) % nMFH]);

        for (int i = 0; i < nMFH; ++i)
            for (int g = 0; g < nMFD; ++g)
                cmp_marginsf_scale(kMarginsFHand[i], kMarginsFHand[(i + 1) % nMFH],
                                   kMarginsFHand[(i + 2) % nMFH], kMarginsFHand[(i + 3) % nMFH],
                                   kMarginsFacD[g]);

        // PkRect / PkRectF 的四个互操作成员：手挑矩形 × 手挑 margins。
        static const int kMarginsRectHand[][4] = {
            { 0, 0, 9, 9 }, { -5, -5, 5, 5 }, { 0, 0, -1, -1 }, { INT_MIN, INT_MIN, -1, -1 },
        };
        const int nMRH = countOf(kMarginsRectHand);
        for (int r = 0; r < nMRH; ++r)
            for (int i = 0; i < nMIT; ++i)
                for (int j = 0; j < nMIT; ++j)
                    for (int k = 0; k < nMIT; ++k)
                        for (int l = 0; l < nMIT; ++l)
                            cmp_rect_margins(kMarginsRectHand[r][0], kMarginsRectHand[r][1],
                                             kMarginsRectHand[r][2], kMarginsRectHand[r][3],
                                             kMarginsIntTok[i], kMarginsIntTok[j],
                                             kMarginsIntTok[k], kMarginsIntTok[l]);

        static const double kMarginsRectFHand[][4] = {
            { 0, 0, 10, 10 }, { -5, -5, 5, 5 }, { 0, 0, 0, 0 }, { 1e6, 1e6, 1e6 + 10, 1e6 + 10 },
        };
        const int nMRFH = countOf(kMarginsRectFHand);
        for (int r = 0; r < nMRFH; ++r)
            for (int i = 0; i < nMFH; ++i)
                cmp_rectf_margins(kMarginsRectFHand[r][0], kMarginsRectFHand[r][1],
                                  kMarginsRectFHand[r][2], kMarginsRectFHand[r][3],
                                  kMarginsFHand[i], kMarginsFHand[(i + 1) % nMFH],
                                  kMarginsFHand[(i + 2) % nMFH], kMarginsFHand[(i + 3) % nMFH]);
    }

    // ═══ Polygon 族（R-21 T2）══════════════════════════════════════════════
    {
        cmp_polygon_constants();

        // PkPolygon（int）：几种点数（0/1/3）× 手挑坐标集，覆盖构造 + 迭代。
        static const int kPolyIntHand[] = {
            0, 1, -1, 5, -5, 10, -10, 100, INT_MAX, INT_MIN,
        };
        const int nPI = countOf(kPolyIntHand);
        {
            const int empty[1][2] = { { 0, 0 } };
            cmp_polygon_ctors(empty, 0);
        }
        for (int i = 0; i < nPI; ++i) {
            const int one[1][2] = { { kPolyIntHand[i], kPolyIntHand[(i + 1) % nPI] } };
            cmp_polygon_ctors(one, 1);
        }
        for (int i = 0; i < nPI; ++i)
            for (int j = 0; j < nPI; ++j) {
                const int tri[3][2] = {
                    { kPolyIntHand[i], kPolyIntHand[j] },
                    { kPolyIntHand[(i + 1) % nPI], kPolyIntHand[(j + 2) % nPI] },
                    { kPolyIntHand[(i + 2) % nPI], kPolyIntHand[(j + 1) % nPI] },
                };
                cmp_polygon_ctors(tri, 3);
            }

        // PkPolygonF：默认构造 + explicit int 版，size 0..5。
        for (int s = 0; s <= 5; ++s) cmp_polygonf_constants(s);

        // 手挑多边形形状目录（浮点）：正方形 / 三角形 / 自相交五角星（钉住
        // OddEvenFill 与 WindingFill 在自相交多边形上给出不同答案，见
        // PkPolygon.cpp 顶部注释）/ 共线三点（退化）/ 单点 / 巨大坐标 /
        // 含 NaN・Inf 的坐标——containsPoint/boundingRect/isClosed/toPolygon/
        // translate/map 共用同一份目录。
        static const double kSquare[][2]   = { { 0, 0 }, { 10, 0 }, { 10, 10 }, { 0, 10 } };
        static const double kTriangle[][2] = { { 0, 0 }, { 4, 0 }, { 2, 3 } };
        static const double kStar[][2]     = {
            { 0, -10 }, { 2.35, 3.24 }, { -9.51, -3.09 }, { 9.51, -3.09 }, { -2.35, 3.24 },
        };
        static const double kCollinear[][2]   = { { 0, 0 }, { 1, 0 }, { 2, 0 } };
        static const double kSinglePoint[][2] = { { 5, 5 } };
        static const double kHuge[][2] = { { 0, 0 }, { 1e300, 0 }, { 1e300, 1e300 }, { 0, 1e300 } };
        static const double kNonFinite[][2]   = { { 0, 0 }, { NAN, 0 }, { 1, 1 }, { 0, INFINITY } };

        struct PolyShape { const double (*pts)[2]; int n; };
        const PolyShape kPolyShapes[] = {
            { kSquare, 4 }, { kTriangle, 3 }, { kStar, 5 }, { kCollinear, 3 },
            { kSinglePoint, 1 }, { kHuge, 4 }, { kNonFinite, 4 },
        };
        const int nPS = countOf(kPolyShapes);

        // 空多边形单独测一次（数组大小 0 用不了上面这套聚合写法）。
        cmp_polygonf_ctors(kSquare, 0);
        cmp_polygonf_unary(kSquare, 0);

        for (int s = 0; s < nPS; ++s) {
            cmp_polygonf_ctors(kPolyShapes[s].pts, kPolyShapes[s].n);
            cmp_polygonf_unary(kPolyShapes[s].pts, kPolyShapes[s].n);
        }

        // PkPolygonF(const PkRectF&)：坐标 token 全组合。
        static const double kPolyRectTok[] = {
            -2.0, -0.0, 0.0, 1.0, 5.0, 1e300, INFINITY, -INFINITY, NAN,
        };
        const int nPRT = countOf(kPolyRectTok);
        for (int i = 0; i < nPRT; ++i)
            for (int j = 0; j < nPRT; ++j)
                for (int k = 0; k < nPRT; ++k)
                    for (int l = 0; l < nPRT; ++l)
                        cmp_polygonf_ctor_rect(kPolyRectTok[i], kPolyRectTok[j],
                                               kPolyRectTok[k], kPolyRectTok[l]);

        // translate/translated：手挑形状 × 手挑位移。
        static const double kPolyOffset[] = {
            0.0, -0.0, 1.0, -1.0, 5.0, -5.0, 0.5, -0.5, 1e300, -1e300, INFINITY, NAN,
        };
        const int nPO = countOf(kPolyOffset);
        for (int s = 0; s < nPS; ++s)
            for (int i = 0; i < nPO; ++i)
                cmp_polygonf_translate(kPolyShapes[s].pts, kPolyShapes[s].n,
                                       kPolyOffset[i], -kPolyOffset[i]);

        // containsPoint：手挑形状 × 查询点 token 网格（正方形/三角形/五角星
        // 各自覆盖内部/外部/边界，网格坐标横跨负值到正值），外加每个顶点本身
        // （射线穿越算法在顶点/边界上的行为最容易分家）。
        static const double kPolyQueryTok[] = {
            -20.0, -10.0, -9.51, -5.0, -2.35, -0.0, 0.0, 2.0, 2.35, 5.0, 9.51, 10.0, 20.0,
        };
        const int nPQT = countOf(kPolyQueryTok);
        for (int s = 0; s < nPS; ++s)
            for (int i = 0; i < nPQT; ++i)
                for (int j = 0; j < nPQT; ++j)
                    cmp_polygonf_containspoint(kPolyShapes[s].pts, kPolyShapes[s].n,
                                               kPolyQueryTok[i], kPolyQueryTok[j]);
        for (int s = 0; s < nPS; ++s)
            for (int i = 0; i < kPolyShapes[s].n; ++i)
                cmp_polygonf_containspoint(kPolyShapes[s].pts, kPolyShapes[s].n,
                                           kPolyShapes[s].pts[i][0], kPolyShapes[s].pts[i][1]);
        cmp_polygonf_containspoint(kSquare, 0, 0.0, 0.0);

        // PkTransform::map(PkPolygonF)：手挑矩阵（覆盖三档：<=TxTranslate 快
        // 路径 / 一般仿射 / TxProject——本类与 Qt 登记在案的偏离分支）
        // × 手挑多边形形状目录。
        static const double kPolyTf[][9] = {
            { 1, 0, 0,   0, 1, 0,   0, 0, 1 },            // 单位（TxNone）
            { 1, 0, 0,   0, 1, 0,   5, -3, 1 },           // 纯平移（快路径）
            { 2, 0, 0,   0, 3, 0,   0, 0, 1 },            // 纯缩放
            { 0, 1, 0,  -1, 0, 0,   0, 0, 1 },             // rotate(90) 精确值
            { 1, 2, 0,   1, 1, 0,   0, 0, 1 },             // 切变
            { 1, 0, 0.5, 0, 1, 0.25, 0, 0, 1 },            // 投影（偏离分支，无裁剪需求）
            { 1, 0, -1,  0, 1, 0,   0, 0, 1 },              // 投影且需要透视裁剪（真 Qt
                                                             //   走 QPainterPath，本类不裁）
        };
        const int nPTf = countOf(kPolyTf);
        for (int m = 0; m < nPTf; ++m)
            for (int s = 0; s < nPS; ++s)
                cmp_transform_map_polygonf(kPolyTf[m], kPolyShapes[s].pts, kPolyShapes[s].n);
        {
            const double empty[1][2] = { { 0, 0 } };
            for (int m = 0; m < nPTf; ++m) cmp_transform_map_polygonf(kPolyTf[m], empty, 0);
        }

        // squareToQuad / quadToSquare：手挑四边形（平行四边形 / 透视梯形 /
        // 单位正方形本身 / 共线退化 / 错误点数）。
        static const double kQuadAffine[][2]     = { { 0, 0 }, { 2, 0 }, { 3, 2 }, { 1, 2 } };
        static const double kQuadPersp[][2]      = { { 0, 0 }, { 4, 0 }, { 3, 2 }, { 1, 2 } };
        static const double kQuadUnit[][2]       = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
        static const double kQuadDegenerate[][2] = { { 0, 0 }, { 1, 0 }, { 2, 0 }, { 3, 0 } };
        static const double kQuadWrong3[][2]     = { { 0, 0 }, { 1, 0 }, { 1, 1 } };
        static const double kQuadWrong5[][2] = {
            { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { 0.5, 0.5 },
        };
        cmp_transform_square_quad(kQuadAffine, 4);
        cmp_transform_square_quad(kQuadPersp, 4);
        cmp_transform_square_quad(kQuadUnit, 4);
        cmp_transform_square_quad(kQuadDegenerate, 4);
        cmp_transform_square_quad(kQuadWrong3, 3);
        cmp_transform_square_quad(kQuadWrong5, 5);
    }

    // ═══ VectorND 族（R-21 T3）══════════════════════════════════════════════
    {
        // 手挑 float token 集：覆盖 0 / ±0 / 1 / -1 / 中等值 / 极大量级
        // （float 里 1e19 的平方已溢出，正好钉住 lengthSquared 的 float 累加 vs
        // length 的 double 累加这条不对称）/ 次正规 / inf / NaN。
        static const float kVecTok[] = {
            0.0f, -0.0f, 1.0f, -1.0f, 2.0f, -2.0f, 0.5f, -0.5f, 3.0f, 4.0f,
            1e-6f, 1e19f, -1e19f, 3e38f, -3e38f, 1e-40f, -1e-40f,
            INFINITY, -INFINITY, NAN,
        };
        const int nVT = countOf(kVecTok);
        static const float kVecFac[] = { 0.0f, -0.0f, 1.0f, -1.0f, 2.0f, 0.5f, -0.5f, 1e19f, INFINITY, NAN };
        const int nVF = countOf(kVecFac);

        // PkVector2D：分量 token 组合（含 f 标量）。nVT² 已经覆盖 400 组，
        // 再加 f 轮转不搞全组合（400×10=4000 次，够钉住逐 API 行为）。
        for (int i = 0; i < nVT; ++i)
            for (int j = 0; j < nVT; ++j)
                cmp_vec2d(kVecTok[i], kVecTok[j], kVecTok[(i + 1) % nVT], kVecTok[(j + 1) % nVT],
                          kVecFac[(i + j) % nVF]);

        // PkVector3D：分量 token 组合（三重循环，样本按移位取避免全笛卡尔爆炸）。
        for (int i = 0; i < nVT; ++i)
            for (int j = 0; j < nVT; ++j)
                cmp_vec3d(kVecTok[i], kVecTok[(i + 1) % nVT], kVecTok[(i + 2) % nVT],
                          kVecTok[j], kVecTok[(j + 1) % nVT], kVecTok[(j + 2) % nVT],
                          kVecFac[(i + j) % nVF]);

        // PkVector4D：分量 token 组合（含 w 的透视除法形态——w=0 是
        // toVector2DAffine/toVector3DAffine 的退化分支）。
        for (int i = 0; i < nVT; ++i)
            for (int j = 0; j < nVT; ++j)
                cmp_vec4d(kVecTok[i], kVecTok[(i + 1) % nVT], kVecTok[(i + 2) % nVT], kVecTok[(i + 3) % nVT],
                          kVecTok[j], kVecTok[(j + 1) % nVT], kVecTok[(j + 2) % nVT], kVecTok[(j + 3) % nVT]);
    }

    for (const auto &kv : g_tags)
        std::printf("DIFFTAG %s %ld\n", kv.first.c_str(), kv.second);
    // 分母行。**契约上是新增的第三种行**：run_oracle.sh 只把 DIFFTAG / DIFF
    // 当判据，DIFFDEN 纯粹是给人读的（"命中 N 次里分家 M 次"），
    // 加它不影响既有的两条契约。只为**出现过差异**的 tag 打，不是全量。
    for (const auto &kv : g_tags)
        std::printf("DIFFDEN %s %ld\n", kv.first.c_str(), g_tag_seen[kv.first]);
    // 规则三的机器闸门：见 g_apis 上方那段。run_oracle.sh 拿这批行与
    // oracle/api_seen.expected、oracle/rect_api.map 三向核对。
    for (const auto &a : g_apis)
        std::printf("APISEEN %s\n", a.c_str());
    std::printf("DIFF total=%ld mismatch=%ld\n", g_total, g_mismatch);
    return 0;   // 已声明的偏离不算失败；判定在 run_oracle.sh
}
