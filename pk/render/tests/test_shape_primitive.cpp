// R-51 对拍断言：Pk 侧形状原语 vs 真 Qt 5.15.7。
//
// 期望值**不在这里生成**——它来自 PK_RENDER_SHAPE_PRIMITIVE_GOLDEN 指向的文件，
// 那份文件由 oracle/run_shape_primitive.sh 跑**真 Qt 侧** oracle 产出
// （R线-spec「对拍怎么做」：校验值必须来自对真 Qt 的实测探针，不能是猜的）。
// 测试自己算期望值等于同义反复，没有判别力。
//
// 这不是 <某个真实测试类> 的替代品——它是**复刻真实调用点形状的 driver**
// （R线-spec「依赖墙挡住真实测试类时」那条降级路径）。真实调用点：
//   libs/flake/KisHandlePainterHelper.cpp:100/115/160/180/224/290
//   libs/flake/KoPathShape.cpp:152
//   plugins/tools/tool_knife/CutThroughShapeStrategy.cpp:387-388
//   plugins/tools/tool_smart_patch/kis_tool_smart_patch.cpp:251/255
//   plugins/tools/tool_enclose_and_fill/subtools/KisToolBasicBrushBase.cpp:290
//   plugins/tools/tool_transform2/TransformToolPlatform.cpp:123/221
//   plugins/tools/tool_transform2/kis_warp_transform_strategy.cpp:284-297
//   plugins/tools/defaulttool/defaulttool/ShapeRotateStrategy.cpp:87
// 它们编在 libs/flake / plugins/tools 各自的大模块里，其 CMake 生成产物（导出头等）
// 不在 R-51 的 locks 内（依赖墙），本任务范围内编不动真实测试类。
// oracle 的用例表逐条复刻这些调用点的**参数形状**（同一个 PkRectF/PkPolygonF、
// 同样的 pen/brush 组合）。**真实测试类的编译级证据仍欠着**，已登记在
// pk/render/README.md。
#include "shape_primitive_cases.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main()
{
    const char *path = std::getenv("PK_RENDER_SHAPE_PRIMITIVE_GOLDEN");
    if (!path) {
        std::cerr << "PK_RENDER_SHAPE_PRIMITIVE_GOLDEN not set\n";
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

    std::vector<std::string> actual;
    for (const auto &c : pkShapeCases::table()) {
        CaseImage image = makeCaseImage();
        fillCaseImage(image);
        PkImageRasterBackend backend(image);
        PkPainter painter(backend);
        painter.setRenderHint(PkPainter::RenderHint::Antialiasing, true);
        painter.setPen(c.hasPen ? PkPen(PkColor(Pk::black), c.penWidth) : PkPen(Pk::NoPen));
        painter.setBrush(c.hasBrush ? PkBrush(PkColor(Pk::red)) : PkBrush(Pk::NoBrush));
        if (c.polygon) {
            PkPolygonF polygon;
            for (const auto &p : c.points) polygon.append(p);
            painter.drawPolygon(polygon);
        } else {
            painter.drawEllipse(c.ellipseRect);
        }
        std::ostringstream line;
        line << c.name << ' ' << pkShapeCases::digest(image);
        actual.push_back(line.str());
    }

    if (actual.size() != expected.size()) {
        std::cerr << "case count mismatch: pk=" << actual.size()
                  << " qt=" << expected.size() << '\n';
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

    std::cout << "shape primitives match Qt 5.15.7 oracle (" << actual.size() << " cases)\n";
    return 0;
}
