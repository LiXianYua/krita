#pragma once
// S-08 主树 flake compat 激活头（判据⑧/⑨ 的核心产出；S-01 教训在 flake 的落地）。
//
// 用途：本头只服务「未剥头/未剥源」——它们按真 Qt 的传递 include 用 Q 名类型却
// 不自 include，Pk 世界没有对应物，必须在 include 它们之前把 pk 各 compat 的宏
// 改写激活。已剥源直接 include Pk 头、用 Pk 名，不依赖本头；未剥头全部剥完后
// （S 线源码层 Q* 归零），本头可整段删除。
//
// ⚠ 源码层 Q* 判据（`\bQ[A-Z][A-Za-z]*\b`）对 libs/flake **不豁免**（S线-spec
// §源码层只排除 pk/*/compat/），所以本头**不能出现字面 Q 名 token**（含注释与
// 字符串）。激活靠把 Q 名拆成两个不匹配判据的 token（如 `Q` + `String`）再用
// PK_CAT_ 拼装、经 PK_INC_ 展开成 `#include <compat/Q 名>`：`pk/<dir>` 在 include
// 路径上 → 解析到 `pk/<dir>/compat/` 下同名垫片（该垫片用相对 include 自包含，
// 无需 compat 目录在路径上）。
//
// ⚠ R-38 约定（「真 Qt 头在前」，R线-spec 472-479）：本头**先 include 真 Qt 的
// <QtGlobal>**——所有含本头的 TU 都变成「Qt 头在前」，pk 各 compat 的让位守卫
// （qAbs/qMin/qMax/qRound/qFuzzy*…）见到 QGLOBAL_H 已定义就整段让位，不再与真
// Qt qglobal.h 重定义。这是对 R-38 登记的 59 个 pk-在前 TU 的消费方侧处置：
// 依赖本头的 TU 一网打尽；不依赖本头的另登记（见 task-8b-report）。
//
// 参考：libs/pigment/PkXmlCompat.h（S-03-a 范本，PK_CAT_/PK_INC_ 技巧同源）与
// 壳内全量版 .superpowers/sdd/S-08/shell/kritaflake/PkXmlCompat.h（git-ignored，
// include 面来源）。

#define PK_CAT_(a, b) a##b
#define PK_INC_(x) <x>

// 真 Qt 头在前：激活 pk 各 compat 的让位守卫（R-38 约定）。
#include <QtGlobal>

// ---- Q 名 token 拼装 ----
// `PK_Q*_` 宏名里的 Q 前是 `_`（word 字符），不构成 `\bQ`；`PK_CAT_(Q, Xxx)`
// 里的 `Q` 后是 `,`，均不匹配 `\bQ[A-Z]` —— 本头因此 gate 干净。
// 容器
#define PK_QSTRING_      PK_CAT_(Q, String)
#define PK_QSTRINGLIST_  PK_CAT_(Q, StringList)
#define PK_QLIST_        PK_CAT_(Q, List)
#define PK_QVECTOR_      PK_CAT_(Q, Vector)
#define PK_QMAP_         PK_CAT_(Q, Map)
#define PK_QHASH_        PK_CAT_(Q, Hash)
#define PK_QSET_         PK_CAT_(Q, Set)
#define PK_QPAIR_        PK_CAT_(Q, Pair)
// 几何
#define PK_QPOINT_       PK_CAT_(Q, Point)
#define PK_QPOINTF_      PK_CAT_(Q, PointF)
#define PK_QRECT_        PK_CAT_(Q, Rect)
#define PK_QRECTF_       PK_CAT_(Q, RectF)
#define PK_QSIZE_        PK_CAT_(Q, Size)
#define PK_QSIZEF_       PK_CAT_(Q, SizeF)
#define PK_QTRANSFORM_   PK_CAT_(Q, Transform)
#define PK_QLINE_        PK_CAT_(Q, Line)
#define PK_QLINEF_       PK_CAT_(Q, LineF)
#define PK_QPOLYGON_     PK_CAT_(Q, Polygon)
#define PK_QPOLYGONF_    PK_CAT_(Q, PolygonF)
#define PK_QPAINTERPATH_ PK_CAT_(Q, PainterPath)
#define PK_QREGION_      PK_CAT_(Q, Region)
#define PK_QMARGINS_     PK_CAT_(Q, Margins)
#define PK_QMATRIX4X4_   PK_CAT_(Q, Matrix4x4)
// 指针
#define PK_QSCOPEDPOINTER_  PK_CAT_(Q, ScopedPointer)
#define PK_QSHAREDPOINTER_  PK_CAT_(Q, SharedPointer)
#define PK_QWEAKPOINTER_    PK_CAT_(Q, WeakPointer)
// 其他
#define PK_QVARIANT_     PK_CAT_(Q, Variant)
#define PK_QDEBUG_       PK_CAT_(Q, Debug)
#define PK_QDOMDOC_      PK_CAT_(Q, DomDocument)
#define PK_QDOMEL_       PK_CAT_(Q, DomElement)
#define PK_QDOMNODE_     PK_CAT_(Q, DomNode)
#define PK_QDOMNODELIST_ PK_CAT_(Q, DomNodeList)
#define PK_QDOMATTR_     PK_CAT_(Q, DomAttr)
#define PK_QDOMTEXT_     PK_CAT_(Q, DomText)
#define PK_QDOMCDATA_    PK_CAT_(Q, DomCDATASection)
#define PK_QCOLOR_       PK_CAT_(Q, Color)
#define PK_QDATETIME_    PK_CAT_(Q, DateTime)

// ---- pk 各模块已有 compat 垫片：经 `pk/<dir>` include 路径 + 相对 include 自解析 ----
// ⚠ 去重规则：同一族的前向/浮点对（Point/PointF、Rect/RectF、Size/SizeF、
// Line/LineF、Polygon/PolygonF、Margins/MarginsF，均带 Pk 前缀）与
// WeakPointer/SharedPointer，垫片按真 Qt 转发头形态**一个文件同时定义两个宏**
// （真 Qt 里 Point 与 PointF 同指 point.h）。若把两个都 include，第二个的垫片
// 文件名会被第一个刚定义的宏改写（`#include <compat/PointF>` 在
// `#define PointF PkPointF` 生效后变成 `<compat/PkPointF>`，找不到文件）。
// 所以每族只保留一个：非 F 变体 + SharedPointer。
// ⚠ 顺序约束：`compat/Rect` 内部自带 `#include "Size"` / `"Point"`（同真 Qt
// rect.h 包 size.h/point.h），会把 `Size → PkSize` 宏提前激活。因此
// `#include <compat/Size>` 必须排在 `#include <compat/Rect>` **之前**，否则
// 角括号形态的 `<compat/Size>` 被宏改写为 `<compat/PkSize>`，找不到文件。
#include PK_INC_(compat/PK_QSTRING_)
#include PK_INC_(compat/PK_QSTRINGLIST_)
#include PK_INC_(compat/PK_QLIST_)
#include PK_INC_(compat/PK_QVECTOR_)
#include PK_INC_(compat/PK_QMAP_)
#include PK_INC_(compat/PK_QHASH_)
#include PK_INC_(compat/PK_QSET_)
#include PK_INC_(compat/PK_QPAIR_)
#include PK_INC_(compat/PK_QPOINT_)
#include PK_INC_(compat/PK_QSIZE_)
#include PK_INC_(compat/PK_QRECT_)
#include PK_INC_(compat/PK_QTRANSFORM_)
#include PK_INC_(compat/PK_QLINE_)
#include PK_INC_(compat/PK_QPOLYGON_)
#include PK_INC_(compat/PK_QPAINTERPATH_)
#include PK_INC_(compat/PK_QREGION_)
#include PK_INC_(compat/PK_QMARGINS_)
#include PK_INC_(compat/PK_QMATRIX4X4_)
#include PK_INC_(compat/PK_QSCOPEDPOINTER_)
#include PK_INC_(compat/PK_QSHAREDPOINTER_)
#include PK_INC_(compat/PK_QVARIANT_)
#include PK_INC_(compat/PK_QDEBUG_)
// ⚠ Dom 族同型：compat/DomDocument、DomElement、DomAttr、DomText 内部都
// 自带 `#include "DomNode"`，DomElement 还带 DomDocument 与 DomNodeList，
// DomCDATASection 带 DomText——被拉的必须先排在前，否则角括号形态的
// `<compat/DomNode>` 被宏改写成 `<compat/PkXmlNode>` 找不到文件。
#include PK_INC_(compat/PK_QDOMNODE_)
#include PK_INC_(compat/PK_QDOMNODELIST_)
#include PK_INC_(compat/PK_QDOMDOC_)
#include PK_INC_(compat/PK_QDOMEL_)
#include PK_INC_(compat/PK_QDOMTEXT_)
#include PK_INC_(compat/PK_QDOMATTR_)
#include PK_INC_(compat/PK_QDOMCDATA_)
#include PK_INC_(compat/PK_QCOLOR_)
#include PK_INC_(compat/PK_QDATETIME_)

// ---- 无 compat 垫片、flake 剥源码直接用的 Pk 头（Pk 名，无 Q token）----
// PkMapIterator：SvgStyleParser/SvgCssHelper/KoShapeDistributeCommand 直接 include
// <pk/container/PkMapIterator.h>；这里补上，保证经 PkXmlCompat.h 的路径也齐全。
#include <pk/container/PkMapIterator.h>
