#!/usr/bin/env bash
# R-64 可复现性闸门：一条命令重跑本任务用过的六支探针（Pk 侧 1 支 + Qt 侧 5 支），
# 并打印五条关键读数。任何一步失败 → 非 0 退出。
#
# 每支探针回答什么问题、读数怎么读：见同目录 README.md。
# 五条读数（与 docs/superpowers/plans/R-64.md §1.2/§1.3 逐字对应）：
#   ① probe   (SUMMARY)  mism[bounds-vs-qtVB]=0  mism[default-vs-qtNoVB]=2  mism[default-vs-qtVB]=161
#   ② isolate            ellipse-vs-path6=4  ellipse-vs-path17=0  path6-vs-path17=4
#   ③ control            diff Qt(drawEllipse) vs Qt(drawPath) under implicit stretch = 4
#   ④ mirror             MIRROR total=288 mismatch=0
#   ⑤ focus              qt/pk viewBox 逐位相同，px dA=-2 ×4，diff pixels = 4
#
# 形态照抄 pk/render/oracle/run_svg_primitive.sh：find_env_script() 向上找 krita-ci-env/env、
# macOS 用 otool -L / ccache / pkg-config 取 Qt5Svg。Pk 侧编译与链接 flags **现场**从已经在的
# svg_primitive_oracle_pk 目标取（ninja -t commands），本脚本不写死任何平台/构建路径。
set -eo pipefail

render_root=$(cd "$(dirname "$0")/../.." && pwd)
probes_dir="$render_root/oracle/probes"

# krita-ci-env/env 与工作空间根同级；脚本可能从主 checkout 或 worktree 跑 → 向上找，不数层数。
find_env_script()
{
    local dir="$render_root"
    while [[ "$dir" != "/" ]]; do
        if [[ -f "$dir/krita-ci-env/env" ]]; then printf '%s\n' "$dir/krita-ci-env/env"; return 0; fi
        dir=$(dirname "$dir")
    done
    return 1
}
env_script="${KRITA_CI_ENV:-}"
if [[ -z "$env_script" ]]; then
    env_script=$(find_env_script) || {
        printf 'cannot locate krita-ci-env/env above %s\n' "$render_root" >&2
        printf 'set KRITA_CI_ENV=<path to krita-ci-env/env> and retry\n' >&2
        exit 2
    }
fi
if [[ ! -f "$env_script" ]]; then
    printf 'dependency env script not found: %s\n' "$env_script" >&2
    exit 2
fi
# shellcheck disable=SC1090
source "$env_script"
if [[ -n "${KDECI_CC_CACHE:-}" ]]; then export CCACHE_DIR="$KDECI_CC_CACHE"; fi

# 只依赖**已经存在**的构建目录：默认值与 run_svg_primitive.sh 相同（$render_root/build），
# 但本脚本不 configure 它——没配过就报错让调用者先跑 run_svg_primitive.sh。
build_dir="${PK_RENDER_BUILD_DIR:-$render_root/build}"
if [[ ! -f "$build_dir/build.ninja" ]]; then
    printf 'no configured build dir at %s\n' "$build_dir" >&2
    printf 'run pk/render/oracle/run_svg_primitive.sh first, or set PK_RENDER_BUILD_DIR=<configured dir>\n' >&2
    exit 2
fi

# 确保 Pk 侧目标已构建（已最新时是 no-op）。这一步失败即非 0。
ninja -C "$build_dir" svg_primitive_oracle_pk

# ── Pk 侧编译/链接 flags：现场从 svg_primitive_oracle_pk 目标取，不写死路径 ──
all_cmds=$(ninja -C "$build_dir" -t commands svg_primitive_oracle_pk)
compile_cmd=$(printf '%s\n' "$all_cmds" | grep -E -- '-c .*svg_primitive_oracle\.cpp' | head -1 || true)
link_cmd=$(printf '%s\n' "$all_cmds" | grep -E -- ' -o svg_primitive_oracle_pk( |$)' | tail -1 || true)
png_src=$(printf '%s\n' "$all_cmds" | grep -oE -- '-c [^ ]*ImageShapePngData\.cpp' | head -1 | sed 's/^-c //' || true)
if [[ -z "$compile_cmd" || -z "$link_cmd" || -z "$png_src" ]]; then
    printf 'could not recover compile/link commands for svg_primitive_oracle_pk from ninja\n' >&2
    exit 2
fi
# 编译 flags = 命令去掉 ` -MD ...` 之后的那段（留下 ccache c++ -I... -isystem... -std... -fwrapv）
pk_compile_flags=${compile_cmd%% -MD *}
# 链接：前缀（编译器 + 链接选项，到第一个目标文件前）+ 后缀（-o <target> 之后的全部库）
# ninja 的链接行可能是 `<phony> && <compiler> ...`；`&&` 经变量展开会变成普通参数，
# 所以先去掉 `&&` 之前的那段，只留下真正的编译器调用。
pk_link_prefix=${link_cmd%% CMakeFiles/*}
pk_link_prefix=${pk_link_prefix##*&& }
pk_link_suffix=${link_cmd#*-o svg_primitive_oracle_pk }
pk_link_suffix=${pk_link_suffix% && :}

oracle_build="$build_dir/oracle-probes"
mkdir -p "$oracle_build"

# macOS 让 pkdocs 找得到 build 目录里的 dylib（rpath 一般已烘死，这里再兜一层；Linux 用 LD_）。
if [[ "$(uname -s)" == "Darwin" ]]; then
    export DYLD_LIBRARY_PATH="$build_dir/pkfont/pkimage:$build_dir${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
else
    export LD_LIBRARY_PATH="$build_dir/pkfont/pkimage:$build_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

echo "=== build: Pk-side document generator (pkdocs) ==="
# shellcheck disable=SC2086
$pk_compile_flags -c "$probes_dir/pkdocs.cpp" -o "$oracle_build/pkdocs.o"
# shellcheck disable=SC2086
$pk_compile_flags -c "$png_src" -o "$oracle_build/pngdata.o"
# ninja 的链接行里库是相对 **build 目录** 的（libpkrender.a、pkfont/pkimage/libpkimageio.dylib…），
# 而 ninja 自己就在该目录里跑——所以这里也 cd 进去链接。
( cd "$build_dir" && $pk_link_prefix "$oracle_build/pkdocs.o" "$oracle_build/pngdata.o" \
    -o "$oracle_build/pkdocs" $pk_link_suffix )

# ── Qt 侧比较器：唯一链真 Qt 的一步（不进任何产物）────────────────────────────
qt_flags=$(pkg-config --cflags --libs Qt5Gui Qt5Svg)
qt_rpath=()
if [[ "$(uname -s)" == "Darwin" ]]; then
    qt_rpath=(-Wl,-rpath,"$(pkg-config --variable=libdir Qt5Svg)")
fi

echo "=== build: Qt-side probes ==="
for probe in probe isolate control mirror focus; do
    ccache c++ -std=c++17 -fPIC -DPK_SVG_QT_ORACLE \
        -I"$render_root/oracle" \
        "$probes_dir/$probe.cpp" \
        $qt_flags "${qt_rpath[@]}" -o "$oracle_build/$probe"
done

# 形态契约：Qt 侧比较器必须真的链上 Qt（macOS 用 otool -L，Linux 用 ldd）。
if [[ "$(uname -s)" == "Darwin" ]]; then
    otool -L "$oracle_build/probe" | grep -qi 'Qt' || {
        printf 'Qt-side probe does not link Qt\n' >&2; exit 2; }
else
    ldd "$oracle_build/probe" | grep -qi 'qt' || {
        printf 'Qt-side probe does not link Qt\n' >&2; exit 2; }
fi

# ── 运行：Pk 侧产出文档，Qt 侧读它 ────────────────────────────────────────────
# headless 下 QtTest/QSvgRenderer 无 X11；QSvgGenerator 又对每个文档发一条
# QFont::setPointSizeF 警告（文档里 font-size="-1"，无碍）——关掉 Qt 自己的 warning 级日志，
# 读数仍在 stdout。这两样都只影响日志，不影响任何读数。
export QT_QPA_PLATFORM=offscreen
export QT_LOGGING_RULES="${QT_LOGGING_RULES:-*.warning=false}"

pkdocs_txt="$oracle_build/pkdocs.txt"
"$oracle_build/pkdocs" > "$pkdocs_txt"

echo "=== ① probe (three comparison faces; SUMMARY) ==="
"$oracle_build/probe" "$pkdocs_txt" | tail -1

echo "=== ② isolate (precision isolation) ==="
"$oracle_build/isolate" | grep -E 'diff '

echo "=== ③ control (Qt vs Qt) ==="
"$oracle_build/control" | tail -1

echo "=== ④ mirror (full-table mirror) ==="
"$oracle_build/mirror" "$pkdocs_txt" | tail -1

echo "=== ⑤ focus (svg-ellipse#19:pen2.5:nofill) ==="
"$oracle_build/focus" "$pkdocs_txt" "svg-ellipse#19:pen2.5:nofill" \
    | grep -E 'viewBox:|scale:|px\(|diff pixels'

printf 'probes: all five readings printed above (exit 0)\n'
