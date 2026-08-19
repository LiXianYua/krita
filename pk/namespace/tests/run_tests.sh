#!/usr/bin/env bash
# 建 pk/namespace 独立工程、跑单测，再跑两条自证：
#   判据③ —— test_pknamespace 里不得有 Qt 未定义符号；
#   locks  —— 工作树的改动必须全部落在 pk/namespace/ 前缀内。
set -eu
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

BUILD=pk/namespace/build
cmake -S pk/namespace -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug >/dev/null
cmake --build "$BUILD" -j"$(nproc)" >/dev/null

"./$BUILD/test_pknamespace"

# 判据③：替代品本体不得有 Qt 未定义符号。本模块是纯头文件（pknamespace 是
# INTERFACE 目标，没有 .a 产物），判据③由**单测二进制**代查——它只链 pktest +
# 本模块头，真混进 Qt 依赖会在 nm 现形。`-C` 不能省：demangle 前的符号名带
# 版本后缀（Qt_5），不 demangle 的 grep -i qt 会漏掉非 Qt 前缀的真实依赖。
undef=$(nm -u -C "$BUILD/test_pknamespace" 2>/dev/null | grep -i qt || true)
if [ -n "$undef" ]; then
    printf 'nm -u -C test_pknamespace 里有 Qt 符号：\n%s\n' "$undef" >&2
    exit 1
fi
printf 'nm -u -C %s/test_pknamespace | grep -i qt: 无输出\n' "$BUILD"

# locks 自证：R-27 Task 2 只许动 pk/namespace/。build/ 已被根 .gitignore 排除，
# 所以这里连未跟踪文件一起查，出现前缀之外的路径就判失败。判定交给 git 自己的
# pathspec，不解析 porcelain 的输出文本（理由同 pk/global 的 run_tests.sh）。
printf '\ngit status --porcelain:\n'
git status --porcelain
stray=$(git status --porcelain -- . ':(exclude)pk/namespace')
if [ -n "$stray" ]; then
    printf 'run_tests.sh: 有改动落在 pk/namespace/ 之外：\n%s\n' "$stray" >&2
    exit 1
fi
printf 'git status --porcelain: 改动全部落在 pk/namespace/ 前缀内\n'
