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
# 规则三（每个已实现的重载都要有自己的 rec()）的机器闸门，见下面 §APISEEN
APIEXP=pk/geometry/oracle/api_seen.expected
RECTMAP=pk/geometry/oracle/rect_api.map
RECTHDR=pk/geometry/PkRect.h

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
# 加上之后两侧都按二补数回绕，比的仍然是同一件事，且结果与 -O0 逐字相同。
# **两侧本来都是 UB，我们比的是"钉死之后的行为"，这不等于 Krita 发布构建里的
# 行为**（那边不带 -fwrapv）—— 完整口径写在 README 的覆盖度缺口。
# pk/geometry/CMakeLists.txt 里同样带着这个旗标（target_compile_options PUBLIC），
# 两边必须一致，否则对拍与单测钉的不是同一套行为。
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
CXXFLAGS_ORACLE=(-std=c++17 -O2 -fwrapv -fPIC -DQT_NO_DEBUG)

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
# ⚠ **`|| rc=$?` 不是可有可无的写法。** `set -e` 之下 `cmd > log` 一旦非零就
# 立刻终止整个脚本，下面的 `rc=$?` 与 `tail` 是**死代码** —— 对拍程序崩溃时
# 一行诊断都打不出来（复评注入时实测撞上：SIGFPE、退出码 136、无任何输出）。
# `|| rc=$?` 让非零退出被消费掉，控制权才走得到诊断分支。
rc=0
LD_LIBRARY_PATH="$QT/lib" timeout 600 "$OUT" > "$LOG" 2>&1 || rc=$?
if [ "$rc" -ne 0 ]; then
    echo "run_oracle.sh: 对拍程序退出码 $rc（契约要求 0，即使 mismatch>0）" >&2
    # ⚠ 写成 `[ ... ] && echo ...` 会再犯同一个错：条件为假时整条语句返回 1，
    # `set -e` 当场退出，下面的 tail 又变成死代码。用 if/fi。
    if [ "$rc" -gt 128 ]; then
        echo "  （>128：被信号 $((rc - 128)) 杀掉；124 = timeout 超时）" >&2
    fi
    echo "  $LOG 末 20 行：" >&2
    tail -20 "$LOG" >&2
    exit 1
fi
grep -E '^(DIFFTAG|DIFF) ' "$LOG" || true

# ── §APISEEN：规则三的机器闸门 ───────────────────────────────────────────
#
# 规则三是「**每个已实现的重载都要有自己的 rec()**」。Task 3 复评实测过它的
# 反面：`PkSizeF::scale(qreal,qreal,mode)` 少写了一条 rec，把那个重载整个改坏
# 之后 **93 630 039 次比对一条都没红、本脚本退出码 0 放行**。在那之前这条规则
# 只能靠人手列对照表 —— 而 Rect 是 8 分量、重载多得多，同类漏写更容易发生。
#
# 这里把它变成机器对账，三件事任一不成立就 FAIL：
#   ① 程序打出的 APISEEN 集合 == api_seen.expected（多一条少一条都算漂移）
#   ② PkRect.h **类体里的每一条声明**都在 rect_api.map 里有一行，反之亦然
#      —— 这一条才是真正堵住 Task 3 那个洞的：加了重载却没写 rec() 时，
#      新声明在 map 里找不到对应行，当场 FAIL
#   ③ rect_api.map 里列的每个标签都真的出现在 APISEEN 里
# 期望清单仍然是人维护的，但**漂移由机器抓** —— 机器查机械的那一半。
python3 - "$LOG" "$APIEXP" "$RECTMAP" "$RECTHDR" <<'PY'
import re, sys

log, exp_path, map_path, hdr_path = sys.argv[1:5]

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

# PkRect.h 类体里的声明指纹：去注释 → 取 class 体 public 段 → 逐条声明
# 规范化（去形参名、去默认实参、把 `T &x` / `T *x` 收成 `T&` / `T*`）。
src = re.sub(r'//[^\n]*', '', open(hdr_path, encoding='utf-8').read())
m = re.search(r'class PkRect\s*\{(.*?)\n\s*private:', src, re.S)
if not m:
    print('FAIL: 解析不出 PkRect 的类体 —— 头文件结构变了，闸门失效', file=sys.stderr)
    sys.exit(1)
decls, miss = [], []
for stmt in m.group(1).split(';'):
    stmt = ' '.join(stmt.split())
    if not stmt:
        continue
    mm = re.search(r'(operator[^\s(]*|~?[A-Za-z_][A-Za-z0-9_]*)\s*\(([^()]*)\)'
                   r'\s*(?:const)?\s*(?:noexcept)?\s*$', stmt)
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
    decls.append('%s(%s)' % (mm.group(1), ','.join(ps)))
if miss:
    ok = False
    print('FAIL: PkRect.h 类体里有解析不了的声明（闸门会漏掉它们）：', file=sys.stderr)
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
    mapping[cols[0]] = [x for x in cols[1].split(',') if x]

undeclared = [d for d in decls if d not in mapping]
orphan = [k for k in mapping if k not in decls]
if undeclared:
    ok = False
    print('FAIL: PkRect.h 里有声明在 %s 里没有对应行（规则三闸门 ②）——' % map_path,
          file=sys.stderr)
    print('      新加的重载必须同时有一条自己的 rec() 和这里的一行：', file=sys.stderr)
    for d in undeclared:
        print('  ' + d, file=sys.stderr)
if orphan:
    ok = False
    print('FAIL: %s 里有行对不上 PkRect.h 的任何声明（成员删了却留着行）：' % map_path,
          file=sys.stderr)
    for k in orphan:
        print('  ' + k, file=sys.stderr)

for d in decls:
    for lab in mapping.get(d, []):
        if lab not in seen:
            ok = False
            print('FAIL: %s 映射到标签 %s，但对拍里根本没有这条 rec()（闸门 ③）'
                  % (d, lab), file=sys.stderr)

print('\n规则三机器对账：PkRect.h 声明 %d 条，rect_api.map %d 行，'
      'APISEEN %d 个（期望 %d）' % (len(decls), len(mapping), len(seen), len(expected)))
sys.exit(0 if ok else 1)
PY

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
