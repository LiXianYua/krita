#!/usr/bin/env bash
# pk/image 与真 Qt5 的逐输入对拍：定位真 Qt → 编译 → ldd 确认真链上了 Qt →
# 跑 → 把 DIFFTAG 与 image.deviation 双向核对 → 打结论。
#
# 骨架照抄 pk/geometry/oracle/run_oracle.sh（R-03 已 VERIFIED 交付），只把路径
# 换成 pk/image、链接库换成 Qt5Core+Qt5Gui（QImage 住在 libQt5Gui.so）。
# **本文件省掉了 geometry 那份 Task 4 修复轮加的头文件解析机器闸门
# （§APISEEN 的 ②：把 PkRect.h 类体声明与 *_api.map 逐行核对）**——image 的
# API 面（~35 个）比 Rect/Transform 族小得多，image_difftest.cpp 顶部已经有
# 一张人工维护的「API 对拍点对照表」（规则三自审），这是本 Task 明确记录的
# 规模裁剪，见 image.deviation 底部与 task-4-report.md。
#
# 退出码：0 = 全部差异都已声明且 canary 齐全；非 0 = FAIL（原因打在 stderr）。
set -euo pipefail
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

QT=${PK_QT_PREFIX:-/mnt/ssd-disk/liyang/projects/krita-ci-env/_install}
SRC=pk/image/oracle/image_difftest.cpp
DEV=pk/image/oracle/image.deviation
OUT=pk/image/build/image_difftest
LOG=pk/image/build/image_difftest.out

[ -f "$QT/include/QtGui/qimage.h" ] || { echo "找不到真 Qt5 的头：$QT/include/QtGui/qimage.h" >&2; exit 1; }
[ -f "$QT/lib/libQt5Gui.so" ]       || { echo "找不到真 Qt5 的库：$QT/lib/libQt5Gui.so" >&2; exit 1; }

# QImage 住在 libQt5Gui 里（QColor/QTransform 同理），QSize/QRect 住在
# libQt5Core 里——两个都链、两个都查（同 geometry 先例查 Core+Gui 的理由）。
QT_LIBS="-lQt5Core -lQt5Gui"
LDD_REQUIRE="libQt5Core libQt5Gui"

# ⚠ **-I 里绝不能出现 compat**：垫片一旦被拉进来，<QImage> 会解析到
# compat/QImage（`#define QImage PkImage`），两侧变成同一个类型，跑出来必然
# 零差异且看不出破绽。
INCS=("$QT/include" "$QT/include/QtCore" "$QT/include/QtGui" "pk/image" "pk/geometry")
for i in "${INCS[@]}"; do
    case "$i" in
        *compat*) echo "run_oracle.sh: -I 里出现了 compat 垫片目录：$i" >&2; exit 1;;
    esac
done
INCFLAGS=()
for i in "${INCS[@]}"; do INCFLAGS+=("-I$i"); done

# 旗标口径同 geometry 先例（Task 4 修复轮裁决）：
#   · 不带 -fwrapv——PkTransform.cpp 里 mapRect 用到的判据要与 Qt 那侧同一套
#     有符号溢出语义，带了会凭空多出差异（geometry.deviation 顶部有完整
#     背景，PkImage::transformed() 复用 PkTransform::mapRect，同一个道理）。
#   · -DQT_NO_DEBUG——避免 qsize.h/qimage.h 头文件内联代码里的 Q_ASSERT
#     展开成 abort()（本文件没有故意喂出会触发 Q_ASSERT 的输入，但保持与
#     geometry 先例一致的旗标口径，且对齐 Krita 发布构建旗标）。
CXXFLAGS_ORACLE=(-std=c++17 -O2 -fPIC -DQT_NO_DEBUG)

mkdir -p pk/image/build
printf '编译：g++ %s %s %s\n' "${CXXFLAGS_ORACLE[*]}" "${INCFLAGS[*]}" "$SRC"
g++ "${CXXFLAGS_ORACLE[@]}" "${INCFLAGS[@]}" -o "$OUT" "$SRC" \
    -L"$QT/lib" -Wl,-rpath-link,"$QT/lib" -Wl,-rpath,"$QT/lib" $QT_LIBS

# 判据：真的链上了 Qt。链不上说明两侧都编到了替代品，零差异是假的。
printf '\nldd %s | grep -i qt:\n' "$OUT"
LD_LIBRARY_PATH="$QT/lib" ldd "$OUT" | grep -i qt || true
for lib in $LDD_REQUIRE; do
    if ! LD_LIBRARY_PATH="$QT/lib" ldd "$OUT" | grep -q "$lib"; then
        echo "run_oracle.sh: ldd 里看不到 $lib —— 没有真的链上 Qt" >&2
        exit 1
    fi
done

printf '\n跑对拍：\n'
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

# ── DIFFTAG ↔ image.deviation 双向核对（与 geometry 先例同一套判据）─────
python3 - "$LOG" "$DEV" <<'PY'
import sys

log, dev = sys.argv[1], sys.argv[2]

seen, den, diff_lines = {}, {}, []
for line in open(log, encoding='utf-8', errors='replace'):
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
if total < 10000:
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
    if len(reason) < 20:
        print(f'FAIL: {dev}:{n} 理由只有 {len(reason)} 个码点，门槛 20', file=sys.stderr); sys.exit(1)
    declared[(api, tag)] = (int(want), reason)

undeclared = sorted(k for k in seen if k not in declared)
drift = sorted((k, declared[k][0], seen.get(k, 0)) for k in declared
               if seen.get(k, 0) != declared[k][0])
stale = sorted(k for k in declared if k not in seen and declared[k][0] == 0)

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
    print('FAIL: 出现未在 image.deviation 里声明的差异（= 没人判断过它可不可接受）：',
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
    print('WARN: 额度写着 0 又确实没观察到 —— 这行对闸门不起作用，建议删掉：'
          + ', '.join(f'{a} {t}' for a, t in stale), file=sys.stderr)

sys.exit(0 if ok else 1)
PY

printf '\nrun_oracle.sh: 通过 —— 全部差异都已声明，canary 齐全\n'
