#!/usr/bin/env bash
# pk/render 的**收尾入口**（R-76 重写）：一次跑完本目录的全部判据。
#
#   1) 建薄壳工程（libpkrender.a / test_* / 三个 Qt-free 的 oracle 输出体）；
#   2) ctest 全量 —— **Not Run 必须为 0**（两个 EXCLUDE_FROM_ALL 的 target 点名构建）；
#   3) test_pkrender —— 它不在 ctest 里（CMakeLists 只给了 add_executable），自己跑；
#   4) 三条 oracle 闸门（真 Qt 侧对拍）：
#        oracle/run_shape_primitive.sh · oracle/run_svg_primitive.sh ·
#        oracle/probes/run_probes.sh
#   5) 判据③：libpkrender.a 的 Qt 符号面 + **现场挑的判别力对照物**（分母与命中都打出来）。
#
# 为什么这三条 oracle 必须在这里：R线-spec〈判据必须在收尾路径上，否则它是装饰〉
# （规矩是 R-63 在 pk/geometry 撞出来的）。同一形态在 pk/render 又出现一次 —— 本脚本
# 当时**本机根本跑不起来**（第 4 行写死 Linux 路径 /mnt/ssd-disk/...），于是三条 oracle
# 一条都不在任何路径上。代价是每次收尾变慢（三条合计约 30 s）：那是判据的正常价格，
# 真嫌慢就把对拍做快，别把它从路径上摘掉。
set -eo pipefail

render_root=$(cd "$(dirname "$0")/.." && pwd)
repo_root=$(cd "$render_root/../.." && pwd)
build_dir="${PK_RENDER_BUILD_DIR:-$render_root/build}"

# ── 依赖前缀：向上找 krita-ci-env/env ────────────────────────────────────────
# 形态照抄 oracle/run_shape_primitive.sh 的 find_env_script()：krita-ci-env 是**工作空间
# 根的同级**，而本脚本既可能从主 checkout 跑、也可能从 <工作空间>/krita-worktrees/<ID>/
# 跑 —— 两者离工作空间根的层数不同，所以不数层数，直接向上找。
# （R-76 之前这里是 `source /mnt/ssd-disk/liyang/projects/krita-ci-env/env`：本机必然失败。）
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
set -u
if [[ -n "${KDECI_CC_CACHE:-}" ]]; then export CCACHE_DIR="$KDECI_CC_CACHE"; fi
# 三条 oracle 与本脚本**共用同一个 build 目录**（run_probes.sh 不 configure，只认已有的）。
export PK_RENDER_BUILD_DIR="$build_dir"

# ── 配置 + 构建 ──────────────────────────────────────────────────────────────
# macOS 上必须显式给部署目标 13.3：pk/ 层用到 std::to_chars 的浮点重载（macOS ≥ 13.3），
# 而 CI env 把 MACOSX_DEPLOYMENT_TARGET 设成 10.15。**必须与三条 oracle 用同一个值** ——
# 不一致会让同一个 build 目录被两种编译行反复重建（跑一遍收尾 = 一次全量重编）。
extra_cmake_args=()
if [[ "$(uname -s)" == "Darwin" ]]; then
    extra_cmake_args+=(-DCMAKE_OSX_DEPLOYMENT_TARGET=13.3)
fi

cmake -S "$render_root" -B "$build_dir" -G Ninja \
    "${extra_cmake_args[@]}" \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
ninja -C "$build_dir"

# pk/render/CMakeLists.txt:10-11 用 EXCLUDE_FROM_ALL 把 pk/font、pk/xml 加进来 ⇒ 默认
# `all` 不含它们的测试可执行文件 ⇒ ctest 报 2 条 Not Run（**未跑到 ≠ 跑绿了**）。
# 点名构建这两个 target —— 本脚本就是「把闸门串进收尾路径」的那个地方。
ninja -C "$build_dir" test_pkfont test_pkxml

# test_pkrender 不在 ctest 清单里（CMakeLists 只给了 add_executable，没有 add_test）。
"$build_dir/test_pkrender"

# ── ctest：**Not Run 必须是 0** ──────────────────────────────────────────────
# 「未跑到」不是「跑绿了」（R线-spec，R-72 实测）。判据要**结构地**钉住：把 ctest 实际
# 跑到的条数与 `ctest -N` 的总数比，不相等就红 —— 将来再加一个 EXCLUDE_FROM_ALL 的
# target、或新增测试忘了构建，这里**当场**变红，而不是静默少跑一条。
ctest_log="$build_dir/ctest.log"
set +e
ctest --test-dir "$build_dir" --output-on-failure 2>&1 | tee "$ctest_log"
ctest_rc=${PIPESTATUS[0]}
set -e

total=$(ctest --test-dir "$build_dir" -N | grep -cE '^ *Test +#[0-9]+:' || true)
ran=$(grep -cE '^ *[0-9]+/[0-9]+ Test +#[0-9]+:' "$ctest_log" || true)
not_run=$(grep -cE '\*\*\*(Not Run|Disabled)' "$ctest_log" || true)
printf 'ctest: total=%s ran=%s not_run=%s exit=%s\n' "$total" "$ran" "$not_run" "$ctest_rc"
if [ "$not_run" -ne 0 ] || [ "$ran" -ne "$total" ]; then
    sed -n '/The following tests did not run:/,$p' "$ctest_log" >&2
    printf 'run_tests.sh: ctest 有未跑到的用例（total=%s ran=%s not_run=%s）' \
           "$total" "$ran" "$not_run" >&2
    printf ' —— 未跑到不等于跑绿了\n' >&2
    exit 1
fi
if [ "$ctest_rc" -ne 0 ]; then
    printf 'run_tests.sh: ctest 退出码 %s（失败用例的输出在上面的日志里）\n' "$ctest_rc" >&2
    exit 1
fi

# ── 判据④：三条 oracle 闸门（真 Qt 侧对拍）───────────────────────────────────
bash "$render_root/oracle/run_shape_primitive.sh"
bash "$render_root/oracle/run_svg_primitive.sh"
bash "$render_root/oracle/probes/run_probes.sh"

# 前两条 oracle 会在源码树里重写 golden —— 幂等：两侧一致时逐字节相同、`git status` 干净。
# 这里**只报不断言**（任务合法地更新 golden 时它本来就该非空）。
printf '\ngit status --porcelain -- pk/render/oracle:\n'
git -C "$repo_root" status --porcelain -- pk/render/oracle

# ── 判据③：libpkrender.a 的 Qt 符号面 ────────────────────────────────────────
archive="$build_dir/libpkrender.a"
test_binary="$build_dir/test_pkrender"

# 简报里那条笼统的 name scan 照旧跑、照旧原样报出来：它匹配的是被搬过来的兼容命名与
# Pk 辅助名，所以「grep 退出码为 0」不是 Qt 链接的证据，也不当成 pass 报。
set +e
nm -u -C "$archive" | grep -i qt
strict_qt_name_grep_exit=${PIPESTATUS[1]}
set -e
printf 'strict_qt_name_grep_exit=%s (compatibility names are expected)\n' \
    "$strict_qt_name_grep_exit"

# 去掉本独立闭包内三处经评审的兼容专用符号后，强制真 Qt 类/C ABI 符号为 0。
filter_real_qt_symbols()
{
    sed -E \
        -e '/PkColor::PkColor\(Qt::GlobalColor\)/d' \
        -e '/PkSize::scaled\(PkSize const&, Qt::AspectRatioMode\) const/d' \
        -e '/pk_qt_assert\(char const\*, char const\*, int\)/d' \
        | grep -E '(^|[^[:alnum:]_])Q[A-Z][[:alnum:]_]*|(^|[[:space:]])(q[A-Z][[:alnum:]_]*\(|q(rand|srand|strcmp|stricmp|strnicmp|strncpy)\(|_?qt_[[:alnum:]_]*|qt[A-Z][[:alnum:]_]*)'
}

filter_qt_command_tokens()
{
    grep -Ei 'Qt5|Qt6|libQt|Qt::[A-Za-z_]|(^|[[:space:]])(-I|-isystem|-iquote|-F)[^[:space:]]*[/\\](qt|Qt)([/\\]|$)|(^|[[:space:]])(-I|-isystem|-iquote)[[:space:]]+[^[:space:]]*[/\\](qt|Qt)([/\\]|$)|(^|[[:space:]])-DQT_[A-Za-z0-9_]*(=|[[:space:]]|$)'
}

# 两侧都压测一遍匹配器：搬过来的兼容拼写要放行，代表性 Qt 类与 C ABI 符号必须留住。
matcher_probe=$(printf '%s\n' \
    '                 U PkColor::PkColor(Qt::GlobalColor)' \
    '                 U PkSize::scaled(PkSize const&, Qt::AspectRatioMode) const' \
    '                 U pk_qt_assert(char const*, char const*, int)' \
    '                 U QPainter::drawImage(QRectF const&, QImage const&)' \
    '                 U operator<<(QDebug&, PkThing const&)' \
    '                 U vtable for QImage' \
    '                 U qt_version_tag' \
    '                 U qFatal(char const*, ...)' \
    '                 U qrand()' \
    '                 U qsrand(unsigned int)' \
    '                 U qstrcmp(char const*, char const*)' \
    '                 U qtHookData' \
    | filter_real_qt_symbols)
expected_matcher_probe=$(printf '%s\n' \
    '                 U QPainter::drawImage(QRectF const&, QImage const&)' \
    '                 U operator<<(QDebug&, PkThing const&)' \
    '                 U vtable for QImage' \
    '                 U qt_version_tag' \
    '                 U qFatal(char const*, ...)' \
    '                 U qrand()' \
    '                 U qsrand(unsigned int)' \
    '                 U qstrcmp(char const*, char const*)' \
    '                 U qtHookData')
if [[ "$matcher_probe" != "$expected_matcher_probe" ]]; then
  printf 'real Qt symbol matcher self-check failed\n' >&2
  printf '%s\n' "$matcher_probe" >&2
  exit 1
fi

# 命令闭包判据同样自测：Qt 的 include 路径与 QT_* 定义即使不出现 Qt 库名也是依赖。
command_matcher_probe=$(printf '%s\n' \
    'c++ -I /opt/qt/include -c accidental-separated-I.cpp' \
    'c++ -I/opt/qt/include/QtCore -DQT_CORE_LIB -c accidental.cpp' \
    'c++ -isystem /opt/Qt6/include -DQT_NO_KEYWORDS -c accidental2.cpp' \
    'c++ -I/opt/quiet/include -DNOT_QT_DEFINE -c harmless.cpp' \
    | filter_qt_command_tokens || true)
expected_command_matcher_probe=$(printf '%s\n' \
    'c++ -I /opt/qt/include -c accidental-separated-I.cpp' \
    'c++ -I/opt/qt/include/QtCore -DQT_CORE_LIB -c accidental.cpp' \
    'c++ -isystem /opt/Qt6/include -DQT_NO_KEYWORDS -c accidental2.cpp')
if [[ "$command_matcher_probe" != "$expected_command_matcher_probe" ]]; then
  printf 'Qt command-closure matcher self-check failed\n' >&2
  printf '%s\n' "$command_matcher_probe" >&2
  exit 1
fi

archive_raw=$(nm -u -C "$archive" | wc -l | tr -d ' ')
real_qt_symbols=$(
    nm -u -C "$archive" | filter_real_qt_symbols || true
)
if [[ -n "$real_qt_symbols" ]]; then
  printf '%s\n' "$real_qt_symbols"
  printf 'real Qt undefined symbols found in %s\n' "$archive" >&2
  exit 1
fi
printf 'criterion3 numerator: %s — denominator=%s undefined-symbol lines, hits=0\n' \
    "$archive" "$archive_raw"

# 动态依赖闭包：Linux 用 readelf/ldd，macOS 用 otool -L —— 同一件事的两种工具。
# （R-76 之前只有 Linux 那半：本机要么 command not found、要么被 set -e 直接带红。）
dynamic_dependencies()
{
    if [[ "$(uname -s)" == "Darwin" ]]; then
        otool -L "$1"
    else
        readelf -d "$1"
        ldd "$1"
    fi
}
if dynamic_dependencies "$test_binary" | grep -Ei 'Qt|not found'; then
  printf 'Qt or unresolved dynamic dependency found in %s\n' "$test_binary" >&2
  exit 1
fi
if ninja -C "$build_dir" -t commands pkrender | filter_qt_command_tokens; then
  printf 'Qt token found in pkrender command closure\n' >&2
  exit 1
fi

# ── 判别力对照物：**每次都现场挑**（R线-spec〈判别力对照物〉）─────────────────
# 「0 命中」只有在对照物本身非空时才构成证据；对照物会**过期** —— README 里那条
# `libkritaflake.dylib … 39 hits` 现在是 **0/663**（那棵树被 R-57 清干净了），拿它当
# 对照就是拿一个恒空的东西自证。本脚本的对照物 = 上面第三条 oracle 刚编出来的 Qt 侧
# 探针（同一棵树、同一次运行里产出，`otool -L` 可验它真链 Qt）。分母与命中都打出来。
control="$build_dir/oracle-probes/probe"
if [[ ! -x "$control" ]]; then
    printf 'run_tests.sh: 判别力对照物 %s 不存在（run_probes.sh 没产出）\n' "$control" >&2
    exit 1
fi
control_raw=$(nm -u -C "$control" | wc -l | tr -d ' ')
control_hits=$(nm -u -C "$control" | grep -Ec '\bQ[A-Z][A-Za-z0-9_]*\b' || true)
printf 'discriminating control: %s — denominator=%s undefined-symbol lines, hits=%s\n' \
    "$control" "$control_raw" "$control_hits"
if [[ "$control_raw" -eq 0 || "$control_hits" -eq 0 ]]; then
    printf 'run_tests.sh: 判别力对照物为空（分母 %s / 命中 %s）——「0 命中」不构成证据\n' \
           "$control_raw" "$control_hits" >&2
    exit 1
fi

printf 'real_qt_symbol_scan=clean\n'
printf 'real_qt_symbol_matcher_self_check=clean\n'
printf 'dynamic_qt_needed_scan=clean\n'
printf 'pkrender_command_closure_qt_scan=clean\n'
