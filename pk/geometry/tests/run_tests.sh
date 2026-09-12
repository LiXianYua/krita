#!/usr/bin/env bash
# 建 pk/geometry 独立工程、跑单测，再跑两条自证：
#   判据③ —— libpkgeometry.a 里不得有 Qt 未定义符号；
#   locks  —— 工作树的改动必须全部落在**本任务的** locks 内（从 .exec/tasks.yaml 读；解析不出时降级 WARN）。
set -eu
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

BUILD=pk/geometry/build
cmake -S pk/geometry -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_COMPILER_LAUNCHER=ccache \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache >/dev/null
cmake --build "$BUILD" -j"$(nproc)" >/dev/null

"./$BUILD/test_pkgeometry"

"$BUILD/test_pkgeometry_debugstream"

# ── R-58：arc 大角度档的 UBSan 闸门 ───────────────────────────────────────
# 判据是**退出码**：目标用 -fno-sanitize-recover=all 编，第一条 runtime error 就 abort。
# 为什么必须单独一个 target、不能复用 pkgeometry：`-fwrapv` 会让 UBSan 的
# signed-integer-overflow 检查静默失效（实测），理由全文在 CMakeLists.txt 里。
if ! "$BUILD/pk_arc_band_ubsan"; then
    printf 'run_tests.sh: arc 大角度档的 UBSan 闸门红了 —— 上面是 sanitizer 的原始报告\n' >&2
    exit 1
fi

# 判据③：替代品本体不得有 Qt 未定义符号。查的是 pk/geometry 编出来的静态库。
# 与 pk/test 那条同义：静态库允许留未定义符号，真混进 Qt 依赖就会在这里现形
#（可执行文件那种查法是恒真的，链接行里根本没 Qt 库，见 pk/test/README.md §5）。
undef=$(nm -u "$BUILD/libpkgeometry.a" 2>/dev/null | grep -i qt || true)
if [ -n "$undef" ]; then
    printf 'nm -u libpkgeometry.a 里有 Qt 符号：\n%s\n' "$undef" >&2
    exit 1
fi
printf 'nm -u %s/libpkgeometry.a | grep -i qt: 无输出\n' "$BUILD"

# ---------------------------------------------------------------------------
# compat 垫片的「先包 QtGlobal」纪律 —— **逐个垫片的回归守卫**
#
# 纪律（compat/QtGlobal 顶部、README 都写着）：每个类型垫片都必须在包各自的
# Pk 头之前先包 compat/QtGlobal。漏了那行，先 include 该垫片、再 include
# pk/test 那份 <QtGlobal> 的 TU 里，PkGlobal.h 会先定义 qAbs、pk/test 那份随后
# 再定义一次 —— "redefinition of 'template<class T> constexpr T qAbs'" 硬错。
# 这正是 Task 78 拿真实调用点压出来的那条 Critical。
#
# 为什么必须**一个垫片一个 TU**：include guard 只认第一次。一旦任何一个垫片把
# compat/QtGlobal 带进了 TU，后面再包的垫片漏没漏那行都看不出来 —— 所以不能
# 在一个 TU 里把七个垫片一起包了了事。
#
# **垫片清单是 glob 出来的，新增 compat 垫片自动纳入**，不需要有人记得来加一行。
GUARDDIR="$BUILD/compat_include_guard"
mkdir -p "$GUARDDIR"
guard_fail=0
for shim in pk/geometry/compat/*; do
    base=$(basename "$shim")
    [ "$base" = "QtGlobal" ] && continue   # 它自己就是被"先包"的那一个，没有上游
    printf '#include "%s/%s"\n#include "%s/pk/test/compat/QtGlobal"\nint main() { return 0; }\n' \
           "$PWD" "$shim" "$PWD" > "$GUARDDIR/guard_$base.cpp"
    if ! g++ -std=c++17 -fsyntax-only "$GUARDDIR/guard_$base.cpp" \
             2>"$GUARDDIR/guard_$base.err"; then
        printf 'compat/%s 没有先包 compat/QtGlobal（或与 pk/test 那份撞了）：\n' "$base" >&2
        head -3 "$GUARDDIR/guard_$base.err" >&2
        guard_fail=1
    fi
done
if [ "$guard_fail" -ne 0 ]; then
    printf 'run_tests.sh: compat 垫片的「先包 QtGlobal」纪律被破坏\n' >&2
    exit 1
fi
printf 'compat 垫片「先包 QtGlobal」逐个守卫（%s 个）: 全部通过\n' \
       "$(ls pk/geometry/compat | grep -cv '^QtGlobal$')"

# locks 自证：改动必须落在**本任务的** locks 内（R-56 改）。
#
# 这条自证原先写死「只许动 pk/geometry/」（R-03 的锁假设）。fork 是**各任务共享**
# 的仓库，别的任务（R-53 锁含 pk/variant、pk/time、pk/container）跑同一个脚本时
# 最后一步必红 —— 而**真越界与预期越界在输出上不可区分**，于是它退化成噪音，
# 还卡在跑测试路径的最后一步。判据的前提不成立时它不该断言。
#
# locks 从 .exec/tasks.yaml 读，**解析器用一份、不写第二份**（.exec/lib_tasks.py）。
# 任务 ID：worktree 目录名（<工作空间>/krita-worktrees/<ID>）→ 退而取分支名 `x/<ID>`。
# 工作空间根：本仓库根的上一级与上两级各试一次（主 checkout 与 worktree 两种层级）。
# ⚠ **source 过 krita-ci-env/env 后，PATH 上的 python3 是 CI 前缀那份、没装 pyyaml**
#（R-56 修复轮实测：Python 3.13.5，`import yaml` 直接 ModuleNotFoundError）。lib_tasks
# .load_yaml() 靠 pyyaml，缺了它只会返回 {}、判据恒 WARN。所以下面先探一个能
# `import yaml` 的解释器再调 load_yaml()：**换的只是解释器，解析器仍是 lib_tasks 那一份**。
# 解析不出（不在工作空间里 / 真没有 pyyaml / 任务 ID 认不出）→ **WARN**，不 FAIL。
#
# 判定仍交给 git 自己的 pathspec，**不解析 porcelain 的输出文本**。理由：
#   · 改名行的形状是 `R  old -> new`，按列切出来是 "old -> new" 这一整串。
#     `pk/geometry/x -> pk/other/x`（真·越界改名）以 pk/geometry/ 开头，
#     前缀过滤会把它当成合规改动放过去 —— 实测复现过。
#   · 含空格/非 ASCII 的路径 git 默认加引号并转义（core.quotePath），切出来是
#     `"pk/geometry/\344\270\255..."`，前缀过滤反过来误判成越界 —— 也实测复现过。
printf '\ngit status --porcelain:\n'
git status --porcelain

# lib_tasks.load_yaml() 需要 pyyaml；source 过 env 后 PATH 上的 python3（CI 前缀那份）没有它。
# 探一个能 import yaml 的解释器 —— 都用 lib_tasks，**不写第二份 YAML 解析器**。
PYBIN=python3
if ! python3 -c 'import yaml' >/dev/null 2>&1; then
    for _c in /usr/bin/python3 /opt/homebrew/bin/python3 /usr/local/bin/python3; do
        if [ -x "$_c" ] && "$_c" -c 'import yaml' >/dev/null 2>&1; then PYBIN=$_c; break; fi
    done
fi

LOCKS_JSON=$("$PYBIN" - "$PWD" <<'PY'
import json, os, re, subprocess, sys
root = sys.argv[1]

m = re.search(r'/krita-worktrees/([^/]+)/?$', root)
tid = m.group(1) if m else None
if tid is None:
    try:
        br = subprocess.check_output(['git', 'rev-parse', '--abbrev-ref', 'HEAD'],
                                     cwd=root, text=True).strip()
    except Exception:
        br = ''
    m = re.fullmatch(r'[^/]+/(.+)', br)      # x/R-56 → R-56
    tid = m.group(1) if m else ''

for up in ('..', '../..'):
    base = os.path.normpath(os.path.join(root, up, '.exec'))
    if not os.path.isfile(os.path.join(base, 'lib_tasks.py')):
        continue
    sys.path.insert(0, base)
    try:
        import lib_tasks
        y = lib_tasks.load_yaml()
    except Exception:
        break
    locks = (y.get(tid) or {}).get('locks')
    if locks is None:
        break
    print(json.dumps({'tid': tid, 'locks': locks, 'src': base}))
    sys.exit(0)
print(json.dumps({'tid': tid, 'locks': None, 'src': None}))
PY
)
LOCKS=$(printf '%s' "$LOCKS_JSON" | "$PYBIN" -c 'import json,sys; v=json.load(sys.stdin)["locks"]; print(" ".join(v) if v else "")')
TID=$(printf '%s' "$LOCKS_JSON" | "$PYBIN" -c 'import json,sys; print(json.load(sys.stdin)["tid"])')

if [ -z "$LOCKS" ]; then
    printf 'locks 自证：认不出任务「%s」的 locks（不在工作空间的 worktree 里 / 没装 pyyaml）\n' \
           "$TID" >&2
    printf '  —— 判据的前提不成立，这里只提示、不断言（R-56 起如此，理由见上）。\n' >&2
    git status --porcelain | sed 's/^/    /' >&2
else
    excludes=()
    for l in $LOCKS; do excludes+=(":(exclude)$l"); done
    stray=$(git status --porcelain -- . "${excludes[@]}")
    if [ -n "$stray" ]; then
        printf 'run_tests.sh: 有改动落在本任务 %s 的 locks（%s）之外：\n%s\n' \
               "$TID" "$LOCKS" "$stray" >&2
        exit 1
    fi
    printf 'git status --porcelain: 改动全部落在本任务 %s 的 locks 内（%s）\n' "$TID" "$LOCKS"
fi

# R-39 path boolean operations: distinct real-Qt/Pk differential oracle.
pk/geometry/oracle/run_pathops_oracle.sh
