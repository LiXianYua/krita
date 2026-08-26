#!/usr/bin/env bash
# S-09-b Task 1 -- Step 6: standalone compile of the three ported indexcolors
# TUs without Qt.
#
# The ported files still include libs/image headers (kis_filter_registry.h,
# KoColorSpaceMaths.h, KoColorSpaceRegistry.h, kis_filter_configuration.h ...),
# which is exactly the set the kritaimage thin shell compiles. The shell's
# mechanism for resolving the Qt compat macros (Q_ASSERT_X, Q_DECLARE_METATYPE,
# QT_VERSION_CHECK, Q_DECLARE_FLAGS, ...) is a forced pre-include of
# PkCompatAll.h on every TU, so this standalone compile mirrors that:
#
#   -include PkCompatAll.h        (resolved from $shell/compat)
#   -fwrapv -fno-operator-names   (the shell's PUBLIC compile options)
#   the full SHELL_INCLUDE_DIRS + SHELL_COMPAT_DIRS include set
#
# Pass criteria:
#   - the three TUs compile to .o with no Qt library symbols left: the only
#     matches of `nm -u <obj> | grep -i qt` are pk's own `Qt::GlobalColor`
#     namespace and `pk_qt_assert` (pk assert backend), never Q* library
#     classes.
# Prints "GRAFT PASS indexcolors standalone compile (no Qt symbols)" and exits 0.
set -euo pipefail

cd "$(dirname "$0")"

root="$(cd ../../../../.. && pwd)"
shell="${PK_SHELL:-/mnt/ssd-disk/liyang/projects/paint_tips/.exec/shell/kritaimage}"
qt_prefix="${PK_QT_PREFIX:-/mnt/ssd-disk/liyang/projects/krita-ci-env/_install}"

out_dir="${TMPDIR:-/tmp}/indexcolors-obj"
rm -rf "$out_dir"
mkdir -p "$out_dir"

incs=(
  -I"$shell" -I"$shell/build"
  -I"$root"
  -I"$root/plugins/filters/indexcolors" -I"$(pwd)"
  -I"$root/libs/image" -I"$root/libs/image/tiles3" -I"$root/libs/image/tiles3/swap"
  -I"$root/libs/image/brushengine" -I"$root/libs/image/commands" -I"$root/libs/image/commands_new"
  -I"$root/libs/image/filter" -I"$root/libs/image/floodfill" -I"$root/libs/image/generator"
  -I"$root/libs/image/layerstyles" -I"$root/libs/image/lazybrush" -I"$root/libs/image/processing"
  -I"$root/libs/image/3rdparty" -I"$root/libs/image/3rdparty/lock_free_map"
  -I"$root/sdk/tests"
  -I"$root/libs/global" -I"$root/libs/pigment" -I"$root/libs/pigment/compositeops" -I"$root/libs/pigment/resources"
  -I"$root/libs/command" -I"$root/libs/store" -I"$root/libs/resources" -I"$root/libs/version"
  -I"$root/libs/metadata" -I"$root/libs/psdutils" -I"$root/libs/koplugin" -I"$root/libs/multiarch"
  -I"$root/pk/string" -I"$root/pk/container" -I"$root/pk/variant" -I"$root/pk/uuid" -I"$root/pk/port"
  -I"$root/pk/pointer" -I"$root/pk/geometry" -I"$root/pk/global" -I"$root/pk/log" -I"$root/pk/concurrent"
  -I"$root/pk/image" -I"$root/pk/time" -I"$root/pk/config" -I"$root/pk/flags" -I"$root/pk/color"
  -I"$root/pk/signal" -I"$root/pk/sql" -I"$root/pk/xml" -I"$root/pk/namespace"
  -I"$shell/compat"
  -I"$root/pk/global/compat" -I"$root/pk/test/compat" -I"$root/pk/string/compat" -I"$root/pk/container/compat"
  -I"$root/pk/color/compat" -I"$root/pk/geometry/compat" -I"$root/pk/image/compat" -I"$root/pk/log/compat"
  -I"$root/pk/signal/compat" -I"$root/pk/variant/compat" -I"$root/pk/xml/compat" -I"$root/pk/flags/compat"
  -I"$root/pk/pointer/compat" -I"$root/pk/concurrent/compat" -I"$root/pk/time/compat" -I"$root/pk/port/compat"
  -I"$root/pk/config/compat" -I"$root/pk/sql/compat"
  -I"$qt_prefix/include" -I"$qt_prefix/include/Imath" -I"$qt_prefix/include/OpenEXR" -I"$qt_prefix/include/eigen3"
  -I"$shell/build/_deps/pugixml-src/src"
  -I"$root/pk/port/graft/stubs"
)

common=(-std=c++17 -fwrapv -fno-operator-names -include PkCompatAll.h)
g++ "${common[@]}" "${incs[@]}" -c "$root/plugins/filters/indexcolors/palettegeneratorconfig.cpp" -o "$out_dir/palettegeneratorconfig.o"
g++ "${common[@]}" "${incs[@]}" -c "$root/plugins/filters/indexcolors/indexcolorpalette.cpp" -o "$out_dir/indexcolorpalette.o"
g++ "${common[@]}" "${incs[@]}" -c "$root/plugins/filters/indexcolors/indexcolors.cpp" -o "$out_dir/indexcolors.o"

# S-line: no Qt library symbols. The only allowed `qt` substring matches are
# pk's own Qt namespace and pk_qt_assert (pk assert backend), plus Krita's
# KoColor::fromQColor (method name, not Qt).
qt_hits="$(nm -u "$out_dir"/*.o 2>/dev/null | awk '$1=="U"{print $2}' | sort -u | c++filt | grep -i qt || true)"
bad="$(printf '%s\n' "$qt_hits" | grep -E 'Q(DataStream|Color|String|ByteArray|Object|Variant|Hash|List|Map|Vector|Set|Rect|Point|Size|File|Image|Buffer|IODevice|Timer|Painter|Brush|Pen|Font)' || true)"
if [ -n "$bad" ]; then
    echo "FAIL: real Qt library symbols remain in objects:" >&2
    printf '%s\n' "$bad" >&2
    exit 1
fi

echo "indexcolors standalone compile OK (no Qt library symbols)"
echo "nm -u hits (pk/Krita names only):"
printf '%s\n' "$qt_hits"
echo "GRAFT PASS indexcolors standalone compile (no Qt symbols)"
