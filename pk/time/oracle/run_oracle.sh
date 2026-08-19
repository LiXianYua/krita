#!/usr/bin/env bash
# run_oracle.sh —— 编译并跑 PkDateTime/PkElapsedTimer ↔ QDateTime/QElapsedTimer 对拍程序
#
# 用法：./run_oracle.sh [输出目录]     默认 ./build（已被顶层 .gitignore 排除）
#
# 三段链接旗标每一条都对应一个实测过的坑，照抄 pk/string/oracle/run_oracle.sh：
#   -fPIC                      ci-env 的 Qt 用 -reduce-relocations 编的，不加编不过
#   -Wl,-rpath-link             链接期找 ICU 符号要用，不加会报 undefined
#   -Wl,--disable-new-dtags     发 DT_RPATH 而非 DT_RUNPATH，否则运行期找不到 ICU

set -euo pipefail

# TZ=UTC：QDateTime::fromMSecsSinceEpoch 等工厂函数默认 timeSpec()==Qt::LocalTime，
# 渲染日历字段（toString()/fromString()）时走系统本地时区；PkDateTime 恒定按 UTC
# 渲染日历字段。钉死运行环境本地时区为 UTC，两侧"本地时区"与"UTC"重合，
# toSecsSinceEpoch() 之外的日历字段比较才有意义、且跨机器可复现——仿 R-13
# LC_ALL=C.UTF-8 的 I4 先例（run_oracle.sh 显式钉住运行环境，往后重跑的人不用
# 去猜这次是在哪个时区下跑的）。
export TZ=UTC
export LC_ALL=C.UTF-8

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FORK_ROOT="$(cd "$HERE/../../.." && pwd)"
OUT="${1:-$HERE/build}"

: "${PK_QT_PREFIX:=/mnt/ssd-disk/liyang/projects/krita-ci-env/_install}"
if [ ! -f "$PK_QT_PREFIX/lib/libQt5Core.so" ] || [ ! -f "$PK_QT_PREFIX/include/QtCore/qdatetime.h" ]; then
    echo "找不到真 Qt5：PK_QT_PREFIX=$PK_QT_PREFIX 下缺 lib/libQt5Core.so 或 include/QtCore/qdatetime.h" >&2
    exit 1
fi

mkdir -p "$OUT"

PKTIME_SRC="
    $FORK_ROOT/pk/time/PkDateTime.cpp
    $FORK_ROOT/pk/time/PkElapsedTimer.cpp
"

CXXFLAGS="-std=c++17 -fPIC -O2
    -I$FORK_ROOT/pk/time
    -I$PK_QT_PREFIX/include
    -I$PK_QT_PREFIX/include/QtCore"
LDFLAGS="-L$PK_QT_PREFIX/lib -lQt5Core
    -Wl,-rpath-link,$PK_QT_PREFIX/lib
    -Wl,-rpath,$PK_QT_PREFIX/lib
    -Wl,--disable-new-dtags"

echo "=== 编译 difftest_time ==="
# shellcheck disable=SC2086
g++ $CXXFLAGS "$HERE/difftest_time.cpp" $PKTIME_SRC -o "$OUT/difftest_time" $LDFLAGS

echo
echo "=== ldd（必须是 ci-env 那份 Qt）==="
ldd "$OUT/difftest_time" | grep -iE 'qt5core|icu' || true

echo
echo "=== 跑 difftest_time ==="
# 不能写成「先跑、再拿 $?」：set -e 生效时，命令一旦非 0 退出整个脚本就已经
# 终止，根本轮不到下一行的 rc=$? 判断——照抄 pk/string/oracle 的 if/! 写法。
if ! "$OUT/difftest_time" > "$OUT/time.out" 2>&1; then
    echo "difftest_time 退出码非 0（契约要求 0，即使 mismatch>0）" >&2
    exit 1
fi
grep -E '^ORACLE-QT ' "$OUT/time.out"
grep -E '^DIFFTAG ' "$OUT/time.out" || true
grep -E '^DIFF total=' "$OUT/time.out"

echo
echo "出现过的 DIFFTAG 必须在 $HERE/R-16.deviation 里逐条声明过。"
