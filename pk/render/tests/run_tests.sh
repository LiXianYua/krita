#!/usr/bin/env bash
set -eo pipefail
render_root=$(cd "$(dirname "$0")/.." && pwd)
source /mnt/ssd-disk/liyang/projects/krita-ci-env/env
set -u
export CCACHE_DIR="${KDECI_CC_CACHE}"
build_dir="$render_root/build"
archive="$build_dir/libpkrender.a"
test_binary="$build_dir/test_pkrender"

cmake -S "$render_root" -B "$build_dir" -G Ninja \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
ninja -C "$build_dir"
"$test_binary"

# The brief's blanket name scan is intentionally run and reported verbatim. It
# matches the copied compatibility namespace and Pk helper names, so a zero grep
# exit is not evidence of Qt linkage and is not represented as a pass.
set +e
nm -u -C "$archive" | grep -i qt
strict_qt_name_grep_exit=${PIPESTATUS[1]}
set -e
printf 'strict_qt_name_grep_exit=%s (compatibility names are expected)\n' \
    "$strict_qt_name_grep_exit"

# Enforce undefined real-Qt class/C ABI symbols after removing the three
# reviewed compatibility-only symbols observed in this standalone closure.
filter_real_qt_symbols()
{
    sed -E \
        -e '/PkColor::PkColor\(Qt::GlobalColor\)/d' \
        -e '/PkSize::scaled\(PkSize const&, Qt::AspectRatioMode\) const/d' \
        -e '/pk_qt_assert\(char const\*, char const\*, int\)/d' \
        | grep -E '(^|[^[:alnum:]_])Q[A-Z][[:alnum:]_]*|(^|[[:space:]])(q[A-Z][[:alnum:]_]*\(|_?qt_[[:alnum:]_]*)'
}

# Pressure-test both sides of the matcher: copied compatibility spellings are
# allowed, while representative Qt class and C ABI symbols must be retained.
matcher_probe=$(printf '%s\n' \
    '                 U PkColor::PkColor(Qt::GlobalColor)' \
    '                 U PkSize::scaled(PkSize const&, Qt::AspectRatioMode) const' \
    '                 U pk_qt_assert(char const*, char const*, int)' \
    '                 U QPainter::drawImage(QRectF const&, QImage const&)' \
    '                 U operator<<(QDebug&, PkThing const&)' \
    '                 U vtable for QImage' \
    '                 U qt_version_tag' \
    '                 U qFatal(char const*, ...)' \
    | filter_real_qt_symbols)
expected_matcher_probe=$(printf '%s\n' \
    '                 U QPainter::drawImage(QRectF const&, QImage const&)' \
    '                 U operator<<(QDebug&, PkThing const&)' \
    '                 U vtable for QImage' \
    '                 U qt_version_tag' \
    '                 U qFatal(char const*, ...)')
if [[ "$matcher_probe" != "$expected_matcher_probe" ]]; then
  printf 'real Qt symbol matcher self-check failed\n' >&2
  printf '%s\n' "$matcher_probe" >&2
  exit 1
fi

real_qt_symbols=$(
    nm -u -C "$archive" | filter_real_qt_symbols || true
)
if [[ -n "$real_qt_symbols" ]]; then
  printf '%s\n' "$real_qt_symbols"
  printf 'real Qt undefined symbols found in %s\n' "$archive" >&2
  exit 1
fi

if readelf -d "$test_binary" | grep -Ei 'NEEDED.*Qt'; then
  printf 'Qt dynamic dependency found in %s\n' "$test_binary" >&2
  exit 1
fi
if ldd "$test_binary" | grep -Ei 'Qt|not found'; then
  printf 'Qt or unresolved dynamic dependency found in %s\n' "$test_binary" >&2
  exit 1
fi
if ninja -C "$build_dir" -t commands pkrender | grep -Ei 'Qt5|Qt6|libQt'; then
  printf 'Qt token found in pkrender command closure\n' >&2
  exit 1
fi

printf 'real_qt_symbol_scan=clean\n'
printf 'real_qt_symbol_matcher_self_check=clean\n'
printf 'dynamic_qt_needed_scan=clean\n'
printf 'pkrender_command_closure_qt_scan=clean\n'
