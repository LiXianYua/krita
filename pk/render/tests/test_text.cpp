// R-55 对拍断言：Pk 侧 setFont/drawText vs 期望值文件。
//
// 期望值**不在这里生成**——它来自 PK_RENDER_TEXT_GOLDEN 指向的文件，那份文件由
// oracle/run_text.sh 产出（R线-spec「对拍怎么做」：校验值必须来自对真 Qt 的实测
// 探针，不能是猜的）。
//
// ⚠⚠ 本机这份 golden 是 **Pk 侧自产**的，不是 Qt 金标 ⚠⚠
//   参照系依赖平台：Qt 在 Linux 走 FreeType、macOS 走 CoreText，而本机 Qt 二进制里
//   一个 QFontEngineFT 都没有（实测 0，见 pk/font/oracle/compare.cmake 与本文件打印
//   的出生证明）。所以 run_text.sh 在本机**不跑 Qt 侧**，只把 Pk 侧输出落成期望值。
//   ⇒ **这份测试在本机证明的是「Pk 侧输出自注入以来没漂」**（回归守卫、变异注入的
//   判红判绿者），**不是「与 Qt 逐像素相等」**。Qt 出生证明欠着，登记在
//   plan §6 第 6 条与 pk/render/README.md。有 FreeType 宿主的后来者跑一次
//   run_text.sh 就会把这一栏换成 Qt 出生证明，本测试随之升级为真跨侧判据。
//
// 这不是 <某个真实测试类> 的替代品——它是**复刻真实调用点形状的 driver**
// （R线-spec「依赖墙挡住真实测试类时」那条降级路径）。真实调用点：
//   libs/flake/text/KoSvgTextShape_p_output.cpp:686 （Private::paintDebug 的字形索引标签）
//   plugins/tools/svgtexttool/SvgTextCursor.cpp:880 （paintDecorations 的排版手柄名）
// 它们编在 libs/flake / 插件各自的大模块里，其 CMake 生成产物不在 R-55 的 locks 内
// （依赖墙），本任务范围内编不动真实测试类。oracle 的用例表逐条复刻这两处的
// **参数形状**（同一个默认 PkFont、同一个 PkPointF、同样的 pen/brush、同样的
// save→setTransform→setPen→drawText→restore 顺序）。**真实测试类的编译级证据仍欠着**，
// 已登记在 pk/render/README.md。
#include "text_cases.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main()
{
    const char *path = std::getenv("PK_RENDER_TEXT_GOLDEN");
    if (!path) {
        std::cerr << "PK_RENDER_TEXT_GOLDEN not set\n";
        return 2;
    }
    std::ifstream golden(path);
    if (!golden) {
        std::cerr << "cannot open golden: " << path << '\n';
        return 2;
    }

    std::vector<std::string> expected;
    std::vector<std::string> birthCert;
    for (std::string line; std::getline(golden, line);) {
        if (line.empty())
            continue;
        if (line.front() == '#') {
            birthCert.push_back(line);
            continue;
        }
        if (line.rfind("backend ", 0) == 0)  // provenance 行按定义两侧不同，不比
            continue;
        expected.push_back(line);
    }
    if (expected.empty()) {
        std::cerr << "golden is empty: " << path << '\n';
        return 2;
    }

    std::vector<std::string> actual;
    const std::uint32_t blank = pkTextCases::emptyDigest();
    int degenerate = 0;
    for (const auto &c : pkTextCases::table()) {
        const CaseImage image = pkTextCases::renderCase(c);
        const std::uint32_t value = pkTextCases::digest(image);
        std::ostringstream line;
        line << "DIFFTAG " << c.api << ' ' << c.tag << ' ' << value;
        actual.push_back(line.str());
        if (value == blank)
            ++degenerate;
    }
    const std::size_t caseCount = actual.size();
    {
        // oracle 输出体末行是自报总计，期望值里也有——一并比对，免得「期望值少了
        // 最后一行」被行数相等掩盖过去。
        std::ostringstream totalLine;
        totalLine << "DIFF total=" << caseCount << " mismatch=0";
        actual.push_back(totalLine.str());
    }

    if (actual.size() != expected.size()) {
        std::cerr << "line count mismatch: pk=" << actual.size()
                  << " golden=" << expected.size() << '\n';
        return 1;
    }

    int failures = 0;
    for (std::size_t i = 0; i < actual.size(); ++i) {
        if (actual[i] != expected[i]) {
            std::cerr << "MISMATCH\n  golden: " << expected[i] << "\n  pk:     " << actual[i]
                      << '\n';
            ++failures;
        }
    }
    if (failures) {
        std::cerr << failures << " line(s) diverge from golden\n";
        return 1;
    }

    // 诚实的计数：与空图同摘要的用例是**恒真**的（什么都没画），它们证明的是
    // 「退化输入不崩、且行为稳定」，不是「画得对」。分开报，免得用例总数被读成
    // 同等数量的判别力。
    std::cout << "text cases match golden: " << caseCount << " cases ("
              << (caseCount - static_cast<std::size_t>(degenerate)) << " discriminating, "
              << degenerate << " degenerate/no-op)\n";
    // 出生证明原样打印——**读的人必须能一眼看出这是不是 Qt 金标**。
    for (const auto &line : birthCert)
        std::cout << "golden provenance " << line << '\n';
    return 0;
}
