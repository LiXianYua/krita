#!/usr/bin/env bash
# Task 4：KisMimeDatabase.h 真实消费者零改动编译试接。
#
# Q-7 的 19 个真实消费者（libs/koplugin/KisMimeDatabase.cpp 本身不算，它是被
# 替换对象）逐个实测，**全部 EXPECT_FAIL、0 个 EXPECT_PASS**——这不是覆盖不
# 全，是结构性事实：每个真实调用点都嵌在一个大 .cpp 里，这个 .cpp 自己的
# `#include` 链在真正引用到 KisMimeDatabase 之前，先撞上另一个尚未交付/尚无
# 归口的 Qt 类型或未剥离的 Krita 类。已经把当前全部已交付 R 任务
# （R-01 pk/string、R-02 pk/container、R-03 pk/geometry、R-04 pk/pointer、
# R-05 pk/signal、R-11 pk/test、R-12 pk/port）的 compat/ 全部挂上 -I，
# 并补了 CMake 生成导出头的本地 stub（照抄 pk/port/graft/stubs/ 先例）、
# 补了跨库 -I 排除路径找不到的假阻塞，卡点依旧——详见
# .superpowers/sdd/R-09/task-4-report.md。
#
# Q-7 本身的算法正确性已经由 Task 3 的自测证过（37/37 条表条目逐字节核对）；
# 本脚本的职责纯粹是把「真实消费者卡在哪个未交付能力上」登记成可执行断言，
# 供后续 S 批次交付对应能力后第一时间发现「缺口已消失，需要把 check_expect_fail
# 升级成 check_pass」——而不是证明现在就能编过。
set -u
export LC_ALL=C   # EXPECT_FAIL 的错因正则含英文编译器消息片段，locale 不同会
                   # 把 "fatal error" 之类译成中文，固定 LC_ALL=C 避免跨机器
                   # 失效（先例：pk/port/graft/graft_check.sh 评审 M-1）。
cd "$(dirname "$0")/../../../.." || exit 1

CXX=${CXX:-g++}
INC=(
    -include pk/config/compat/KisMimeDatabase.h
    -include pk/string/compat/QString
    -I pk/config/compat -I pk/config
    -I pk/string/compat -I pk/string
    -I pk/container/compat -I pk/container
    -I pk/geometry/compat -I pk/geometry
    -I pk/log/compat -I pk/log
    -I pk/pointer/compat -I pk/pointer
    -I pk/port/compat -I pk/port
    -I pk/signal/compat -I pk/signal
    -I pk/test/compat -I pk/test
    -I pk/config/tests/graft/stubs
    -I libs -I libs/resources -I libs/pigment -I libs/flake -I libs/store
    -I libs/global -I libs/impex -I libs/flake/text -I plugins
)
fail=0

try_compile() {
    # $1 = 文件路径；额外把候选文件自身目录及其上一级目录也加进 -I，覆盖
    # 「#include "同目录头"」与「#include <上级目录/头>」两种真实写法。
    local f="$1" d1 d2
    d1="$(dirname "$f")"
    d2="$(dirname "$d1")"
    "$CXX" -std=c++17 -fsyntax-only "${INC[@]}" -I "$d1" -I "$d2" "$f"
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
    # $1 = 文件路径, $2 = 错因正则, $3 = 归属说明（写进日志，供人核对）
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

# ── EXPECT_FAIL：全部 18 个真实消费者，逐个卡在自己 #include 链更上游的
#    未交付/未认领类型上，一个都没走到真正引用 KisMimeDatabase 的那一行 ──
check_expect_fail libs/impex/KisImportExportManager.cpp                              'QByteArray'         'R-02（PkByteArray：pk/port/README.md 早在 S-01 节预判过"S-01 实际开工前应该先确认 R-02 状态"；R-02(pk/container) 现已 VERIFIED，实测仍不含 PkByteArray——问题已从"待核"收敛为"确认存在的缺口，待排期"）'
check_expect_fail libs/impex/KisDocument.cpp                                         'QColor'             '尚无归口任务（R-09 Task 3 已报告：不在 R-03 几何、不在 R-06 PkVariant 范围）'
check_expect_fail libs/flake/KoShapeSavingContext.cpp                                'fatal error: Qt:'   '尚无归口任务（Qt module umbrella 头，无 R 任务声明覆盖）'
check_expect_fail libs/flake/svg/SvgStyleWriter.cpp                                  'QGradientStops'     '尚无归口任务'
check_expect_fail libs/flake/svg/SvgSavingContext.cpp                                'QFont'              '尚无归口任务（docs/TASKS.md 核实：本 worktree 无 docs/ 时未能核对，已由主会话侧的 task-agent 复核确认无归口任务）'
check_expect_fail libs/resources/KisFolderStorage.cpp                                'QDateTime'          'R-16（时钟 → std::chrono，docs/TASKS.md 明确把 QDateTime 记在范围内，实测 28 文件/75 处，NOT_STARTED）'
check_expect_fail libs/resources/KisMemoryStorage.cpp                                'QDateTime'          'R-16（同上）'
check_expect_fail libs/resources/KisRequiredResourcesOperators.cpp                   'QImage'             'R-15（沿用 pk/port/graft/graft_check.sh 既有登记）'
check_expect_fail libs/resources/KisStoragePlugin.cpp                                'QDateTime'          'R-16（同上）'
check_expect_fail libs/resources/KoResourceBundle.cpp                                'QDomDocument'       'R-07（Q-2 XML → pugixml，覆盖 QDom* 系列，NOT_STARTED）'
check_expect_fail libs/resources/KisResourceLocator.cpp                              'QDateTime'          'R-16（同上）'
check_expect_fail libs/resources/KisResourceLoaderRegistry.cpp                       'QImage'             'R-15'
check_expect_fail libs/resources/KisResourceLoader.cpp                               'QImage'             'R-15'
check_expect_fail libs/pigment/resources/KoPattern.cpp                               'QImage'             'R-15'
check_expect_fail plugins/flake/imageshape/ImageShape.cpp                            'QSharedDataPointer' '尚无归口任务（pk/pointer 现有 QScopedPointer/QSharedPointer/QWeakPointer/QScopedArrayPointer 四个，不含此类型）'
check_expect_fail plugins/impex/csv/csv_saver.cpp                                    'QFile'              'S-01（按 pk/port/README.md §5 既定：R-12 只出 PkStream 接口，具体文件/内存/zip 适配器延后到 S-01，非缺口）'
check_expect_fail plugins/impex/qimageio/kis_qimageio_export.cpp                     'QVariant'           'R-06（按 R-09 Task 4 brief「待回报」节口径，R-06 = PkVariant）'
check_expect_fail plugins/tools/defaulttool/referenceimagestool/ToolReferenceImages.cpp 'QPointer'        'R-05（已 VERIFIED，PkPointer 类型与 #define 已交付于 pk/signal/compat/QObject，但该目录缺一个独立的 pk/signal/compat/QPointer 转发文件——真实调用点 #include <QPointer> 不经过 <QObject>，现有垫片覆盖不到这条 include 形式；是 R-05 的一个小跟进项，不是新任务）'

dirty=$(git status --porcelain -- libs/ plugins/)
if [ -n "$dirty" ]; then
    printf '  源树被改动了 —— 试接必须零改动\n' >&2
    printf '%s\n' "$dirty" >&2
    fail=1
fi

exit "$fail"
