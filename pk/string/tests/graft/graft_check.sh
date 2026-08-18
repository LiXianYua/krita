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
#
# R-13 Task 5 实测过把 libs/global/KisRectsGrid.cpp 加进来（它是 arg(int,int)
# 重载唯一已知真实调用点），结论：不可行，未加入。
#   卡点：KisRectsGrid.cpp 与它的头分别在同一目录（libs/global/），quote-include
#   `#include "kis_algebra_2d.h"` 按 GCC 规则优先在**当前文件所在目录**找，会
#   命中 libs/global/kis_algebra_2d.h 的**真身**，而不是 pk/geometry/graft/stubs/
#   里那份 R-03 为 KisRectsGridTest.cpp（在 libs/global/tests/ 目录）准备的垫片
#   ——那份垫片只对「自己所在目录不存在同名头」的情况生效，对 KisRectsGrid.cpp
#   自己不生效，与 -I 顺序无关。真身 kis_algebra_2d.h 继续需要 QPainterPath /
#   QTransform（`libs/global/kis_algebra_2d.h:10-15`），这两个在 pk/geometry 与
#   pk/string 的现有垫片/compat 里都没有覆盖，是一整块尚未搭过的几何绘制 API 面，
#   超出本任务 pk/string 范围。`arg(int,int)` 的语法正确性继续由 test_format.cpp
#   的 4 条 arg(int,fieldWidth) 用例 + oracle 对拍 argIntFieldWidth 覆盖，详见
#   task-5-report.md。
#
# R-13 Task 5 顺带发现并修复：KoProgressProxy.{h,cpp} 已被 D-005（提交 f531d07，
# 在本分支 base 历史里）从 libs/widgetutils 搬到了 libs/global——本脚本原先仍
# 引用旧路径 libs/widgetutils/KoProgressProxy.cpp，该文件已不存在，试接目标自
# D-005 落地后这条一直是失败的（未被后续任务发现）。已改成新路径
# libs/global/KoProgressProxy.cpp，并新增 stubs/kritaglobal_export.h（搬到
# libs/global 后头文件用的导出宏从 KRITAWIDGETUTILS_EXPORT 换成了
# KRITAGLOBAL_EXPORT，原有 kritawidgetutils_export.h 不再覆盖）。
set -u
cd "$(dirname "$0")/../../../.." || exit 1

CXX=${CXX:-g++}
SHIM=pk/string/compat/QString
INC=(-I pk/string/compat -I pk/string -I pk/string/tests/graft/stubs)
fail=0

for f in libs/global/KoProgressProxy.cpp libs/version/KritaVersionWrapper.cpp; do
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
