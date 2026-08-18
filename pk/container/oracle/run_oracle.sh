#!/usr/bin/env bash
# run_oracle.sh —— 编译并跑三个对拍程序，汇总结果
#
# 用法：./run_oracle.sh [输出目录]     默认输出目录 = ./build（已被顶层 .gitignore 排除）
#
# ── 三段链接旗标，每一段都对应一个实测过的坑，别简化 ──────
#
#   -fPIC              **编译期**必需：ci-env 的 Qt 用 -reduce-relocations 编的，
#                      不加 qglobal.h 直接 #error
#   -Wl,-rpath-link    不加**根本链不上**：libQt5Core 的 DT_NEEDED 里有
#                      libicui18n.so.70 / libicuuc.so.70，链接器找不到就把 ICU
#                      符号全报 undefined。$prefix/lib 下的开发符号链接指向 72.1，
#                      -licui18n 救不了
#   -Wl,--disable-new-dtags
#                      让 ld 发 DT_RPATH 而非 DT_RUNPATH —— **RUNPATH 不传递给
#                      依赖的依赖**，只写 -rpath 的话运行期报
#                      「libicui18n.so.70: cannot open shared object file」
#
# **系统 /usr/lib/x86_64-linux-gnu/libQt5Core.so.5 是 5.15.13，ci-env 那份是
# 5.15.7。** 旗标少一段，ld.so 会从 cache 里捞系统那份 —— 程序照跑，但对拍的
# 对手已经不是你编译时那份。所以对拍程序运行期自报 qVersion()，跑完记得核对。
#
# ── -I 里**绝不能**出现 pk/container/compat ────────────────
# 给了两侧会解析成同一个类型，对拍恒等、永远绿。各对拍源顶部的 static_assert
# 会在编译期炸，但别指望它兜底 —— 一开始就别写。

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FORK_ROOT="$(cd "$HERE/../../.." && pwd)"          # <fork>/pk/container/oracle → <fork>
OUT="${1:-$HERE/build}"

: "${PK_QT_PREFIX:=/mnt/ssd-disk/liyang/projects/krita-ci-env/_install}"
if [ ! -f "$PK_QT_PREFIX/lib/libQt5Core.so" ] || [ ! -f "$PK_QT_PREFIX/include/QtCore/qstring.h" ]; then
    echo "找不到真 Qt5：PK_QT_PREFIX=$PK_QT_PREFIX 下缺 lib/libQt5Core.so 或 include/QtCore/qstring.h" >&2
    exit 1
fi

mkdir -p "$OUT"

PKSTRING_SRC="
    $FORK_ROOT/pk/string/PkStringCodec.cpp
    $FORK_ROOT/pk/string/PkString_core.cpp
    $FORK_ROOT/pk/string/PkString_format.cpp
    $FORK_ROOT/pk/string/PkString_query.cpp
"

# 容器全是头文件模板（没有 extern template），所以只需要把 pk/string 的实现喂进来。
# -Wno-deprecated-declarations：`QMap::values(key)` / `QHash::values(key)` 在 5.15
# 标了 deprecated（叫人改用 QMultiMap），但 PkAssocContainer 照样提供了这个方法，
# 它就在被替代的 API 面里 —— **故意**要对拍，不是误用。
CXXFLAGS="-std=c++17 -fPIC -O2 -Wno-deprecated-declarations
    -I$FORK_ROOT/pk/container
    -I$FORK_ROOT/pk/string
    -I$PK_QT_PREFIX/include
    -I$PK_QT_PREFIX/include/QtCore"
LDFLAGS="-L$PK_QT_PREFIX/lib -lQt5Core
    -Wl,-rpath-link,$PK_QT_PREFIX/lib
    -Wl,-rpath,$PK_QT_PREFIX/lib
    -Wl,--disable-new-dtags"

SUITES="seq assoc set"

for s in $SUITES; do
    echo "=== 编译 difftest_$s ==="
    # shellcheck disable=SC2086
    g++ $CXXFLAGS "$HERE/difftest_$s.cpp" $PKSTRING_SRC -o "$OUT/difftest_$s" $LDFLAGS
done

echo
echo "=== ldd（必须是 ci-env 那份 Qt 与 icu 70）==="
ldd "$OUT/difftest_seq" | grep -iE 'qt5core|icu' || true

TOTAL=0
MISMATCH=0
for s in $SUITES; do
    echo
    echo "=== 跑 difftest_$s ==="
    "$OUT/difftest_$s" > "$OUT/$s.out" 2>&1
    rc=$?
    if [ "$rc" -ne 0 ]; then
        echo "difftest_$s 退出码 $rc（契约要求 0，即使 mismatch>0）" >&2
        exit 1
    fi
    grep -E '^ORACLE-QT ' "$OUT/$s.out"
    grep -E '^DIFFTAG ' "$OUT/$s.out" || true
    line=$(grep -E '^DIFF total=' "$OUT/$s.out")
    echo "$line"
    t=$(echo "$line" | sed -E 's/.*total=([0-9]+).*/\1/')
    m=$(echo "$line" | sed -E 's/.*mismatch=([0-9]+).*/\1/')
    TOTAL=$((TOTAL + t))
    MISMATCH=$((MISMATCH + m))
done

echo
echo "=== 汇总 ==="
echo "DIFF-ALL total=$TOTAL mismatch=$MISMATCH"
echo "（每个程序自己的 stdout 严格守契约：恰好一行 DIFF、0..N 行 DIFFTAG，"
echo "  ORACLE-COVER / MISMATCH 明细行判据不读，是给人看覆盖度与现场的。）"
echo
echo "出现过的 DIFFTAG 必须在 $HERE/R-02.deviation 里逐条声明过。"
