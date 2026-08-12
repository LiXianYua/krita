#!/usr/bin/env bash
# Task 6a/6b：真实 Krita 测试类经 pk/log + pk/string + pk/test 试接，源树零改动。
# 结构照抄 pk/test/graft/graft_run.sh 的六步（复制→rename.sed 机械改名→生成
# binder→driver.cpp→编译→跑→nm -u 自证），差别：
#   1. 编译行多链 pk/log + pk/string（-I 与静态库）
#   2. 除测试的 .h/.cpp 外，被测实现（非测试文件）原地编译进产物，不复制不改名
#   3. 加 -DQT_NO_DEBUG（理由见 pk/log/tests/graft/kis_debug_build.sh）
set -eu
cd "$(dirname "$0")/../../../.." || exit 1     # → fork 仓库根

BUILD=pk/log/tests/graft/build
SED=pk/test/graft/rename.sed
STUBS=pk/log/tests/graft/stubs
CXX=${CXX:-g++}
rc=0

# 三个依赖静态库，缺了就地建（run_tests.sh 通常已经建过）。
if [ ! -f pk/test/build/libpktest.a ]; then
    cmake -S pk/test -B pk/test/build -DCMAKE_BUILD_TYPE=Debug >/dev/null
    cmake --build pk/test/build -j"$(nproc)" >/dev/null
fi
if [ ! -f pk/string/build/libpkstring.a ]; then
    cmake -S pk/string -B pk/string/build -DCMAKE_BUILD_TYPE=Debug >/dev/null
    cmake --build pk/string/build -j"$(nproc)" >/dev/null
fi
if [ ! -f pk/log/build/libpklog.a ]; then
    cmake -S pk/log -B pk/log/build -DCMAKE_BUILD_TYPE=Debug >/dev/null
    cmake --build pk/log/build -j"$(nproc)" >/dev/null
fi

# $STUBS 必须排在 pk/test/compat 前面：两边都有一个叫 QtGlobal 的文件（pk/test/
# compat/QtGlobal 只给 pk/test 自己用的最小标量集，没有 quint64/qint64 等），
# 角括号 <QtGlobal> 按 -I 顺序查找，谁在前谁赢。我们的 QtGlobal 会自己再用相对
# 路径 quote-include 到 pk/test/compat/QtGlobal（有 #pragma once 兜底），所以
# pk/test 自己内部需要的 qAbs/qFuzzyCompare 仍然可见，顺序调换不丢东西。
COMMON_INC=(-I pk/log -I pk/log/compat -I pk/string -I pk/string/compat -I "$STUBS" -I pk/test -I pk/test/compat)
# kis_debug.h 从没写过 #include <QString>，真 Qt 靠 <QDebug>/<QLoggingCategory>
# 透传把它带进来——同 pk/log/tests/graft/kis_debug_build.sh 的手法，编译参数，
# 不是对调用点的改动。
#
# 第二条 -include 是本试接特有的坑：真实测试头（KisRandomGenerator2DTest.h）
# 直接用 quint64 却不 #include <QtGlobal>——真 Qt 里这条透传链是
# <simpletest.h> → "QObject"(quote) → "QtGlobal"(quote，同目录) 带进来的。
# pk/test/compat/QtGlobal 只放了 pk/test 自己用的最小标量集（qAbs 等），quote-
# include 按"同目录优先"规则，不管 -I 顺序怎么排都会先命中它，我们扩展过的
# pk/log/tests/graft/stubs/QtGlobal（quint64 等在里面）根本插不进这条链。
# -include 把我们的 QtGlobal 提到翻译单元最前面，绕开这条链路——同 QString
# 那条一样，是编译参数，不是对调用点的改动。
FORCE_INCLUDE=(-include "$STUBS/QtGlobal" -include pk/string/compat/QString)
DEFS=(-DQT_NO_DEBUG)

# 评审 Minor 项：这个文件名只在 CMakeLists.txt 的 FetchContent 兜底路径下
# 成立，且 Debug 构建产出 libspdlogd.a、Release 产出 libspdlog.a（无 d
# 后缀）；I-02 把 spdlog 装进依赖前缀之后 find_package 生效，两个文件名都
# 不存在。两个名字都探一遍，都没有就报清楚的错——不在这里解决
# find_package 命中系统安装位置这种更深的情形（那种情形下 spdlog 根本不在
# pk/log/build 下，需要另外的链接方式，超出本条 Minor 修复的范围）。
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
LIBS=(pk/test/build/libpktest.a pk/log/build/libpklog.a pk/string/build/libpkstring.a "$SPDLOG_LIB")

# run_one：试接一个真实测试类。
#   $1 name          试接名，也是产物名
#   $2 srcdir        测试 .h/.cpp 所在目录
#   $3 hdr           测试头文件名
#   $4 src           测试源文件名
#   $5 extra_incdirs 逗号分隔，测试 #include "X.h" 需要的额外 -I（被测实现所在目录）
#   $6 extra_sources 逗号分隔，被测实现的 .cpp（原地编译，不复制不改名）
#   $7 extra_force   逗号分隔，只对本试接的 driver 生效的额外 -include（相对仓库根）
#   $8 probe_file    可选，一段要 cat 进 driver.cpp 尾部的运行期探针源
#                    （相对仓库根）。给了这个参数时，driver.cpp 会先把 main
#                    宏定义成 pk_r08_original_main 再 #include 测试源——测试
#                    源展开出的 main（SIMPLE_TEST_MAIN）因此改名，不与探针
#                    自己定义的新 main 冲突；探针跑完再转调 pk_r08_original_main
#                    把真实测试跑掉。测试源一个字节没改，只是 driver.cpp 自己
#                    的预处理器把名字换了——见 kis_shared_ptr_probe.inc 顶注。
run_one() {
    local name="$1" srcdir="$2" hdr="$3" src="$4" extra_incdirs="$5" extra_sources="$6" extra_force="${7:-}" probe_file="${8:-}"
    local work="$BUILD/$name"
    rm -rf "$work"; mkdir -p "$work"

    # ① 复制 —— 源树一个字节都不动
    cp "$srcdir/$hdr" "$srcdir/$src" "$work/"

    # ② D-23 机械改名，只此一项改动
    sed -i -f "$SED" "$work/$hdr" "$work/$src"

    # ③ 生成 binder（替代 moc 的测试发现）
    python3 pk/test/pk_test_moc.py "$work/$hdr" -o "$work/binder.inc"

    # ③.5 driver.cpp：把改名后的测试 .cpp 与 binder.inc 塞进同一个 TU
    if [ -n "$probe_file" ]; then
        {
            printf '// 运行期探针挂接，见 %s。\n' "$probe_file"
            printf '#include "PkLogSink.h"\n'
            printf '#include <cstdio>\n#include <cstdlib>\n#include <string>\n#include <vector>\n'
            printf '#define main pk_r08_original_main\n'
            printf '#include "%s"\n' "$src"
            printf '#undef main\n'
            printf '#include "binder.inc"\n\n'
        } > "$work/driver.cpp"
        cat "$probe_file" >> "$work/driver.cpp"
    else
        printf '#include "%s"\n#include "binder.inc"\n' "$src" > "$work/driver.cpp"
    fi

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
    if ! "$CXX" -std=c++17 -DPK_TEST_NO_QT_MACRO_ALIASES "${DEFS[@]}" "${driver_force[@]}" \
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
            if ! "$CXX" -std=c++17 "${DEFS[@]}" "${FORCE_INCLUDE[@]}" \
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
    if ! "$CXX" -std=c++17 "${objs[@]}" "${LIBS[@]}" -o "$work/$name" 2>"$work/link.log"; then
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

run_one KisRandomGenerator2DTest \
    libs/image/tests KisRandomGenerator2DTest.h KisRandomGenerator2DTest.cpp \
    "libs/image,libs/global" \
    "libs/image/KisRandomGenerator2D.cpp,libs/global/kis_debug.cpp"

# Task 6b：kis_shared_ptr_test.cpp 不 #include <QScopedPointer>/<QVector>——
# 真 Qt 靠别的头透传把它们带进来，我们没有复刻那条透传链，只能整体前置绕开
# （同 QString/QtGlobal 那两条的手法，是编译参数不是对调用点的改动）。
# kis_shared_ptr.h:479 直接用 QAtomicInt 却不 #include <QAtomicInt>（真 Qt 靠
# 别的头把它带进来），同样的坑同样的手法。
run_one KisSharedPtrTest \
    libs/image/tests kis_shared_ptr_test.h kis_shared_ptr_test.cpp \
    "libs/global,libs/image" \
    "libs/global/kis_shared.cpp,libs/global/kis_debug.cpp" \
    "$STUBS/QAtomicInt,$STUBS/QScopedPointer,$STUBS/QVector" \
    pk/log/tests/graft/kis_shared_ptr_probe.inc

# 源树零改动自证
if ! git diff --quiet -- libs/image/tests libs/image libs/global; then
    printf '  源树被改动了 —— 试接必须零改动\n' >&2
    git diff --stat -- libs/image/tests libs/image libs/global >&2
    rc=1
fi

exit "$rc"
