// R-55 文字对拍：两侧各编一份 —— 加 -DPK_TEXT_QT_ORACLE 走真 QPainter，否则走
// PkPainter + PkImageRasterBackend。两边打印同一份「api + tag + 像素摘要」表，
// 由 run_text.sh 逐行 diff（形制照 pk/render/oracle/shape_primitive_oracle.cpp）。
//
// 输出行（R线-spec「对拍怎么做 · 形态契约」）：
//     backend <name>                 恰一行，自报家门；run_text.sh 用 `grep -v '^backend '` 剥掉
//     DIFFTAG <api> <tag> <digest>   每个用例一行（digest = 该用例渲染结果的 FNV-1a）
//     DIFF total=<N> mismatch=<M>    恰一行
//
// **为什么 `mismatch` 在这里恒为 0**：本工装是两个二进制的 diff 形态（Qt 侧必须手工
// 编译、Pk 侧走 CMake target，二者不同 TU——本工程不得 find_package(Qt…)），单个二进制
// 没有「另一侧」可比，算不出跨侧 mismatch。plan §3.4 同一条 bullet 要求的正是
// `grep -v '^backend ' 再 diff`，跨侧计数只有比较者（run_text.sh）知道，故真正的
// `DIFF total=<N> mismatch=<M>`（M 可 >0）由 **run_text.sh** 打印；这里这一行是单侧自报，
// 恒 0，存在的意义是让两侧输出行形一致、可被 diff 逐行对齐。
//
// 退出码**恒 0**：判定差异该不该存在是 reviewer 的事，不是对拍程序的事。
#include "text_cases.h"

#include <cstdint>
#include <iostream>

int main()
{
    std::cout << "backend " << kBackendName << '\n';

    std::uint32_t total = 0;
    for (const auto &c : pkTextCases::table()) {
        const CaseImage image = pkTextCases::renderCase(c);
        const std::uint32_t value = pkTextCases::digest(image);
        std::cout << "DIFFTAG " << c.api << ' ' << c.tag << ' ' << value << '\n';
        ++total;
    }
    std::cout << "DIFF total=" << total << " mismatch=0\n";
    return 0;
}
