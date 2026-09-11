// R-52 对拍断言：Pk 侧两个 blur 内核构造段 vs 真 Qt 5.15.7。
//
// 期望值**不在这里生成**——它来自 PK_RENDER_BLUR_KERNEL_GOLDEN 指向的文件，那份文件
// 由 oracle/run_blur_kernel.sh 跑**真 Qt 侧** oracle 产出（R线-spec「对拍怎么做」：
// 校验值必须来自对真 Qt 的实测探针，不能是猜的）。测试自己算期望值等于同义反复。
//
// 这不是 <某个真实测试类> 的替代品——它是**复刻真实调用点形状的 driver**
//（R线-spec「依赖墙挡住真实测试类时」那条降级路径）。真实调用点：
//   plugins/filters/blur/kis_motion_blur_filter.cpp   （[GAP]，本批 Task 2 恢复）
//   plugins/filters/blur/kis_lens_blur_filter.cpp     （[GAP]，本批 Task 3 恢复）
// 它们编在 plugins/filters/blur 的大 target 里，其 CMake 生成产物不在本任务的 locks
// 内（依赖墙）。oracle 的 Pk 分支与那两个滤镜里将要写下的代码**逐行同形**
//（plan §3.2），L2 靠 reviewer 对读，L3 由 Task 5 的端到端 driver 兜底。
#include "blur_kernel_cases.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int nonZeroCount(const pkBlurKernel::Kernel &kernel)
{
    int count = 0;
    for (int value : kernel.values) {
        if (value != 0) {
            ++count;
        }
    }
    return count;
}

}  // namespace

int main()
{
    const char *path = std::getenv("PK_RENDER_BLUR_KERNEL_GOLDEN");
    if (!path) {
        std::cerr << "PK_RENDER_BLUR_KERNEL_GOLDEN not set\n";
        return 2;
    }
    std::ifstream golden(path);
    if (!golden) {
        std::cerr << "cannot open golden: " << path << '\n';
        return 2;
    }

    std::vector<std::string> expected;
    for (std::string line; std::getline(golden, line);) {
        if (!line.empty() && line.front() != '#' && line.rfind("backend ", 0) != 0)
            expected.push_back(line);
    }
    if (expected.empty()) {
        std::cerr << "golden is empty: " << path << '\n';
        return 2;
    }

    // 用例顺序与 oracle/blur_kernel_oracle.cpp 完全一致（同一份语料、同一个循环嵌套）。
    std::vector<std::string> actual;
    long long motionCases = 0;
    long long lensCases = 0;
    long long discriminating = 0;   // 内核取值 ≥2 个非零
    long long singlePixel = 0;      // 内核恰好 1 个非零（形状最弱，与 BASE 的单位核最像）
    long long nothingDrawn = 0;     // 内核全零（恒真：两侧都什么都没画）

    const auto record = [&](const std::string &tag, const pkBlurKernel::Kernel &kernel) {
        actual.push_back(pkBlurKernel::formatKernelLine(tag, kernel));
        const int nnz = nonZeroCount(kernel);
        if (nnz == 0) {
            ++nothingDrawn;
        } else if (nnz == 1) {
            ++singlePixel;
        } else {
            ++discriminating;
        }
    };

    for (int angle : pkBlurKernel::kMotionAngles) {
        for (int length : pkBlurKernel::kMotionLengths) {
            ++motionCases;
            record(pkBlurKernel::motionTag(angle, length),
                   pkBlurKernel::renderMotionCase(angle, length));
        }
    }
    for (const char *shape : pkBlurKernel::kLensShapes) {
        for (unsigned int radius : pkBlurKernel::kLensRadii) {
            for (unsigned int rotation : pkBlurKernel::kLensRotations) {
                ++lensCases;
                record(pkBlurKernel::lensTag(shape, radius, rotation),
                       pkBlurKernel::renderLensCase(shape, radius, rotation));
            }
        }
    }

    if (actual.size() != expected.size()) {
        std::cerr << "case count mismatch: pk=" << actual.size() << " qt=" << expected.size()
                  << '\n';
        return 1;
    }

    int failures = 0;
    for (std::size_t i = 0; i < actual.size(); ++i) {
        if (actual[i] != expected[i]) {
            std::cerr << "MISMATCH\n  qt: " << expected[i] << "\n  pk: " << actual[i] << '\n';
            ++failures;
        }
    }
    if (failures) {
        std::cerr << failures << " case(s) diverge from Qt\n";
        return 1;
    }

    // 诚实的计数：把「有判别力」与两类弱证据分开报，免得 138 这个数被读成 138 条
    // 独立判别力。`singlePixel` 的内核只有一个非零像素——它与 BASE 的单位核
    //（`setZero()` + 中心置 1）形态最接近，是这一批里最弱的一档；`nothingDrawn`
    // 证明的是「退化输入不崩、且两侧同样什么都不画」，不是「画得对」。
    std::cout << "blur kernels match Qt 5.15.7 oracle: " << actual.size() << " cases ("
              << motionCases << " motion + " << lensCases << " lens; " << discriminating
              << " discriminating, " << singlePixel << " single-pixel, " << nothingDrawn
              << " nothing-drawn)\n";
    return 0;
}
