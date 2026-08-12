#!/usr/bin/env bash
# 建 pk/geometry 独立工程、跑单测，再跑两条自证：
#   判据③ —— libpkgeometry.a 里不得有 Qt 未定义符号；
#   locks  —— 工作树的改动必须全部落在 pk/geometry/ 前缀内。
set -eu
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

BUILD=pk/geometry/build
cmake -S pk/geometry -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug >/dev/null
cmake --build "$BUILD" -j"$(nproc)" >/dev/null

"./$BUILD/test_pkgeometry"

# 判据③：替代品本体不得有 Qt 未定义符号。查的是 pk/geometry 编出来的静态库。
# 与 pk/test 那条同义：静态库允许留未定义符号，真混进 Qt 依赖就会在这里现形
#（可执行文件那种查法是恒真的，链接行里根本没 Qt 库，见 pk/test/README.md §5）。
undef=$(nm -u "$BUILD/libpkgeometry.a" 2>/dev/null | grep -i qt || true)
if [ -n "$undef" ]; then
    printf 'nm -u libpkgeometry.a 里有 Qt 符号：\n%s\n' "$undef" >&2
    exit 1
fi
printf 'nm -u %s/libpkgeometry.a | grep -i qt: 无输出\n' "$BUILD"

# locks 自证：R-03 只许动 pk/geometry/。build/ 已被根 .gitignore 排除，所以这里
# 连未跟踪文件一起查，出现任何 pk/geometry/ 之外的路径就判失败。
printf '\ngit status --porcelain:\n'
git status --porcelain
stray=$(git status --porcelain | awk '{ print substr($0, 4) }' | grep -v '^pk/geometry/' || true)
if [ -n "$stray" ]; then
    printf 'run_tests.sh: 有改动落在 pk/geometry/ 之外：\n%s\n' "$stray" >&2
    exit 1
fi
printf 'git status --porcelain: 改动全部落在 pk/geometry/ 前缀内\n'
