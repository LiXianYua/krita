#!/usr/bin/env bash
# 建 pk/log 独立工程、跑自测、再跑 nm -u 的零 Qt 自证。
set -eu
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

BUILD=pk/log/build
cmake -S pk/log -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug >/dev/null
cmake --build "$BUILD" -j"$(nproc)" >/dev/null

"./$BUILD/test_pklog"

# 判据③：替代品本体不得有 Qt 未定义符号。查的是 pk/log 编出来的静态库。
undef=$(nm -u "$BUILD/libpklog.a" 2>/dev/null | grep -i qt || true)
if [ -n "$undef" ]; then
    printf 'nm -u libpklog.a 里有 Qt 符号：\n%s\n' "$undef" >&2
    exit 1
fi
printf 'nm -u libpklog.a | grep -i qt: 无输出\n'

# Task 5：真实 kis_debug.h/.cpp 零改动试接——它自己会按需建 pk/string，
# 并对 libs/global 做 git diff --quiet 自证。
./pk/log/tests/graft/kis_debug_build.sh

echo "== Task 6：真实 Krita 测试类经 pk/log + pk/string + pk/test 试接 =="
./pk/log/tests/graft/graft_run.sh
