#include "PkGlobal.h"

#include <cmath>
#include <limits>

// Qt 5.15.7：qnumeric.h 只声明，实现走 qnumeric.cpp → qnumeric_p.h 的
// qt_is_nan / qt_inf，那两个就是 std::isnan 与
// std::numeric_limits<double>::infinity()。非 inline 是 Qt 的形态，照抄——
// 见 PkGlobal.h 里这两项上方的说明。

bool qIsNaN(double d)
{
    return std::isnan(d);
}

double qInf()
{
    return std::numeric_limits<double>::infinity();
}
