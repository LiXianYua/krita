#pragma once

// PkLine / PkLineF 的单测类。
//
// 生成器 pk_test_moc.py 只扫 .h，且只认类体里字面出现的 "Q_OBJECT" 与
// "private Q_SLOTS:"，所以这两个 token 必须真的出现在本文件里。
// 本地等价展开、不走 pk/test/compat/QObject 的理由与 rect_case.h 相同。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../../test/PkTest.h"

class PkLineCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    // ── PkLine（int，最小面）──
    void lineDefaultAndFourCtors();
    void lineImplicitPromotionToLineF();

    // ── PkLineF 构造与布局 ──
    void lineFDefaultCtorIsAllZero();
    void lineFFourConstructors();
    void lineFLayoutIsFourQreal();

    // ── 取值器 ──
    void lineFAccessorsX1Y1X2Y2();
    void lineFDxDy();
    void lineFIsNullIsFuzzy();

    // ── length / setLength ──
    void lineFLength();
    void lineFSetLength();
    void lineFSetLengthOnNullLine();

    // ── angle / setAngle / angleTo（out-of-line，靠对拍/差分脚本逼出来的公式）──
    void lineFAngleCardinalDirections();
    void lineFAngleOfNullLineIsNegativeZero();
    void lineFSetAngle();
    void lineFAngleTo();
    void lineFAngleToWithNullLineIsZero();

    // ── unitVector / normalVector ──
    void lineFUnitVector();
    void lineFNormalVector();

    // ── translate / translated ──
    void lineFTranslateAndTranslated();

    // ── pointAt：不夹持 t ──
    void lineFPointAtExtrapolates();

    // ── center（任务清单漏查、实测有真实调用点的一项）──
    void lineFCenterIsMidpoint();

    // ── setP1 / setP2 ──
    void lineFSetP1SetP2();

    // ── intersects：三种 IntersectType ──
    void lineFIntersectsBounded();
    void lineFIntersectsUnbounded();
    void lineFIntersectsParallelIsNone();
    void lineFIntersectsAcceptsNullptr();

    // ── fromPolar（任务清单漏查、实测有真实调用点的一项）──
    void lineFFromPolar();

    // ── operator== / != ──
    void lineFEqualityIsFuzzy();

    // ── 跨切面 ──
    void lineFNoexceptSurfaceMatchesQt();
};

#undef Q_SLOTS
#undef Q_OBJECT
