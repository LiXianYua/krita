// R-87：PkRect 的 `std::ostream` 插入运算符 —— PK_COMPARE 判红时给「实际值」。
//
// **为什么在这里（类型所有者）而不是 pk/test**：pk/test/PkTestCompare.h 的设计
// 就是「SFINAE 检测 ostream 可插入性，不需要为每个类型写重载」（该文件 :77-82
// 的注释），所以值文本的**生产者**落类型所有者、`pk/test` 一个字节都不用改。
//
// **文本来源** = 真 Qt 5.15.7 `QtTest/qtest.h:168-173`：
//   template<> inline char *toString(const QRect &s)
//   { qsnprintf(msg, …, "QRect(%d,%d %dx%d) (bottomright %d,%d)",
//               s.left(), s.top(), s.width(), s.height(), s.right(), s.bottom()); }
// **只把类型名换成 PkRect**（登记偏离，见 pk/geometry/README.md「偏离登记」）。
//
// ⚠ 与 pk/geometry/PkGeometryDebug.cpp:107 的 `PkDebug operator<<(PkDebug, const
//   PkRect&)` **是两条通道、两种文本**（那条无 bottomright，形如 `PkRect(1,2 3x4)`）：
//   真 Qt 本身就是 `QTest::toString`（测试库，逐类型特化）与 `QDebug`（类型头）两条
//   通道、文本不同（探针① 结论 3）。**不许把两者「统一」**。
//   两条通道互不干扰是实测结论：`dbg << rect` 仍然选中 PkDebug 那条自由运算符
//   （探针③ [b]，无二义），本运算符只被 `std::ostream` / `pkTestToString` 那条通道选中。
#include "PkRect.h"

#include <ostream>

std::ostream &operator<<(std::ostream &os, const PkRect &r)
{
    return os << "PkRect(" << r.left() << "," << r.top() << " " << r.width() << "x"
              << r.height() << ") (bottomright " << r.right() << "," << r.bottom() << ")";
}
