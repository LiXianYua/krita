#!/usr/bin/env bash
# pk/color 的收尾入口：建独立工程 → 跑单测 → 跑 R-87 判据 → 判据③（零 Qt 符号）。
#
# 形态照抄 pk/geometry/tests/run_tests.sh（同一套「配置 + 构建 + 逐个跑 + 自证」的结构）。
# ⚠ 但**不是**逐字复制：pk/geometry 那份里 R-39 pathops 对拍、R-58 UBSan 闸门、
#   R-63 把 run_oracle.sh/graft_run.sh 串进路径、compat 垫片的「先包 QtGlobal」逐个守卫、
#   以及几何自己的 locks 自证——那些都是 **pk/geometry 的**判据链，与 pk/color 无关，
#   不搬过来（搬过来只会给 pk/color 的收尾凭空加几条它管不着的红/绿）。
#
# ⚠ **pk/color/oracle/run_oracle.sh 与 pk/color/graft/graft_check.sh 刻意不串**：
#   R-87 的 plan §4-B 明确把对拍侧划在范围外（它链真 Qt，是判据② 的工具）。
#   要串它们请另立任务，不要顺手加进来——对拍红一次的代价见 pk/geometry/README.md
#   的 R-58/R-63 两节。
set -eu
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

BUILD=pk/color/build

# macOS 上必须显式给部署目标 13.3：pk/ 层用到 std::to_chars 的浮点重载（macOS ≥ 13.3），
# 而 krita-ci-env/env 把 MACOSX_DEPLOYMENT_TARGET 设成 10.15 ⇒ 不覆盖就会在
# pk/string/PkString_format.cpp 报 `'to_chars' is unavailable: introduced in macOS 13.3`。
# pkcolor **PUBLIC 链 pkstring** ⇒ 本脚本一定会编到那个 TU，这条不是可选项。
# 既有先例：pk/render/tests/run_tests.sh:62-72、pk/image/oracle/run_oracle.sh:143。
extra_cmake_args=()
if [ "$(uname -s)" = "Darwin" ]; then
    extra_cmake_args+=(-DCMAKE_OSX_DEPLOYMENT_TARGET=13.3)
fi

# 读的是 env 里的 $KDECI_CC_CACHE（每次 cmake 配置都要带 ccache 两个 -D，漏了等于全量重编）。
if [ -n "${KDECI_CC_CACHE:-}" ]; then export CCACHE_DIR="$KDECI_CC_CACHE"; fi

cmake -S pk/color -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug \
      "${extra_cmake_args[@]}" \
      -DCMAKE_C_COMPILER_LAUNCHER=ccache \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache >/dev/null

# `nproc` 在本机不存在（实测 `which nproc` → not found，source 过 env 依然没有）。
# pk/geometry 那份直接 `-j"$(nproc)"`，靠「命令替换失败不触发 set -e」侥幸过；新脚本
# 不沿用这个侥幸，取一个真实存在的核数。
JOBS=$( (nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4) )
cmake --build "$BUILD" -j"$JOBS" >/dev/null

# ── 单测 + R-87 判据，**都从 ctest 走** ──────────────────────────────────────
# pk/color 的收尾入口本来就是 ctest（impact-map §5：pk/color 此前没有 run_tests.sh，
# 它的收尾入口是 CMakeLists 里的 `add_test(NAME test_pkcolor …)`）。R-87 的判据
# `test_pkcolor_comparable` 也因此**注册成 add_test**，与 test_pkcolor 同门进入 ——
# 这就是「判据必须在收尾路径上」的落点：不在路径上的判据是装饰。
ctest --test-dir "$BUILD" --output-on-failure

# ── 判据③（R线-spec 强判据）：替代品本体不得有 Qt 未定义符号 ─────────────────
# ⚠ **强口径**：`nm -u -C`（-C 反修饰不能省）+ `grep -E '\bQ[A-Z][A-Za-z0-9_]*\b'`。
#   **不用 `grep -i qt`** —— 旧口径已被 R线-spec 2026-09-15 判为「不构成证据」
#   （R-70 实测：强判据命中 14，旧口径漏 10）。pk/test 与 pk/geometry 那两份至今仍是
#   旧口径，那是它们的任务要修的事，**本文件新写，直接按强口径来**。
#   pkcolor PUBLIC 链 pkstring/pkglobal/pktest ⇒ 查 libpkcolor.a 即覆盖整条链。
undef=$(nm -u -C "$BUILD/libpkcolor.a" 2>/dev/null | grep -E '\bQ[A-Z][A-Za-z0-9_]*\b' || true)
if [ -n "$undef" ]; then
    printf 'nm -u -C libpkcolor.a 里有 Qt 符号：\n%s\n' "$undef" >&2
    exit 1
fi
printf 'nm -u -C %s/libpkcolor.a | grep -E "\\bQ[A-Z][A-Za-z0-9_]*\\b": 无输出\n' "$BUILD"

printf '\npk/color 收尾路径: 全部通过\n'
