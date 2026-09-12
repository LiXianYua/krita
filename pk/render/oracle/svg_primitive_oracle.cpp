// Pk 侧（Qt-free）：逐用例跑 PkPainter + PkSvgPainterBackend，打印
// `<用例名>\t<文档正文>`（一行一用例）。
//
// 文档正文由 PkSvgPainterBackend::document() 产出，**不含换行**（单份文档一行），
// 所以「一行一用例」安全；用例名只用 [A-Za-z0-9#:._-]，第一个 '\t' 之前是名字
// （协议见 docs/superpowers/plans/R-54.md Task 2 Step 2）。
//
// 骨架照抄 pk/render/oracle/shape_primitive_oracle.cpp（「两侧各编一份、喂同一输入集」），
// 差别只在产物：那边是像素摘要、两侧各写一份 diff；这里是 SVG 文档正文，由 Qt 侧
// 比较器读 argv[1] 后渲染比对（R-54 §1.3——两份文档文本按定义不同，不能文本 diff）。
#include "svg_primitive_cases.h"

#include <iostream>

int main()
{
    for (const auto &c : pkSvgCases::table()) {
        // document() 里既无 '\t' 也无 '\n'，直接拼即可。
        std::cout << c.name << '\t' << pkSvgCases::documentFor(c) << '\n';
    }
    return 0;
}
