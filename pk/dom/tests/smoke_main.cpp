// R-50 冒烟测试：用 pugixml 后端的 PkDom 解析一段 SVG，校验 text/svg 实测用到的
// 调用点（documentElement / tagName / attribute / elementsByTagName / text /
// firstChild / toElement / toText）。零 Qt。
#include "PkDomDocument.h"
#include "PkDomElement.h"
#include <cstdio>
#include <string>

static int g_fail = 0;
#define CHECK(cond, msg) do {                                              \
    if (!(cond)) { printf("FAIL: %s\n", msg); ++g_fail; }                  \
} while (0)

int main() {
    const std::string svg =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"50\">"
        "  <g id=\"root\">"
        "    <rect x=\"10\" y=\"20\" fill=\"red\">hello</rect>"
        "    <text x=\"5\">world</text>"
        "  </g>"
        "</svg>";

    PkDomDocument doc;
    std::string err;
    bool ok = doc.setContent(svg, false, &err);
    CHECK(ok, ("setContent 应成功，实际报错: " + err).c_str());
    if (!ok) return 1;

    PkDomElement *root = doc.documentElement();
    CHECK(root != nullptr, "documentElement 非空");
    CHECK(root->tagName() == "svg", "根节点 tagName==svg");
    CHECK(root->attribute("width") == "100", "svg@width==100");

    PkDomNodeList gs = root->elementsByTagName("g");
    CHECK(gs.length() == 1, "elementsByTagName(g) 命中 1");
    PkDomElement *g = gs.at(0);
    CHECK(g != nullptr, "g 元素可取");
    CHECK(g->attribute("id") == "root", "g@id==root");

    PkDomNodeList rects = root->elementsByTagName("rect");
    CHECK(rects.length() == 1, "elementsByTagName(rect) 命中 1");
    PkDomElement *rect = rects.at(0);
    CHECK(rect->attribute("fill") == "red", "rect@fill==red");
    // rect 的文本子节点（递归 text()）
    CHECK(rect->text() == "hello", "rect 文本==hello");

    PkDomNodeList texts = root->elementsByTagName("text");
    CHECK(texts.length() == 1, "elementsByTagName(text) 命中 1");
    CHECK(texts.at(0)->text() == "world", "text 元素文本==world");

    // firstChild -> toElement 遍历
    PkDomNode *fc = g->firstChild();
    CHECK(fc && fc->toElement() && fc->toElement()->tagName() == "rect",
          "g 首子为 rect 元素");

    if (g_fail == 0) { printf("pkdom_smoke: ALL PASS\n"); return 0; }
    printf("pkdom_smoke: %d FAILED\n", g_fail);
    return 1;
}
