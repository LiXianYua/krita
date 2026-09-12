// R-54 批 2 ctest：**Qt-free 回归守卫**。
//
// 它重新生成 Pk 侧文档，与 oracle/svg_primitive_golden.txt 的 `doc=` 列（文档正文的
// FNV-1a）逐例比对。期望值**不在这里生成**——那份金标由 oracle/run_svg_primitive.sh 跑
// **真 Qt 侧比较器**产出（R线-spec〈对拍怎么做〉：校验值必须来自对真 Qt 的实测，不能是猜的）。
//
// ⚠ 能力边界（报告与 README 都要写清，别把 ctest 绿说成等价性绿）：
//   金标里的 `qt=`/`pk=` 两列是**像素摘要**，由 Qt 侧比较器用真 QSvgRenderer 渲染两侧文档
//   算出；Qt-free 的这里**算不出来、也不校验**。所以本测试守的是「**Pk 文档没漂**」，
//   **证明不了与 Qt 等价**。等价性只由 run_svg_primitive.sh（mismatch=0）在同一提交上给出。
//
// 计数口径与 shape_primitive 的测试一致：与「空画布」同像素摘要的用例是**恒真**的
// （两侧都什么都没画），单独报出来，免得总数被读成 N 条独立判别力。空画布摘要不在
// Qt-free 侧凭空造——它随金标一起由 Qt 侧比较器以 `# blank=<hex>` 附上，这里只读它。
#include "svg_primitive_cases.h"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::uint32_t fnv1a(const std::string &bytes)
{
    std::uint32_t hash = 2166136261u;
    for (unsigned char byte : bytes) {
        hash ^= byte;
        hash *= 16777619u;
    }
    return hash;
}

std::string hex32(std::uint32_t value)
{
    std::ostringstream out;
    out << std::hex << value;
    return out.str();
}

std::uint32_t parseHex(const std::string &text)
{
    std::uint32_t value = 0;
    std::istringstream in(text);
    in >> std::hex >> value;
    return value;
}

struct Expected {
    std::string name;
    std::string docHex;
    std::uint32_t qtDigest;
};

} // namespace

int main()
{
    const char *path = std::getenv("PK_RENDER_SVG_PRIMITIVE_GOLDEN");
    if (!path) {
        std::cerr << "PK_RENDER_SVG_PRIMITIVE_GOLDEN not set\n";
        return 2;
    }
    std::ifstream golden(path);
    if (!golden) {
        std::cerr << "cannot open golden: " << path << '\n';
        return 2;
    }

    bool haveBlank = false;
    std::uint32_t blank = 0;
    std::vector<Expected> expected;
    for (std::string line; std::getline(golden, line);) {
        if (line.empty()) continue;
        if (line.front() == '#') {
            const std::string key = "# blank=";
            if (line.rfind(key, 0) == 0) {
                blank = parseHex(line.substr(key.size()));
                haveBlank = true;
            }
            continue;
        }
        std::istringstream in(line);
        Expected e;
        std::string docField, qtField, pkField;
        if (!(in >> e.name >> docField >> qtField >> pkField)) continue;
        e.docHex = docField.rfind("doc=", 0) == 0 ? docField.substr(4) : docField;
        e.qtDigest = qtField.rfind("qt=", 0) == 0 ? parseHex(qtField.substr(3)) : 0;
        expected.push_back(e);
    }
    if (expected.empty()) {
        std::cerr << "golden is empty: " << path << '\n';
        return 2;
    }
    if (!haveBlank) {
        std::cerr << "golden has no '# blank=<hex>' line: " << path << '\n';
        return 2;
    }

    const auto cases = pkSvgCases::table();
    if (cases.size() != expected.size()) {
        std::cerr << "case count mismatch: pk=" << cases.size()
                  << " golden=" << expected.size() << '\n';
        return 1;
    }

    int failures = 0;
    int vacuous = 0;
    for (std::size_t i = 0; i < cases.size(); ++i) {
        const std::string doc = pkSvgCases::documentFor(cases[i]);
        const std::string actual = hex32(fnv1a(doc));
        if (cases[i].name != expected[i].name || actual != expected[i].docHex) {
            std::cerr << "MISMATCH\n  golden: " << expected[i].name << " doc=" << expected[i].docHex
                      << "\n  pk:     " << cases[i].name << " doc=" << actual << '\n';
            ++failures;
        }
        if (expected[i].qtDigest == blank) ++vacuous;
    }
    if (failures) {
        std::cerr << failures << " case(s) drifted from golden\n";
        return 1;
    }

    std::cout << "svg primitives match golden documents: " << cases.size() << " cases ("
              << (cases.size() - vacuous) << " discriminating, " << vacuous
              << " degenerate/no-op; doc= column only, qt=/pk= pixel columns are Qt-side)\n";
    return 0;
}
