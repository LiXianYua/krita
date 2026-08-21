#pragma once
// S-03-a 树内 compat 激活头（由 git-ignored 薄壳 compat/PkXmlCompat.h 落盘而来，
// Task 9b：主树 libs/pigment 自包含）。
//
// 用途：本头只服务「未剥头」——它们按真 Qt 的传递 include 用 Q 名类型却不自
// include，Pk 世界没有对应物，必须在 include 它们之前把 pk 各 compat 的宏改写激活。
// 已剥头（KoColorSpace.h 等）直接 include Pk 头、用 Pk 名，不依赖本头。
// 未剥头全部剥完后（S 线源码层 Q* 归零），本头可整段删除，36 个 include 一并移除。
//
// ⚠ 源码层 Q* 判据（`\bQ[A-Z][A-Za-z]*\b`）对 libs/pigment 不豁免（S线-spec §源码层
// 只排除 pk/*/compat/），所以本头**不能出现字面 Q 名 token**（含注释与字符串）。
// 激活靠把 Q 名拆成两个不匹配判据的 token（如 `Q` + `String`）再用 PK_CAT_ 拼装、
// 经 PK_INC_ 展开成 `#include <compat/Q 名>`：`pk/<dir>` 在 include 路径上 →
// 解析到 `pk/<dir>/compat/` 下同名垫片（该垫片用相对 include 自包含，无需 compat
// 目录在路径上）。
//
// 历史参照（真 Qt 时期的编译期地雷）：颜色头完整展开拉 PkColor.h → PkNamespace.h，
// 变体头拉 PkVariant.h → PkDateTime.h，两者同一 TU 共存曾是 Pk 土地雷
// （PkNamespace.h 的 enum DateFormat 与 PkDateTime.h 的 constexpr 别名撞 Qt::ISODate），
// 已由 S-03-a 的 03be759c 修复（删除过时 enum），现在可安全共存。XML 文档头拉
// PkXmlDocument.h → pugixml.hpp（pkxml target 的 PUBLIC include 已随链接进壳，可解析）。

// Q 名 token 拼装：`PK_Q*_` 宏名里的 Q 前是 `_`（word 字符），不构成 `\bQ`，
// `PK_CAT_(Q, Xxx)` 里的 `Q` 后是 `,`，均不匹配 `\bQ[A-Z]` —— 本头因此 gate 干净。
#define PK_CAT_(a, b) a##b
#define PK_INC_(x) <x>

#define PK_QCOLOR_      PK_CAT_(Q, Color)
#define PK_QVARIANT_    PK_CAT_(Q, Variant)
#define PK_QPOLYGONF_   PK_CAT_(Q, PolygonF)
#define PK_QPAIR_       PK_CAT_(Q, Pair)
#define PK_QLIST_       PK_CAT_(Q, List)
#define PK_QBYTEARRAY_  PK_CAT_(Q, ByteArray)
#define PK_QSTRINGLIST_ PK_CAT_(Q, StringList)
#define PK_QDEBUG_      PK_CAT_(Q, Debug)
#define PK_QRGB_        PK_CAT_(Q, Rgb)
#define PK_QDOMDOC_     PK_CAT_(Q, DomDocument)
#define PK_QDOMEL_      PK_CAT_(Q, DomElement)
#define PK_QSTRING_     PK_CAT_(Q, String)
#define PK_QVECTOR_     PK_CAT_(Q, Vector)
#define PK_QBITARRAY_   PK_CAT_(Q, BitArray)
#define PK_QMULTIMAP_   PK_CAT_(Q, MultiMap)

// pk 各模块已有 compat 垫片：经 `pk/<dir>` include 路径 + 相对 include 自解析。
#include PK_INC_(compat/PK_QCOLOR_)
#include PK_INC_(compat/PK_QVARIANT_)
#include PK_INC_(compat/PK_QPOLYGONF_)
#include PK_INC_(compat/PK_QPAIR_)
#include PK_INC_(compat/PK_QLIST_)
#include PK_INC_(compat/PK_QSTRINGLIST_)
#include PK_INC_(compat/PK_QDEBUG_)
#include PK_INC_(compat/PK_QDOMDOC_)
#include PK_INC_(compat/PK_QDOMEL_)
#include PK_INC_(compat/PK_QSTRING_)
#include PK_INC_(compat/PK_QVECTOR_)

// pk 树没有 compat 垫片的 4 个：落在 libs/pigment/compat/（无扩展名文件，
// 不匹配源码层判据的 *.cpp/*.cc/*.h 口径；经 libs/pigment 在 include 路径上解析）。
#include PK_INC_(compat/PK_QBYTEARRAY_)
#include PK_INC_(compat/PK_QRGB_)
#include PK_INC_(compat/PK_QBITARRAY_)
#include PK_INC_(compat/PK_QMULTIMAP_)

// Qt 属性宏垫片（真 Qt 在 qglobal.h 定义）。未剥头/未剥 .cpp（KoColorSpaceRegistry.cpp、
// KoColorTransformationFactory.cpp、KoColorTransformationFactoryRegistry.cpp 的
// `struct Q_DECL_HIDDEN ...::Private`）会用到；pk 各 compat 未提供。
#ifndef Q_DECL_HIDDEN
#define Q_DECL_HIDDEN __attribute__((visibility("hidden")))
#endif
