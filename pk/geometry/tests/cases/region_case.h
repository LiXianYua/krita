#pragma once

// PkRegion 的单测类。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../../test/PkTest.h"

class PkRegionCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    // ── 构造 / 空 ──
    void defaultCtorIsEmpty();
    void ctorFromRect();

    // ── 覆盖谓词 ──
    void isEmptyAndIsNull();
    void boundingRect();
    void containsPoint();
    void containsRectCoverage();

    // ── 并 / 差 / 交 ──
    void unionRect();
    void subtractRect();
    void intersectRect();
    void unionRegion();
    void subtractRegion();
    void xorRegion();

    // ── 迭代 / rects ──
    void beginEndIteration();
    void rectsReturnsMerged();
    void rectCount();

    // ── 平移 ──
    void translateAndTranslated();

    // ── 相等 ──
    void equalityByCoverage();
};
