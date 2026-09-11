// pk/geometry 的调试流运算符 —— QDebug operator<< 的零 Qt 对应物。
//
// 落位依据（四条，任一条单独成立都指向 pk/geometry）：① 真 Qt 自己就放在类型自己的
// 头里（qpoint.h:418 的 Q_CORE_EXPORT QDebug operator<<(QDebug, const QPointF&)）；
// ② pk/log/PkDebug.h 明确「不 include 也不链接 pk/string」，把几何类型的运算符放进
// pk/log 是让最低层反向认识上层值类型；③ 既有先例 libs/resources/KisTag.h:113 与
// libs/global/kis_pinned_shared_ptr.h:62 都是类型所有者自己定义；
// ④ R-08 的 plan 就把这条路写成设计（「96 处用户重载全靠它」）。
//
// 输出格式 = 真 Qt 5.15.7 原文，**只有类型名用 pk 自己的名字**（登记偏离，见
// pk/geometry/README.md）。真 Qt 的原文取证命令在 oracle/debugstream_qt.cpp 文件头。
//
// ⚠ **整串一次插入，不要分量逐个流**。真 Qt 用 QDebugStateSaver 保存/恢复 space
// 标志；PkDebug 没有这个设施，且它的 space() 会吐一个分隔符、nospace() 不吐。
// 「nospace() 开头 + space() 结尾」的写法在 nospace 上下文里会多吐一个空格
//（实测反例：qDebug().nospace() << "Z" << QPoint(0,0) << QPointF(0,0) 真 Qt 是
// ZQPoint(0,0)QPointF(0,0)）。一次插入不碰任何流状态，净效果与 QDebugStateSaver 等价。
#include "PkPoint.h"
#include "PkSize.h"
#include "PkRect.h"
#include "PkLine.h"
#include "PkMargins.h"
#include "PkPolygon.h"
#include "PkTransform.h"

#include "PkDebug.h"

#include <sstream>
#include <string>

namespace {

// 与 PkDebug 内部的 std::ostringstream 默认格式一致（precision 6 / general）。
template <typename T>
std::string num(const T &v)
{
    std::ostringstream os;
    os << v;
    return os.str();
}

// ⚠ Point/Rect/Line 家族分量间无空格，Size/Margins 家族有一个空格 ——
// 这是真 Qt 的原文，不是笔误，**不许"统一"**。
std::string str(const PkPoint &p)    { return "PkPoint(" + num(p.x()) + "," + num(p.y()) + ")"; }
std::string str(const PkPointF &p)   { return "PkPointF(" + num(p.x()) + "," + num(p.y()) + ")"; }
std::string str(const PkSize &s)     { return "PkSize(" + num(s.width()) + ", " + num(s.height()) + ")"; }
std::string str(const PkSizeF &s)    { return "PkSizeF(" + num(s.width()) + ", " + num(s.height()) + ")"; }
std::string str(const PkRect &r)     { return "PkRect(" + num(r.x()) + "," + num(r.y()) + " " + num(r.width()) + "x" + num(r.height()) + ")"; }
std::string str(const PkRectF &r)    { return "PkRectF(" + num(r.x()) + "," + num(r.y()) + " " + num(r.width()) + "x" + num(r.height()) + ")"; }
std::string str(const PkMargins &m)  { return "PkMargins(" + num(m.left()) + ", " + num(m.top()) + ", " + num(m.right()) + ", " + num(m.bottom()) + ")"; }
std::string str(const PkMarginsF &m) { return "PkMarginsF(" + num(m.left()) + ", " + num(m.top()) + ", " + num(m.right()) + ", " + num(m.bottom()) + ")"; }

std::string str(const PkLine &l)  { return "PkLine(" + str(l.p1()) + "," + str(l.p2()) + ")"; }
std::string str(const PkLineF &l) { return "PkLineF(" + str(l.p1()) + "," + str(l.p2()) + ")"; }

// 元素之间**一个分隔符都没有**（真 Qt 走 dbg.nospace()）。
std::string str(const PkPolygon &p)
{
    std::string out = "PkPolygon(";
    for (int i = 0; i < p.size(); ++i) out += str(p.at(i));
    return out + ")";
}
std::string str(const PkPolygonF &p)
{
    std::string out = "PkPolygonF(";
    for (int i = 0; i < p.size(); ++i) out += str(p.at(i));
    return out + ")";
}

// PkTransform 的档位名 —— 真 Qt 的 QTransform 调试运算符打出来的就是这六个枚举名
// （原文见 oracle/debugstream_qt.cpp 的 8 行 QTransform 探针）。
// switch **写全、不写 default** 是刻意的：将来枚举加了值，-Wswitch 会在编译期逮到，
// 而不是静默打成别的名字。
const char *typeName(PkTransform::TransformationType t)
{
    switch (t) {
    case PkTransform::TxNone:      return "TxNone";
    case PkTransform::TxTranslate: return "TxTranslate";
    case PkTransform::TxScale:     return "TxScale";
    case PkTransform::TxRotate:    return "TxRotate";
    case PkTransform::TxShear:     return "TxShear";
    case PkTransform::TxProject:   return "TxProject";
    }
    return "<unknown>";
}

// 格式：`PkTransform(type=<档位名>, 11=<a> 12=<b> 13=<c> 21=<d> 22=<e> 23=<f>
// 31=<g> 32=<h> 33=<i>)` —— `type=` 之后是 `, `（逗号+空格），九个分量之间**一个空格**。
// ⚠ `type()` 是惰性的（PkTransform.h 文件头「惰性缓存」）：调用它会就地重算并改写
// m_type/m_dirty。**只调一次**存进局部变量，别在拼接里重复调用。
std::string str(const PkTransform &t)
{
    const PkTransform::TransformationType ty = t.type();
    return "PkTransform(type=" + std::string(typeName(ty))
        + ", 11=" + num(t.m11()) + " 12=" + num(t.m12()) + " 13=" + num(t.m13())
        + " 21=" + num(t.m21()) + " 22=" + num(t.m22()) + " 23=" + num(t.m23())
        + " 31=" + num(t.m31()) + " 32=" + num(t.m32()) + " 33=" + num(t.m33())
        + ")";
}

} // namespace

PkDebug operator<<(PkDebug dbg, const PkPoint &v)    { return dbg << str(v).c_str(); }
PkDebug operator<<(PkDebug dbg, const PkPointF &v)   { return dbg << str(v).c_str(); }
PkDebug operator<<(PkDebug dbg, const PkSize &v)     { return dbg << str(v).c_str(); }
PkDebug operator<<(PkDebug dbg, const PkSizeF &v)    { return dbg << str(v).c_str(); }
PkDebug operator<<(PkDebug dbg, const PkRect &v)     { return dbg << str(v).c_str(); }
PkDebug operator<<(PkDebug dbg, const PkRectF &v)    { return dbg << str(v).c_str(); }
PkDebug operator<<(PkDebug dbg, const PkLine &v)     { return dbg << str(v).c_str(); }
PkDebug operator<<(PkDebug dbg, const PkLineF &v)    { return dbg << str(v).c_str(); }
PkDebug operator<<(PkDebug dbg, const PkMargins &v)  { return dbg << str(v).c_str(); }
PkDebug operator<<(PkDebug dbg, const PkMarginsF &v) { return dbg << str(v).c_str(); }
PkDebug operator<<(PkDebug dbg, const PkPolygon &v)  { return dbg << str(v).c_str(); }
PkDebug operator<<(PkDebug dbg, const PkPolygonF &v) { return dbg << str(v).c_str(); }
PkDebug operator<<(PkDebug dbg, const PkTransform &v) { return dbg << str(v).c_str(); }
