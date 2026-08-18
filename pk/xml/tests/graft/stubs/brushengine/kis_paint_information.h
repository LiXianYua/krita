#pragma once

// 编译期占位 —— 整个 KisPaintInformation（`libs/image/brushengine/`）不在
// pk/xml（本任务）范围内，也没有任何 R 任务认领（它是画笔引擎的核心值类型，
// 真身拖着 KisRandomSource/KisPerStrokeRandomSource/QRandomGenerator/
// boost::random/QMutex/QHash 一整棵与 XML 无关的依赖树）。
//
// 候选 B（kis_distance_information.cpp + kis_distance_information_test.cpp）
// 用它的地方：
//   · `KisDistanceInformation::Private::lastPaintInformation` 值成员
//     （需要完整类型才能确定 Private 的大小/生成默认构造）；
//   · `registerPaintedDab(const KisPaintInformation&, ...)`/
//     `lockCurrentDrawingAngle(const KisPaintInformation&) const` 两个方法体
//     里调用 `info.pos()`/`info.pressure()`/`info.drawingAngle(false)`；
//   · `kis_lod_transform.h`（真品）的
//     `KisPaintInformation map(KisPaintInformation pi) const` 内联方法体
//     调用 `pi.pos()`/`pi.setPos()`/`pi.setLevelOfDetail()`；
//   · **测试文件本身**（`kis_distance_information_test.cpp`）的
//     `testInterpolation()`（**不是** brief 要求验证的
//     `testInitInfoXMLClone`，那是 `testInitInfo()` slot 内部的私有方法）
//     真的构造 `KisPaintInformation p1(startPos, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0,
//     startTime, 0.0)` 这样的 9 参数重载，并调用 `p1.currentTime()`。
//
// 以上全部只需要在编译期类型检查通过——本试接的 `graft_run_b.sh` 用 pk/test
// harness 的命令行过滤（`PkTest::execPlan` 的 `selected(filters, fn.name)`,
// 见 pk/test/PkTestRunner.cpp）只跑 `testInitInfo` 这一个 slot（覆盖
// `testInitInfoEquality`/`testInitInfoXMLClone` 两个私有方法，即 brief 要求
// 的判据②），**不跑** `testInterpolation`——那个 slot 依赖的插值数学需要
// `KisPaintInformation`/`KisAlgebra2D` 的真实行为，本占位只保证它能编译，
// 不保证数值正确，`testInterpolation` 从未被本试接执行到。README.md
// 「已知偏离清单」已记录这一处比候选 A 更深的占位——占位对象是一整个 Krita
// 生产类型，不是纯 Qt/KDE 类型。
//
// 构造函数签名逐字照抄真品声明（`libs/image/brushengine/kis_paint_information.h`
// 74-97 行），是为了让测试文件里那几处**未被执行到但仍需类型检查通过**的
// 调用点能正确决议重载，不是随意凑的参数表。唯一偏离：真品第三个构造函数的
// `pressure` 默认值是 `PRESSURE_DEFAULT`（定义在 `kis_global.h`，本 .cpp 里
// 直到 `#include "kis_algebra_2d.h"` 才可见，晚于本头文件被处理的时间点），
// 这里改用字面量 `1.0`——没有任何真实调用点依赖这个默认值（本试接范围内的
// 调用全部显式传参）。
class KisPaintInformation {
public:
    KisPaintInformation(const QPointF &pos,
                        qreal pressure,
                        qreal xTilt,
                        qreal yTilt,
                        qreal rotation,
                        qreal tangentialPressure,
                        qreal perspective,
                        qreal time,
                        qreal speed)
        : _pos(pos), _pressure(pressure), _time(time)
    {
        (void)xTilt; (void)yTilt; (void)rotation;
        (void)tangentialPressure; (void)perspective; (void)speed;
    }

    KisPaintInformation(const QPointF &pos,
                        qreal pressure,
                        qreal xTilt,
                        qreal yTilt,
                        qreal rotation)
        : _pos(pos), _pressure(pressure)
    {
        (void)xTilt; (void)yTilt; (void)rotation;
    }

    KisPaintInformation(const QPointF &pos = QPointF(), qreal pressure = 1.0)
        : _pos(pos), _pressure(pressure)
    {
    }

    KisPaintInformation(const KisPaintInformation &) = default;
    void operator=(const KisPaintInformation &rhs) { _pos = rhs._pos; _pressure = rhs._pressure; _time = rhs._time; }
    ~KisPaintInformation() = default;

    const QPointF &pos() const { return _pos; }
    void setPos(const QPointF &pos) { _pos = pos; }

    qreal pressure() const { return _pressure; }
    qreal drawingAngle(bool considerLockedAngle = false) const { (void)considerLockedAngle; return 0.0; }
    qreal currentTime() const { return _time; }

    void setLevelOfDetail(int) {}

private:
    QPointF _pos;
    qreal _pressure = 0.0;
    qreal _time = 0.0;
};
