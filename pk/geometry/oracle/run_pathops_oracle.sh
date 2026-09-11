#!/usr/bin/env bash
# R-39 path boolean operations 的逐输入对拍：定位真 Qt → 编译 → 确认真链上了 Qt →
# 跑 → 校验 DIFF/DIFFTAG/FAMILY 与 pathops.deviation。
#
# ── 两栖（R-56 加 macOS 支，与 run_oracle.sh 同形）────────────────────────
# 本机是 macOS arm64，Qt 的落地形态与 Linux 不同，几处必须分叉（头文件位置 /
# 库的位置与链接旗标 / 运行时依赖检查 / 运行期库路径），与 run_oracle.sh 的
# 同一段说明逐字同义：
#   macOS `$QT/lib/QtCore.framework/Headers` + `-F` + `otool -L` + `DYLD_*`；
#   Linux `$QT/include/QtCore` + `-lQt5*` + `ldd` + `LD_LIBRARY_PATH`。
# **判据本身两边逐字相同**，分叉只在「怎么把真 Qt 编进来、怎么确认编进来了」。
set -euo pipefail
cd "$(dirname "$0")/../../.." || exit 1

UNAME_S=$(uname -s)

# 依赖前缀。优先 PK_QT_PREFIX，其次 source 过 env 时的 CMAKE_PREFIX_PATH，
# 再按约定找「工作空间上层」的 krita-ci-env/_install，最后是 Linux 的既有默认
#（**原值一字不改**，与 run_oracle.sh:31-54 同形）。
if [ -n "${PK_QT_PREFIX:-}" ]; then
    QT_ROOT=$PK_QT_PREFIX
elif [ "$UNAME_S" = "Darwin" ]; then
    QT_ROOT=""
    if [ -n "${CMAKE_PREFIX_PATH:-}" ]; then
        _p=$(printf '%s' "$CMAKE_PREFIX_PATH" | cut -d: -f1)
        [ -d "$_p" ] && QT_ROOT=$_p
    fi
    if [ -z "$QT_ROOT" ]; then
        for _c in "$PWD/../krita-ci-env/_install" "$PWD/../../krita-ci-env/_install"; do
            if [ -d "$_c" ]; then QT_ROOT=$(cd "$_c" && pwd); break; fi
        done
    fi
    [ -n "$QT_ROOT" ] || { echo "找不到 Qt 依赖前缀：既没有 PK_QT_PREFIX，也没 source 过 env" >&2
                           echo "  见 MACOS-SHELL-NOTES.md §1。" >&2; exit 1; }
else
    QT_ROOT=/mnt/ssd-disk/liyang/projects/krita-ci-env/_install
fi
SRC=pk/geometry/oracle/painterpath_pathops_difftest.cpp
OUT=pk/geometry/build/pathops_difftest
LOG=pk/geometry/build/pathops_difftest.out
COVERAGE_LOG=pk/geometry/build/pathops_difftest.coverage
INVALID_STDOUT=pk/geometry/build/pathops_difftest.invalid-stdout
DEV=pk/geometry/oracle/pathops.deviation

if [ "$UNAME_S" = "Darwin" ]; then
    QT_HDRS=("$QT_ROOT/lib/QtCore.framework/Headers" "$QT_ROOT/lib/QtGui.framework/Headers")
    QT_ARTIFACTS=("$QT_ROOT/lib/QtCore.framework/QtCore" "$QT_ROOT/lib/QtGui.framework/QtGui")
    QT_LINK=(-F"$QT_ROOT/lib" -Wl,-rpath,"$QT_ROOT/lib" -framework QtCore -framework QtGui)
    QT_RUNLIBS=("DYLD_FRAMEWORK_PATH=$QT_ROOT/lib" "DYLD_LIBRARY_PATH=$QT_ROOT/lib")
    SHARED_DEP_TOOL="otool -L"
    LDD_REQUIRE="QtCore QtGui"
    INCS=("${QT_HDRS[0]}" "${QT_HDRS[1]}")
else
    QT_HDRS=("$QT_ROOT/include/QtCore" "$QT_ROOT/include/QtGui")
    QT_ARTIFACTS=("$QT_ROOT/include/QtGui/QPainterPath" "$QT_ROOT/lib/libQt5Gui.so.5")
    QT_LINK=(-L"$QT_ROOT/lib" -Wl,-rpath-link,"$QT_ROOT/lib" -Wl,-rpath,"$QT_ROOT/lib" -lQt5Gui -lQt5Core)
    QT_RUNLIBS=("LD_LIBRARY_PATH=$QT_ROOT/lib")
    SHARED_DEP_TOOL="ldd"
    LDD_REQUIRE="libQt5Gui libQt5Core"
    INCS=("$QT_ROOT/include" "${QT_HDRS[0]}" "${QT_HDRS[1]}")
fi
INCS+=("pk/geometry" "pk/global" "pk/container")

for a in "${QT_ARTIFACTS[@]}"; do
    [ -f "$a" ] || { echo "找不到真 Qt 的库：$a" >&2; exit 1; }
done
[ -d "${QT_HDRS[1]}" ] || { echo "找不到真 Qt 的头：${QT_HDRS[1]}" >&2; exit 1; }

# ⚠ **-I 里绝不能出现 compat**（照抄 run_oracle.sh:105-131 的同一条硬闸门，成本为零）：
# 垫片一旦被拉进来，`<QPainterPath>` 会解析到 compat 垫片侧，两侧变成同一个类型，
# 跑出来必然零差异且看不出破绽。`-I pk/geometry` 是必须的，`compat/` 不是。
for i in "${INCS[@]}"; do
    case "$i" in
        *compat*) echo "run_pathops_oracle.sh: -I 里出现了 compat 垫片目录：$i" >&2; exit 1;;
    esac
done
INCFLAGS=()
for i in "${INCS[@]}"; do INCFLAGS+=("-I$i"); done

mkdir -p pk/geometry/build
CXXBIN=${CXX:-c++}
"$CXXBIN" -std=c++17 -O2 -fPIC -DQT_NO_DEBUG "${INCFLAGS[@]}" -o "$OUT" "$SRC" "${QT_LINK[@]}"

# 判据：**真的链上了 Qt**。链不上说明两侧都编到了替代品，零差异是假的。
printf '%s %s | grep -i qt:\n' "$SHARED_DEP_TOOL" "$OUT"
env "${QT_RUNLIBS[@]}" $SHARED_DEP_TOOL "$OUT" | grep -i qt || true
for lib in $LDD_REQUIRE; do
    env "${QT_RUNLIBS[@]}" $SHARED_DEP_TOOL "$OUT" | grep -q "$lib" || {
        echo "oracle did not link $lib" >&2
        exit 1
    }
done

env "${QT_RUNLIBS[@]}" "$OUT" > "$LOG" 2> "$COVERAGE_LOG"
if grep -Ev '^(DIFF total=[0-9]+ mismatch=[0-9]+|DIFFTAG .+ [0-9]+)$' "$LOG" > "$INVALID_STDOUT"; then
    echo "oracle stdout contains records outside DIFF/DIFFTAG" >&2
    head -30 "$INVALID_STDOUT" >&2
    exit 1
fi
if grep -Ev '^FAMILY [a-z0-9-]+$' "$COVERAGE_LOG" > /dev/null; then
    echo "oracle coverage stream contains invalid records" >&2
    head -30 "$COVERAGE_LOG" >&2
    exit 1
fi
for family in empty move-only consecutive-moves zero-length-line rectangle ellipse open-polyline closed-polyline nested-rings disjoint-compound bow-tie star adversarial-cubic tight-cubic-bounds close-same-line close-different-line fuzzy-close-normalization edit-last-element add-polygon; do
    grep -qx "FAMILY $family" "$COVERAGE_LOG" || { echo "missing family $family" >&2; exit 1; }
done

[ ! -s "$DEV" ] || { echo "$DEV must be empty without an approved deviation" >&2; exit 1; }
line=$(grep '^DIFF total=' "$LOG")
[ "$(grep -c '^DIFF total=' "$LOG")" -eq 1 ] || { echo "expected exactly one DIFF line" >&2; exit 1; }
printf '%s\n' "$line"
mismatch=$(printf '%s\n' "$line" | sed -E 's/.*mismatch=([0-9]+).*/\1/')
if [ "$mismatch" -ne 0 ]; then
    grep '^DIFFTAG ' "$LOG" | head -30 >&2 || true
    echo "pathops oracle mismatch=$mismatch" >&2
    exit 1
fi
if grep -q '^DIFFTAG ' "$LOG"; then
    echo "DIFFTAG present with mismatch=0" >&2
    exit 1
fi
