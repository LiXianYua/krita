#!/usr/bin/env bash
# 建 pk/test 独立工程、跑 harness 自测、再跑 nm -u 的零 Qt 自证。
set -eu
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

BUILD=pk/test/build
cmake -S pk/test -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug >/dev/null
cmake --build "$BUILD" -j"$(nproc)" >/dev/null

"./$BUILD/selftest_pktest"

# 判据③：替代品本体不得有 Qt 未定义符号。查的是 pk/test 编出来的静态库。
undef=$(nm -u "$BUILD/libpktest.a" 2>/dev/null | grep -i qt || true)
if [ -n "$undef" ]; then
    printf 'nm -u libpktest.a 里有 Qt 符号：\n%s\n' "$undef" >&2
    exit 1
fi
printf 'nm -u libpktest.a | grep -i qt: 无输出\n'
