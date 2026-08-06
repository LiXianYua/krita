#!/usr/bin/env bash
# 真实调用点零改动试接：把 <QString> 解析到垫片，语法检查两个真实的 Krita 源文件。
# 两个文件本身一个字都没改——兼容性全靠编译参数和垫片。
#
# 为什么除了 -I 还要 -include：
#   KoProgressProxy.cpp 先 include 自己的头（第 7 行），头里第 12 行是
#   `class QString;` 前置声明；`#include <QString>` 在第 9 行才出现。
#   只给 -I 的话，`class QString;` 被解析时垫片还没进来 → 那是一个**真的**
#   叫 QString 的类，于是头里 setAutoNestedName(const QString&) 用的是它，
#   而 .cpp 里的定义（在 include <QString> 之后）用的是 PkString，报
#   "no declaration matches ‘void KoProgressProxy::setAutoNestedName(const PkString&)’"。
#   -include 把垫片提到翻译单元最前面，`class QString;` 就被宏改写成
#   `class PkString;`，两边重新一致。**这是编译参数，不是对调用点的改动。**
#   真实剥离时对应的是构建系统里的 force-include，或调用点自然会被换成 PkString。
set -u
cd "$(dirname "$0")/../../../.." || exit 1

CXX=${CXX:-g++}
SHIM=pk/string/compat/QString
INC=(-I pk/string/compat -I pk/string -I pk/string/tests/graft/stubs)
fail=0

for f in libs/widgetutils/KoProgressProxy.cpp libs/version/KritaVersionWrapper.cpp; do
    d=$(dirname "$f")
    out=$("$CXX" -std=c++17 -fsyntax-only -include "$SHIM" "${INC[@]}" -I "$d" "$f" 2>&1)
    rc=$?
    if [ "$rc" -eq 0 ]; then
        printf '  graft OK: %s\n' "$f"
    else
        printf '  graft FAILED: %s\n' "$f"
        printf '%s\n' "$out" | head -20 | sed 's/^/    /'
        fail=1
    fi
done
exit $fail
