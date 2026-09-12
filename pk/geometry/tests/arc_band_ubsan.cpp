// R-58：arc 大角度档的 UBSan 闸门。
//
// **这不是单测，是闸门**：它不断言任何取值，只负责把「大角度档不许有 UB」这条
// 常驻化 —— 判据是**进程的退出码**（配 `-fno-sanitize-recover=all`，第一条
// runtime error 就 abort），不是文件里打印了什么。
//
// 覆盖的档（实测的 6 个 UB 站点，见 README「坐标守卫」第 6 点）：
//   · `arcTo(rect, a, -90)`      a ∈ 越界档  → pkCurvesForArc 的四处 int 算术
//   · `arcTo(rect, 0, s)`        s 越界      → 同上（sweep 先 clamp，但上界仍进）
//   · `arcMoveTo(rect, a)`       a ∈ 越界档  → pkFindEllipseCoords 的 `int quadrant`
//   · `addEllipse(rect)`、`addRoundedRect(...)`、`arcTo(...,0,-360)` 全量 —— 回归见证
//
// ⚠ 本 TU **不 include 任何 Qt 头**，也不 include pk 源码：源码由 CMake 一并编进
// 这个可执行文件（见 CMakeLists.txt 的 pk_arc_band_ubsan，**已剔除 PkGeometryDebug.cpp**）。
#include "PkPainterPath.h"

#include <cmath>
#include <cstdio>

namespace {

const double kBand[] = {
    2147483700.0, 2147483701.0, 2147484000.0, 3e9, 1e10, 1e12, 1e15, 1e16, 1e18,
    1e20, 1e127, -2147483700.0, -1e10, -1e15, -1e18, -1e127,
    1e9, 2147483648.0, 2147483699.0, 0.0, 45.0, -45.0,
};

void sweep()
{
    const PkRectF r(1.0, 2.0, 3.0, 4.0);
    for (double a : kBand) {
        for (double s : { -90.0, 90.0, 0.0, 360.0, -360.0, 359.0 }) {
            PkPainterPath p1; p1.moveTo(1.0, 2.0); p1.arcTo(r, a, s);
            PkPainterPath p2; p2.arcTo(r, a, s);
            PkPainterPath p3; p3.arcMoveTo(r, a);
            PkPainterPath p4; p4.arcMoveTo(r, a + s);
        }
        PkPainterPath p5; p5.addEllipse(r);
        PkPainterPath p6; p6.addEllipse(PkRectF(2.0, 2.0, 1.0, 28.0));
        PkPainterPath p7; p7.addRoundedRect(r, 1.0, 1.0);
        PkPainterPath p8; p8.arcTo(r, a, -360.0);
    }
}

} // namespace

int main()
{
    sweep();
    std::printf("arc_band_ubsan: 扫完 %zu 个大角度值，无 UB 报告\n",
                sizeof(kBand) / sizeof(kBand[0]));
    return 0;
}
