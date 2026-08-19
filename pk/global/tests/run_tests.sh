#!/usr/bin/env bash
# 建 pk/global 独立工程、跑单测，再跑两条自证：
#   判据③ —— libpkglobal.a 里不得有 Qt 未定义符号；
#   locks  —— 工作树的改动必须全部落在 pk/global/ 前缀内。
set -eu
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

BUILD=pk/global/build
cmake -S pk/global -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug >/dev/null
cmake --build "$BUILD" -j"$(nproc)" >/dev/null

"./$BUILD/test_pkglobal"

# 判据③：替代品本体不得有 Qt 未定义符号。查的是 pk/global 编出来的静态库。
# 静态库允许留未定义符号，真混进 Qt 依赖就会在这里现形（可执行文件那种查法是
# 恒真的，链接行里根本没 Qt 库，见 pk/test/README.md §5）。
undef=$(nm -u "$BUILD/libpkglobal.a" 2>/dev/null | grep -i qt || true)
if [ -n "$undef" ]; then
    printf 'nm -u libpkglobal.a 里有 Qt 符号：\n%s\n' "$undef" >&2
    exit 1
fi
printf 'nm -u %s/libpkglobal.a | grep -i qt: 无输出\n' "$BUILD"

# locks 自证：R-18 只许动 pk/global/ 与 pk/geometry/（折叠的转发头与让位都在
# geometry 侧）。build/ 已被根 .gitignore 排除，所以这里连未跟踪文件一起查，
# 出现两个前缀之外的路径就判失败。
#
# 判定交给 git 自己的 pathspec，**不解析 porcelain 的输出文本**。理由与 R-03 的
# run_tests.sh 相同：改名行的形状是 `R  old -> new`，按列切出来是 "old -> new"
# 这一整串，前缀过滤会误判；含空格/非 ASCII 的路径 git 默认加引号并转义，切出来
# 反过来误判。`-- . ':(exclude)pk/global' ':(exclude)pk/geometry'` 让 git 按路径
# 语义筛：只要这两个前缀之外有任何改动（含未跟踪、含改名的任一端）输出就非空。
# 空输出 = 合规。
printf '\ngit status --porcelain:\n'
git status --porcelain
stray=$(git status --porcelain -- . ':(exclude)pk/global' ':(exclude)pk/geometry')
if [ -n "$stray" ]; then
    printf 'run_tests.sh: 有改动落在 pk/global/ 与 pk/geometry/ 之外：\n%s\n' "$stray" >&2
    exit 1
fi
printf 'git status --porcelain: 改动全部落在 pk/global/ 与 pk/geometry/ 前缀内\n'
