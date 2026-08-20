#!/usr/bin/env bash
# run_oracle.sh —— 编译并跑 PkString ↔ QString 对拍程序
#
# 用法：./run_oracle.sh [输出目录]     默认 ./build（已被顶层 .gitignore 排除）
#
# 三段链接旗标每一条都对应一个实测过的坑，照抄 pk/container/oracle/run_oracle.sh：
#   -fPIC                      ci-env 的 Qt 用 -reduce-relocations 编的，不加编不过
#   -Wl,-rpath-link             链接期找 ICU 符号要用，不加会报 undefined
#   -Wl,--disable-new-dtags     发 DT_RPATH 而非 DT_RUNPATH，否则运行期找不到 ICU

set -euo pipefail

# I4：%L 分组是固定逗号分组、不跟随运行时 locale 的范围决策（见 R-13.deviation），
# 但对拍程序本身此前没有钉住自己跑在哪个 locale 下——同一份对拍程序在
# LC_ALL=de_DE.UTF-8 下会跑出未声明的差异。显式钉在一个几乎所有机器都有的
# locale 上，往后重跑的人不用去猜这次是在哪个 locale 下跑的。
export LC_ALL=C.UTF-8

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FORK_ROOT="$(cd "$HERE/../../.." && pwd)"
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
    $FORK_ROOT/pk/string/PkString_query.cpp
    $FORK_ROOT/pk/string/PkString_format.cpp
"

CXXFLAGS="-std=c++17 -fPIC -O2
    -I$FORK_ROOT/pk/string
    -I$FORK_ROOT/pk/container
    -I$PK_QT_PREFIX/include
    -I$PK_QT_PREFIX/include/QtCore"
LDFLAGS="-L$PK_QT_PREFIX/lib -lQt5Core
    -Wl,-rpath-link,$PK_QT_PREFIX/lib
    -Wl,-rpath,$PK_QT_PREFIX/lib
    -Wl,--disable-new-dtags"

echo "=== 编译 difftest_string ==="
# shellcheck disable=SC2086
g++ $CXXFLAGS "$HERE/difftest_string.cpp" $PKSTRING_SRC -o "$OUT/difftest_string" $LDFLAGS

echo
echo "=== ldd（必须是 ci-env 那份 Qt 与 icu 70）==="
ldd "$OUT/difftest_string" | grep -iE 'qt5core|icu' || true

echo
echo "=== 跑 difftest_string ==="
# 不能写成「先跑、再拿 $?」：set -e 生效时，命令一旦非 0 退出整个脚本就已经
# 终止，根本轮不到下一行的 rc=$? 判断——这段防御代码原先写得不对，改成
# 不依赖"先失败再判断"的 if/! 写法。
if ! "$OUT/difftest_string" > "$OUT/string.out" 2>&1; then
    echo "difftest_string 退出码非 0（契约要求 0，即使 mismatch>0）" >&2
    exit 1
fi
grep -E '^ORACLE-QT ' "$OUT/string.out"
grep -E '^ORACLE-COVER toLower ' "$OUT/string.out"
grep -E '^ORACLE-COVER toUpper ' "$OUT/string.out"
grep -E '^DIFFTAG ' "$OUT/string.out" || true
grep -E '^DIFF total=[0-9]+ mismatch=0$' "$OUT/string.out"

echo
echo "出现过的 DIFFTAG 必须在 $HERE/R-13.deviation 里逐条声明过。"
