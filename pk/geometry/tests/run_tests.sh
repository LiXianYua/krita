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
#
# 判定交给 git 自己的 pathspec，**不解析 porcelain 的输出文本**。理由：
#   · 改名行的形状是 `R  old -> new`，按列切出来是 "old -> new" 这一整串。
#     `pk/geometry/x -> pk/other/x`（真·越界改名）以 pk/geometry/ 开头，
#     前缀过滤会把它当成合规改动放过去 —— 实测复现过。
#   · 含空格/非 ASCII 的路径 git 默认加引号并转义（core.quotePath），切出来是
#     `"pk/geometry/\344\270\255..."`，前缀过滤反过来误判成越界 —— 也实测复现过。
# `-- . ':(exclude)pk/geometry'` 让 git 按路径语义筛：只要 pk/geometry 之外有
# 任何改动（含未跟踪、含改名的任一端）输出就非空。空输出 = 合规。
printf '\ngit status --porcelain:\n'
git status --porcelain
stray=$(git status --porcelain -- . ':(exclude)pk/geometry')
if [ -n "$stray" ]; then
    printf 'run_tests.sh: 有改动落在 pk/geometry/ 之外：\n%s\n' "$stray" >&2
    exit 1
fi
printf 'git status --porcelain: 改动全部落在 pk/geometry/ 前缀内\n'
