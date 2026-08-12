#pragma once
// 试接垫片 —— 不是 R-03 的交付物。
//
// 调用点写的是 `#include <QtCore/qmath.h>`（libs/global/KisRectsGrid.cpp:9 与
// libs/global/kis_lod_transform_base.h:8），带 QtCore/ 前缀，所以垫片也要放在
// 同名子目录下才命中。
//
// 归属：qmath.h 那一整套（qSin/qCos/qPow/qLn/qSqrt/qDegreesToRadians …）在
// `docs/Qt替代品选型.md` 的口径里属于标量数学工具面。R-03 的 PkGlobal.h 只做了
// 实测有调用点的那十项（qreal/qAbs/qMin/qMax/qBound/qRound/qFuzzyCompare/
// qFuzzyIsNull/qIsNaN/qInf），**qFloor / qCeil 不在其中**——它们是 qmath.h 的
// 东西，不是 qglobal.h 的。这里只补两个试接真正用到的：
//   · qFloor —— KisRectsGrid.cpp:20、kis_lod_transform_base.h 的 scaleToLod
//   · qCeil  —— 同一个头里 lodToScale 邻近路径，一并带上免得漏
//
// 两个函数体**逐字抄自真 Qt 5.15.7**
// （/mnt/ssd-disk/liyang/projects/krita-ci-env/_install/include/QtCore/qmath.h:68-78），
// 连 `using std::floor;` 那两行也照抄 —— 那不是花样，是 Qt 让 float 实参走
// std::floor(float) 重载的写法。
#include <QtGlobal>
#include <cmath>

inline int qCeil(qreal v)
{
    using std::ceil;
    return int(ceil(v));
}

inline int qFloor(qreal v)
{
    using std::floor;
    return int(floor(v));
}
