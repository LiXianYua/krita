#!/usr/bin/env bash
# ============================================================================
# R-18 判据②：拿**真实 Krita 测试类**零改动跑绿 + 标量调用点 driver。
#
# 红线与 R-03 相同：被测源与测试源，**一个字节都不许改**。唯一允许的改动是
# rename.sed 对构建目录里那份**副本**做的 D-23 机械改名。手工改了测试源就说明
# 我们的 API 形状不对 —— 那正是试接要抓的东西，回去改 pk/global 里的标量。
#
# 两个目标：
#   ① TestKoIntegerMaths（target kritapigment，被测 libs/pigment/KoIntegerMaths.h）
#      —— 真实测试类，必须真的跑绿。pk/test（R-11）已用同形态 graft 跑绿过它，
#      harness 形态已被验证；这里用 pk/global 的 -I 顺序重跑一遍，证明折叠后的
#      地基在真实测试类面前不回归。
#   ② driver_global_scalars.cpp —— 复刻 KisLager.h / KisZug.h / kis_algebra_2d.h
#      真实调用点的代码形状 + Task 2 探针值的 driver。**不是真实测试文件**：
#      三个头依赖 lager/zug vendored 库（根 CMake FetchContent，本机无）与
#      QPointF/QLineF 几何类型（归 R-21/R-22 未交付），环境凑不齐，所以按
#      2026-08-18 裁决走 driver 降级路径（四条要求全满足，见 driver 文件头）。
#
# 构建产物落在 pk/global/graft/build/，**不与 pk/global/build/ 混用**
# （那是 tests/run_tests.sh 与 oracle 的地盘）。
#
# ⚠ **libpktest.a 不自建**：硬约束要求它由 pk/global/tests/run_tests.sh（或
#   pk/test 的构建）先产出，缺了就报错退出。libpkglobal.a 按 R-03 形态自建进
#   本 build 目录。
# ============================================================================
set -eu
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

GRAFT=pk/global/graft
BUILD=$GRAFT/build
STUBS=$GRAFT/stubs
SED=$GRAFT/rename.sed
CXX=${CXX:-g++}
rc=0

# -fwrapv：与 pk/global/CMakeLists.txt 里 pkglobal 的 PUBLIC 编译选项一致
#          （qAbs(INT_MIN) 这类有符号溢出 UB 的消费者必须带上）。
# -DPK_TEST_NO_QT_MACRO_ALIASES：关掉 pk/test/compat/QTest 里的
#          QCOMPARE→PK_COMPARE 一类别名。**不许关掉这条** —— 有了它，
#          rename.sed 漏改一处就在编译期报 "'QCOMPARE' was not declared"，
#          试接才真正证明 D-23 的机械改名可行，而不是靠别名把漏改悄悄编过。
CXXFLAGS="-std=c++17 -fwrapv -DPK_TEST_NO_QT_MACRO_ALIASES"

# -I 顺序有讲究，别调（brief 指定）：
#   $STUBS 最前（本轮两个目标都不拉 Krita 内部头，stubs/ 目前只有说明文件，
#           槽位留着，将来目标变多时在同一位插入垫片）。
#   然后 pk/global → pk/global/compat → pk/test → pk/test/compat →
#   pk/geometry/compat 依次。`#include <QtGlobal>` 命中 pk/global/compat 那份
#   （超集链，自己 __has_include 拉 pk/test 与 pk/geometry 两份），pk/test 那份
#   让位机制保证 PkGlobal.h 在检测到 PK_GLOBAL_SCALARS_FROM_PKTEST 后整段让位。
INCS="-I $STUBS -I pk/global -I pk/global/compat -I pk/test -I pk/test/compat \
      -I pk/geometry/compat"

# ---------------------------------------------------------------------------
# 0. 规则表零分叉自证。
#
# $GRAFT/rename.sed 是 pk/test/graft/rename.sed 的**逐字副本**（brief 要求
# "复制，不改原件"）。保持逐字相同才能让下面这条 diff 成为机器可查的证据：
# D-23 的规则表只有一份，没有在 R-18 这边偷偷分叉出第二套。
# ---------------------------------------------------------------------------
if ! diff -q pk/test/graft/rename.sed "$SED" >/dev/null; then
    printf '  rename.sed 与 pk/test/graft/rename.sed 不一致 —— D-23 的规则表分叉了\n' >&2
    diff -u pk/test/graft/rename.sed "$SED" >&2 || true
    exit 1
fi

# ---------------------------------------------------------------------------
# 1. 依赖库。
#    · libpktest.a —— **不自建**，由 pk/global/tests/run_tests.sh 的构建先产出。
#    · libpkglobal.a —— 自建进本 build 目录（R-03 形态：试接有自己的一份，不
#      复用 pk/global/build/ 的产物）。
# ---------------------------------------------------------------------------
mkdir -p "$BUILD"

if [ ! -f pk/global/build/libpktest.a ]; then
    printf '  libpktest.a 不存在 —— 先跑 pk/global/tests/run_tests.sh 的构建步骤\n' >&2
    exit 1
fi

build_lib() {
    local out="$1"; shift
    printf '  建 %s\n' "$out"
    local objs=""
    local src obj
    for src in "$@"; do
        obj="$BUILD/$(basename "${src%.cpp}").o"
        # shellcheck disable=SC2086
        "$CXX" $CXXFLAGS -c "$src" -o "$obj"
        objs="$objs $obj"
    done
    # shellcheck disable=SC2086
    ar rcs "$out" $objs
}

build_lib "$BUILD/libpkglobal.a" pk/global/PkGlobal.cpp

# ---------------------------------------------------------------------------
# 2. run_one —— 目标 ①（真实测试类）。
#
#   $1 name    可执行文件名 / 工作子目录名
#   $2 testdir 测试源所在目录（仓库根相对路径）
#   $3 hdr     测试类头文件名
#   $4 src     测试类 .cpp 文件名
#   $5 incdir  被测头所在的 libs/ 目录（-I 用；KoIntegerMaths.h 在 libs/pigment，
#              经引号 include 的"先按包含者所在目录找"回落到这里）
# ---------------------------------------------------------------------------
run_one() {
    local name="$1" testdir="$2" hdr="$3" src="$4" incdir="$5"
    local work="$BUILD/$name"
    rm -rf "$work"; mkdir -p "$work"

    # ① 复制 —— 源树一个字节都不动
    cp "$testdir/$hdr" "$testdir/$src" "$work/"

    # ② D-23 机械改名，**唯一**允许的改动，且只作用于副本
    sed -i -f "$SED" "$work/$hdr" "$work/$src"

    # ③ 生成 binder（替代 moc 的测试发现）。.inc 而非 .cpp：产物全是类内定义
    #    （隐式 inline），只能被 #include，不能作为独立翻译单元编译。
    python3 pk/test/pk_test_moc.py "$work/$hdr" -o "$work/binder.inc"

    # ③.5 driver.cpp：PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须在同一
    #     翻译单元里看见它的完整定义（Task 5 报告里的 ODR 硬规则）。真实测试类
    #     的 .cpp 只允许 rename.sed 的机械改名、不能往里加 #include，所以这层
    #     粘合只能由 graft 自己的 driver.cpp 来做 —— 它不是复制自源树的文件，
    #     是构建期胶水。
    printf '#include "%s"\n#include "binder.inc"\n' "$src" > "$work/driver.cpp"

    # ④ 编译链接。
    # shellcheck disable=SC2086
    "$CXX" $CXXFLAGS $INCS -I "$incdir" -I "$testdir" -I "$work" \
        "$work/driver.cpp" \
        "$BUILD/libpkglobal.a" pk/global/build/libpktest.a \
        -o "$work/$name" 2>"$work/compile.log" || {
            printf '  试接编译失败: %s\n' "$name"
            sed 's/^/    /' "$work/compile.log" | head -80
            rc=1
            return
        }

    # ⑤ 跑
    if "./$work/$name" >"$work/run.log" 2>&1; then
        printf '  试接跑绿: %s (%s)\n' "$name" "$testdir"
        grep -E '^(PASS|FAIL|Totals)' "$work/run.log" | sed 's/^/    /'
    else
        printf '  试接跑挂: %s\n' "$name"
        sed 's/^/    /' "$work/run.log" | head -40
        rc=1
        return
    fi

    # ⑥ 判据③：产物不得有 Qt 未定义符号。
    #
    # **对这个静态链接的可执行文件这条断言是恒真的**（链接行里没有任何 Qt 库，
    # 真出现未定义的 Qt 符号会在链接期就失败，走不到这里）。留着它是因为判据
    # 要求这种形式的证据，别把它当成"我们查过了"—— 真正有判别力的是
    # tests/run_tests.sh 里对 libpkglobal.a 那条（静态库允许有未定义符号）。
    local undef
    undef=$(nm -u "$work/$name" 2>/dev/null | grep -i qt || true)
    if [ -n "$undef" ]; then
        printf '  试接产物含 Qt 符号: %s\n%s\n' "$name" "$undef"
        rc=1
    else
        printf '    nm -u %s | grep -i qt: 无输出\n' "$name"
    fi
}

# ---------------------------------------------------------------------------
# 3. 目标 ①：TestKoIntegerMaths（真实测试类，必须真的跑绿）
# ---------------------------------------------------------------------------
run_one TestKoIntegerMaths \
        libs/pigment/tests TestKoIntegerMaths.h TestKoIntegerMaths.cpp \
        libs/pigment

# ---------------------------------------------------------------------------
# 4. 目标 ②：driver_global_scalars.cpp（标量调用点 driver，降级路径）。
#
#    不是真实测试类（没有 QObject/slots，不经过 pk_test_moc.py），是一个自包含
#    的 main()：复刻三个真实头的调用形状并断言 Task 2 探针值。编译、跑、nm -u。
# ---------------------------------------------------------------------------
DRIVER_WORK=$BUILD/driver_global_scalars
rm -rf "$DRIVER_WORK"; mkdir -p "$DRIVER_WORK"

# shellcheck disable=SC2086
"$CXX" $CXXFLAGS $INCS \
    "$GRAFT/driver_global_scalars.cpp" "$BUILD/libpkglobal.a" \
    -o "$DRIVER_WORK/driver_global_scalars" 2>"$DRIVER_WORK/compile.log" || {
        printf '  driver 编译失败\n'
        sed 's/^/    /' "$DRIVER_WORK/compile.log" | head -80
        rc=1
    }

if [ "$rc" -eq 0 ]; then
    if "$DRIVER_WORK/driver_global_scalars" >"$DRIVER_WORK/run.log" 2>&1; then
        printf '  driver 跑绿: driver_global_scalars\n'
        grep -E '^(PASS|FAIL|Totals)' "$DRIVER_WORK/run.log" | sed 's/^/    /'
    else
        printf '  driver 跑挂\n'
        sed 's/^/    /' "$DRIVER_WORK/run.log" | head -40
        rc=1
    fi

    if [ "$rc" -eq 0 ]; then
        # 顶层作用域，不用 local（bash 的 local 只能在函数里用）
        undef=$(nm -u "$DRIVER_WORK/driver_global_scalars" 2>/dev/null | grep -i qt || true)
        if [ -n "$undef" ]; then
            printf '  driver 产物含 Qt 符号:\n%s\n' "$undef"
            rc=1
        else
            printf '    nm -u driver_global_scalars | grep -i qt: 无输出\n'
        fi
    fi
fi

# ---------------------------------------------------------------------------
# 5. 源树零改动自证。
#
# 目标① 的真实测试源（libs/pigment/tests）必须一字未动；目标② 的三个真实调用点
# （libs/global 下的 KisLager.h / KisZug.h / kis_algebra_2d.h）是"读来复刻形状"
# 的对象，graft 不复制不修改它们 —— 列在这里让这条自证也覆盖它们。
# ---------------------------------------------------------------------------
GRAFTED_FILES="
libs/pigment/tests/TestKoIntegerMaths.h
libs/pigment/tests/TestKoIntegerMaths.cpp
libs/global/KisLager.h
libs/global/KisZug.h
libs/global/kis_algebra_2d.h
"

# shellcheck disable=SC2086
if ! git diff --quiet -- $GRAFTED_FILES; then
    printf '  源树被改动了 —— 试接必须零改动\n' >&2
    # shellcheck disable=SC2086
    git diff --stat -- $GRAFTED_FILES >&2
    rc=1
else
    printf '  git diff --quiet 自证（5 个文件）: 源树零改动\n'
fi

exit "$rc"
