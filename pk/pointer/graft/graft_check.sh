#!/usr/bin/env bash
# R-04 Task 3：两个真实 Krita 测试类经 pk/pointer（本任务）+ pk/container（R-02）
# 试接，源树零改动。结构照抄 pk/log/tests/graft/graft_run.sh 的六步（复制→
# rename.sed 机械改名→生成 binder→driver.cpp→编译→跑→nm -u 自证），差别：
#   1. 编译行链 pk/pointer + pk/container + pk/geometry + pk/log + pk/string
#      （五个只读依赖，与 pk/pointer/CMakeLists.txt 消费 pk/container/pk/test
#      的方式同源：只读，不改）。
#   2. QScopedPointer 与 QVector 走**本任务与 R-02 交付的真品**（compat/），不是
#      R-08 当年（那两个类型都还没交付时）留下的局部垫片——这正是本试接要证明
#      的事：真品接得上真实调用点。QAtomicInt（R-10）与 qConstOverload（R-18）
#      仍未交付，继续用局部垫片，见 stubs/ 下两个文件头部注释。
#   3. 除测试的 .h/.cpp 外，被测实现（非测试文件）原地编译进产物，不复制不改名。
#   4. 加 -DQT_NO_DEBUG（理由见 pk/log/tests/graft/kis_debug_build.sh，本任务
#      沿用同一条：kis_debug.h 依赖 QT_NO_DEBUG 语义关掉一段 Q_ASSERT 重定义）。
set -e
source /mnt/ssd-disk/liyang/projects/krita-ci-env/env
export CCACHE_DIR="$KDECI_CC_CACHE"
set -u
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

BUILD=pk/pointer/graft/build/ninja
DEPS_BUILD="$BUILD/deps"
SED=pk/pointer/graft/rename.sed
STUBS=pk/pointer/graft/stubs
CXX=${CXX:-g++}
CXX_CMD=(ccache "$CXX")
rc=0

# 五个只读依赖在本任务的 ignored build/ 下分别配置；不复用它们
# 源目录旁可能由别的任务留下的 Makefiles cache。
configure_dependency() {
    local module="$1" target="$2"
    local dep_build="$DEPS_BUILD/$module"
    cmake -S "pk/$module" -B "$dep_build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache >/dev/null
    cmake --build "$dep_build" --target "$target" -j"$(nproc)" >/dev/null
}
configure_dependency test pktest
configure_dependency string pkstring
configure_dependency log pklog
configure_dependency container pkcontainer
configure_dependency geometry pkgeometry

# $STUBS 必须排在 pk/test/compat 前面：两边都有一个叫 QtGlobal 的文件
#（pk/test/compat/QtGlobal 只给 pk/test 自己用的最小标量集，没有 quint64/
# Q_ASSERT 等）。角括号 <QtGlobal> 按 -I 顺序查找，谁在前谁赢。我们的 QtGlobal
# 会自己再用相对路径 quote-include 到 pk/test/compat/QtGlobal（有 #pragma once
# 兜底），所以 pk/test 自己内部需要的 qAbs/qFuzzyCompare 仍然可见，顺序调换不
# 丢东西。同理 pk/pointer/compat、pk/container/compat 排在最前，保证
# <QSharedPointer>/<QScopedPointer>/<QVector> 解析到本任务/R-02 的真品。
COMMON_INC=(-I pk/pointer -I pk/pointer/compat -I pk/container -I pk/container/compat \
            -I pk/geometry -I pk/geometry/compat \
            -I pk/log -I pk/log/compat -I pk/string -I pk/string/compat \
            -I "$STUBS" -I pk/test -I pk/test/compat)

# kis_debug.h 从没写过 #include <QString>，真 Qt 靠 <QDebug>/<QLoggingCategory>
# 透传把它带进来——同 pk/log/tests/graft/kis_debug_build.sh 的手法，编译参数，
# 不是对调用点的改动。两个候选统一带上，无害（QAtomicInt/QScopedPointer/QVector
# 各自只在需要它的候选里追加，见 run_one 的 extra_force 形参）。
FORCE_INCLUDE=(-include "$STUBS/QtGlobal" -include pk/string/compat/QString)
DEFS=(-DQT_NO_DEBUG)

SPDLOG_LIB=""
for cand in "$DEPS_BUILD/log/libspdlogd.a" "$DEPS_BUILD/log/libspdlog.a"; do
    if [ -f "$cand" ]; then SPDLOG_LIB="$cand"; break; fi
done
if [ -z "$SPDLOG_LIB" ]; then
    printf 'graft_check.sh: 找不到 spdlog 静态库（试过 libspdlogd.a 与 libspdlog.a，\n' >&2
    printf '  pk/log/build 下都没有）。这条探测只覆盖 pk/log/CMakeLists.txt 的\n' >&2
    printf '  FetchContent 兜底路径；如果这次是 find_package 命中了系统安装的\n' >&2
    printf '  spdlog，请另外确认链接方式。\n' >&2
    exit 1
fi
LIBS=("$DEPS_BUILD/test/libpktest.a" "$DEPS_BUILD/log/libpklog.a"
      "$DEPS_BUILD/string/libpkstring.a" "$DEPS_BUILD/container/libpkcontainer.a"
      "$DEPS_BUILD/geometry/libpkgeometry.a" "$SPDLOG_LIB")

# run_one：试接一个真实测试类。
#   $1 name          试接名，也是产物名
#   $2 srcdir        测试 .h/.cpp 所在目录
#   $3 hdr           测试头文件名
#   $4 src           测试源文件名
#   $5 extra_incdirs 逗号分隔，测试 #include "X.h" 需要的额外 -I（被测实现所在目录）
#   $6 extra_sources 逗号分隔，被测实现的 .cpp（原地编译，不复制不改名）
#   $7 extra_force   逗号分隔，只对本试接的 driver 生效的额外 -include（相对仓库根）
run_one() {
    local name="$1" srcdir="$2" hdr="$3" src="$4" extra_incdirs="$5" extra_sources="$6" extra_force="${7:-}"
    local work="$BUILD/$name"
    rm -rf "$work"; mkdir -p "$work"

    # ① 复制 —— 源树一个字节都不动
    cp "$srcdir/$hdr" "$srcdir/$src" "$work/"

    # ② D-23 机械改名，只此一项改动
    sed -i -f "$SED" "$work/$hdr" "$work/$src"

    # ③ 生成 binder（替代 moc 的测试发现）
    python3 pk/test/pk_test_moc.py "$work/$hdr" -o "$work/binder.inc"

    # ③.5 driver.cpp：把改名后的测试 .cpp 与 binder.inc 塞进同一个 TU
    printf '#include "%s"\n#include "binder.inc"\n' "$src" > "$work/driver.cpp"

    local extra_inc_flags=()
    if [ -n "$extra_incdirs" ]; then
        local d; IFS=',' read -ra _dirs <<< "$extra_incdirs"
        for d in "${_dirs[@]}"; do extra_inc_flags+=(-I "$d"); done
    fi

    local driver_force=("${FORCE_INCLUDE[@]}")
    if [ -n "$extra_force" ]; then
        local f; IFS=',' read -ra _forces <<< "$extra_force"
        for f in "${_forces[@]}"; do driver_force+=(-include "$f"); done
    fi

    # ④ 编译 driver（测试 TU）
    local objs=("$work/driver.o")
    if ! "${CXX_CMD[@]}" -std=c++17 -DPK_TEST_NO_QT_MACRO_ALIASES "${DEFS[@]}" "${driver_force[@]}" \
        "${COMMON_INC[@]}" "${extra_inc_flags[@]}" -I "$srcdir" -I "$work" \
        -c "$work/driver.cpp" -o "$work/driver.o" 2>"$work/compile_driver.log"; then
        printf '  试接编译失败: %s (driver)\n' "$name"
        sed 's/^/    /' "$work/compile_driver.log" | head -80
        rc=1; return
    fi

    # 被测实现：原地编译（不复制、不改名），一起进产物
    if [ -n "$extra_sources" ]; then
        local es; IFS=',' read -ra _extra_srcs <<< "$extra_sources"
        for es in "${_extra_srcs[@]}"; do
            local objname; objname="$work/$(basename "$es" .cpp).o"
            if ! "${CXX_CMD[@]}" -std=c++17 "${DEFS[@]}" "${FORCE_INCLUDE[@]}" \
                "${COMMON_INC[@]}" "${extra_inc_flags[@]}" \
                -c "$es" -o "$objname" 2>"$work/compile_$(basename "$es" .cpp).log"; then
                printf '  试接编译失败: %s (%s)\n' "$name" "$es"
                sed 's/^/    /' "$work/compile_$(basename "$es" .cpp).log" | head -80
                rc=1; return
            fi
            objs+=("$objname")
        done
    fi

    # ⑤ 链接
    if ! "${CXX_CMD[@]}" -std=c++17 "${objs[@]}" "${LIBS[@]}" -o "$work/$name" 2>"$work/link.log"; then
        printf '  试接链接失败: %s\n' "$name"
        sed 's/^/    /' "$work/link.log" | head -80
        rc=1; return
    fi

    # 跑
    if "./$work/$name" >"$work/run.log" 2>&1; then
        printf '  试接跑绿: %s (%s)\n' "$name" "$srcdir"
        grep -E '^(PASS|FAIL|Totals)' "$work/run.log" | sed 's/^/    /'
    else
        printf '  试接跑挂: %s\n' "$name"
        sed 's/^/    /' "$work/run.log" | head -40
        rc=1; return
    fi

    # ⑥ 判据③：产物不得有 Qt 未定义符号。
    local undef
    undef=$(nm -u "$work/$name" 2>/dev/null | grep -i qt || true)
    if [ -n "$undef" ]; then
        printf '  试接产物含 Qt 符号: %s\n%s\n' "$name" "$undef"
        rc=1
    else
        printf '    nm -u %s | grep -i qt: 无输出\n' "$name"
    fi
}

# 候选①：libs/image/tests/kis_shared_ptr_test.cpp（target kritaimage）。
# 不 #include <QScopedPointer>/<QVector>/<QAtomicInt>——真 Qt 靠别的头透传把它们
# 带进来，我们没有复刻那条透传链，只能整体前置绕开（同 QString/QtGlobal 那两条
# 的手法，是编译参数不是对调用点的改动）。QScopedPointer/QVector 走真品
# （pk/pointer、pk/container），QAtomicInt 走局部垫片（stubs/QAtomicInt，归 R-10）。
run_one KisSharedPtrTest \
    libs/image/tests kis_shared_ptr_test.h kis_shared_ptr_test.cpp \
    "libs/global,libs/image" \
    "libs/global/kis_shared.cpp,libs/global/kis_debug.cpp" \
    "$STUBS/QAtomicInt,pk/pointer/compat/QScopedPointer,pk/container/compat/QVector"

# 候选②：libs/global/tests/KisMplTest.cpp（target kritaglobal，与候选①不同
# target）。KisMplTest.h 只 #include <QTest>（不像候选①那样经 <simpletest.h>
# 透传 QObject）——真 Qt 的 <QtTest/qtest.h> 会透传 <QtCore/qobject.h>，我们的
# compat/QTest 没有复刻那条透传链，需要强制把 QObject 垫片提到最前面。
# QSharedPointer 同理：真 Qt 的 <QDebug>（qdebug.h:54）透传
# <QtCore/qsharedpointer.h>，我们的 compat/QDebug 不做这层透传。
# qConstOverload 归 R-18，未交付，用局部垫片（stubs/qConstOverload）。
# QRect 同理：真 Qt 的 <QTest>（qtestcase.h 经 qtest.h:65 透传
# <QtCore/qrect.h>）把它带进来，我们的 compat/QTest 不做这层透传，走 R-03
# 已交付的真品（pk/geometry/compat/QRect）。
# std::accumulate（<numeric>）：KisMplTest.cpp 自己没 #include <numeric>，真 Qt
# 下某个透传链（Qt 的重量级头互相 #include 得很随意）把它带了进来；这是标准库
# 头的透传缺口，不是 Qt 类型替代缺口，同一种"编译参数补透传"手法处理。
run_one KisMplTest \
    libs/global/tests KisMplTest.h KisMplTest.cpp \
    "libs/global" \
    "libs/global/kis_shared.cpp" \
    "pk/test/compat/QObject,pk/pointer/compat/QSharedPointer,pk/geometry/compat/QRect,$STUBS/qConstOverload,numeric"

# 源树零改动自证
if ! git diff --quiet -- libs/image/tests libs/global/tests libs/image libs/global; then
    printf '  源树被改动了 —— 试接必须零改动\n' >&2
    git diff --stat -- libs/image/tests libs/global/tests libs/image libs/global >&2
    rc=1
fi

exit "$rc"
