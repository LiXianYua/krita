#!/usr/bin/env bash
# pk/global 与真 Qt5 的逐输入对拍：定位真 Qt → 编译 → ldd 确认真链上了 Qt →
# 跑 → 把 DIFFTAG 与 global.deviation 双向核对 → 打结论。
#
# **判据自己编译、自己执行**：这样「这份输出是不是它产生的」不需要推断，
# 一条 g++ 加一条 timeout 就把整类 attestation 问题关死了。
#
# 退出码：0 = 全部差异都已声明且 canary 齐全；非 0 = FAIL（原因打在 stderr）。
set -euo pipefail
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

QT=${PK_QT_PREFIX:-/mnt/ssd-disk/liyang/projects/krita-ci-env/_install}
SRC=pk/global/oracle/global_difftest.cpp
DEV=pk/global/oracle/global.deviation
OUT=pk/global/build/global_difftest
LOG=pk/global/build/global_difftest.out
# 规则三（每个已实现的重载都要有自己的 rec()）的机器闸门，见下面 §APISEEN
APIEXP=pk/global/oracle/api_seen.expected

[ -f "$QT/include/QtCore/qglobal.h" ] || { echo "找不到真 Qt5 的头：$QT/include/QtCore/qglobal.h" >&2; exit 1; }
[ -f "$QT/lib/libQt5Core.so" ]        || { echo "找不到真 Qt5 的库：$QT/lib/libQt5Core.so" >&2; exit 1; }

# 标量函数全在 libQt5Core 里（qglobal.h/qmath.h/qlatin1string.h 的 inline +
# qnumeric.h 的 Q_CORE_EXPORT out-of-line）。没有 QTransform 那类住 Gui 的成员。
QT_LIBS="-lQt5Core"
LDD_REQUIRE="libQt5Core"

# ⚠ **-I 里绝不能出现 compat**：垫片一旦被拉进来，<QtGlobal> 会解析到
# pk/test 的 compat/QtGlobal（qFuzzyCompare/qFuzzyIsNull 被 #define 成
# pkFuzzyCompare/pkFuzzyIsNull），P 侧 PkGlobal.h 的让位机制被触发，两侧只剩
# 一份实现 —— 跑出来必然零差异且看不出破绽。difftest 源里有 #error 兜底，
# 但一开始就别写。
INCS=("$QT/include" "$QT/include/QtCore" "pk/global")
for i in "${INCS[@]}"; do
    case "$i" in
        *compat*) echo "run_oracle.sh: -I 里出现了 compat 垫片目录：$i" >&2; exit 1;;
    esac
done
INCFLAGS=()
for i in "${INCS[@]}"; do INCFLAGS+=("-I$i"); done

# ⚠ **对拍侧带 -fwrapv**（与 geometry 的 run_oracle.sh 相反，这条是**有意为之**，
# 理由不同，见下）：geometry 不带是因为它的 out-of-line 实现编在 libQt5Core.so 里、
# 那侧没带 -fwrapv，对拍 TU 带了对不上。**global 的 out-of-line 符号只有 qnumeric.h
# 的 qIsNaN/qInf/qQNaN/qNaN（纯浮点，-fwrapv 对它们零影响）**，其余全是头文件
# inline（qglobal.h/qmath.h）——qAbs/qMin/qMax/qBound/qFuzzy*/qFloor/qCeil 这些标量
# 函数编进对拍 TU 时吃的是**我们的旗标**。带 -fwrapv 的理由是**两侧必须在同一套
# 有符号溢出语义下比较**：
#   ① 出货侧一致：shipping 构建（CMakeLists 那份 -fwrapv，PUBLIC）里
#      qAbs(INT_MIN) 这类 `-t` 溢出按二补数回绕、与 -O0 逐字相同；对拍侧不带
#      -fwrapv 就是 UB，两侧不在同一语义下，观察到的就不是出货形态；
#   ② 都带 -fwrapv 后，整数溢出两侧一致地回绕（Q/P 两侧的公式逐字相同），
#      比的是同一件事。
# ⚠ 浮点→int 越界（int(inf)）是另一类 UB，-fwrapv 管不着；实机上运行期两侧都编成
# 同一条 cvttsd2si，取值一致 —— 所以对拍的输入必须来自**运行期数组**（编译期常量
# 折叠给的是另一个答案，R-03 实测）。
#
# ⚠ **-DQT_NO_DEBUG 是必需的**：PkGlobal 的 Q_ASSERT 宏与真 Qt 的关闭条件不同
# （详见 global.deviation 首行登记）。两侧都 -DQT_NO_DEBUG 之后宏展开一致，这正
# 是登记行想锁的口径。
CXXFLAGS_ORACLE=(-std=c++17 -O2 -fPIC -fwrapv -DQT_NO_DEBUG)

mkdir -p pk/global/build
printf '编译：g++ %s %s %s\n' "${CXXFLAGS_ORACLE[*]}" "${INCFLAGS[*]}" "$SRC"
g++ "${CXXFLAGS_ORACLE[@]}" "${INCFLAGS[@]}" -o "$OUT" "$SRC" \
    -L"$QT/lib" -Wl,-rpath-link,"$QT/lib" -Wl,-rpath,"$QT/lib" $QT_LIBS

# 判据：**真的链上了 Qt**。链不上说明两侧都编到了替代品，零差异是假的。
# （P 侧是 pkoracle:: 里的 constexpr inline，编译时全内联，零 Qt —— ldd 看见
# libQt5Core 只可能来自 Q 侧的 :qIsNaN/:qFloor 那些 Q_CORE_EXPORT 符号。）
printf '\nldd %s | grep -i qt:\n' "$OUT"
LD_LIBRARY_PATH="$QT/lib" ldd "$OUT" | grep -i qt || true
for lib in $LDD_REQUIRE; do
    if ! LD_LIBRARY_PATH="$QT/lib" ldd "$OUT" | grep -q "$lib"; then
        echo "run_oracle.sh: ldd 里看不到 $lib —— 没有真的链上 Qt" >&2
        exit 1
    fi
done

printf '\n跑对拍：\n'
# ⚠ **`|| rc=$?` 不是可有可无的写法。** `set -e` 之下 `cmd > log` 一旦非零就
# 立刻终止整个脚本，下面的 `rc=$?` 与 `tail` 是**死代码**。`|| rc=$?` 让非零
# 退出被消费掉，控制权才走得到诊断分支。
rc=0
LD_LIBRARY_PATH="$QT/lib" timeout 600 "$OUT" > "$LOG" 2>&1 || rc=$?
if [ "$rc" -ne 0 ]; then
    echo "run_oracle.sh: 对拍程序退出码 $rc（契约要求 0，即使 mismatch>0）" >&2
    if [ "$rc" -gt 128 ]; then
        echo "  （>128：被信号 $((rc - 128)) 杀掉；124 = timeout 超时）" >&2
    fi
    echo "  $LOG 末 20 行：" >&2
    tail -20 "$LOG" >&2
    exit 1
fi
grep -E '^(DIFFTAG|DIFFDEN|DIFF) ' "$LOG" || true

# ── §APISEEN：规则三的机器闸门 ───────────────────────────────────────────
# 规则三是「**每个已实现的重载都要有自己的 rec()**」。标量侧是**自由函数**，
# 没有类体可解析（geometry 的声明→map 对账是给成员函数用的），所以这半边只守
# ① 程序打出的 APISEEN 集合 == api_seen.expected（多一条少一条都算漂移）。
# 清单内容来自 brief 的 API×tag 表：哪个重载上了表，这里就必须有一行。
python3 - "$LOG" "$APIEXP" <<'PY'
import sys

log, exp_path = sys.argv[1], sys.argv[2]
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

print('\n规则三机器对账：APISEEN %d 个（期望 %d）' % (len(seen), len(expected)))
sys.exit(0 if ok else 1)
PY

# ── DIFFTAG ↔ global.deviation 双向核对 ──────────────────────────────────
# 理由长度按 **UTF-8 码点**计，与 locale 无关（${#var} 的口径跟着 locale 走）。
# 抄自 geometry 同节；**Q_ASSERT 登记行不需要特判**：它作为 DIFFTAG 打出、
# 也在 deviation 里声明（期望计数 1），走的就是普通「已声明」通道。
python3 - "$LOG" "$DEV" <<'PY'
import sys

log, dev = sys.argv[1], sys.argv[2]

seen, den, diff_lines = {}, {}, []
for line in open(log, encoding='utf-8', errors='replace'):
    # DIFFDEN 是**分母**（该 tag 的谓词命中过多少次比对），**不进闸门**：
    # 第三列的额度仍然只比分子（DIFFTAG）。分母是给人推导用的。
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
if total < 12000:
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
# **额度闸门**：已声明 tag 的计数写进 deviation 第三列，漂移即 FAIL。
# 「掉到 0」也是漂移：遍历 declared 全集，没观察到的按实得 0 计。
drift = sorted((k, declared[k][0], seen.get(k, 0)) for k in declared
               if seen.get(k, 0) != declared[k][0])
stale = sorted(k for k in declared if k not in seen and declared[k][0] == 0)

# canary 必须全部出现：它们是「比较管道还活着」的自证。
canaries = sorted(k for k in declared if k[0] == 'canary')
missing_canary = [k for k in canaries if k not in seen]

print(f'\n对拍结论：total={total} mismatch={mismatch} '
      f'tag={len(seen)}（其中 canary {len(canaries)}）')
for k, v in sorted(seen.items()):
    kind = 'canary' if k[0] == 'canary' else ('已声明' if k in declared else '**未声明**')
    want = f'，期望 {declared[k][0]}' if k in declared else ''
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
    print('FAIL: 出现未在 global.deviation 里声明的差异（= 没人判断过它可不可接受）：',
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
