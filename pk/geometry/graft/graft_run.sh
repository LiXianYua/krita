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

# ⚠ **macOS 部署目标**（R-63）：`source <env>` 会把 MACOSX_DEPLOYMENT_TARGET 设成
# 10.15，clang 拿它当默认的 -mmacosx-version-min ⇒ pk/string/PkString_format.cpp
# 的 std::to_chars 浮点重载（:408/:743）被判 "introduced in macOS 13.3"、编不过。
# 主树靠 add_definitions(-mmacosx-version-min=…) 顶掉它，薄壳没有那一段，只能自己补
# （同 MACOS-SHELL-NOTES §2 的 -DCMAKE_OSX_DEPLOYMENT_TARGET=13.3，同一件事）。
# **只在 Darwin 上加** —— 它是 Apple 工具链专属旗标，Linux 上加了直接是 unknown argument。
if [ "$(uname -s)" = "Darwin" ]; then
    CXXFLAGS="$CXXFLAGS -mmacosx-version-min=13.3"
fi

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
# ⚠ **R-63 追加的两条**：`libs/global/KisRectsGrid.h:11` 现在 `#include <PkVector.h>`
# （在 `pk/container/`）、`libs/global/kis_pointer_utils.h:10` 现在
# `#include <PkSharedPointer.h>`（在 `pk/pointer/`）—— 两条都是 R-03 之后新增的依赖，
# 脚本没跟。**推断这两条与平台无关**：Linux 上同样是 `fatal error: 'PkVector.h' file not found`（**推断**：本机无 Linux，未在 Linux 实测）。
INCS="-I $STUBS -I pk/test -I pk/test/compat -I pk/geometry -I pk/geometry/compat \
      -I pk/string -I pk/string/compat -I pk/container -I pk/pointer"

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

# ---------------------------------------------------------------------------
# 1.7 冻结表用的是 GNU sed 的 `\b` 词边界。**BSD sed 不认 `\b`、而且不报错** ——
#     它把 `\b` 当普通字符，于是 18 条规则一条都不匹配、改名一个都不生效，
#     后果是试接编译期整屏 `use of undeclared identifier 'QCOMPARE'`（R-63 实测）。
#     本机没有 GNU sed（`ls /opt/homebrew/bin/gsed` → No such file）。
#
#     打法：**先探本机 sed 认不认 `\b`**。认 → 冻结表原样用（Linux 侧行为一字节不变）；
#     不认 → 在**构建目录里**派生一份 BSD 形态。表仍然只有一份真源
#     （`pk/test/graft/rename.sed`），派生方向是单向的。
#     本表 18 条模式的 `\b` **全部在模式首**（`grep -n '^s' rename.sed` 逐条核过），
#     模式首的词边界等价物是 `[[:<:]]`（BSD 认，GNU 也认）。
# ---------------------------------------------------------------------------
BSD_SED=$BUILD/rename.bsd.sed
if printf 'QVERIFY(\n' | sed -e 's/\bQVERIFY(/PK_VERIFY(/' | grep -q 'PK_VERIFY('; then
    SED_TABLE=$SED                       # 本机 sed 认 GNU \b，冻结表原样用
else
    sed -e 's/\\b/[[:<:]]/g' "$SED" > "$BSD_SED"
    # 自证：派生表把 `[[:<:]]` 换回 `\b` 之后必须与冻结表**逐字节相同**。
    # 派生器坏掉（比如把别的反斜杠也吃掉）会在这里当场 FAIL，而不是静默少改几条规则 ——
    # 「静默少改」正是这个脚本已经吃过一次的亏。
    # 用 python3 做反向替换：本脚本已经依赖 python3（pk_test_moc.py），不引入新依赖，
    # 也绕开"用 sed 验证 sed"的循环。
    if ! python3 - "$SED" "$BSD_SED" <<'PY'
import sys
a = open(sys.argv[1], 'rb').read()
b = open(sys.argv[2], 'rb').read()
sys.exit(0 if b.replace(b'[[:<:]]', b'\\b') == a else 1)
PY
    then
        printf 'R-63: rename.sed 的 BSD 派生表与冻结表不一致 —— 派生器坏了，拒绝继续\n' >&2
        exit 1
    fi
    printf '  sed 不认 GNU \\b ⇒ 已派生 BSD 形态改名表：%s\n' "$BSD_SED"
    SED_TABLE=$BSD_SED                   # 用派生出来的 BSD 形态表
fi

# ---------------------------------------------------------------------------
# 1.6 D-23 机械改名的**就地应用**。
#
# `sed -i -f SCRIPT FILE` 是 GNU 形态：BSD sed（macOS 的 /usr/bin/sed）把 `-f` 当成
# `-i` 的后缀、报 `sed: 1: "…": extra characters at the end of p command`，
# 而脚本是 `set -eu`，当场 exit 1（R-63 实测的层 b1）。
# 这里**不用 `-i` 家族的写法**，改走"写临时文件再 mv" —— 两种 sed 都认，没有平台分支。
sed_apply() {
    local f
    for f in "$@"; do
        sed -f "$SED_TABLE" "$f" > "$f.sedtmp" && mv "$f.sedtmp" "$f"
    done
}

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

# pk/string 的 PkString_format.cpp（PkString::toLatin1/toUtf8）要
# pk/container/PkByteArray.cpp 里的构造 —— 只建 pkstring 会在**链接期**报
# `Undefined symbols: PkByteArray::PkByteArray(char const*, int)`（R-63 实测的层 d；
# **推断**与平台无关——本机无 Linux，未在 Linux 实测）。源表 = pk/container/CMakeLists.txt 的 add_library(pkcontainer STATIC …)。
PKCONTAINER_SRCS="PkByteArray.cpp PkArrayData.cpp PkVector.cpp PkList.cpp PkMap.cpp \
PkHash.cpp PkSet.cpp PkStack.cpp PkQueue.cpp"

# 对账闸门：那份 CMakeLists 加一项而这里没跟 → 当场 FAIL，而不是编出半个库、
# 在链接期报一个看不出所以然的未定义符号。（形态同上面 rename.sed 的 diff 闸门。）
if ! python3 - pk/container/CMakeLists.txt "$PKCONTAINER_SRCS" <<'PY'
import re, sys
txt = open(sys.argv[1], encoding='utf-8').read()
m = re.search(r'add_library\(\s*pkcontainer\s+STATIC\s*\n(.*?)\n\s*\)', txt, re.S)
if not m:
    print('R-63: 解析不出 pk/container/CMakeLists.txt 的 add_library(pkcontainer STATIC …)'
          ' —— 闸门失效，请人工核对', file=sys.stderr)
    sys.exit(1)
declared = re.findall(r'[A-Za-z0-9_]+\.cpp', m.group(1))
if declared != sys.argv[2].split():
    print('R-63: graft 的 libpkcontainer.a 源表与 pk/container/CMakeLists.txt 不一致',
          file=sys.stderr)
    print('  CMakeLists.txt:', declared, file=sys.stderr)
    print('  graft_run.sh  :', sys.argv[2].split(), file=sys.stderr)
    sys.exit(1)
PY
then
    exit 1
fi

build_lib "$BUILD/libpkcontainer.a" $(for s in $PKCONTAINER_SRCS; do printf 'pk/container/%s ' "$s"; done)

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
    sed_apply "$work/$hdr" "$work/$src"
    for f in $graftsrcs; do
        sed_apply "$work/$(basename "$f")"
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
        "$BUILD/libpkcontainer.a" \
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

# ⚠ **目标② 多两个 `-include`**（R-63）：这个测试类**还没迁移**，它照旧写
# `QPolygonF` / `QPointF`，而被测头已经迁移（`kis_four_point_interpolator_backward.h:14`
# 从 `#include <QPolygonF>` 变成了 `#include <PkPolygon.h>`）⇒ 那两个 Qt 名字不再随包含链
# 进来。判据② 不许改测试源，所以按 compat 层的既有机制把它补回去：那些垫片本来就是
# 按**文件名**被 `#include` 找到的 `#define` 垫片，而这里调用点压根不 include 它 ——
# `-include` 是同一机制的 TU 级形式，属于**构建行**，不是对源码的改动（同 `-I` / `-D`
# 和构建期胶水 driver.cpp）。**不新写类型别名** —— 别名只有 compat/ 那一份真源。
#
# 目标① 不需要：`libs/global/tests/KisRectsGridTest.cpp` 已经迁到 Pk 类型名了。
run_one KisFourPointInterpolatorTest \
        libs/image/tests KisFourPointInterpolatorTest.h KisFourPointInterpolatorTest.cpp \
        "" \
        "-I libs/image -I libs/global -include pk/geometry/compat/QPolygonF -include pk/geometry/compat/QPointF"

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
