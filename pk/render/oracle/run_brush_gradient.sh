#!/usr/bin/env bash
set -eo pipefail
render_root=$(cd "$(dirname "$0")/.." && pwd)
source /mnt/ssd-disk/liyang/projects/krita-ci-env/env
set -u
export CCACHE_DIR="$KDECI_CC_CACHE"
oracle_build="$render_root/build/oracle-brush-gradient"
mkdir -p "$oracle_build"
cmake -S "$render_root" -B "$render_root/build" -G Ninja \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
ninja -C "$render_root/build" pkrender
ccache c++ -std=c++17 -fPIC -DPK_BRUSH_QT_ORACLE \
    "$render_root/oracle/brush_gradient.cpp" \
    $(pkg-config --cflags --libs Qt5Gui) -o "$oracle_build/qt"
"$oracle_build/qt" > "$oracle_build/qt.txt"
ccache c++ -std=c++17 -fwrapv \
    -I"$render_root" -I"$render_root/../geometry" -I"$render_root/../global" \
    -I"$render_root/../color" -I"$render_root/../container" \
    "$render_root/oracle/brush_gradient.cpp" \
    -Wl,--start-group "$render_root/build/libpkrender.a" \
    "$render_root/build/libpkgeometry.a" -Wl,--end-group -o "$oracle_build/pk"
"$oracle_build/pk" > "$oracle_build/pk.txt"
diff -u "$oracle_build/qt.txt" "$oracle_build/pk.txt"
printf 'brush gradient Qt oracle: identical\n'
