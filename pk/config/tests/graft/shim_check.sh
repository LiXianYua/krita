#!/usr/bin/env bash
# 最终评审 I-2：compat/KisMimeDatabase.h 这层 #define KisMimeDatabase
# PkMimeDatabase 间接从没有被真正编过、跑过——Task 3 的自测直接
# #include "../PkMimeDatabase.h"（绕开了垫片），Task 4 的 18 个真实消费者
# 试接全部卡在 KisMimeDatabase 引用之前更上游的未交付类型上（见
# graft_check.sh 顶部注释），一次都没有走到真正引用 KisMimeDatabase:: 的那一行。
#
# 本脚本独立编译 shim_probe.cpp：它用尖括号 #include <KisMimeDatabase.h>，
# 靠 -I pk/config/compat（不是 -I pk/config）解析到垫片，垫片再把
# KisMimeDatabase:: 重写成 PkMimeDatabase::，验证这条间接链路真的通。
#
# 依赖库查找逻辑照抄 graft_run.sh：libpkconfig.a / libpkstring.a 缺了就地建。
set -eu
cd "$(dirname "$0")/../../../.." || exit 1     # → fork 仓库根

BUILD=pk/config/tests/graft/build
CXX=${CXX:-g++}
rc=0

if [ ! -f pk/config/build/libpkconfig.a ] || [ ! -f pk/config/build/libpkstring.a ]; then
    cmake -S pk/config -B pk/config/build -DCMAKE_BUILD_TYPE=Debug >/dev/null
    cmake --build pk/config/build -j"$(nproc)" >/dev/null
fi

mkdir -p "$BUILD"

# -I pk/container / -I pk/geometry：PkMimeDatabase.h #include "PkStringList.h"，
# PkConfigGroup.h（compat/KConfigGroup 等 M-3 探针会拉到）还 #include
# "PkPoint.h"，同 graft_run.sh 的既有理由（只取头文件目录，两者全部内联/模板，
# 不需要链它们的静态库）。
INC=(-I pk/config/compat -I pk/config -I pk/string/compat -I pk/string -I pk/container -I pk/geometry)

# ① 编译探针：#include <KisMimeDatabase.h> 只靠 compat 的 -I 解析，不额外
#   -include pk/config/PkMimeDatabase.h，确保走的是真实消费者会走的尖括号路径。
if ! "$CXX" -std=c++17 "${INC[@]}" \
    -c pk/config/tests/graft/shim_probe.cpp -o "$BUILD/shim_probe.o" \
    2>"$BUILD/shim_probe_compile.log"; then
    printf '  shim_check 编译失败: shim_probe.cpp\n'
    sed 's/^/    /' "$BUILD/shim_probe_compile.log" | head -80
    exit 1
fi
printf '    编译通过: shim_probe.cpp（#include <KisMimeDatabase.h> 经 compat 垫片解析）\n'

# ② 链接
if ! "$CXX" -std=c++17 "$BUILD/shim_probe.o" \
    pk/config/build/libpkconfig.a pk/config/build/libpkstring.a \
    -o "$BUILD/shim_probe" 2>"$BUILD/shim_probe_link.log"; then
    printf '  shim_check 链接失败\n'
    sed 's/^/    /' "$BUILD/shim_probe_link.log" | head -80
    exit 1
fi

# ③ 跑，并核对断言结果
if "./$BUILD/shim_probe" >"$BUILD/shim_probe_run.log" 2>&1; then
    printf '  shim_check 跑绿:\n'
    sed 's/^/    /' "$BUILD/shim_probe_run.log"
else
    printf '  shim_check 跑挂:\n'
    sed 's/^/    /' "$BUILD/shim_probe_run.log" | head -40
    rc=1
fi

# ④ 产物不得有 Qt 未定义符号
undef=$(nm -u "$BUILD/shim_probe" 2>/dev/null | grep -i qt || true)
if [ -n "$undef" ]; then
    printf '  shim_check 产物含 Qt 符号:\n%s\n' "$undef"
    rc=1
else
    printf '    nm -u shim_probe | grep -i qt: 无输出\n'
fi

# ⑤（M-3，非必做）compat/KConfigGroup、compat/KSharedConfig、
#    compat/ksharedconfig.h 三个垫片此前从没被任何测试/试接编译过——
#    只做 -fsyntax-only 存在性检查，不接入判据①②④。
if ! "$CXX" -std=c++17 -fsyntax-only "${INC[@]}" \
    pk/config/tests/graft/compat_shims_probe.cpp \
    2>"$BUILD/compat_shims_probe_compile.log"; then
    printf '  shim_check（M-3）三个未测垫片编译失败:\n'
    sed 's/^/    /' "$BUILD/compat_shims_probe_compile.log" | head -80
    rc=1
else
    printf '    编译通过（M-3）: compat/KConfigGroup + compat/KSharedConfig + compat/ksharedconfig.h\n'
fi

exit "$rc"
