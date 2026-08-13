#!/usr/bin/env bash
# 建 pk/pointer 独立工程、跑单测，再跑两条自证：
#   Qt 符号判据 —— libpkpointer.a 里不得有 Qt 未定义符号；
#   locks       —— 工作树的改动必须全部落在 pk/pointer/ 前缀内。
# 形态照 pk/geometry/tests/run_tests.sh。
set -eu
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

BUILD=pk/pointer/build
cmake -S pk/pointer -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug >/dev/null
cmake --build "$BUILD" -j"$(nproc)" >/dev/null

"./$BUILD/test_pkpointer"

# 替代品本体不得有 Qt 未定义符号。查的是 pk/pointer 编出来的静态库
#（不是 test_pkpointer 可执行文件——静态库允许留未定义符号，真混进 Qt 依赖会
# 在这里现形；可执行文件那种查法是恒真的，链接行里根本没 Qt 库，见
# pk/test/README.md §5）。
undef=$(nm -u "$BUILD/libpkpointer.a" 2>/dev/null | grep -i qt || true)
if [ -n "$undef" ]; then
    printf 'nm -u libpkpointer.a 里有 Qt 符号：\n%s\n' "$undef" >&2
    exit 1
fi
printf 'nm -u %s/libpkpointer.a | grep -i qt: 无输出\n' "$BUILD"

# 每个 compat 垫片必须能独立编译（只挂自己的 include 路径，什么都不多包）——
# 回归守卫，防止某个垫片偷偷依赖了另一个垫片先被包过这件事。
guard_fail=0
GUARDDIR="$BUILD/compat_include_guard"
mkdir -p "$GUARDDIR"
for shim in pk/pointer/compat/*; do
    base=$(basename "$shim")
    printf '#include "%s/%s"\nint main() { return 0; }\n' "$PWD" "$shim" \
        > "$GUARDDIR/guard_$base.cpp"
    if ! g++ -std=c++17 -fsyntax-only "$GUARDDIR/guard_$base.cpp" \
             2>"$GUARDDIR/guard_$base.err"; then
        printf 'compat/%s 不能独立编译：\n' "$base" >&2
        head -5 "$GUARDDIR/guard_$base.err" >&2
        guard_fail=1
    fi
done
if [ "$guard_fail" -ne 0 ]; then
    printf 'run_tests.sh: compat 垫片独立编译检查失败\n' >&2
    exit 1
fi
printf 'compat 垫片独立编译检查（%s 个）: 全部通过\n' "$(ls pk/pointer/compat | wc -l)"

# locks 自证：R-04 只许动 pk/pointer/。build/ 已被根 .gitignore 排除，所以这里
# 连未跟踪文件一起查，出现任何 pk/pointer/ 之外的路径就判失败。判定交给 git
# 自己的 pathspec，不解析 porcelain 的输出文本（理由见 pk/geometry/tests/run_tests.sh
# 同一段注释：改名行与含特殊字符路径的输出形状不适合按列切）。
printf '\ngit status --porcelain:\n'
git status --porcelain
stray=$(git status --porcelain -- . ':(exclude)pk/pointer')
if [ -n "$stray" ]; then
    printf 'run_tests.sh: 有改动落在 pk/pointer/ 之外：\n%s\n' "$stray" >&2
    exit 1
fi
printf 'git status --porcelain: 改动全部落在 pk/pointer/ 前缀内\n'
