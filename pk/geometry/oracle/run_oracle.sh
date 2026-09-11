#!/usr/bin/env bash
# pk/geometry 与真 Qt5 的逐输入对拍：定位真 Qt → 编译 → 确认真链上了 Qt →
# 跑 → 把 DIFFTAG 与 geometry.deviation 双向核对 → 打结论。
#
# **判据自己编译、自己执行**：这样「这份输出是不是它产生的」不需要推断，
# 一条编译器调用加一条超时就把整类 attestation 问题关死了。
#
# 退出码：0 = 全部差异都已声明且 canary 齐全；非 0 = FAIL（原因打在 stderr）。
#
# ── 两栖（S-18 加 macOS 支）─────────────────────────────────────────────
# 本机是 macOS arm64，Qt 的落地形态与 Linux 不同，四处必须分叉：
#   ① 头文件位置：Linux `$QT/include/QtCore/qpoint.h`；macOS 在 framework 里
#      `$QT/lib/QtCore.framework/Headers/qpoint.h`。
#   ② 库的位置 + 链接旗标：Linux `-L$QT/lib -lQt5Core -lQt5Gui`；
#      macOS `-F$QT/lib -framework QtCore -framework QtGui`。
#   ③ 运行时依赖检查：Linux `ldd` + `LD_LIBRARY_PATH`；macOS `otool -L` +
#      `DYLD_FRAMEWORK_PATH`/`DYLD_LIBRARY_PATH`（macOS 的 dyld 不认 LD_*）。
#   ④ 超时：macOS 没有 GNU `timeout`（也不是 coreutils 默认装的），用本脚本
#      自带的 kill-after 包装。
# **判据本身两边逐字相同**（同一份 geometry_difftest.cpp、同一套 DIFFTAG ↔
# geometry.deviation 核对），分叉只在「怎么把真 Qt 编进来、怎么确认编进来了」。
# 「平台相关的测试期望值」这条口径见 `$PK/docs/superpowers/specs/S线-spec.md`
#「平台相关的测试期望值：改成与真 Qt 运行期对拍」：**判据是「真 Qt 与 pk 当场
# 对拍是否一致」，不是「两边各自跑出来的数是否相同」** —— 本脚本就是那个判据，
# 所以它必须能在这台机器上跑起来（在此之前它自 R-18 起就编不过，见下）。
set -euo pipefail
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

UNAME_S=$(uname -s)

# 依赖前缀。优先 PK_QT_PREFIX（原本就有）→ Darwin 上再退到 CMAKE_PREFIX_PATH
# （`source <prefix>/env` 会设它）→ Darwin 上按约定默认找「工作空间上层」的
# krita-ci-env/_install → 最后是 Linux 的既有默认（**原值，一字未改**）。
if [ -n "${PK_QT_PREFIX:-}" ]; then
    QT=$PK_QT_PREFIX
elif [ "$UNAME_S" = "Darwin" ]; then
    QT=""
    if [ -n "${CMAKE_PREFIX_PATH:-}" ]; then
        _p=$(printf '%s' "$CMAKE_PREFIX_PATH" | cut -d: -f1)
        [ -d "$_p" ] && QT=$_p
    fi
    if [ -z "$QT" ]; then
        # fork 根是 <工作空间>/krita 或 <工作空间>/krita-worktrees/<ID>，
        # 两种层级都要试；命中哪个用哪个。
        for _c in "$PWD/../krita-ci-env/_install" "$PWD/../../krita-ci-env/_install"; do
            if [ -d "$_c" ]; then QT=$(cd "$_c" && pwd); break; fi
        done
    fi
    [ -n "$QT" ] || { echo "找不到 Qt 依赖前缀：既没有 PK_QT_PREFIX，也没 source 过 env，" >&2
                      echo "  工作空间上层也没有 krita-ci-env/_install。见 MACOS-SHELL-NOTES.md §1。" >&2
                      exit 1; }
else
    QT=/mnt/ssd-disk/liyang/projects/krita-ci-env/_install
fi
SRC=pk/geometry/oracle/geometry_difftest.cpp
DEV=pk/geometry/oracle/geometry.deviation
OUT=pk/geometry/build/geometry_difftest
LOG=pk/geometry/build/geometry_difftest.out
# 规则三（每个已实现的重载都要有自己的 rec()）的机器闸门，见下面 §APISEEN
APIEXP=pk/geometry/oracle/api_seen.expected
# 每组一行 `<头文件>|<类名,类名…>|<map 文件>`。**加一族就在这里加一行**，
# 闸门代码只有一份。Task 4 修复轮之前只有 Rect 一行（评审 M-4）。
# ⚠ PkRect.h **出现两次**（PkRect 一行、PkRectF 一行）：解析器按
# `class <名字>\s*\{ … private:` 定位类体，一个头文件里的两个类各查各的 map。
# 不合成一行是有意的：两个类的重载集差得远（contains 一族的 proper 参数、
# toRect/toAlignedRect），合成一份 map 之后"哪条声明属于哪个类"就只能靠人读了。
API_GROUPS=(
    "pk/geometry/PkRect.h|PkRect|pk/geometry/oracle/rect_api.map"
    "pk/geometry/PkPoint.h|PkPoint,PkPointF|pk/geometry/oracle/point_api.map"
    "pk/geometry/PkSize.h|PkSize,PkSizeF|pk/geometry/oracle/size_api.map"
    "pk/geometry/PkRect.h|PkRectF|pk/geometry/oracle/rectf_api.map"
    "pk/geometry/PkTransform.h|PkTransform|pk/geometry/oracle/transform_api.map"
    "pk/geometry/PkLine.h|PkLine,PkLineF|pk/geometry/oracle/line_api.map"
    "pk/geometry/PkMargins.h|PkMargins,PkMarginsF|pk/geometry/oracle/margins_api.map"
    "pk/geometry/PkPolygon.h|PkPolygon,PkPolygonF|pk/geometry/oracle/polygon_api.map"
    "pk/geometry/PkVectorND.h|PkVector2D,PkVector3D,PkVector4D|pk/geometry/oracle/vectornd_api.map"
    "pk/geometry/PkMatrix4x4.h|PkMatrix4x4|pk/geometry/oracle/matrix4x4_api.map"
    "pk/geometry/PkRegion.h|PkRegion|pk/geometry/oracle/region_api.map"
)

if [ "$UNAME_S" = "Darwin" ]; then
    QT_HDRS=("$QT/lib/QtCore.framework/Headers" "$QT/lib/QtGui.framework/Headers")
    QT_ARTIFACTS=("$QT/lib/QtCore.framework/QtCore" "$QT/lib/QtGui.framework/QtGui")
    # QTransform 住在 QtGui 里（QPoint/QSize/QRect 在 QtCore）——两边都要链、
    # 两边都要查，理由与 Linux 支一字不差（见下面 Linux 支的注释）。
    QT_LINK=(-F"$QT/lib" -Wl,-rpath,"$QT/lib" -framework QtCore -framework QtGui)
    QT_RUNLIBS=("DYLD_FRAMEWORK_PATH=$QT/lib" "DYLD_LIBRARY_PATH=$QT/lib")
    SHARED_DEP_TOOL="otool -L"
    LDD_REQUIRE="QtCore QtGui"
else
    QT_HDRS=("$QT/include/QtCore" "$QT/include/QtGui")
    QT_ARTIFACTS=("$QT/include/QtCore/qpoint.h" "$QT/lib/libQt5Core.so")
    QT_LINK=(-L"$QT/lib" -Wl,-rpath-link,"$QT/lib" -Wl,-rpath,"$QT/lib" -lQt5Core -lQt5Gui)
    QT_RUNLIBS=("LD_LIBRARY_PATH=$QT/lib")
    SHARED_DEP_TOOL="ldd"
    LDD_REQUIRE="libQt5Core libQt5Gui"
fi

[ -f "${QT_HDRS[0]}/qpoint.h" ] || {
    echo "找不到真 Qt5 的头：${QT_HDRS[0]}/qpoint.h" >&2; exit 1; }
for a in "${QT_ARTIFACTS[@]}"; do
    [ -f "$a" ] || { echo "找不到真 Qt5 的库：$a" >&2; exit 1; }
done

# ⚠ **-I 里绝不能出现 compat**：垫片一旦被拉进来，<QPoint> 会解析到
# compat/QPoint（两个 #define），两侧变成同一个类型，跑出来必然零差异且看不出
# 破绽。difftest 源里有 #error 与 static_assert 两道兜底，但一开始就别写。
# ⚠ `pk/global` 是 R-18 折叠后新加的一项，**不是可选**：`geometry_difftest.cpp`
# 要 `#include "PkGlobal.cpp"`，而该文件 R-18 从 `pk/geometry/` 搬到了 `pk/global/`
#（`pk/geometry/PkGlobal.h` 只剩一个转发头）。少了这一项直接编不过
#（`'PkGlobal.cpp' file not found`）。加上它**同时修好 Linux 支**——那边自 R-18 起
# 同样编不过，只是没人在这台机器上跑过这个脚本。
INCS=("${QT_HDRS[0]}" "${QT_HDRS[1]}" "pk/geometry" "pk/global")
for i in "${INCS[@]}"; do
    case "$i" in
        *compat*) echo "run_oracle.sh: -I 里出现了 compat 垫片目录：$i" >&2; exit 1;;
    esac
done
INCFLAGS=()
for i in "${INCS[@]}"; do INCFLAGS+=("-I$i"); done

# ⚠ **对拍侧刻意不带 -fwrapv**（Task 4 修复轮的 controller 裁决，与库/单测侧相反）。
#
# 背景：`operator|` / `operator&` / `contains(QRect)` / `intersects` 的实现不在头
# 文件里，**编在 libQt5Core.so 里**，对拍 TU 的旗标到不了那侧。它们的翻正判据
# 写作 `x2 - x1 + 1 < 0`，跨距越出 int 时是有符号溢出 UB —— 对拍 TU 带 -fwrapv
# 时我们这侧按二补数回绕、.so 那侧按"溢出不会发生"推导，于是凭空多出
# **704 589 条差异**，而它们不是 PkRect 的行为偏离，纯粹是旗标不对等。
#
# 去掉 -fwrapv 之后同一份源码、同一批输入 mismatch = **3（只剩 canary）**：
# 对拍 TU 与 libQt5Core.so 现在带的是同一套旗标，比的是同一件事。
#   · geometry.deviation 回到全 R-03 canary-only，「任何非 canary tag = FAIL」
#     这条 R 线 spec 的地基保住；
#   · 原本被 `span-overflow-ub` 整片豁免的 **3 286 575 次比对（占 2.62%）**
#     现在是真比对（实测计数，不是估算）；
#   · 与 **Krita 发布构建的旗标一致**（那边同样不带 -fwrapv），对拍证明的东西
#     离出货形态更近。
# **代价**（写进 README）：溢出输入上"两侧一致"变成了「同编译器同旗标下的巧合」，
# 换一个 GCC 编出来的 Qt 可能冒出新 tag —— 但那是 FAIL（响的）不是静默放行。
#
# ⚠ **pk/geometry/CMakeLists.txt 里库与单测那份 -fwrapv 保留不变**，两边刻意不
# 一致：库侧没有 .so 对等物的问题，而 `-Os` 无 -fwrapv 时 pointManhattanLength()
# 会变红（Task 2 裁决，实测仍复现）。
#
# 曾经写在这里的另一条理由 ——「对拍这边 -O2 无 -fwrapv 时 std::to_string 打印
# 溢出后的负数直接段错误」—— **已不复现**：本轮跑满 125 338 365 次比对退出码 0。
# 那句现在时陈述随本轮删除，别再拿它当保留 -fwrapv 的依据。
# 浮点→int 越界（int(inf)）是另一类 UB，-fwrapv 管不着；实机上运行期两侧都编成
# 同一条 cvttsd2si，取值一致，但编译期常量折叠给的是另一个答案 —— 对拍的输入
# 来自运行期数组，天然不会被折叠；单测那边靠 noFold() 顶。
# ⚠ **-DQT_NO_DEBUG 是必需的，从 Task 3（Size 族）起。** qsize.h 的
# `operator/` 与 `operator/=` 里有 `Q_ASSERT(!qFuzzyIsNull(c))`，不定义这个宏时
# Q_ASSERT 展开成 qt_assert(...) → **abort()**：对拍一喂"除以 0"就整个程序死掉，
# 那一整片输入永远比不了。定义之后 Q_ASSERT 展开成 `(void)(false && cond)`，
# 与 **Krita 发布构建里的形态完全一致**（Qt5 的 cmake 模块给任何非 Debug 构建
# 追加 -DQT_NO_DEBUG，见 krita 根 CMakeLists.txt:968 那条 option 的说明文字）。
# pk/geometry 不实现 Q_ASSERT（断言设施归 R-08），这条登记在 README 偏离清单。
# 它只作用于**头文件里的 inline 代码**（libQt5Core 里的 out-of-line 实现是
# 编好的，不受影响），且 qpoint.h 里一个 Q_ASSERT 都没有 —— 实测加它前后
# Point 族的 total/mismatch 逐字不变。
CXXFLAGS_ORACLE=(-std=c++17 -O2 -fPIC -DQT_NO_DEBUG)

mkdir -p pk/geometry/build
CXXBIN=${CXX:-c++}
printf '编译：%s %s %s %s\n' "$CXXBIN" "${CXXFLAGS_ORACLE[*]}" "${INCFLAGS[*]}" "$SRC"
"$CXXBIN" "${CXXFLAGS_ORACLE[@]}" "${INCFLAGS[@]}" -o "$OUT" "$SRC" "${QT_LINK[@]}"

# ── §CONSTEXPR：PkTransform.h 不得出现 constexpr ─────────────────────────
#
# QTransform **一个 constexpr 成员都没有**（qtransform.h 里连 Q_DECL_CONSTEXPR
# 都没出现过），而 PkPoint/PkSize/PkRect 三族全是 constexpr —— 恰好相反。
# 给替代品顺手加一个 constexpr 就是**比 Qt 多一档能力**：调用点能把它用进常量
# 表达式，换回真 Qt 时当场编不过，而"多一档"这个方向对拍看不见（它只比取值）。
#
# 为什么是文本闸门而不是断言：C++17 里**没法用 static_assert 反证「某表达式不是
# 常量表达式」**（能断言"是"，不能断言"不是"）。tests/test_transform.cpp 的
# transformConstexprSurfaceIsGatedByText 只负责证明 harness 分得开两种情况，
# 真正守住这条的是下面这三行。
#
# 去注释再查：头文件的说明文字里本来就会提到这个词（`Q_DECL_CONSTEXPR`、
# 「一个 constexpr 都没有」），直接 grep 会恒真。
if sed 's|//.*||' pk/geometry/PkTransform.h | grep -q 'constexpr'; then
    echo "run_oracle.sh: PkTransform.h 里出现了 constexpr —— QTransform 一个都没有，" >&2
    echo "  加上去等于替代品比 Qt 多一档能力（说明见本脚本 §CONSTEXPR）：" >&2
    sed 's|//.*||' pk/geometry/PkTransform.h | grep -n 'constexpr' >&2
    exit 1
fi
printf 'PkTransform.h 去注释后无 constexpr：与 qtransform.h 的能力面一致\n'

# 判据：**真的链上了 Qt**。链不上说明两侧都编到了替代品，零差异是假的。
printf '\n%s %s | grep -i qt:\n' "$SHARED_DEP_TOOL" "$OUT"
env "${QT_RUNLIBS[@]}" $SHARED_DEP_TOOL "$OUT" | grep -i qt || true
for lib in $LDD_REQUIRE; do
    if ! env "${QT_RUNLIBS[@]}" $SHARED_DEP_TOOL "$OUT" | grep -q "$lib"; then
        echo "run_oracle.sh: $SHARED_DEP_TOOL 里看不到 $lib —— 没有真的链上 Qt" >&2
        exit 1
    fi
done

printf '\n跑对拍：\n'
# 超时用 kill-after 包装，**不用 GNU `timeout`**：macOS 上它不存在（也不是
# coreutils 默认装的），而这一条判据要能在本机跑。语义与 `timeout 600` 对齐：
# 到点 SIGKILL、把 128+9=137 交回去（Linux 上 timeout 交的是 124，所以下面
# 那句诊断两种码都要认）。
#
# ⚠ **`|| rc=$?` 不是可有可无的写法。** `set -e` 之下 `cmd > log` 一旦非零就
# 立刻终止整个脚本，下面的 `rc=$?` 与 `tail` 是**死代码** —— 对拍程序崩溃时
# 一行诊断都打不出来（复评注入时实测撞上：SIGFPE、退出码 136、无任何输出）。
# `|| rc=$?` 让非零退出被消费掉，控制权才走得到诊断分支。
rc=0
env "${QT_RUNLIBS[@]}" "$OUT" > "$LOG" 2>&1 &
ORACLE_PID=$!
( sleep 600; kill -9 "$ORACLE_PID" 2>/dev/null ) & ORACLE_KILLER=$!
wait "$ORACLE_PID" || rc=$?
kill -9 "$ORACLE_KILLER" 2>/dev/null || true
wait "$ORACLE_KILLER" 2>/dev/null || true
if [ "$rc" -ne 0 ]; then
    echo "run_oracle.sh: 对拍程序退出码 $rc（契约要求 0，即使 mismatch>0）" >&2
    # ⚠ 写成 `[ ... ] && echo ...` 会再犯同一个错：条件为假时整条语句返回 1，
    # `set -e` 当场退出，下面的 tail 又变成死代码。用 if/fi。
    if [ "$rc" -gt 128 ]; then
        echo "  （>128：被信号 $((rc - 128)) 杀掉；124 = Linux timeout 超时、137 = 本脚本 kill-after 超时）" >&2
    fi
    echo "  $LOG 末 20 行：" >&2
    tail -20 "$LOG" >&2
    exit 1
fi
grep -E '^(DIFFTAG|DIFFDEN|DIFF) ' "$LOG" || true

# ── §APISEEN：规则三的机器闸门 ───────────────────────────────────────────
#
# 规则三是「**每个已实现的重载都要有自己的 rec()**」。Task 3 复评实测过它的
# 反面：`PkSizeF::scale(qreal,qreal,mode)` 少写了一条 rec，把那个重载整个改坏
# 之后 **93 630 039 次比对一条都没红、本脚本退出码 0 放行**。在那之前这条规则
# 只能靠人手列对照表 —— 而 Rect 是 8 分量、重载多得多，同类漏写更容易发生。
#
# 这里把它变成机器对账，三件事任一不成立就 FAIL：
#   ① 程序打出的 APISEEN 集合 == api_seen.expected（多一条少一条都算漂移）
#   ② **每个头文件类体里的每一条声明**都在对应的 <族>_api.map 里有一行，反之亦然
#      —— 这一条才是真正堵住 Task 3 那个洞的：加了重载却没写 rec() 时，
#      新声明在 map 里找不到对应行，当场 FAIL
#   ③ map 里列的每个标签都真的出现在 APISEEN 里
# 期望清单仍然是人维护的，但**漂移由机器抓** —— 机器查机械的那一半。
#
# ⚠ **Task 4 修复轮（评审 M-4）：②③ 从只管 Rect 扩到 Point / Size 三族。**
# 在那之前 Point/Size 只有 api_seen.expected 这一份清单，而它的内容来自对拍
# 程序自己打出的 APISEEN —— 用 rec() 去证明 rec() 没漏，是自证循环。
# 接进同一个解析器之后三族同一条判据：头文件声明是**独立来源**。
python3 - "$LOG" "$APIEXP" "${API_GROUPS[@]}" <<'PY'
import re, sys

log, exp_path = sys.argv[1], sys.argv[2]
groups = [g.split('|') for g in sys.argv[3:]]

seen = {l[len('APISEEN '):].strip()
        for l in open(log, encoding='utf-8', errors='replace')
        if l.startswith('APISEEN ')}
expected = {l.strip() for l in open(exp_path, encoding='utf-8')
            if l.strip() and not l.startswith('#')}

ok = True
if seen != expected:
    ok = False
    print('FAIL: APISEEN 与 %s 不一致（规则三闸门 ①）' % exp_path, file=sys.stderr)
    for a in sorted(seen - expected):
        print('  程序打出但清单里没有：%s' % a, file=sys.stderr)
    for a in sorted(expected - seen):
        print('  清单里有但程序没打出：%s' % a, file=sys.stderr)


# 内联函数体：**换成 `;` 而不是留着**。留着的话按 `;` 切会切进函数体内部，
# 声明与半截体粘成一句、正则匹配不上 —— 那条声明从 decls 里**消失**，闸门就
# 不再守它。实测两条这样丢过：PkPoint::dotProduct 与 PkPointF::dotProduct。
#
# ⚠ 失败模式是**响的，不是静默的**：丢掉的那条同时在 miss 里留下一个碎片，而
# miss 非空 → ok=False → FAIL。逐头文件核过 `旧 decls + 旧 miss == 新 decls`
# （PkPoint 29+1=30、PkPointF 25+1=26、Size/Rect 三个类 miss=0、前后逐字不变）。
# 这个缺陷在父提交上从未被触发：解析器当时**只指向 PkRect.h**，而 PkRect.h 类体
# 里没有内联函数体 —— 缺陷是潜伏的，不是被触发了却没报。换成 `;` 是为了把类体
# 带内联实现的头文件（PkPoint.h/PkSize.h）也纳进来，不是为了堵一个静默放行的洞。
#
# ⚠ **结构性前提①**：按字符数花括号，**块注释 `/* */` 里的花括号会打乱 depth**
# （`//` 行注释在进来之前已经去掉了，`{` `}` 出现在字符串/字符字面量里同理）。
# 三个头文件现在都没有块注释，所以现在成立 —— 将来抄这套骨架的头文件里一旦出现
# 块注释，depth 会算错、整段类体被吃掉，届时要先去块注释再进这里。
def strip_bodies(s):
    out, depth = [], 0
    for ch in s:
        if ch == '{':
            depth += 1; continue
        if ch == '}':
            depth -= 1
            if depth == 0:
                out.append(';')
            continue
        if depth == 0:
            out.append(ch)
    return ''.join(out)


# 类体里的声明指纹：去注释 → 取 class 体 public 段 → 去内联函数体 → 逐条声明
# 规范化（去形参名、去默认实参、把 `T &x` / `T *x` 收成 `T&` / `T*`）。
# 键带类名前缀：PkPoint/PkPointF 这类孪生类有大量同名同参声明，不带前缀会撞键。
#
# ⚠ **结构性前提②**（继承自 Task 3 的骨架，非本轮引入）：下面那条 `class X {(.*?)
# \n\s*private:` 是**非贪婪**的，停在**第一个** `private:`。三个头文件现在都是
# 「public 段一整块 + 末尾一个 private:」的形态，所以现在成立 —— 将来若出现
# `private:` 之后**再开 `public:`** 的段，那些成员会**静默**漏掉（不是 miss、不是
# FAIL：它们压根没进 m.group(1)，闸门①②③三道都看不见它们）。抄这套骨架前先确认
# 头文件是不是这个形态；不是就得改成扫全类体、按访问说明符切段。
#
# ⚠ **`class X` 与 `{` 之间允许一个可选的基类子句**（R-21 T2 新增，`class
# PkPolygon : public PkVector<PkPoint>` 是这个 Task 的第一个有继承的类）。
# `(?:\s*:\s*[^{]*)?` 吃掉 `: public PkVector<PkPoint>` 这一段——`[^{]*`
# 在遇到第一个 `{`（类体开始）之前停，不会吃过界。六个既有类都没有基类
# 子句，这条可选分支在它们身上匹配空串，闸门行为逐字不变。
def parse_decls(hdr_path, cls):
    src = re.sub(r'//[^\n]*', '', open(hdr_path, encoding='utf-8').read())
    m = re.search(r'class %s(?:\s*:\s*[^{]*)?\s*\{(.*?)\n\s*private:' % re.escape(cls), src, re.S)
    if m is None:
        return None, None
    decls, miss, const_marks = [], [], []
    for stmt in strip_bodies(m.group(1)).split(';'):
        stmt = ' '.join(stmt.split())
        if not stmt:
            continue
        # ⚠ **枚举声明跳过**（Task 6 起，PkTransform::TransformationType 是第一个）。
        # strip_bodies 按花括号剥函数体，枚举体也被它剥掉，剩下 `enum X` 这一句
        # —— 它匹配不上下面的函数声明正则，会掉进 miss 里把闸门整个判 FAIL。
        # 枚举没有"重载"可言、也没有对应的 rec()，规则三本来就管不着它，
        # 所以这里放行，**但只放行严格长成 `enum [class] 名字 [: 底层类型]`
        # 这一种**：多一个括号、多一个星号都不匹配，函数声明伪装不成枚举。
        # 枚举取值本身由 PkTransform.cpp 尾部的 static_assert 与
        # geometry_difftest.cpp 顶部「两侧枚举取值一致」那条 static_assert 守。
        # ⚠ 前缀 `public:` 要一起吃掉：类体第一条声明前面就是它，而按 `;` 切
        # 不会把访问说明符切开（它后面是 `:` 不是 `;`）。函数声明那条正则用的是
        # re.search，前缀天然被跳过；这里是 fullmatch，必须显式写出来。
        if re.fullmatch(r'(?:(?:public|protected|private)\s*:\s*)?'
                        r'enum(\s+class)?\s+[A-Za-z_][A-Za-z0-9_]*'
                        r'(\s*:\s*[A-Za-z_][A-Za-z0-9_ ]*)?', stmt):
            continue
        # ⚠ **typedef 别名声明跳过**（R-21 T5 新增，`typedef const PkRect
        # *const_iterator` 是第一个）：typedef 没有"重载"可言、也没有对应的
        # rec()，规则三管不着它（与枚举同一理由）。只放行严格长成
        # `typedef 类型 名字` 这一种。
        if re.fullmatch(r'(?:(?:public|protected|private)\s*:\s*)?typedef\s+.+', stmt):
            continue
        mm = re.search(r'(operator\s*\(\s*\)|operator[^\s(]*|~?[A-Za-z_][A-Za-z0-9_]*)\s*\(([^()]*)\)'
                       r'\s*(const)?\s*(?:noexcept)?\s*$', stmt)
        if not mm:
            miss.append(stmt); continue
        ps = []
        for p in mm.group(2).split(','):
            p = ' '.join(p.split('=')[0].split())
            p = re.sub(r'\s+[A-Za-z_][A-Za-z0-9_]*$', '', p)
            p = p.replace(' &', '&').replace(' *', '*')
            p = re.sub(r'([&*])[A-Za-z_][A-Za-z0-9_]*$', r'\1', p)
            if p:
                ps.append(p)
        decls.append('%s::%s(%s)' % (cls, mm.group(1), ','.join(ps)))
        # 记录这条声明是不是 const 限定的（R-21 T3 首次撞上 const/non-const
        # 重载对：`float &operator[](int)` 与 `float operator[](int) const` 剥掉
        # const 后是同一个指纹，会触发上面的重复指纹 FAIL。记下 const 标记，
        # 下面统一消歧）。
        const_marks.append(mm.group(3) is not None)
    # ⚠ **const/non-const 重载对消歧**（R-21 T3 新增）：`operator[]` 这类成员
    # 有 `T& op(int)` 与 `T op(int) const` 两个重载，剥掉 const 后指纹相同。
    # 处置：同一个类里指纹出现两次及以上时，const 那一条的指纹追加 ` const`
    # 后缀，使两条可区分。既有七个类没有一个有 const/non-const 同签名重载对，
    # 所以这条分支对它们不改变任何行为（它们的指纹都只出现一次）。
    from collections import Counter
    cnt = Counter(decls)
    for i in range(len(decls)):
        if cnt[decls[i]] > 1 and const_marks[i]:
            decls[i] += ' const'
    return decls, miss


summary = []
for hdr_path, classes, map_path in groups:
    decls, miss = [], []
    for cls in classes.split(','):
        d, ms = parse_decls(hdr_path, cls)
        if d is None:
            ok = False
            print('FAIL: 解析不出 %s 的类体（%s）—— 头文件结构变了，闸门失效'
                  % (cls, hdr_path), file=sys.stderr)
            continue
        decls += d
        miss += ms
    dup = sorted({d for d in decls if decls.count(d) > 1})
    if dup:
        ok = False
        print('FAIL: %s 解析出重复的声明指纹（会让 map 少一行也查不出来）：' % hdr_path,
              file=sys.stderr)
        for d in dup:
            print('  ' + d, file=sys.stderr)
    if miss:
        ok = False
        print('FAIL: %s 类体里有解析不了的声明（闸门会漏掉它们）：' % hdr_path,
              file=sys.stderr)
        for s in miss:
            print('  ' + s, file=sys.stderr)

    mapping = {}
    for n, line in enumerate(open(map_path, encoding='utf-8'), 1):
        if line.startswith('#') or not line.strip():
            continue
        cols = line.rstrip('\n').split('\t')
        if len(cols) != 2:
            print('FAIL: %s:%d 不是两列 tab 分隔' % (map_path, n), file=sys.stderr)
            sys.exit(1)
        if not cols[1].strip():
            print('FAIL: %s:%d 标签列为空 —— 每条声明都必须落到至少一条 rec()'
                  % (map_path, n), file=sys.stderr)
            sys.exit(1)
        # ⚠ 多标签用 **`;`** 分隔，不能用逗号：标签名自己就含逗号
        # （`operator*(float,rev)`、`S::scale(w,h)`），用逗号切会把一个标签
        # 劈成两半，然后闸门③ 拿两个不存在的名字去查 —— 实测踩过。
        mapping[cols[0]] = [x for x in cols[1].split(';') if x]

    undeclared = [d for d in decls if d not in mapping]
    orphan = [k for k in mapping if k not in decls]
    if undeclared:
        ok = False
        print('FAIL: %s 里有声明在 %s 里没有对应行（规则三闸门 ②）——'
              % (hdr_path, map_path), file=sys.stderr)
        print('      新加的重载必须同时有一条自己的 rec() 和这里的一行：', file=sys.stderr)
        for d in undeclared:
            print('  ' + d, file=sys.stderr)
    if orphan:
        ok = False
        print('FAIL: %s 里有行对不上 %s 的任何声明（成员删了却留着行）：'
              % (map_path, hdr_path), file=sys.stderr)
        for k in orphan:
            print('  ' + k, file=sys.stderr)

    for d in decls:
        for lab in mapping.get(d, []):
            if lab not in seen:
                ok = False
                print('FAIL: %s 映射到标签 %s，但对拍里根本没有这条 rec()（闸门 ③）'
                      % (d, lab), file=sys.stderr)
    summary.append('%s 声明 %d 条 / %s %d 行'
                   % (hdr_path.rsplit('/', 1)[-1], len(decls),
                      map_path.rsplit('/', 1)[-1], len(mapping)))

print('\n规则三机器对账：' + '；'.join(summary))
print('                APISEEN %d 个（期望 %d）' % (len(seen), len(expected)))
sys.exit(0 if ok else 1)
PY

# ── DIFFTAG ↔ geometry.deviation 双向核对 ────────────────────────────────
# 理由长度按 **UTF-8 码点**计，与 locale 无关（${#var} 的口径跟着 locale 走，
# LC_ALL=C 下会把 25 字中文数成 75，门槛静默松掉）。
python3 - "$LOG" "$DEV" <<'PY'
import sys

log, dev = sys.argv[1], sys.argv[2]

seen, den, diff_lines = {}, {}, []
for line in open(log, encoding='utf-8', errors='replace'):
    # DIFFDEN 是**分母**（该 tag 的谓词命中过多少次比对）。
    # ⚠ 它**不进闸门**：第三列的额度仍然只比分子（DIFFTAG）。分母是给人推导
    # 「为什么恰好是这么多」用的 —— 光有分子说不清"喂了多少次里分家这么多"。
    # 必须在 DIFFTAG 那条 elif 之前判，否则 startswith('DIFF ') 那条也吃不到它
    # （'DIFFDEN ' 不以 'DIFF ' 开头，所以其实无所谓，但顺序写清楚免得以后踩）。
    if line.startswith('DIFFDEN '):
        parts = line.split()
        if len(parts) != 4:
            print(f'FAIL: DIFFDEN 行格式不对：{line.rstrip()}', file=sys.stderr); sys.exit(1)
        den[(parts[1], parts[2])] = int(parts[3])
    elif line.startswith('DIFFTAG '):
        parts = line.split()
        if len(parts) != 4:
            print(f'FAIL: DIFFTAG 行格式不对：{line.rstrip()}', file=sys.stderr); sys.exit(1)
        seen[(parts[1], parts[2])] = int(parts[3])
    elif line.startswith('DIFF '):
        diff_lines.append(line.rstrip())

if len(diff_lines) != 1:
    print(f'FAIL: DIFF 行必须恰好一行，实得 {len(diff_lines)} 行', file=sys.stderr); sys.exit(1)
kv = dict(p.split('=', 1) for p in diff_lines[0].split()[1:])
total, mismatch = int(kv['total']), int(kv['mismatch'])
if total < 100000:
    print(f'FAIL: total={total} 太小，喂几条就报一致等于没对拍', file=sys.stderr); sys.exit(1)

declared = {}
for n, line in enumerate(open(dev, encoding='utf-8'), 1):
    if line.startswith('#') or not line.strip():
        continue
    cols = line.rstrip('\n').split('\t')
    if len(cols) != 4:
        print(f'FAIL: {dev}:{n} 不是四列 tab 分隔'
              f'（<api> <tag> <期望计数> <理由>）', file=sys.stderr); sys.exit(1)
    api, tag, want, reason = cols
    if not want.strip().isdigit():
        print(f'FAIL: {dev}:{n} 第三列「{want}」不是十进制整数计数', file=sys.stderr)
        sys.exit(1)
    if len(reason) < 20:                      # str 的长度就是码点数
        print(f'FAIL: {dev}:{n} 理由只有 {len(reason)} 个码点，门槛 20', file=sys.stderr); sys.exit(1)
    declared[(api, tag)] = (int(want), reason)

undeclared = sorted(k for k in seen if k not in declared)
# **额度闸门**（Task 4 修复轮，评审 Important 1）：已声明的 tag 曾经是**无限
# 额度**的白名单 —— 键在清单里就放行，计数只打印。评审员实测把 4 个 .so 侧
# API 的判据加宽（16 处源码），差异从 704 589 涨到 1 485 313（2.1 倍），
# **两个脚本双双 exit=0**。现在计数写进 geometry.deviation 第三列，漂移即 FAIL。
#
# **「掉到 0」也是漂移**（Task 4 修复轮 2，复评 Important B）：drift 曾经只
# 遍历 `seen`，于是「声明 500 次、实得 499 次」FAIL，而「声明 500 次、实得
# **0** 次」只落进下面的 stale WARN、exit 0 —— **幅度最大的那一档反而最松**。
# 第三列是**额度**不是上限：用少了与用超了一样说明行为变了却没人判断过（那条
# 路径被优化掉、输入生成器改窄、判据被改宽到不再触发，都长这样）。所以 drift
# 现在覆盖 `declared` 全集，没观察到的按**实得 0** 计。
drift = sorted((k, declared[k][0], seen.get(k, 0)) for k in declared
               if seen.get(k, 0) != declared[k][0])
# 与 drift 的分工（**同一条差异不会既 FAIL 又 WARN**）：
#   · 额度 > 0 却一次没观察到 → 上面的 drift **FAIL**（期望 N、实得 0），不到这里；
#   · 额度写着 **0** 且确实没观察到 → 才落到这里 WARN。这种行对闸门没有任何
#     作用：同一个 tag 真出现时，声明了走 drift FAIL、没声明走 undeclared FAIL，
#     结果一样 —— 所以它只是一行死配置，提醒删掉，不是错误。
# canary 行（额度 1）若消失会同时触发 drift 与下面的 missing_canary，两条都是
# FAIL、不矛盾：drift 报的是数字，missing_canary 报的是「比较管道已经死了」这个
# 具体诊断，后者才是要看的那句。
stale = sorted(k for k in declared if k not in seen and declared[k][0] == 0)

# canary 必须全部出现：它们是「比较管道还活着」的自证，消失就说明 mismatch
# 这个数字已经不反映任何东西了。
canaries = sorted(k for k in declared if k[0] == 'canary')
missing_canary = [k for k in canaries if k not in seen]

print(f'\n对拍结论：total={total} mismatch={mismatch} '
      f'tag={len(seen)}（其中 canary {len(canaries)}）')
for k, v in sorted(seen.items()):
    kind = 'canary' if k[0] == 'canary' else ('已声明' if k in declared else '**未声明**')
    want = f'，期望 {declared[k][0]}' if k in declared else ''
    # 「命中 N 次里分家 M 次」—— 额度那一列为什么恰好是这么多，靠这个读。
    n = den.get(k)
    ratio = f'（命中 {n} 次{"，命中即分家" if n == v else f"，另 {n - v} 次两侧相同"}）' \
        if n is not None else ''
    print(f'  {k[0]} {k[1]} {v}{want}{ratio}  [{kind}]')
if den:
    print(f'  ── 分母合计 {sum(den.values())}，分子合计 {sum(seen.values())}')

ok = True
if missing_canary:
    print('FAIL: canary 消失了 —— 比较管道被写死/被优化掉/tag 构造断了：'
          + ', '.join(f'{a} {t}' for a, t in missing_canary), file=sys.stderr)
    ok = False
if undeclared:
    print('FAIL: 出现未在 geometry.deviation 里声明的差异（= 没人判断过它可不可接受）：',
          file=sys.stderr)
    for a, t in undeclared:
        print(f'  {a} {t} {seen[(a, t)]}', file=sys.stderr)
    ok = False
if drift:
    print('FAIL: 已声明的 tag 计数漂移（额度用超/用少 = 行为变了却没人判断过）：',
          file=sys.stderr)
    for (a, t), want, got in drift:
        print(f'  {a} {t} 期望 {want}，实得 {got}（差 {got - want:+d}）', file=sys.stderr)
    ok = False
if stale:
    print('WARN: 额度写着 0 又确实没观察到 —— 这行对闸门不起作用（真出现时无论'
          '声明与否都 FAIL），建议删掉：'
          + ', '.join(f'{a} {t}' for a, t in stale), file=sys.stderr)

sys.exit(0 if ok else 1)
PY

printf '\nrun_oracle.sh: 通过 —— 全部差异都已声明，canary 齐全\n'
