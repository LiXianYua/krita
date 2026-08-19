#pragma once

// PkPolygon / PkPolygonF 的单测类，外加 PkTransform 新解开的
// map(PkPolygonF)/squareToQuad/quadToSquare 三个重载（R-21 T2）。
//
// 生成器 pk_test_moc.py 只扫 .h，且只认类体里字面出现的 "Q_OBJECT" 与
// "private Q_SLOTS:"，所以这两个 token 必须真的出现在本文件里。
// 本地等价展开、不走 pk/test/compat/QObject 的理由与 rect_case.h 相同。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../../test/PkTest.h"

class PkPolygonCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    // ── PkPolygon（int，最小面：构造 + 迭代）──
    void polygonDefaultCtorIsEmpty();
    void polygonFromVectorCtorIsImplicit();
    void polygonInitializerListCtor();
    void polygonIteratorAndPushBackForBoost();

    // ── PkPolygonF 构造 ──
    void polygonFDefaultCtorIsEmpty();
    void polygonFSizeCtorIsExplicit();
    void polygonFFromVectorCtorIsImplicit();
    void polygonFFromRectCtorIsFivePointsClockwise();

    // ── translate / translated ──
    void polygonFTranslate();
    void polygonFTranslated();

    // ── isClosed ──
    void polygonFIsClosed();

    // ── boundingRect ──
    void polygonFBoundingRect();
    void polygonFBoundingRectOfEmptyIsZero();

    // ── toPolygon ──
    void polygonFToPolygonRounds();

    // ── containsPoint：Qt::FillRule ──
    void polygonFContainsPointSquareInsideOutsideVertex();
    void polygonFContainsPointEmptyIsFalse();
    void polygonFContainsPointStarDistinguishesFillRule();

    // ── PkTransform::map(PkPolygonF) ──
    void transformMapPolygonFTranslateFastPath();
    void transformMapPolygonFRotateGeneralPath();

    // ── PkTransform::squareToQuad / quadToSquare ──
    void transformSquareToQuadAffineParallelogram();
    void transformSquareToQuadPerspectiveTrapezoid();
    void transformSquareToQuadWrongCountFails();
    void transformQuadToSquareRoundTrips();
};

#undef Q_SLOTS
#undef Q_OBJECT
