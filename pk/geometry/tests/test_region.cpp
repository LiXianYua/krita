#include "cases/region_case.h"
#include "../PkRegion.h"

#include <cstdint>

#include "pk_binder_region_case.inc"

// ---------------------------------------------------------------------------
// 期望值按**覆盖谓词**语义（见 PkRegion.h 头部与 R-21 plan.md「问 4」的裁决）：
// 不逐位对齐 Qt 的矩形划分，只保证 isEmpty / boundingRect / contains / 面积 /
// rects() 汇总面积正确。逐输入覆盖对拍在 geometry_difftest.cpp 的 Region 族。
// ---------------------------------------------------------------------------

void PkRegionCase::defaultCtorIsEmpty()
{
    PkRegion r;
    PK_VERIFY(r.isEmpty());
    PK_VERIFY(r.isNull());
    PK_VERIFY(r.rectCount() == 0);
    PK_VERIFY(r.begin() == r.end());
}

void PkRegionCase::ctorFromRect()
{
    PkRegion r(PkRect(0, 0, 10, 10));
    PK_VERIFY(!r.isEmpty());
    PK_VERIFY(r.rectCount() == 1);
}

void PkRegionCase::isEmptyAndIsNull()
{
    PK_VERIFY(PkRegion().isEmpty());
    PK_VERIFY(!PkRegion(PkRect(0, 0, 1, 1)).isEmpty());
}

void PkRegionCase::boundingRect()
{
    PkRegion r;
    r += PkRect(0, 0, 10, 10);
    r += PkRect(20, 20, 5, 5);
    PK_VERIFY(r.boundingRect() == PkRect(0, 0, 25, 25));
    PK_VERIFY(PkRegion().boundingRect() == PkRect());
}

void PkRegionCase::containsPoint()
{
    PkRegion r(PkRect(0, 0, 10, 10));
    PK_VERIFY(r.contains(PkPoint(5, 5)));
    PK_VERIFY(!r.contains(PkPoint(20, 20)));
}

void PkRegionCase::containsRectCoverage()
{
    PkRegion r(PkRect(0, 0, 10, 10));
    PK_VERIFY(r.contains(PkRect(2, 2, 4, 4)));
    PK_VERIFY(!r.contains(PkRect(2, 2, 20, 4)));
}

void PkRegionCase::unionRect()
{
    PkRegion r(PkRect(0, 0, 10, 10));
    r += PkRect(5, 5, 10, 10);   // 重叠
    PK_VERIFY(r.boundingRect() == PkRect(0, 0, 15, 15));
    // 覆盖面积 = 10*10 + 10*10 - 5*5（重叠） = 175
    long long area = 0;
    for (const PkRect &rc : r.rects()) area += (long long)rc.width() * rc.height();
    PK_VERIFY(area == 175);
}

void PkRegionCase::subtractRect()
{
    PkRegion r(PkRect(0, 0, 10, 10));
    r -= PkRect(5, 5, 5, 5);     // 右下角
    // 覆盖面积 = 100 - 25 = 75
    long long area = 0;
    for (const PkRect &rc : r.rects()) area += (long long)rc.width() * rc.height();
    PK_VERIFY(area == 75);
    PK_VERIFY(!r.contains(PkPoint(7, 7)));
    PK_VERIFY(r.contains(PkPoint(2, 2)));
}

void PkRegionCase::intersectRect()
{
    PkRegion r(PkRect(0, 0, 10, 10));
    r &= PkRect(5, 5, 10, 10);
    PK_VERIFY(r.boundingRect() == PkRect(5, 5, 5, 5));
    PK_VERIFY(r.rectCount() == 1);
}

void PkRegionCase::unionRegion()
{
    PkRegion a(PkRect(0, 0, 10, 10));
    PkRegion b(PkRect(5, 5, 10, 10));
    PkRegion c = a | b;
    long long area = 0;
    for (const PkRect &rc : c.rects()) area += (long long)rc.width() * rc.height();
    PK_VERIFY(area == 175);
}

void PkRegionCase::subtractRegion()
{
    PkRegion a(PkRect(0, 0, 10, 10));
    PkRegion b(PkRect(5, 5, 5, 5));
    PkRegion c = a - b;
    long long area = 0;
    for (const PkRect &rc : c.rects()) area += (long long)rc.width() * rc.height();
    PK_VERIFY(area == 75);
}

void PkRegionCase::xorRegion()
{
    PkRegion a(PkRect(0, 0, 10, 10));
    PkRegion b(PkRect(5, 5, 10, 10));
    PkRegion c = a ^ b;
    // 对称差面积 = (a-b) + (b-a) = 75 + 75 = 150
    long long area = 0;
    for (const PkRect &rc : c.rects()) area += (long long)rc.width() * rc.height();
    PK_VERIFY(area == 150);
}

void PkRegionCase::beginEndIteration()
{
    PkRegion r(PkRect(0, 0, 10, 10));
    r += PkRect(20, 0, 10, 10);
    int count = 0;
    for (auto it = r.begin(); it != r.end(); ++it) {
        ++count;
        PK_VERIFY(!it->isEmpty());
    }
    PK_VERIFY(count == r.rectCount());
}

void PkRegionCase::rectsReturnsMerged()
{
    // 两个水平相邻的矩形合并成一个。
    PkRegion r(PkRect(0, 0, 10, 10));
    r += PkRect(10, 0, 10, 10);
    const std::vector<PkRect> v = r.rects();
    PK_VERIFY(v.size() == 1);
    PK_VERIFY(v[0] == PkRect(0, 0, 20, 10));
}

void PkRegionCase::rectCount()
{
    PkRegion r;
    PK_VERIFY(r.rectCount() == 0);
    r += PkRect(0, 0, 10, 10);
    PK_VERIFY(r.rectCount() == 1);
}

void PkRegionCase::translateAndTranslated()
{
    PkRegion r(PkRect(0, 0, 10, 10));
    r.translate(5, -3);
    PK_VERIFY(r.boundingRect() == PkRect(5, -3, 10, 10));
    PkRegion t = r.translated(1, 1);
    PK_VERIFY(t.boundingRect() == PkRect(6, -2, 10, 10));
}

void PkRegionCase::equalityByCoverage()
{
    PkRegion a(PkRect(0, 0, 10, 10));
    PkRegion b(PkRect(0, 0, 10, 10));
    PK_VERIFY(a == b);
    PK_VERIFY(!(a != b));
    b += PkRect(20, 20, 1, 1);
    PK_VERIFY(a != b);
}

int run_region_tests()
{
    PkRegionCase tc;
    const char *argv[] = {"test_pkgeometry"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
