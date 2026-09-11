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
