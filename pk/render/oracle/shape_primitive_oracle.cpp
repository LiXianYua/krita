// 两侧各编一份：-DPK_SHAPE_QT_ORACLE 走真 QPainter，否则走 PkPainter +
// PkImageRasterBackend。两边打印同一份「用例名 + 像素摘要」表，diff 即判据。
//
// 渲染实现在 shape_primitive_cases.h 里，**本文件不再内联一份**——两侧共用同一份，
// 抄第二份的结果是两边悄悄漂移，对拍就变成自己跟自己比。
//
// 形制照抄 pk/render/oracle/brush_gradient.cpp（R线-spec「对拍怎么做·形态契约」：
// 两侧**真的分别** include 各自的头，不是同一个实现换个壳）。
#include "shape_primitive_cases.h"

#include <iostream>

int main()
{
    std::cout << "backend " << kBackendName << '\n';
    for (const auto &c : pkShapeCases::table()) {
        const CaseImage image = pkShapeCases::renderCase(c);
        std::cout << c.name << ' ' << pkShapeCases::digest(image) << '\n';
    }
    return 0;
}
