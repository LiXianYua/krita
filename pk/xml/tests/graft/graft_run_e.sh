#!/usr/bin/env bash
# R-25 Task 3 判据②：候选 E —— libs/pigment/resources/KoColorSet.cpp
# （lineNumber()/columnNumber() 34 处真实调用点所在文件）的形状对齐试探。
#
# 现场撞墙记录（-fsyntax-only 直接试接 KoColorSet.cpp 本身，见
# graft_run_e_driver.cpp 头注释有完整背景）：
#
#   $ g++ -std=c++17 -DQT_NO_DEBUG -I libs/pigment -I libs/pigment/resources \
#       -I $QT/include -I $QT/include/QtCore -I $QT/include/QtXml \
#       -I $QT/include/QtGui -fsyntax-only libs/pigment/resources/KoColorSet.cpp
#   ...
#   libs/pigment/DebugPigment.h:11:10: fatal error: kritapigment_export.h: No such file or directory
#
#   stub 掉 kritapigment_export.h 之后继续往下：
#   libs/pigment/resources/KoColorSet.cpp:38:10: fatal error: klocalizedstring.h: No such file or directory
#
# 第一堵墙与 R-25 task-1-report.md 候选 A（kis_kra_loader_test.cpp）撞的是
# 同一堵——libs/pigment 整库需要自己的 CMake generate_export_header()；第二
# 堵是 KF5::I18n 依赖，比 Task 1 报告记录的更深。且
# `libs/pigment/tests/CMakeLists.txt` 里 TestKoColorSet.cpp 是
# `LINK_LIBRARIES kritapigment`（链接已构建库），不是把 KoColorSet.cpp 源码
# 编进测试可执行文件——计划 Task 3 checklist 原先设想的"整个 KoColorSet.cpp
# 被编译进测试可执行文件即是编译期证据"这条路径不成立。均不是 30-45 分钟
# 时间盒内能解决的规模，且 libs/pigment 整库不在 pk/xml 的 locks 范围内。
#
# 退化为 graft_run_e_driver.cpp——逐字复刻 KoColorSet.cpp 两处真实调用点
# （loadSbzSwatchbook() DOM 侧 / loadXml() Stream 侧）用到
# lineNumber()/columnNumber() 的调用形状，只用 pk/xml 自己已交付的 compat
# 垫片编译。
#
# 四段式（同 graft_run_c.sh/graft_run_d.sh）：①编译 driver ②链接 pk/xml 自己
# 的 build 产物 ③跑 ④nm -u 判据③。driver.cpp 是新增文件，不是从任何源树
# 文件复制/改名而来。
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
    printf 'graft_run_e.sh: 找不到 pugixml 头文件目录\n' >&2
    exit 1
fi

mkdir -p "$BUILD"
INC="-I pk/xml -I pk/xml/compat -I pk/string -I pk/string/compat -I $PUGIXML_INC"

# ① 编译 driver（形状对齐版本，逐字对照见文件头注释）。
if ! "$CXX" -std=c++17 $INC -c "$GRAFT/graft_run_e_driver.cpp" -o "$BUILD/driver_e.o" \
    2>"$BUILD/compile_e.log"; then
    printf '  试接编译失败: graft_run_e_driver.cpp\n'
    sed 's/^/    /' "$BUILD/compile_e.log" | head -60
    exit 1
fi

# ② 链接：只需要 pk/xml 自己的产物（不需要 pktest harness，driver 自带 main()）。
if ! "$CXX" -std=c++17 "$BUILD/driver_e.o" \
    pk/xml/build/libpkxml.a pk/xml/build/libpkstring.a pk/xml/build/libpugixml.a \
    -o "$BUILD/graft_run_e" 2>"$BUILD/link_e.log"; then
    printf '  试接链接失败\n'
    sed 's/^/    /' "$BUILD/link_e.log" | head -60
    exit 1
fi

# ③ 跑：验证 KoColorSet.cpp 两处真实调用点（loadSbzSwatchbook() DOM 侧 /
# loadXml() Stream 侧）的调用形状运行期不崩溃、行为符合预期分支。
if "./$BUILD/graft_run_e" >"$BUILD/run_e.log" 2>&1; then
    printf '  试接跑绿: KoColorSetLineColShape\n'
    sed 's/^/    /' "$BUILD/run_e.log"
else
    printf '  试接跑挂: KoColorSetLineColShape\n'
    sed 's/^/    /' "$BUILD/run_e.log" | head -40
    rc=1
fi

# ④ 判据③：pk/xml 自己的产物依旧零 Qt 符号（这条试接过程本身不改
# pk/xml 源码，只是新增一个 graft 驱动程序，正常情况下不会影响这一条，
# 但改了 CMake/新增源文件之后照例复核一遍）。
if nm -u -C pk/xml/build/libpkxml.a | grep -qi qt; then
    printf '  判据③失败: libpkxml.a 里出现 Qt 符号\n'
    rc=1
else
    printf '  判据③通过: libpkxml.a 无未决 Qt 符号\n'
fi

exit $rc
