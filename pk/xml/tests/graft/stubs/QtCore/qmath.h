#pragma once
#include <cmath>

// `<QtCore/qmath.h>` 模块路径垫片 —— 不是功能占位，是真实转发实现（用
// `<cmath>` 对应函数，`qreal` 即 `double`，语义与真 Qt `qmath.h` 一致）。
//
// 需要这份垫片的根因：候选 B（kis_distance_information.cpp:13
// `#include <QtCore/qmath.h>`）与试接脚本的 `-I $BOOST_INC`
// （`krita-ci-env/_install/include`）指向**同一个前缀**——那个前缀下真的装着
// 一份完整 Qt 5.15（因为 boost 头文件与本机 Qt 运行时共用一个 CI 安装
// 前缀），所以 `<QtCore/qmath.h>` 若不被本文件抢先命中，会解析到真
// `qglobal.h`，与 `pk/geometry/compat/QtGlobal`/`pk/test/compat/QtGlobal`
// 已经定义过的 `qAbs`/`qRound`/`qMin`/`qMax` 等重复定义报错（`-I $STUBS`
// 排在最前，本文件因此优先命中）。
//
// 只实现真实调用点用到的一个子集（`qSqrt`/`qCos`/`qSin`/`qAtan2`/`qFloor`/
// `qCeil`），够 `kis_distance_information.cpp`/`kis_lod_transform_base.h`
// 编译期通过即可——`testInitInfoXMLClone` 不会真正求值这几个函数（它们只出现
// 在 `KisDistanceInformation`/`KisLodTransformBase` 的插值/坐标变换方法体里，
// 该测试用例只经过 `KisDistanceInitInfo::toXML`/`fromXML`），但既然实现成本
// 与写占位一样，直接照 `<cmath>` 语义转发，不留假结果。
using qreal = double;

inline int qCeil(qreal v) { return int(std::ceil(v)); }
inline int qFloor(qreal v) { return int(std::floor(v)); }
inline qreal qFabs(qreal v) { return std::fabs(v); }
inline qreal qSin(qreal v) { return std::sin(v); }
inline qreal qCos(qreal v) { return std::cos(v); }
inline qreal qTan(qreal v) { return std::tan(v); }
inline qreal qAcos(qreal v) { return std::acos(v); }
inline qreal qAsin(qreal v) { return std::asin(v); }
inline qreal qAtan(qreal v) { return std::atan(v); }
inline qreal qAtan2(qreal y, qreal x) { return std::atan2(y, x); }
inline qreal qSqrt(qreal v) { return std::sqrt(v); }
inline qreal qLn(qreal v) { return std::log(v); }
inline qreal qExp(qreal v) { return std::exp(v); }
inline qreal qPow(qreal x, qreal y) { return std::pow(x, y); }
