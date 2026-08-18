#!/usr/bin/env bash
# R-25 Task 1 判据②：候选 C —— libs/psd/psd_layer_section.cpp:596
# （importNode() 7 处真实调用点之一）的形状对齐试探。
#
# 计划里排在候选 C 前面的两条都撞了同一堵墙，记录在
# .superpowers/sdd/R-25/task-1-report.md：
#   · plugins/impex/libkra/kis_kra_loader_test.cpp（候选 A）：30 分钟时间盒，
#     一路补 kritalibkra_export.h（stub）→ KoColor.h（-I libs/pigment）→
#     kritapigment_export.h（缺，停），libs/pigment 整库不在 pk/xml 的
#     locks 范围内，时间盒到点。
#   · libs/psd/psd_layer_section.cpp 全文件 -fsyntax-only（候选 B）：同一类
#     墙——psd_layer_section.h 第一个 include 就是 kritapsd_export.h（缺），
#     往下还要连续解析 KoColor.h/KoColorSpace.h/kis_image.h/kis_node.h/
#     KoShapeManager.h 等 libs/pigment + libs/image + libs/flake 的完整生产
#     依赖链——同样不是时间盒内能解决的规模。
#
# 候选 C 退化为计划本身已有的先例（README §11.3 / 设计①-b 对 SvgParser.cpp
# 的处理）：不编译真实文件，改在 graft_run_c_driver.cpp 里**逐字符**复刻
# psd_layer_section.cpp:578-600 `mergePatternsXMLSection()` 用到 importNode()
# 的那几行调用形状（类型、参数顺序、参数类型全部对齐），只用 pk/xml 自己已
# 交付的 compat 垫片编译——只证明"签名字面兼容,编译器不报 no matching
# function",不证明整份 psd_layer_section.cpp 能编译。逐字对照见 driver 文件
# 头注释。
#
# 四段式（比 graft_run_a.sh/b.sh 简单——不需要真实测试文件、不需要 kis_debug
# 之类的生产胶水）：①编译 driver ②链接 pk/xml 自己的 build 产物 ③跑
# ④nm -u 判据③。driver.cpp 是新增文件，不是从任何源树文件复制/改名而来。
set -eu
cd "$(dirname "$0")/../../../.." || exit 1     # → fork 仓库根

GRAFT=pk/xml/tests/graft
BUILD=$GRAFT/build
CXX=${CXX:-g++}
rc=0

if [ ! -f pk/xml/build/libpkxml.a ]; then
    cmake -S pk/xml -B pk/xml/build >/dev/null
    cmake --build pk/xml/build -j"$(nproc)" >/dev/null
fi

PUGIXML_INC=$(find pk/xml/build/_deps -maxdepth 2 -type d -name src -path "*pugixml*" | head -1)
if [ -z "$PUGIXML_INC" ]; then
    printf 'graft_run_c.sh: 找不到 pugixml 头文件目录\n' >&2
    exit 1
fi

mkdir -p "$BUILD"
INC="-I pk/xml -I pk/xml/compat -I pk/string -I pk/string/compat -I $PUGIXML_INC"

# ① 编译 driver（形状对齐版本，逐字对照见文件头注释）。
if ! "$CXX" -std=c++17 $INC -c "$GRAFT/graft_run_c_driver.cpp" -o "$BUILD/driver_c.o" \
    2>"$BUILD/compile_c.log"; then
    printf '  试接编译失败: graft_run_c_driver.cpp\n'
    sed 's/^/    /' "$BUILD/compile_c.log" | head -60
    exit 1
fi

# ② 链接：只需要 pk/xml 自己的产物（不需要 pktest harness，driver 自带 main()）。
if ! "$CXX" -std=c++17 "$BUILD/driver_c.o" \
    pk/xml/build/libpkxml.a pk/xml/build/libpkstring.a pk/xml/build/libpugixml.a \
    -o "$BUILD/graft_run_c" 2>"$BUILD/link_c.log"; then
    printf '  试接链接失败\n'
    sed 's/^/    /' "$BUILD/link_c.log" | head -60
    exit 1
fi

# ③ 跑：验证深拷贝语义与真实调用点期望的效果一致
#    （dst.Patterns 补齐 src 里的两个子节点，含完整子树）。
if "./$BUILD/graft_run_c" >"$BUILD/run_c.log" 2>&1; then
    printf '  试接跑绿: mergePatternsXMLSectionShape\n'
    sed 's/^/    /' "$BUILD/run_c.log"
else
    printf '  试接跑挂: mergePatternsXMLSectionShape\n'
    sed 's/^/    /' "$BUILD/run_c.log" | head -40
    rc=1
fi

# ④ 判据③：产物不得有 Qt 未定义符号。
undef=$(nm -u "$BUILD/graft_run_c" 2>/dev/null | grep -i qt || true)
if [ -n "$undef" ]; then
    printf '  试接产物含 Qt 符号:\n%s\n' "$undef"
    rc=1
else
    printf '    nm -u graft_run_c | grep -i qt: 无输出\n'
fi

exit "$rc"
