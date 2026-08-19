#!/usr/bin/env bash
# Task 5：真实调用点零改动语法试接（-fsyntax-only）。
#
# brief 原始指名的两个文件是 libs/resources/KisStoragePlugin.cpp（target
# kritaresources）与 libs/global/KisUsageLogger.cpp（target kritaglobal）。
# 两个都实测过——**都编不过**，而且都不是卡在 PkDateTime/PkElapsedTimer 本身：
#
#   KisStoragePlugin.cpp
#     KisStoragePlugin.h → KisResourceStorage.h（第 13 行 #include <QDateTime>，
#     R-16 已交付，编过）→ 第 16 行 #include <KoResource.h> → 第 10 行
#     #include <QImage> → **QImage 没有垫片**（R-15，NOT_STARTED）。零改动试接
#     必须原样保留这条 include 链，QDateTime 这一层已经证明能通过、卡在它下一行。
#   KisUsageLogger.cpp
#     文件本身第 8 行就是 #include <QScreen>——在第 11 行的 #include <QDateTime>
#     **之前**。QScreen 属于 Qt Widgets/GUI 栈，当前没有任何 R 任务声明覆盖
#     （pk/signal 只到 QObject 一层，不含窗口部件）。
#
# 用 pk/config/tests/graft/graft_check.sh 的 check_expect_fail 手法把这两个
# "卡在别的缺口、不是 PkTime 的缺口"如实登记下来（错因正则 + 归属，来源同一份
# 现场实测，不是抄旧文档）：验证 PkDateTime 那一层确实已经打通、缺口在更下游；
# 一旦 R-15 交付 QImage 或未来有任务覆盖 QScreen，脚本会自动报
# "缺口已消失，请更新登记"，而不是悄悄一直亮红灯或悄悄放宽判据混过去。
#
# 为了不让"两个 target 全部 EXPECT_FAIL、一次真正的 PASS 都没有"，额外找了
# 一个真正编过的第三方文件顶上——搜索方法：先自动扫了全仓（排除 tests/
# benchmarks）全部 37 个真实 QDateTime/QElapsedTimer 调用点，零个能直接过；
# 对其中若干个手工往深处试（加已有先例的 stub 头：kritaimage_export.h、
# kritaglobal_export.h、KoConfig.h、config-memory-leak-tracker.h 等构建期生成
# 头，均为既有 stub 先例，不是新造），逐一撞到 QImage / QScreen / QLineF /
# QPolygon / pugixml.hpp（第三方依赖，路径依赖 /tmp 不可复现）/ freetype+
# harfbuzz 字体栈 / KoColorConversionTransformation.h（Pigment 色彩子系统）
# 等同样未交付的缺口——**只有一个** 真正编过：
#   libs/image/kis_timed_signal_threshold.cpp（target kritaimage）
# 该文件只包 kis_timed_signal_threshold.h（QScopedPointer/QObject/
# kritaimage_export.h）与 QElapsedTimer/kis_debug.h（QDebug/QLoggingCategory/
# kritaglobal_export.h），真实调用点用到 PkElapsedTimer 的 isValid()/start()/
# elapsed()/invalidate() 四个成员函数——恰好是 R-16 Task 1 交付的核心 API 面，
# 零改动、零 stub 之外的手改，真正跑通了整条 include 链。
set -u
export LC_ALL=C   # EXPECT_FAIL 的错因正则含英文编译器消息片段，locale 不同会
                   # 把 "fatal error" 之类译成中文，固定 LC_ALL=C 避免跨机器
                   # 失效（先例：pk/port/graft/graft_check.sh 评审 M-1）。
cd "$(dirname "$0")/../../../.." || exit 1

CXX=${CXX:-g++}
INC=(
    -include pk/string/compat/QString
    -include pk/time/compat/QDateTime
    -include pk/time/compat/QElapsedTimer
    -I pk/time/compat -I pk/time
    -I pk/string/compat -I pk/string
    -I pk/container/compat -I pk/container
    -I pk/geometry/compat -I pk/geometry
    -I pk/log/compat -I pk/log
    -I pk/pointer/compat -I pk/pointer
    -I pk/port/compat -I pk/port
    -I pk/signal/compat -I pk/signal
    -I pk/test/compat -I pk/test
    -I pk/config/compat -I pk/config
    -I pk/concurrent/compat -I pk/concurrent
    -I pk/variant/compat -I pk/variant
    -I pk/xml/compat -I pk/xml
    -I pk/time/tests/graft/stubs
    -I libs -I libs/resources -I libs/pigment -I libs/flake -I libs/store
    -I libs/global -I libs/image -I libs/impex -I libs/flake/text -I plugins
)
fail=0

try_compile() {
    # $1 = 文件路径；额外把候选文件自身目录加进 -I，覆盖「#include "同目录头"」
    # 的写法（照抄 pk/config/tests/graft/graft_check.sh try_compile）。
    local f="$1" d1
    d1="$(dirname "$f")"
    "$CXX" -std=c++17 -fsyntax-only "${INC[@]}" -I "$d1" "$f"
}

check_pass() {
    local f="$1"
    local out
    out=$(try_compile "$f" 2>&1)
    if [ $? -eq 0 ]; then
        printf '  graft OK: %s\n' "$f"
    else
        printf '  graft FAILED（预期该编过）: %s\n' "$f"
        printf '%s\n' "$out" | head -20 | sed 's/^/    /'
        fail=1
    fi
}

check_expect_fail() {
    # $1 = 文件路径, $2 = 错因正则, $3 = 归属说明
    local f="$1" pattern="$2" reason="$3"
    local out
    out=$(try_compile "$f" 2>&1)
    if [ $? -eq 0 ]; then
        printf '  graft 缺口已消失，请更新登记: %s（原登记：%s）\n' "$f" "$reason"
        fail=1
        return
    fi
    if printf '%s\n' "$out" | grep -qE "$pattern"; then
        printf '  graft 按登记失败: %s（卡在 %s → %s）\n' "$f" "$pattern" "$reason"
    else
        printf '  graft 错因不匹配登记: %s（预期匹配 /%s/，归属 %s）\n' "$f" "$pattern" "$reason"
        printf '%s\n' "$out" | head -20 | sed 's/^/    /'
        fail=1
    fi
}

# ── 真正编过：证明 PkElapsedTimer 的四个真实调用点方法在生产代码里零改动可用 ──
check_pass libs/image/kis_timed_signal_threshold.cpp

# ── brief 指名的两个文件：EXPECT_FAIL，卡在 PkTime 范围之外的缺口 ──
check_expect_fail libs/resources/KisStoragePlugin.cpp 'QImage' \
    'R-15（QImage 尚未交付；KisResourceStorage.h:13 的 #include <QDateTime> 已经能通过 R-16 的垫片解析——卡点在其后 KoResource.h:10 的 #include <QImage>，不是 PkTime 的缺口）'
check_expect_fail libs/global/KisUsageLogger.cpp 'QScreen' \
    '尚无归口任务（Qt Widgets/GUI 栈；文件第 8 行 #include <QScreen> 在第 11 行 #include <QDateTime> 之前，卡点先于任何 PkTime 相关代码）'

dirty=$(git status --porcelain -- libs/ plugins/)
if [ -n "$dirty" ]; then
    printf '  源树被改动了 —— 试接必须零改动\n' >&2
    printf '%s\n' "$dirty" >&2
    fail=1
fi

exit "$fail"
