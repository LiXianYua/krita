#!/usr/bin/env bash
# pk/namespace 与真 Qt5 的枚举位值对拍：定位真 Qt → 编译 → ldd 确认真链上了 Qt →
# 跑 → 读 DIFF 行 → 打结论。
#
# 本 oracle 的判别力全部在**编译期**（每个枚举项一条 static_assert，见
# difftest_namespace.cpp 头注释）：编译过了 = 位值全对齐，编译失败 = 有位值不一致。
# 运行期只打一条 DIFF 契约行（mismatch 恒 0），让 run_oracle.sh 的解析与
# pk/global 的 run_oracle.sh 走同一套代码。
#
# 退出码：0 = 通过（编译过 + mismatch=0）；非 0 = FAIL（原因打在 stderr）。
set -euo pipefail
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

QT=${PK_QT_PREFIX:-/home/liyang/projects-ssd/krita-ci-env/_install}
SRC=pk/namespace/oracle/difftest_namespace.cpp
OUT=pk/namespace/build/namespace_difftest
LOG=pk/namespace/build/namespace_difftest.out

[ -f "$QT/include/QtCore/qnamespace.h" ] || { echo "找不到真 Qt5 的头：$QT/include/QtCore/qnamespace.h" >&2; exit 1; }
[ -f "$QT/lib/libQt5Core.so" ]          || { echo "找不到真 Qt5 的库：$QT/lib/libQt5Core.so" >&2; exit 1; }

QT_LIBS="-lQt5Core"
LDD_REQUIRE="libQt5Core"

# ⚠ **-I 里绝不能出现 compat**（理由同 global 的 run_oracle.sh）：垫片一旦被拉
# 进来，<QtGlobal> 会解析到 pk/test 的 compat/QtGlobal，P 侧 PkGlobal.h 的让位
# 机制被触发，oracle 就没在比较全量实现。
INCS=("$QT/include" "$QT/include/QtCore" "pk/namespace" "pk/global" "pk/flags")
for i in "${INCS[@]}"; do
    case "$i" in
        *compat*) echo "run_oracle.sh: -I 里出现了 compat 垫片目录：$i" >&2; exit 1;;
    esac
done
INCFLAGS=()
for i in "${INCS[@]}"; do INCFLAGS+=("-I$i"); done

# 枚举位值对拍不需要 -fwrapv（没有整数溢出运算参与）；-O0 即可，反正判别力在
# 编译期。源文件里对 PkGlobal.h 的标量引用（qAbs 等）不参与任何比较。
# -fPIC 是必需的：真 Qt 以 -reduce-relocations 构建，头文件会 #error 要求
# 位置无关代码（同 pk/global 的 run_oracle.sh）。
CXXFLAGS_ORACLE=(-std=c++17 -O0 -fPIC -DQT_NO_DEBUG)

mkdir -p pk/namespace/build
printf '编译：g++ %s %s %s\n' "${CXXFLAGS_ORACLE[*]}" "${INCFLAGS[*]}" "$SRC"
g++ "${CXXFLAGS_ORACLE[@]}" "${INCFLAGS[@]}" -o "$OUT" "$SRC" \
    -L"$QT/lib" -Wl,-rpath-link,"$QT/lib" -Wl,-rpath,"$QT/lib" $QT_LIBS

# 判据：**真的链上了 Qt**。枚举全编译期内联，链不上 libQt5Core 运行结果也一样，
# 所以必须靠 ldd 证明两侧真的各链各的。
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
    echo "run_oracle.sh: 对拍程序退出码 $rc（契约要求 0）" >&2
    if [ "$rc" -gt 128 ]; then
        echo "  （>128：被信号 $((rc - 128)) 杀掉；124 = timeout 超时）" >&2
    fi
    echo "  $LOG 末 20 行：" >&2
    tail -20 "$LOG" >&2
    exit 1
fi
grep -E '^DIFF ' "$LOG" || true

# ── DIFF 行核对：必须恰好一行、mismatch=0（static_assert 全过则恒 0）。───
python3 - "$LOG" <<'PY'
import sys

log = sys.argv[1]
diff = [l.rstrip() for l in open(log, encoding='utf-8', errors='replace')
        if l.startswith('DIFF ')]
if len(diff) != 1:
    print('FAIL: DIFF 行必须恰好一行，实得 %d 行' % len(diff), file=sys.stderr)
    sys.exit(1)
kv = dict(p.split('=', 1) for p in diff[0].split()[1:])
total, mismatch = int(kv['total']), int(kv['mismatch'])
# 期望值 = PkNamespace.h 实现的枚举项总数（含 PkGlobal.h 的 6 项）。
# 每多抄一个枚举项，下面这两个数就得跟着长——掉到 0 等于没对拍。
if total < 200:
    print('FAIL: total=%d 太小，static_assert 没几条等于没对拍' % total, file=sys.stderr)
    sys.exit(1)
if mismatch != 0:
    print('FAIL: mismatch=%d（static_assert 全过却分家，逻辑矛盾）' % mismatch, file=sys.stderr)
    sys.exit(1)
print('\n对拍结论：DIFF %s' % diff[0])
PY

printf '\nrun_oracle.sh: 通过 —— 全部枚举位值与真 Qt 5.15.7 一致\n'
