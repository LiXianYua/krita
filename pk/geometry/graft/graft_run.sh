#!/usr/bin/env bash
# ============================================================================
# R-03 判据②：拿**真实 Krita 测试类**零改动跑绿。
#
# 单测是我们自己写的、对拍比的是行为 —— 只有试接能证明**API 形状**真的对得上
# 调用点。所以这个脚本的红线是：
#
#   被测源与测试源，**一个字节都不许改**。唯一允许的改动是 rename.sed 对
#   构建目录里那份**副本**做的 D-23 机械改名。手工改了测试源就说明我们的
#   API 形状不对 —— 那正是试接要抓的东西，回去改 pk/geometry 里的类型。
#
# 形态照 pk/test/graft/graft_run.sh（R-11 交付）自建 —— 那份不在 R-03 的 locks
# 里，不能改，所以这里是同形态的第二个 runner，不是对它的修改。
#
# 两个目标：
#   ① KisRectsGridTest       （target kritaglobal，被测 libs/global/KisRectsGrid.{h,cpp}）
#   ② KisFourPointInterpolatorTest（target kritaimage，被测两个全 inline 的头）
#
# 构建产物落在 pk/geometry/graft/build/，**不与 pk/geometry/build/ 混用**
# （那是 tests/run_tests.sh 与 oracle 的地盘）。
# ============================================================================
set -eu
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

GRAFT=pk/geometry/graft
BUILD=$GRAFT/build
STUBS=$GRAFT/stubs
SED=$GRAFT/rename.sed
CXX=${CXX:-g++}
rc=0

# -fwrapv：与 pk/geometry/CMakeLists.txt 里 pkgeometry 的 PUBLIC 编译选项一致。
#          理由是 PkPoint 的整数运算在 INT_MIN/INT_MAX 上是有符号溢出 UB，
#          消费者也必须带上（见那份 CMakeLists 的长注释）。试接就是消费者。
# -DPK_TEST_NO_QT_MACRO_ALIASES：关掉 pk/test/compat/QTest 里的
#          QCOMPARE→PK_COMPARE 一类别名。**不许关掉这条** —— 有了它，
#          rename.sed 漏改一处就在编译期报 "'QCOMPARE' was not declared"，
#          试接才真正证明 D-23 的机械改名可行，而不是靠别名把漏改悄悄编过。
CXXFLAGS="-std=c++17 -fwrapv -DPK_TEST_NO_QT_MACRO_ALIASES"

# -I 顺序有讲究，别调：
#   $STUBS 必须最靠前 —— kis_debug.h / kis_algebra_2d.h 在 libs/global 里也有
#   同名真品，我们要的是垫片那份；而 kis_assert.h / kis_global.h /
#   kis_lod_transform_base.h 在 $STUBS 里**故意没有**同名文件，于是自然落到
#   libs/global 的真品上。哪些用真品、哪些用垫片，就是靠这个"垫片目录里有没有
#   同名文件"来表达的，不是靠 -I 顺序碰运气。
#   —— 但注意 `#include "x.h"` 先按**包含者所在目录**找：真品 kis_global.h 里的
#   `#include "kis_assert.h"` 与 `#include "kis_pointer_utils.h"` 一定落到
#   libs/global 的真品上，-I 顺序管不着。被测源因此也要复制进构建目录
#   （见 run_one 第 ① 步），否则它的引号 include 全部落回 libs/global。
INCS="-I $STUBS -I pk/test -I pk/test/compat -I pk/geometry -I pk/geometry/compat \
      -I pk/string -I pk/string/compat"

# ---------------------------------------------------------------------------
# 0. 规则表零分叉自证。
#
# $GRAFT/rename.sed 是 pk/test/graft/rename.sed 的**逐字副本**（brief 要求
# "复制，不改原件"）。保持逐字相同才能让下面这条 diff 成为机器可查的证据：
# D-23 的规则表只有一份，没有在 R-03 这边偷偷分叉出第二套。
# 规则表要加项时两边一起加，这条 diff 会盯着。
# ---------------------------------------------------------------------------
if ! diff -q pk/test/graft/rename.sed "$SED" >/dev/null; then
    printf '  rename.sed 与 pk/test/graft/rename.sed 不一致 —— D-23 的规则表分叉了\n' >&2
    diff -u pk/test/graft/rename.sed "$SED" >&2 || true
    exit 1
fi

# ---------------------------------------------------------------------------
# 1. 依赖库。**从源码自己建**，不复用 pk/geometry/build/ 或 pk/test/build/ 的
#    产物 —— 那两个目录归 tests/run_tests.sh 与 oracle 管，试接不该要求"先跑过
#    别的脚本"，也不该往别人的构建目录里写东西。
# ---------------------------------------------------------------------------
mkdir -p "$BUILD"

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

build_lib "$BUILD/libpkgeometry.a" \
    pk/geometry/PkPoint.cpp pk/geometry/PkSize.cpp \
    pk/geometry/PkRect.cpp pk/geometry/PkTransform.cpp \
    pk/geometry/PkLine.cpp pk/geometry/PkMargins.cpp \
    pk/geometry/PkPolygon.cpp pk/geometry/PkPainterPath.cpp

build_lib "$BUILD/libpktest.a" \
    pk/test/PkTestCase.cpp pk/test/PkTestRunner.cpp \
    pk/test/PkTestCompare.cpp pk/test/PkTestData.cpp

# R-01 的 PkString —— 目标① 的 libs/global/KisRectsGrid.cpp:23 走
# `KisUsageLogger::log(QString(...).arg(...))`，QString 用 R-01 的真品，不垫。
# ⚠ **R-21 T1 顺手修复**：这四个文件名曾经是 `PkStringData.cpp` 一个文件，
# R-13 把它拆成 `PkString_core/_query/_format.cpp` 三份、新增
# `PkStringCodec.cpp`（见 pk/string/CMakeLists.txt），但没有回头改这里——
# `graft_run.sh` 从那之后就一直编不过（`git stash` 回到 R-21 之前的 HEAD 复现
# 过，不是本任务引入的回归）。R-21 T1 顺手把文件名同步成当前实况，不然整个
# graft 装置对本任务（以及在它之后的任何任务）都是哑的。
build_lib "$BUILD/libpkstring.a" \
    pk/string/PkStringCodec.cpp pk/string/PkString_core.cpp \
    pk/string/PkString_query.cpp pk/string/PkString_format.cpp

# ---------------------------------------------------------------------------
# 2. run_one —— 一次写好吃两个目标。
#
#   $1 name        可执行文件名 / 工作子目录名
#   $2 testdir     测试源所在目录（仓库根相对路径）
#   $3 hdr         测试类头文件名
#   $4 src         测试类 .cpp 文件名
#   $5 graftsrcs   还要一起复制进构建目录的**被测源**（空格分隔，仓库根相对路径；
#                  其中的 .cpp 会一起编译进可执行文件）。全 inline 的被测头
#                  不用列 —— 它们经 $6 的 -I 直接被真品包含，链接面为零。
#   $6 extraincs   该目标额外的 -I（被测头所在的 libs/ 目录）
# ---------------------------------------------------------------------------
run_one() {
    local name="$1" testdir="$2" hdr="$3" src="$4" graftsrcs="$5" extraincs="$6"
    local work="$BUILD/$name"
    rm -rf "$work"; mkdir -p "$work"

    # ① 复制 —— 源树一个字节都不动。
    #    测试源之外还复制被测源，有两个理由：
    #    (a) 引号 include 先查"包含者所在目录"。被测源留在 libs/global 里编译时，
    #        它的 #include "kis_debug.h" 会命中 libs/global 的真品（要 QDebug +
    #        QLoggingCategory，整套 Qt 日志分类系统），-I 顺序救不了。
    #    (b) D-23 的 sed 本来就是要对全树跑的，被测源和测试源一视同仁更贴近真实。
    local f
    cp "$testdir/$hdr" "$testdir/$src" "$work/"
    for f in $graftsrcs; do
        cp "$f" "$work/"
    done

    # ② D-23 机械改名，**唯一**允许的改动，且只作用于副本。
    #    被测源也跑：KisRectsGrid.cpp:22 的 qFuzzyCompare( 会被改成 pkFuzzyCompare(，
    #    与 compat/QtGlobal 里那个 #define 殊途同归，不是行为差异。
    sed -i -f "$SED" "$work/$hdr" "$work/$src"
    for f in $graftsrcs; do
        sed -i -f "$SED" "$work/$(basename "$f")"
    done

    # ③ 生成 binder（替代 moc 的测试发现）。.inc 而非 .cpp：产物全是类内定义
    #    （隐式 inline），只能被 #include，不能作为独立翻译单元编译。
    python3 pk/test/pk_test_moc.py "$work/$hdr" -o "$work/binder.inc"

    # ③.5 driver.cpp：PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须在同一
    #     翻译单元里看见它的完整定义。真实测试类的 .cpp 只允许 rename.sed 的
    #     机械改名、不能往里加 #include，所以这层粘合只能由 graft 自己的
    #     driver.cpp 来做 —— 它不是复制自源树的文件，是构建期胶水。
    printf '#include "%s"\n#include "binder.inc"\n' "$src" > "$work/driver.cpp"

    # ④ 编译链接。
    local extraobjsrc=""
    for f in $graftsrcs; do
        case "$f" in
            *.cpp) extraobjsrc="$extraobjsrc $work/$(basename "$f")" ;;
        esac
    done

    # shellcheck disable=SC2086
    "$CXX" $CXXFLAGS $INCS $extraincs -I "$work" \
        "$work/driver.cpp" $extraobjsrc "$STUBS/graft_stubs.cpp" \
        "$BUILD/libpkgeometry.a" "$BUILD/libpktest.a" "$BUILD/libpkstring.a" \
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
    # tests/run_tests.sh 里对 libpkgeometry.a 那条（静态库允许有未定义符号）。
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
# 3. 两个目标
# ---------------------------------------------------------------------------
run_one KisRectsGridTest \
        libs/global/tests KisRectsGridTest.h KisRectsGridTest.cpp \
        "libs/global/KisRectsGrid.h libs/global/KisRectsGrid.cpp" \
        "-I libs/global"

run_one KisFourPointInterpolatorTest \
        libs/image/tests KisFourPointInterpolatorTest.h KisFourPointInterpolatorTest.cpp \
        "" \
        "-I libs/image -I libs/global"

# ---------------------------------------------------------------------------
# 4. 源树零改动自证。
#
# **两个目标的全部 8 个文件都要列上** —— 漏一个就等于这条自证不覆盖那个文件。
# 目标①：测试源 2 个 + 被测源 2 个（.h/.cpp）
# 目标②：测试源 2 个 + 被测头 2 个（全 inline，无 .cpp）
# ---------------------------------------------------------------------------
GRAFTED_FILES="
libs/global/tests/KisRectsGridTest.h
libs/global/tests/KisRectsGridTest.cpp
libs/global/KisRectsGrid.h
libs/global/KisRectsGrid.cpp
libs/image/tests/KisFourPointInterpolatorTest.h
libs/image/tests/KisFourPointInterpolatorTest.cpp
libs/image/kis_four_point_interpolator_forward.h
libs/image/kis_four_point_interpolator_backward.h
"

# shellcheck disable=SC2086
if ! git diff --quiet -- $GRAFTED_FILES; then
    printf '  源树被改动了 —— 试接必须零改动\n' >&2
    # shellcheck disable=SC2086
    git diff --stat -- $GRAFTED_FILES >&2
    rc=1
else
    printf '  git diff --quiet 自证（8 个文件）: 源树零改动\n'
fi

exit "$rc"
