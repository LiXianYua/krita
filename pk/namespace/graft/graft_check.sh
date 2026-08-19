#!/usr/bin/env bash
# R-27 Task 2 的 graft 试接证据。
#
# 两部分：
#   ① EXPECT_PASS —— 调用形状 driver（instantiate_namespace.cpp）：把
#      `QFlags<Qt::KeyboardModifier>` 组合（pk/flags 的 PkFlags 模板 + 本任务的
#      Qt 枚举族）在 4 个真实调用点形状下编译 + 跑绿。**这是本任务交付的试接
#      证据**：证明枚举 + QFlags 组合可用，真实文件一旦依赖墙打通就能接上。
#   ② EXPECT_FAIL —— 4 个真实生产 .cpp（KoToolBase.cpp / KoToolProxy.cpp /
#      KoShapeRubberSelectStrategy.cpp / DefaultTool.cpp）各登记其 FIRST 阻塞的
#      其它未交付依赖。这些文件在 locks 之外（libs/flake、plugins/tools 归 S 线），
#      编译停在第 1 个 missing include，到不了 QFlags 行 —— 正说明「卡点不在本
#      任务的枚举，而在依赖墙」。锁外处置见 README「锁外处置」。
#
# 形态照抄 pk/flags/graft/graft_check.sh（R-20）。
set -u
export LC_ALL=C
cd "$(dirname "$0")/../../.." || exit 1
CXX=${CXX:-g++}
INC=(
    -include pk/flags/compat/QFlags
    -I pk/flags/compat -I pk/flags
    -I pk/namespace -I pk/global
)
fail=0
try_compile() { "$CXX" -std=c++17 -fsyntax-only "${INC[@]}" -I "$(dirname "$1")" "$1"; }
check_expect_fail() {
    local f="$1" pattern="$2" reason="$3"; local out
    out=$(try_compile "$f" 2>&1)
    if [ $? -eq 0 ]; then printf '  graft 缺口已消失: %s（原登记：%s）\n' "$f" "$reason"; fail=1; return; fi
    if printf '%s\n' "$out" | grep -qE "$pattern"; then
        printf '  graft 按登记失败: %s（卡在 %s → %s）\n' "$f" "$pattern" "$reason"
    else printf '  graft 错因不匹配登记: %s（预期 /%s/，归属 %s）\n' "$f" "$pattern" "$reason"; printf '%s\n' "$out" | head -20 | sed 's/^/    /'; fail=1; fi
}

# ── EXPECT_PASS：调用形状 driver（本任务交付的试接证据）──────────────────
out=$("$CXX" -std=c++17 -include pk/flags/compat/QFlags -I pk/flags/compat -I pk/flags \
      -I pk/namespace -I pk/global \
      pk/namespace/graft/instantiate_namespace.cpp -o /tmp/r27_ns_graft 2>&1)
if [ $? -eq 0 ]; then
    if /tmp/r27_ns_graft; then printf '  graft driver 跑绿（4 个 QFlags<Qt::KeyboardModifier> 调用形状全过）\n'
    else printf '  graft driver 返回非 0（语义错）\n'; fail=1; fi
else printf '  graft driver 编不过:\n%s\n' "$out" | head -20; fail=1; fi

# ── EXPECT_FAIL：4 个真实 .cpp 卡在其它未交付类型（锁外登记）─────────────
check_expect_fail libs/flake/KoToolBase.cpp                        'QDebug'             'R-08 pk/log（<QDebug> 未交付）'
check_expect_fail libs/flake/KoToolProxy.cpp                        'kritaflake_export'  'S-08 剥 flake'
check_expect_fail libs/flake/tools/KoShapeRubberSelectStrategy.cpp  'kritaflake_export'  'S-08 剥 flake'
check_expect_fail plugins/tools/defaulttool/defaulttool/DefaultTool.cpp 'KoInteractionTool'  'S-08 剥 flake'

# 零改动自证
dirty=$(git status --porcelain -- libs/ plugins/)
if [ -n "$dirty" ]; then printf '  源树被改动了 —— 试接必须零改动\n' >&2; printf '%s\n' "$dirty" >&2; fail=1; fi
exit "$fail"
