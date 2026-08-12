#!/usr/bin/env bash
# pk/geometry 与真 Qt5 的逐输入对拍：定位真 Qt → 编译 → ldd 确认真链上了 Qt →
# 跑 → 把 DIFFTAG 与 geometry.deviation 双向核对 → 打结论。
#
# **判据自己编译、自己执行**：这样「这份输出是不是它产生的」不需要推断，
# 一条 g++ 加一条 timeout 就把整类 attestation 问题关死了。
#
# 退出码：0 = 全部差异都已声明且 canary 齐全；非 0 = FAIL（原因打在 stderr）。
set -euo pipefail
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

QT=${PK_QT_PREFIX:-/mnt/ssd-disk/liyang/projects/krita-ci-env/_install}
SRC=pk/geometry/oracle/geometry_difftest.cpp
DEV=pk/geometry/oracle/geometry.deviation
OUT=pk/geometry/build/geometry_difftest
LOG=pk/geometry/build/geometry_difftest.out

[ -f "$QT/include/QtCore/qpoint.h" ] || { echo "找不到真 Qt5 的头：$QT/include/QtCore/qpoint.h" >&2; exit 1; }
[ -f "$QT/lib/libQt5Core.so" ]       || { echo "找不到真 Qt5 的库：$QT/lib/libQt5Core.so" >&2; exit 1; }

# Task 6（PkTransform）要把 -lQt5Gui 加进来，并把 REQUIRE 改成两个库都查。
QT_LIBS="-lQt5Core"
LDD_REQUIRE="libQt5Core"

# ⚠ **-I 里绝不能出现 compat**：垫片一旦被拉进来，<QPoint> 会解析到
# compat/QPoint（两个 #define），两侧变成同一个类型，跑出来必然零差异且看不出
# 破绽。difftest 源里有 #error 与 static_assert 两道兜底，但一开始就别写。
INCS=("$QT/include" "$QT/include/QtCore" "$QT/include/QtGui" "pk/geometry")
for i in "${INCS[@]}"; do
    case "$i" in
        *compat*) echo "run_oracle.sh: -I 里出现了 compat 垫片目录：$i" >&2; exit 1;;
    esac
done
INCFLAGS=()
for i in "${INCS[@]}"; do INCFLAGS+=("-I$i"); done

# ⚠ **-fwrapv 是必需的，不是调优。** QPoint::manhattanLength 是 qAbs(x)+qAbs(y)，
# operator+ 是裸的 int 相加 —— 在 INT_MIN/INT_MAX 上都是**有符号溢出 UB**，而
# 那批输入正是最该对拍的形态（Qt 自己就这么写，替代品照抄）。不加 -fwrapv 时
# -O2 会拿"溢出不可能发生"去推导取值范围，实测的表现是 std::to_string 在打印
# 溢出后的负数时**段错误**（GCC 13 把 __to_chars_len 的分支优化没了）。
# 加上之后两侧都按二补数回绕，比的仍然是同一件事，且结果与 -O0 逐字相同
#（Task 2 报告里贴了 -O0/-O2 两次的 total 与 mismatch 对照）。
# 浮点→int 越界（int(inf)）是另一类 UB，-fwrapv 管不着；实机上两侧都编成
# 同一条 cvttsd2si，取值一致。这条写进 README 的覆盖度缺口。
CXXFLAGS_ORACLE=(-std=c++17 -O2 -fwrapv -fPIC)

mkdir -p pk/geometry/build
printf '编译：g++ %s %s %s\n' "${CXXFLAGS_ORACLE[*]}" "${INCFLAGS[*]}" "$SRC"
g++ "${CXXFLAGS_ORACLE[@]}" "${INCFLAGS[@]}" -o "$OUT" "$SRC" \
    -L"$QT/lib" -Wl,-rpath-link,"$QT/lib" -Wl,-rpath,"$QT/lib" $QT_LIBS

# 判据：**真的链上了 Qt**。链不上说明两侧都编到了替代品，零差异是假的。
printf '\nldd %s | grep -i qt:\n' "$OUT"
LD_LIBRARY_PATH="$QT/lib" ldd "$OUT" | grep -i qt || true
if ! LD_LIBRARY_PATH="$QT/lib" ldd "$OUT" | grep -q "$LDD_REQUIRE"; then
    echo "run_oracle.sh: ldd 里看不到 $LDD_REQUIRE —— 没有真的链上 Qt" >&2
    exit 1
fi

printf '\n跑对拍：\n'
LD_LIBRARY_PATH="$QT/lib" timeout 600 "$OUT" > "$LOG" 2>&1
rc=$?
if [ "$rc" -ne 0 ]; then
    echo "run_oracle.sh: 对拍程序退出码 $rc（契约要求 0，即使 mismatch>0）" >&2
    tail -20 "$LOG" >&2
    exit 1
fi
grep -E '^(DIFFTAG|DIFF) ' "$LOG" || true

# ── DIFFTAG ↔ geometry.deviation 双向核对 ────────────────────────────────
# 理由长度按 **UTF-8 码点**计，与 locale 无关（${#var} 的口径跟着 locale 走，
# LC_ALL=C 下会把 25 字中文数成 75，门槛静默松掉）。
python3 - "$LOG" "$DEV" <<'PY'
import sys

log, dev = sys.argv[1], sys.argv[2]

seen, diff_lines = {}, []
for line in open(log, encoding='utf-8', errors='replace'):
    if line.startswith('DIFFTAG '):
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
    if len(cols) != 3:
        print(f'FAIL: {dev}:{n} 不是三列 tab 分隔', file=sys.stderr); sys.exit(1)
    api, tag, reason = cols
    if len(reason) < 20:                      # str 的长度就是码点数
        print(f'FAIL: {dev}:{n} 理由只有 {len(reason)} 个码点，门槛 20', file=sys.stderr); sys.exit(1)
    declared[(api, tag)] = reason

undeclared = sorted(k for k in seen if k not in declared)
stale      = sorted(k for k in declared if k not in seen)

# canary 必须全部出现：它们是「比较管道还活着」的自证，消失就说明 mismatch
# 这个数字已经不反映任何东西了。
canaries = sorted(k for k in declared if k[0] == 'canary')
missing_canary = [k for k in canaries if k not in seen]

print(f'\n对拍结论：total={total} mismatch={mismatch} '
      f'tag={len(seen)}（其中 canary {len(canaries)}）')
for k, v in sorted(seen.items()):
    kind = 'canary' if k[0] == 'canary' else ('已声明' if k in declared else '**未声明**')
    print(f'  {k[0]} {k[1]} {v}  [{kind}]')

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
if stale:
    print('WARN: 声明了却没观察到（白名单过期，或对拍没走到那条路径）：'
          + ', '.join(f'{a} {t}' for a, t in stale), file=sys.stderr)

sys.exit(0 if ok else 1)
PY

printf '\nrun_oracle.sh: 通过 —— 全部差异都已声明，canary 齐全\n'
