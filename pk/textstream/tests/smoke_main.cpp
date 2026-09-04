// R-50 冒烟测试：PkTextStream 的 std::string* 后端（对齐 QTextStream(&string)）。
// PkStream* 后端（对齐 QTextStream(&outputDevice)）的编译/链接由 PkStream.cpp
// 一并验证；运行时需具体 PkStream 子类，这里只覆盖 string 后端的行为。
#include "PkTextStream.h"
#include <cstdio>
#include <string>

int main() {
    std::string buf;
    PkTextStream ts(&buf);
    ts << "<svg width=\"" << 100 << "\" height=\"" << 50 << "\">hello</svg>\n";
    const std::string want = "<svg width=\"100\" height=\"50\">hello</svg>\n";
    if (buf != want) { printf("FAIL write: got [%s]\n", buf.c_str()); return 1; }

    PkTextStream r(&buf);
    std::string line = r.readLine();
    if (line != "<svg width=\"100\" height=\"50\">hello</svg>") {
        printf("FAIL readLine: got [%s]\n", line.c_str()); return 1;
    }
    printf("pktextstream_smoke: ALL PASS\n");
    return 0;
}
