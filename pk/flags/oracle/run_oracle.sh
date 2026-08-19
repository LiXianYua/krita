#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${1:-$HERE/build}"
: "${PK_QT_PREFIX:=/home/qiansenwei/workspace/Krita_Linux_liyang/krita-ci-env/_install}"
if [ ! -f "$PK_QT_PREFIX/lib/libQt5Core.so" ] || [ ! -f "$PK_QT_PREFIX/include/QtCore/qflags.h" ]; then
    echo "找不到真 Qt5：PK_QT_PREFIX=$PK_QT_PREFIX" >&2; exit 1
fi
mkdir -p "$OUT"
CXXFLAGS="-std=c++17 -fPIC -O2 -I$HERE/.. -I$PK_QT_PREFIX/include -I$PK_QT_PREFIX/include/QtCore"
LDFLAGS="-L$PK_QT_PREFIX/lib -lQt5Core -Wl,-rpath-link,$PK_QT_PREFIX/lib -Wl,-rpath,$PK_QT_PREFIX/lib -Wl,--disable-new-dtags"
g++ $CXXFLAGS "$HERE/difftest_flags.cpp" -o "$OUT/difftest_flags" $LDFLAGS
LD_LIBRARY_PATH="$PK_QT_PREFIX/lib:$LD_LIBRARY_PATH" "$OUT/difftest_flags"