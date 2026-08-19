#!/usr/bin/env bash
# R-17 Task 3 判据②：候选 1 —— libs/resources/KisDatabaseTransactionLock.{h,cpp}
# 零改动编译试接，链接真实、未修改的 .o + graft_run_candidate1_driver.cpp
# （探针驱动，见该文件头注释），跑绿。
#
# 六段式结构照抄 pk/config/tests/graft/graft_run.sh（①原地编译生产 .cpp
# ②编译探针驱动③链接④跑⑤nm -u判据③⑥git diff --quiet零改动自证）——本候选
# 没有专门的 kis_add_test 隔离测试类（R-17 plan §4 问 2 的结论：SQL 访问层
# 没有 1-2 个"专门测 SQL 层"的 Krita 测试类），所以用探针驱动直接调用真实、
# 未修改的 KisDatabaseTransactionLockAdapter::lock()/unlock()/commit()，
# 同 KisCumulativeUndoData 那份先例的形态。
set -eu
cd "$(dirname "$0")/../../../.." || exit 1     # → fork 仓库根

GRAFT=pk/sql/tests/graft
BUILD=$GRAFT/build
STUBS=$GRAFT/stubs
CXX=${CXX:-g++}
rc=0

# ---------------------------------------------------------------------------
# 依赖库：pk/sql 自己的 build（libpksql.a + 它嵌入编译的 pkstring/pkvariant/
# pkgeometry + FetchContent 的 sqlite3_vendored，见 pk/sql/CMakeLists.txt）
# 与 pk/log（libpklog.a + spdlog，QDebug/qWarning 需要）两处 build 目录各自
# 独立，不复用彼此。
# ---------------------------------------------------------------------------
if [ ! -f pk/sql/build/libpksql.a ]; then
    cmake -S pk/sql -B pk/sql/build -DCMAKE_BUILD_TYPE=Debug >/dev/null
    cmake --build pk/sql/build -j"$(nproc)" >/dev/null
fi
if [ ! -f pk/log/build/libpklog.a ]; then
    cmake -S pk/log -B pk/log/build -DCMAKE_BUILD_TYPE=Debug >/dev/null
    cmake --build pk/log/build -j"$(nproc)" >/dev/null
fi

# pk/log 的 spdlog 静态库：Debug 构建下 FetchContent 兜底产出 libspdlogd.a，
# Release 产出 libspdlog.a（无 d 后缀）；find_package 命中系统安装位置时两个
# 都不存在（同 pk/config/tests/graft/graft_run.sh 的探测手法）。
SPDLOG_LIB=""
for cand in pk/log/build/libspdlogd.a pk/log/build/libspdlog.a; do
    if [ -f "$cand" ]; then SPDLOG_LIB="$cand"; break; fi
done
if [ -z "$SPDLOG_LIB" ]; then
    printf 'graft_run_candidate1.sh: 找不到 spdlog 静态库（试过 libspdlogd.a 与\n' >&2
    printf '  libspdlog.a，pk/log/build 下都没有）。这条探测只覆盖\n' >&2
    printf '  CMakeLists.txt 的 FetchContent 兜底路径；如果这次是 find_package\n' >&2
    printf '  命中了系统安装的 spdlog，请另外确认链接方式。\n' >&2
    exit 1
fi

# sqlite3 静态库：pk/sql/CMakeLists.txt PRIVATE 链接，产物名与 find_package/
# FetchContent 两条路径不同（系统路径下没有独立 .a，FetchContent 兜底路径
# 产出 libsqlite3_vendored.a），探测方式同 SPDLOG_LIB。driver.cpp 直接调用
# sqlite3_exec() 建表（PkSqlDatabase 不暴露"执行任意 SQL"之外的建表能力），
# 所以还需要它的头文件目录。
SQLITE_LIB=""
SQLITE_INC=""
if [ -f pk/sql/build/libsqlite3_vendored.a ]; then
    SQLITE_LIB=pk/sql/build/libsqlite3_vendored.a
    SQLITE_INC=$(find pk/sql/build/_deps -maxdepth 1 -type d -name "sqlite3_amalgamation-src" | head -1)
fi

rm -rf "$BUILD"; mkdir -p "$BUILD"

# -I 顺序：$STUBS 必须最靠前——kritaresources_export.h/kis_assert.h 在别处
# 也可能有同名候选，我们要的是本目录下这份占位。
INC=(-I "$STUBS" -I pk/sql -I pk/sql/compat -I pk/string -I pk/string/compat \
     -I pk/variant -I pk/container -I pk/geometry -I pk/log -I pk/log/compat \
     -I libs/global -I libs/resources)
if [ -n "$SQLITE_INC" ]; then
    INC+=(-I "$SQLITE_INC")
fi
DEFS=(-DQT_NO_DEBUG)

# ① 被测实现：libs/resources/KisDatabaseTransactionLock.cpp 原地编译，
#    不复制不改名。
if ! "$CXX" -std=c++17 "${DEFS[@]}" "${INC[@]}" \
    -c libs/resources/KisDatabaseTransactionLock.cpp \
    -o "$BUILD/KisDatabaseTransactionLock.o" \
    2>"$BUILD/compile_impl.log"; then
    printf '  试接编译失败: KisDatabaseTransactionLock.cpp\n'
    sed 's/^/    /' "$BUILD/compile_impl.log" | head -80
    exit 1
fi

# ② 探针驱动：graft_run_candidate1_driver.cpp 直接 include 真实、未修改的
#    KisDatabaseTransactionLock.h + pk/sql 的公开头。
if ! "$CXX" -std=c++17 "${DEFS[@]}" "${INC[@]}" \
    -c "$GRAFT/graft_run_candidate1_driver.cpp" -o "$BUILD/driver.o" \
    2>"$BUILD/compile_driver.log"; then
    printf '  试接编译失败: driver.cpp\n'
    sed 's/^/    /' "$BUILD/compile_driver.log" | head -80
    exit 1
fi

# ③ 链接
LINK_LIBS=(pk/sql/build/libpksql.a pk/sql/build/libpkvariant.a \
           pk/sql/build/libpkgeometry.a pk/sql/build/libpkstring.a \
           pk/log/build/libpklog.a "$SPDLOG_LIB")
if [ -n "$SQLITE_LIB" ]; then
    LINK_LIBS+=("$SQLITE_LIB")
fi
if ! "$CXX" -std=c++17 "$BUILD/KisDatabaseTransactionLock.o" "$BUILD/driver.o" \
    "${LINK_LIBS[@]}" -lpthread \
    -o "$BUILD/graft_candidate1" 2>"$BUILD/link.log"; then
    printf '  试接链接失败\n'
    sed 's/^/    /' "$BUILD/link.log" | head -80
    exit 1
fi

# ④ 跑
if "./$BUILD/graft_candidate1" >"$BUILD/run.log" 2>&1; then
    printf '  试接跑绿: KisDatabaseTransactionLock graft driver\n'
    sed 's/^/    /' "$BUILD/run.log"
else
    printf '  试接跑挂: KisDatabaseTransactionLock graft driver\n'
    sed 's/^/    /' "$BUILD/run.log" | head -40
    rc=1
fi

# ⑤ 判据③：产物不得有 Qt 未定义符号。
undef=$(nm -u "$BUILD/graft_candidate1" 2>/dev/null | grep -i qt || true)
if [ -n "$undef" ]; then
    printf '  试接产物含 Qt 符号:\n%s\n' "$undef"
    rc=1
else
    printf '    nm -u graft_candidate1 | grep -i qt: 无输出\n'
fi

# ⑥ 源树零改动自证。
if ! git diff --quiet -- libs/resources libs/global; then
    printf '  源树被改动了 —— 试接必须零改动\n' >&2
    git diff --stat -- libs/resources libs/global >&2
    rc=1
else
    printf '    git diff --quiet -- libs/resources libs/global: 干净\n'
fi

exit "$rc"
