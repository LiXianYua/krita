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
#
# 评审 I-3：libpkport.a 只含 4 个 Pk*.o + 4 个 PkString*.o——不含
# PkZipArchive.o（zip/CMakeLists.txt 把它单独编进 libpkzip.a）。这个断言此前
# 只查了 libpkport.a，本分支最大的一块真逻辑（PkZipArchive.cpp，438 行）从没
# 被这道闸门覆盖过——今天三个库实测都干净，但 S-01 往 PkZipArchive.cpp 里混
# 进 Qt 时这个断言原本不会响。libpkzip.a 现在一起查。
undef=$(nm -u "$BUILD/libpkport.a" "$BUILD/zip/libpkzip.a" 2>/dev/null | grep -i qt || true)
if [ -n "$undef" ]; then
    printf 'nm -u libpkport.a/libpkzip.a 里有 Qt 符号：\n%s\n' "$undef" >&2
    exit 1
fi
printf 'nm -u libpkport.a libpkzip.a | grep -i qt: 无输出\n'
