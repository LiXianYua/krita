#!/usr/bin/env bash
# R-25 Task 2 判据②：候选 PkStream* 形状 —— libs/image/tests/
# kis_liquify_transform_worker_test.cpp:23-32（getWorkerFromIODeviceXml）
# 的形状对齐试探。原始撞墙记录（-fsyntax-only 试接整份真实测试文件）与
# 退化理由见 graft_run_d_driver.cpp 头注释。
#
# 四段式（同 graft_run_c.sh）：①编译 driver ②链接 pk/xml 自己的 build 产物
# （PkStream.cpp 已经是 pkxml 库的一部分——R-25 Task 2 CMake 改动，不需要
# 额外链接 pk/port）③跑 ④nm -u 判据③。driver.cpp 是新增文件，不是从任何
# 源树文件复制/改名而来。
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
    printf 'graft_run_d.sh: 找不到 pugixml 头文件目录\n' >&2
    exit 1
fi

mkdir -p "$BUILD"
INC="-I pk/xml -I pk/xml/compat -I pk/string -I pk/string/compat -I pk/port -I pk/port/compat -I $PUGIXML_INC"

# ① 编译 driver（形状对齐版本，逐字对照见文件头注释）。
if ! "$CXX" -std=c++17 $INC -c "$GRAFT/graft_run_d_driver.cpp" -o "$BUILD/driver_d.o" \
    2>"$BUILD/compile_d.log"; then
    printf '  试接编译失败: graft_run_d_driver.cpp\n'
    sed 's/^/    /' "$BUILD/compile_d.log" | head -60
    exit 1
fi

# ② 链接：PkStream.cpp 已经是 libpkxml.a 的一部分（R-25 Task 2 CMake 改动），
# 不需要额外链接 pk/port 自己的产物。不需要 pktest harness，driver 自带 main()。
if ! "$CXX" -std=c++17 "$BUILD/driver_d.o" \
    pk/xml/build/libpkxml.a pk/xml/build/libpkstring.a pk/xml/build/libpugixml.a \
    -o "$BUILD/graft_run_d" 2>"$BUILD/link_d.log"; then
    printf '  试接链接失败\n'
    sed 's/^/    /' "$BUILD/link_d.log" | head -60
    exit 1
fi

# ③ 跑：验证 setContent(QIODevice*) 解析出的 data 元素跟真实调用点期望的
# 结构一致（rootElement.firstChildElement("data")）。
if "./$BUILD/graft_run_d" >"$BUILD/run_d.log" 2>&1; then
    printf '  试接跑绿: getWorkerFromIODeviceXmlShape\n'
    sed 's/^/    /' "$BUILD/run_d.log"
else
    printf '  试接跑挂: getWorkerFromIODeviceXmlShape\n'
    sed 's/^/    /' "$BUILD/run_d.log" | head -40
    rc=1
fi

# ④ 判据③：产物不得有 Qt 未定义符号。
undef=$(nm -u "$BUILD/graft_run_d" 2>/dev/null | grep -i qt || true)
if [ -n "$undef" ]; then
    printf '  试接产物含 Qt 符号:\n%s\n' "$undef"
    rc=1
else
    printf '    nm -u graft_run_d | grep -i qt: 无输出\n'
fi

exit "$rc"
