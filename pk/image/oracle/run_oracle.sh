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
# ── R-75：macOS 支 ──────────────────────────────────────────────────────
# 本脚本原本是 Linux-only 的（`g++` + `-lQt5Core/-lQt5Gui` + `ldd` + `timeout`），
# **在 macOS 上编不过也跑不起来**。R-75 按 `pk/geometry/oracle/run_oracle.sh`
# （R-03 交付、S-18 修的 Darwin 支）的先例补上平台分叉，判据本身一字未动：
#   · 头文件：Linux `$QT/include/QtCore/qimage.h`；macOS 在 framework 里
#     `$QT/lib/QtCore.framework/Headers/qimage.h`。
#   · 链接：macOS `-F$QT/lib -framework QtCore -framework QtGui`。
#   · 运行时：macOS `otool -L` + `DYLD_FRAMEWORK_PATH`/`DYLD_LIBRARY_PATH`。
#   · `timeout`：macOS 没有 coreutils 那份，那边不加超时（纯函数式有限循环）。
#   · 依赖前缀解析与 geometry 那份逐字一致（见下）。
# 另：R-75 让本脚本多了两个编译单元——`pk_fileio_bridge.cpp`（`PkImage` 文件 I/O
# 的 `extern "C"` 桥，见该文件顶部）与它要链的 `pkimage`/`pkimageio` 真产物；
# 产物不在时脚本会先跑一次 `cmake` 把薄壳工程建起来。
#
# 退出码：0 = 全部差异都已声明且 canary 齐全；非 0 = FAIL（原因打在 stderr）。
set -euo pipefail
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

SRC=pk/image/oracle/image_difftest.cpp
# R-75：`PkImage` 的文件 I/O（ctor/load/save/invertPixels 的落地）不在
# `image_difftest.cpp` 能内联展开的那份 TU 里——它编进 `pkimageio`。桥见
# `pk/image/oracle/pk_fileio_bridge.cpp` 顶部注释。
BRIDGE=pk/image/oracle/pk_fileio_bridge.cpp
DEV=pk/image/oracle/image.deviation
OUT=pk/image/build/image_difftest
LOG=pk/image/build/image_difftest.out
PKBUILD=pk/image/build

UNAME_S=$(uname -s)

# 依赖前缀。优先 PK_QT_PREFIX（原本就有）→ Darwin 上再退到 CMAKE_PREFIX_PATH
# （`source <prefix>/env` 会设它）→ Darwin 上按约定默认找「工作空间上层」的
# krita-ci-env/_install → 最后是 Linux 的既有默认（**原值，一字未改**）。
# 与 pk/geometry/oracle/run_oracle.sh 的解析逻辑逐字一致，见那份文件 §1。
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
                      echo "  工作空间上层也没有 krita-ci-env/_install。" >&2
                      exit 1; }
else
    QT=/mnt/ssd-disk/liyang/projects/krita-ci-env/_install
fi

# ── 平台分叉（判据本身两边逐字相同，分叉只在「怎么把真 Qt 编进来、怎么确认
#    编进来了」）——三种差异：①头文件位置 ②链接形式 ③运行时依赖检查工具。
#    macOS 上 Qt 是 **framework**：头在 `$QT/lib/QtX.framework/Headers`、链接写
#    `-framework QtCore`、运行时靠 `otool -L` 与 `DYLD_*`。与 geometry 先例同。
if [ "$UNAME_S" = "Darwin" ]; then
    QT_HDRS=("$QT/lib/QtCore.framework/Headers" "$QT/lib/QtGui.framework/Headers")
    QT_ARTIFACTS=("$QT/lib/QtCore.framework/QtCore" "$QT/lib/QtGui.framework/QtGui")
    QT_LINK=(-F"$QT/lib" -Wl,-rpath,"$QT/lib" -framework QtCore -framework QtGui)
    QT_RUNLIBS=("DYLD_FRAMEWORK_PATH=$QT/lib" "DYLD_LIBRARY_PATH=$QT/lib")
    SHARED_DEP_TOOL="otool -L"
    LDD_REQUIRE="QtCore QtGui"
    CXX_BIN=clang++
else
    QT_HDRS=("$QT/include/QtCore" "$QT/include/QtGui")
    QT_ARTIFACTS=("$QT/include/QtGui/qimage.h" "$QT/lib/libQt5Gui.so")
    QT_LINK=(-L"$QT/lib" -Wl,-rpath-link,"$QT/lib" -Wl,-rpath,"$QT/lib" -lQt5Core -lQt5Gui)
    QT_RUNLIBS=("LD_LIBRARY_PATH=$QT/lib")
    SHARED_DEP_TOOL="ldd"
    LDD_REQUIRE="libQt5Core libQt5Gui"
    CXX_BIN=g++
fi

# QImage 住在 QtGui 里（QColor/QTransform 同理），QSize/QRect 住在 QtCore 里
# ——两个都链、两个都查。
[ -f "${QT_HDRS[0]}/qglobal.h" ] || { echo "找不到真 Qt5 的头：${QT_HDRS[0]}/qglobal.h" >&2; exit 1; }
[ -f "${QT_HDRS[1]}/qimage.h" ]  || { echo "找不到真 Qt5 的头：${QT_HDRS[1]}/qimage.h" >&2; exit 1; }
for a in "${QT_ARTIFACTS[@]}"; do
    [ -e "$a" ] || { echo "找不到真 Qt5 的库：$a" >&2; exit 1; }
done

# ⚠ **-I 里绝不能出现 compat**：垫片一旦被拉进来，<QImage> 会解析到
# compat/QImage（`#define QImage PkImage`），两侧变成同一个类型，跑出来必然
# 零差异且看不出破绽。
# ⚠ **模块根 `$QT/include` 只有 Linux 支需要，但绝不能少**：Qt 自己的头用**模块
# 前缀**互相引用（`qimage.h` 就是 `#include <QtCore/qsize.h>`），这个 include 靠
# `-I$QT/include` 解析（那里有 `QtCore/` 子目录）。macOS 靠 `-F` 把它们兜住，
# **而 Linux 没有 `-F`**。与 geometry 先例同（那边 S-18 踩过一次）。
# `pk/container` / `pk/string` / `pk/global` 是 R-75 新增：`PkImage.h` 起用
# `PkString`（`#include "../string/PkString.h"`），而桥 TU 要真编
# `PkImageFileIo.cpp` 那套头。相对 include 其实能解析，显式给上免得以后搬家就断。
if [ "$UNAME_S" = "Darwin" ]; then
    INCS=("${QT_HDRS[0]}" "${QT_HDRS[1]}" "pk/image" "pk/geometry"
          "pk/container" "pk/string" "pk/global")
else
    INCS=("$QT/include" "${QT_HDRS[0]}" "${QT_HDRS[1]}" "pk/image" "pk/geometry"
          "pk/container" "pk/string" "pk/global")
fi
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

mkdir -p "$PKBUILD"

# ── R-75：桥要链 `pkimage`/`pkimageio` 的真产物，所以先确保薄壳工程已建 ──
# 判据用「产物在不在」而不是「无条件重建」：重建一次 5 秒内，但会让本脚本在
# 没改动时白白跑一遍 cmake，且把构建噪音混进对拍日志。缺了才建；建不出来
# 直接 FAIL（对拍程序链不上就是链不上，不要静默降级）。
PKIO_SHARED="$PKBUILD/libpkimageio.dylib"
[ "$UNAME_S" = "Darwin" ] || PKIO_SHARED="$PKBUILD/libpkimageio.so"
if [ ! -f "$PKBUILD/libpkimage.a" ] || [ ! -f "$PKIO_SHARED" ] || [ ! -f "$PKBUILD/libpkstring.a" ]; then
    echo "先建 pk/image 薄壳工程（缺 libpkimage.a / libpkimageio / libpkstring.a）："
    cmake -S pk/image -B "$PKBUILD" -G Ninja -DCMAKE_OSX_DEPLOYMENT_TARGET=13.3
    cmake --build "$PKBUILD"
fi
for f in "$PKBUILD/libpkimage.a" "$PKIO_SHARED" "$PKBUILD/libpkstring.a"; do
    [ -f "$f" ] || { echo "run_oracle.sh: 薄壳产物缺失：$f" >&2; exit 1; }
done

PK_LINK=(-L"$PKBUILD" -Wl,-rpath,"$PKBUILD"
         -lpkimage -lpkgeometry -lpkstring -lpkimageio)

printf '编译：%s %s %s %s %s\n' "$CXX_BIN" "${CXXFLAGS_ORACLE[*]}" \
       "${INCFLAGS[*]}" "$SRC" "$BRIDGE"
"$CXX_BIN" "${CXXFLAGS_ORACLE[@]}" "${INCFLAGS[@]}" -o "$OUT" "$SRC" "$BRIDGE" \
    "${PK_LINK[@]}" "${QT_LINK[@]}"

# 判据：真的链上了 Qt。链不上说明两侧都编到了替代品，零差异是假的。
printf '\n%s %s | grep -i qt:\n' "$SHARED_DEP_TOOL" "$OUT"
env "${QT_RUNLIBS[@]}" $SHARED_DEP_TOOL "$OUT" | grep -i qt || true
for lib in $LDD_REQUIRE; do
    if ! env "${QT_RUNLIBS[@]}" $SHARED_DEP_TOOL "$OUT" | grep -q "$lib"; then
        echo "run_oracle.sh: $SHARED_DEP_TOOL 里看不到 $lib —— 没有真的链上 Qt" >&2
        exit 1
    fi
done

printf '\n跑对拍：\n'
rc=0
# macOS 没有 coreutils 的 `timeout`（`gtimeout` 也未必装），那边就不加超时——
# 对拍程序是纯函数式的有限循环，跑不完只会是死循环，靠 ctest/CI 的外层兜。
RUNENV=("${QT_RUNLIBS[@]}" "DYLD_LIBRARY_PATH=$PKBUILD:$QT/lib")
if [ "$UNAME_S" = "Darwin" ]; then
    env "${RUNENV[@]}" "$OUT" > "$LOG" 2>&1 || rc=$?
else
    env "${RUNENV[@]}" timeout 600 "$OUT" > "$LOG" 2>&1 || rc=$?
fi
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
