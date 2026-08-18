#!/usr/bin/env bash
# Task 2：libs/command/KisCumulativeUndoData.cpp 零改动，经 PkConfigGroup + PkSharedConfig
# 编译试接。没有可用的 kis_add_test 隔离测试类压这个能力（见
# .superpowers/sdd/R-09/task-2-brief.md 的说明），用探针驱动直接调用真实、未修改的
# read()/write()。
#
# 与 task-2-brief.md 草稿的差异（实测核对过，见 task-2-report.md）：
#   - pk/string 不单独在 pk/string/build 建：pk/config/CMakeLists.txt 用
#     add_subdirectory 把它嵌进 pk/config/build，libpkstring.a 产在
#     pk/config/build/ 下，不是 pk/string/build/ 下。
#   - 不链 pk/container / pk/geometry 的静态库（本任务只用它们的头文件，
#     PkStringList/PkPoint 全部内联/模板，pkconfig 自己也没链它们）——但仍需要
#     它们的 -I，因为 PkConfigGroup.h #include 了 PkStringList.h / PkPoint.h。
#   - pk/log 私有链接 spdlog（target_link_libraries PRIVATE），静态库不透传
#     符号，最终链接要显式带上 libspdlogd.a（Debug 构建产物名）。
#   - PkPoint.h 会拉入 pk/geometry/PkGlobal.h，无条件定义 qAbs；kis_assert.h
#     经 <QtGlobal> 拉入的 pk/test/compat/QtGlobal 也定义 qAbs——两者在同一
#     TU 里会撞（"redefinition of qAbs"）。PkGlobal.h 自己有让位机制：探测到
#     宏 qFuzzyCompare 已定义就跳过自己的 qAbs/qFuzzyCompare/qFuzzyIsNull。
#     所以 FORCE 数组必须先把 stubs/QtGlobal（间接 include 真正定义
#     qFuzzyCompare 的 pk/test/compat/QtGlobal）排在 compat/kconfiggroup.h
#     （间接拉 PkGlobal.h）前面——顺序错了就编不过。
set -eu
cd "$(dirname "$0")/../../../.." || exit 1     # → fork 仓库根

BUILD=pk/config/tests/graft/build
STUBS=pk/config/tests/graft/stubs
CXX=${CXX:-g++}
rc=0

if [ ! -f pk/config/build/libpkconfig.a ] || [ ! -f pk/config/build/libpkstring.a ]; then
    cmake -S pk/config -B pk/config/build -DCMAKE_BUILD_TYPE=Debug >/dev/null
    cmake --build pk/config/build -j"$(nproc)" >/dev/null
fi
if [ ! -f pk/log/build/libpklog.a ]; then
    cmake -S pk/log -B pk/log/build -DCMAKE_BUILD_TYPE=Debug >/dev/null
    cmake --build pk/log/build -j"$(nproc)" >/dev/null
fi

# pk/log 的 spdlog 静态库：Debug 构建下 FetchContent 兜底产出 libspdlogd.a，
# Release 产出 libspdlog.a（无 d 后缀）；find_package 命中系统安装位置时两个
# 都不存在（同 pk/log/tests/graft/graft_run.sh 的探测手法）。
SPDLOG_LIB=""
for cand in pk/log/build/libspdlogd.a pk/log/build/libspdlog.a; do
    if [ -f "$cand" ]; then SPDLOG_LIB="$cand"; break; fi
done
if [ -z "$SPDLOG_LIB" ]; then
    printf 'graft_run.sh: 找不到 spdlog 静态库（试过 libspdlogd.a 与 libspdlog.a，\n' >&2
    printf '  pk/log/build 下都没有）。这条探测只覆盖 CMakeLists.txt 的\n' >&2
    printf '  FetchContent 兜底路径；如果这次是 find_package 命中了系统安装的\n' >&2
    printf '  spdlog，请另外确认链接方式。\n' >&2
    exit 1
fi

rm -rf "$BUILD"; mkdir -p "$BUILD"

# -I pk/container / -I pk/geometry：只取头文件目录（PkConfigGroup.h 需要
# PkStringList.h / PkPoint.h），不链它们的静态库——见文件头注释。
INC=(-I pk/config -I pk/config/compat -I pk/log -I pk/log/compat -I pk/string -I pk/string/compat \
     -I pk/container -I pk/geometry -I "$STUBS" -I libs/global -I libs/command)
DEFS=(-DQT_NO_DEBUG)
# 顺序有讲究，见文件头注释：stubs/QtGlobal 必须排第一个，抢在
# compat/kconfiggroup.h（间接拉 PkGlobal.h）前面定义好 qFuzzyCompare，
# PkGlobal.h 的让位分支才会生效，否则 qAbs 会被定义两次。
FORCE=(-include "$STUBS/QtGlobal" -include pk/config/compat/kconfiggroup.h -include pk/string/compat/QString)

# ① 被测实现：libs/command/KisCumulativeUndoData.cpp 原地编译，不复制不改名
if ! "$CXX" -std=c++17 "${DEFS[@]}" "${FORCE[@]}" "${INC[@]}" \
    -c libs/command/KisCumulativeUndoData.cpp -o "$BUILD/KisCumulativeUndoData.o" \
    2>"$BUILD/compile_impl.log"; then
    printf '  试接编译失败: KisCumulativeUndoData.cpp\n'
    sed 's/^/    /' "$BUILD/compile_impl.log" | head -80
    exit 1
fi

# ② 探针驱动：driver.cpp 直接 include 真实头 + 探针源
{
    printf '#include "KisCumulativeUndoData.h"\n'
    printf '#include "PkSharedConfig.h"\n'
    printf '#include "PkConfigGroup.h"\n'
    printf '#include "pk/config/tests/graft/probe/kis_cumulative_undo_data_probe.inc"\n'
} > "$BUILD/driver.cpp"

if ! "$CXX" -std=c++17 "${DEFS[@]}" "${FORCE[@]}" "${INC[@]}" -I . \
    -c "$BUILD/driver.cpp" -o "$BUILD/driver.o" 2>"$BUILD/compile_driver.log"; then
    printf '  试接编译失败: driver.cpp\n'
    sed 's/^/    /' "$BUILD/compile_driver.log" | head -80
    exit 1
fi

# ③ 链接
if ! "$CXX" -std=c++17 "$BUILD/driver.o" "$BUILD/KisCumulativeUndoData.o" \
    pk/config/build/libpkconfig.a pk/log/build/libpklog.a pk/config/build/libpkstring.a "$SPDLOG_LIB" \
    -o "$BUILD/probe" 2>"$BUILD/link.log"; then
    printf '  试接链接失败\n'
    sed 's/^/    /' "$BUILD/link.log" | head -80
    exit 1
fi

# ④ 跑
if "./$BUILD/probe" >"$BUILD/run.log" 2>&1; then
    printf '  试接跑绿: KisCumulativeUndoData probe\n'
    sed 's/^/    /' "$BUILD/run.log"
else
    printf '  试接跑挂: KisCumulativeUndoData probe\n'
    sed 's/^/    /' "$BUILD/run.log" | head -40
    rc=1
fi

# ⑤ 判据③：产物不得有 Qt 未定义符号
undef=$(nm -u "$BUILD/probe" 2>/dev/null | grep -i qt || true)
if [ -n "$undef" ]; then
    printf '  试接产物含 Qt 符号:\n%s\n' "$undef"
    rc=1
else
    printf '    nm -u probe | grep -i qt: 无输出\n'
fi

# ⑥ 源树零改动自证
if ! git diff --quiet -- libs/command; then
    printf '  源树被改动了 —— 试接必须零改动\n' >&2
    git diff --stat -- libs/command >&2
    rc=1
else
    printf '    git diff --quiet -- libs/command: 干净\n'
fi

exit "$rc"
