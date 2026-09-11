// 两侧各编一份：加 -DPK_BLUR_QT_ORACLE 走真 QPainter，否则走 PkPainter +
// PkImageRasterBackend。两边打印同一份「tag + 内核摘要」表，diff 即判据。
//
// 渲染实现与语料在 blur_kernel_cases.h 里，**本文件不再内联一份**——两侧共用同一份，
// 抄第二份的结果是两边悄悄漂移，对拍就变成自己跟自己比。
//
// 形制照抄 pk/render/oracle/shape_primitive_oracle.cpp（R线-spec「对拍怎么做·形态契约」：
// 两侧**真的分别** include 各自的头，不是同一个实现换个壳）。
#include "blur_kernel_cases.h"

#include <iostream>

int main()
{
    std::cout << "backend " << kBackendName << '\n';

    for (int angle : pkBlurKernel::kMotionAngles) {
        for (int length : pkBlurKernel::kMotionLengths) {
            std::cout << pkBlurKernel::formatKernelLine(
                             pkBlurKernel::motionTag(angle, length),
                             pkBlurKernel::renderMotionCase(angle, length))
                      << '\n';
        }
    }

    for (const char *shape : pkBlurKernel::kLensShapes) {
        for (unsigned int radius : pkBlurKernel::kLensRadii) {
            for (unsigned int rotation : pkBlurKernel::kLensRotations) {
                std::cout << pkBlurKernel::formatKernelLine(
                                 pkBlurKernel::lensTag(shape, radius, rotation),
                                 pkBlurKernel::renderLensCase(shape, radius, rotation))
                          << '\n';
            }
        }
    }
    return 0;
}
