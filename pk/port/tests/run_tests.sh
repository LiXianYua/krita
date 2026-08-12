#!/usr/bin/env bash
# 建 pk/port 独立工程、跑 test_pkport，再跑 nm -u 的零 Qt 自证。
# 形态照抄 pk/test/tests/run_tests.sh。
set -eu
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

BUILD=pk/port/build
cmake -S pk/port -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug >/dev/null
cmake --build "$BUILD" -j"$(nproc)" >/dev/null

"./$BUILD/test_pkport"

# 判据：本端口不得有 Qt 未定义符号。查的是 pk/port 编出来的静态库
# ——静态库允许留未定义符号，真混进 Qt 依赖会在这里现形；可执行文件是静态
# 链接产物，链接期就会失败，跑不到这一步（同 pk/test/README.md §5 的说明）。
undef=$(nm -u "$BUILD/libpkport.a" 2>/dev/null | grep -i qt || true)
if [ -n "$undef" ]; then
    printf 'nm -u libpkport.a 里有 Qt 符号：\n%s\n' "$undef" >&2
    exit 1
fi
printf 'nm -u libpkport.a | grep -i qt: 无输出\n'
