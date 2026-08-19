#!/usr/bin/env bash
# run_oracle.sh —— QColor ↔ PkColor 对拍（甲类核心判据）。
# 单 TU 双侧：difftest_color.cpp 把真 Qt 的 <QColor> 与 PkColor.h（连同它的
# PkString/PkGlobal 依赖 .cpp）各自 include 进同一编译单元，逐输入对比。
# 判据：DIFF total=... mismatch=0（或全部 mismatch 落在 color.deviation 登记项）。
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${1:-$HERE/build}"
: "${PK_QT_PREFIX:=/home/liyang/projects-ssd/krita-ci-env/_install}"
if [ ! -f "$PK_QT_PREFIX/lib/libQt5Gui.so" ] || [ ! -f "$PK_QT_PREFIX/include/QtGui/qcolor.h" ]; then
    echo "找不到真 Qt5：PK_QT_PREFIX=$PK_QT_PREFIX" >&2; exit 1
fi
mkdir -p "$OUT"
CXXFLAGS="-std=c++17 -fPIC -O2 -I$HERE/.. -I$PK_QT_PREFIX/include -I$PK_QT_PREFIX/include/QtGui -I$PK_QT_PREFIX/include/QtCore"
LDFLAGS="-L$PK_QT_PREFIX/lib -lQt5Gui -lQt5Core -Wl,-rpath-link,$PK_QT_PREFIX/lib -Wl,-rpath,$PK_QT_PREFIX/lib -Wl,--disable-new-dtags"
g++ $CXXFLAGS "$HERE/difftest_color.cpp" -o "$OUT/difftest_color" $LDFLAGS
export LD_LIBRARY_PATH="${PK_QT_PREFIX}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
"$OUT/difftest_color"
