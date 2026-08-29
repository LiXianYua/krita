#!/usr/bin/env bash
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
source /mnt/ssd-disk/liyang/projects/krita-ci-env/env
export CCACHE_DIR="${KDECI_CC_CACHE}"
cmake -S "$root" -B "$root/build" -G Ninja -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
ninja -C "$root/build"
"$root/build/test_pkrender"
# Pk compatibility enums/functions contain the letters "Qt" in their source-level
# names; only an unresolved Qt library symbol is a dependency violation.
if nm -u -C "$root/build/libpkrender.a" | grep -E 'Qt5(Core|Gui|Widgets)'; then
  exit 1
fi
