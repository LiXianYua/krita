#!/usr/bin/env bash
# R-08 Task 7b：`QDebug operator<<` 零改动核验。
#
# 决策文档断言「QDebug operator<< 有 96 处——Krita 给自己的类型定义了流式
# 输出，换 spdlog 要改成 fmt::formatter 特化」。我们做的是流式门面
# （PkDebug/PkLoggingCategory 经 compat/QDebug、compat/QLoggingCategory 两个
# 宏垫片顶替 QDebug/QLoggingCategory），理论上这些调用点一处都不用改。
# Task 6b 已经实测其中 1 处（kis_pinned_shared_ptr.h:64-68）手工改动 0 处；
# 本脚本把这个结论从 1 处扩到全仓候选头能验的范围。
#
# 做法：对每个候选头生成一个最小 TU（#include <QDebug> + #include "<候选头>"
# + 空 main），用与 graft 相近的编译行编译，把结果分三类：
#   A 原样编过
#   B 只因别的未替代能力（QVector/QPointF/KoID/boost/……，或探针 -I 范围
#     narrow 到只有候选头自己目录、够不到它依赖的另一个真实 Krita 头）
#     编不过——与 pk/log 无关
#   C 因 pk/log 缺能力编不过 —— 缺陷，必须修；修不掉就如实报告卡在哪
#
# 范围上界：不为了让某个头编过而往 stubs/ 堆垫片（那会把 B 类伪装成 A 类）。
# stubs 只保持 Task 5/6 已有的那些（kritaglobal_export.h、kritaimage_export.h
# 等）。本脚本只读 pk/log、不改。
#
# 跑起来会比较久（候选头近百个，每个都是一次独立的 g++ -c），正常现象。
set -u
# 评审 Minor 项：classify_reason() 靠匹配 gcc 的"没有那个文件或目录"这句中文
# 错误串来判定"缺文件"这一类；换一个非中文 locale（LANG/LC_ALL 不是
# zh_CN），gcc 会改说英文，正则匹配落空，全部缺文件的用例都会跌进
# UNCLASSIFIED、脚本以 exit 2 收场——别人在自己机器上复现不出 A=1 B=48 C=0
# 这个结论。锁定 LC_ALL=C，让 gcc 的诊断串在任何机器上都是同一种英文形态。
export LC_ALL=C
cd "$(dirname "$0")/../../../.." || exit 1   # → fork 仓库根

STUBS=pk/log/tests/graft/stubs
CXX=${CXX:-g++}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# Step 1：列候选（brief 给定的原样命令）。
mapfile -t CANDIDATES < <(
    git ls-files '*.h' '*.hpp' | grep -vE '(^|/)(tests|benchmarks)/' \
        | xargs grep -lE 'QDebug[[:space:]]+operator[[:space:]]*<<' | sort
)

# ---------------------------------------------------------------------------
# classify_reason：从编译器第一条 error/fatal error 行里抠出"卡在哪个记号
# 上"，再套几条已知规则归类。规则识别不出的一律标 UNCLASSIFIED，交人工看——
# 宁可漏判成"需要人看"，不能把 C 误判成 B。
# ---------------------------------------------------------------------------
classify_reason() {
    local log="$1" hdr="$2"
    local first
    first=$(grep -m1 -E 'error:' "$log" || true)
    if [ -z "$first" ]; then
        printf 'UNCLASSIFIED|(编译失败但没抓到 error: 行，见 %s)\n' "$log"
        return
    fi

    # 形态一：`fatal error: X: No such file or directory`（找不到头文件）。
    # LC_ALL=C 锁定这句一定是英文（见脚本顶部），不再依赖运行机器的 locale。
    local missing
    missing=$(printf '%s\n' "$first" | sed -nE 's/.*fatal error: ([^:]+): No such file or directory.*/\1/p')
    if [ -n "$missing" ]; then
        case "$missing" in
            *_export.h)
                printf 'B|CMake 生成的导出宏头未生成/本 worktree 没垫（%s），与 pk/log 无关\n' "$missing"
                return ;;
            klocalizedstring.h)
                printf 'B|KI18n 头（%s）未替代，与 pk/log 无关\n' "$missing"
                return ;;
            boost/*)
                printf 'B|boost 头（%s）未替代，与 pk/log 无关\n' "$missing"
                return ;;
            Eigen/*)
                printf 'B|Eigen 头（%s）未替代，与 pk/log 无关\n' "$missing"
                return ;;
            Q*)
                printf 'B|未替代的 Qt 类型/头：%s，与 pk/log 无关\n' "$missing"
                return ;;
        esac
        # 剩下的形态：多是"候选头依赖同仓库另一个真实 Krita 头，但那个头不在
        # 候选头自己的目录里"——本探针的 -I 只加了候选头自己所在目录（brief
        # 给定的编译行如此），够不到。用文件是否真实存在于仓库里做一次核实，
        # 核实到了才归这一类，核实不到就是 UNCLASSIFIED 交人工看。
        local base
        base=$(basename "$missing")
        if git ls-files | grep -qF "/$base" 2>/dev/null || git ls-files | grep -qxF "$base" 2>/dev/null; then
            printf 'B|依赖同仓库另一个真实 Krita 头（%s），不在候选头自己目录，探针 -I 范围够不到——与 pk/log 无关（是探针的目录范围局限，不是类型替代缺口）\n' "$missing"
            return
        fi
        printf 'UNCLASSIFIED|缺文件 %s，来源不明，见 %s\n' "$missing" "$log"
        return
    fi

    # 形态二：`error: 'X' does not name a type` / `X was not declared` 之类——
    # 头文件本身存在（在 -I 范围内，或已被别的规则顶掉），但里面用到的某个
    # 符号没有可见声明（常见于"真 Qt 靠别的头透传把类型带进来，我们没复刻那
    # 条透传链"，Task 5/6 已经踩过不止一次，同一个坑）。
    # gcc 用 Unicode 弯引号 ‘…’ 包裹记号名，不是 ASCII 直引号。
    local sym
    sym=$(printf '%s\n' "$first" | sed -nE 's/.*error: .([A-Za-z_][A-Za-z0-9_:]*). does not name a type.*/\1/p')
    if [ -n "$sym" ]; then
        case "$sym" in
            Q*)
                printf 'B|未替代的 Qt 类型 %s 在此 TU 里不可见（真 Qt 靠别的头透传带进来，探针没有那条透传链），与 pk/log 无关\n' "$sym"
                return ;;
            *)
                printf 'UNCLASSIFIED|符号 %s 不可见，来源不明，见 %s\n' "$sym" "$log"
                return ;;
        esac
    fi

    printf 'UNCLASSIFIED|(规则识别不出的错误形态，第一行：%s；见 %s)\n' "$first" "$log"
}

a_count=0
b_count=0
c_count=0
u_count=0
declare -a a_lines=() b_lines=() c_lines=() u_lines=()

i=0
for hdr in "${CANDIDATES[@]}"; do
    i=$((i + 1))
    hdrdir=$(dirname "$hdr")
    base=$(basename "$hdr")
    tu="$WORK/tu_$i.cpp"
    printf '#include <QDebug>\n#include "%s"\nint main() { return 0; }\n' "$base" > "$tu"
    log="$WORK/log_$i.txt"

    if "$CXX" -std=c++17 -DQT_NO_DEBUG -include pk/string/compat/QString \
        -I pk/log -I pk/log/compat -I pk/string -I pk/string/compat \
        -I "$STUBS" -I "$hdrdir" \
        -c "$tu" -o "$WORK/tu_$i.o" 2>"$log"; then
        a_count=$((a_count + 1))
        a_lines+=("$hdr")
        printf 'A  %s\n' "$hdr"
    else
        classify_result=$(classify_reason "$log" "$hdr")
        cls="${classify_result%%|*}"
        reason="${classify_result#*|}"
        case "$cls" in
            B)
                b_count=$((b_count + 1))
                b_lines+=("$hdr  ::  $reason")
                printf 'B  %s\n     %s\n' "$hdr" "$reason"
                ;;
            C)
                c_count=$((c_count + 1))
                c_lines+=("$hdr  ::  $reason")
                printf 'C  %s\n     %s\n' "$hdr" "$reason"
                ;;
            *)
                u_count=$((u_count + 1))
                u_lines+=("$hdr  ::  $reason")
                printf 'UNCLASSIFIED  %s\n     %s\n' "$hdr" "$reason"
                ;;
        esac
    fi
done

total=${#CANDIDATES[@]}

printf '\n===== A 类（原样编过，%d）=====\n' "$a_count"
for l in "${a_lines[@]:-}"; do [ -n "$l" ] && printf '  %s\n' "$l"; done

printf '\n===== B 类（只因别的未替代能力受阻，与 pk/log 无关，%d）=====\n' "$b_count"
for l in "${b_lines[@]:-}"; do [ -n "$l" ] && printf '  %s\n' "$l"; done

printf '\n===== C 类（因 pk/log 缺能力编不过，缺陷，%d）=====\n' "$c_count"
for l in "${c_lines[@]:-}"; do [ -n "$l" ] && printf '  %s\n' "$l"; done

if [ "$u_count" -gt 0 ]; then
    printf '\n===== UNCLASSIFIED（规则识别不出，需要人工看，%d）=====\n' "$u_count"
    for l in "${u_lines[@]:-}"; do [ -n "$l" ] && printf '  %s\n' "$l"; done
fi

printf '\n结论：%d 个候选头里 A=%d B=%d C=%d' "$total" "$a_count" "$b_count" "$c_count"
if [ "$u_count" -gt 0 ]; then
    printf '（另有 %d 个规则识别不出，UNCLASSIFIED，需人工复核，未计入 A/B/C）' "$u_count"
fi
printf '，因 pk/log 缺能力而需要手工改动的：%d 处\n' "$c_count"

if [ "$u_count" -gt 0 ]; then
    exit 2
fi
exit 0
