#!/usr/bin/env bash
# S-09-b Task 1 -- PaletteGeneratorConfig byte-compatibility verification.
#
# Two sides:
#   palette_qt_fixture  -- real Qt (QColor/QDataStream, Qt_4_6 + BigEndian),
#                          writes the 213-byte blob hex + semantic text.
#   palette_pk_fixture  -- production Pk palettegeneratorconfig.cpp,
#                          reads the Qt blob back, verifies semantics,
#                          re-serializes, prints the same lines.
#
# Pass criteria:
#   - Pk re-serialized hex == real-Qt blob hex (byte-for-byte), and
#   - Pk read-back semantic text == real-Qt semantic text.
#   Both are checked by a single `diff` of the two fixtures' stdout
#   (the PALETTE_READ_SEMANTICS PASS marker is filtered before diffing).
#
# Prints "GRAFT PASS PaletteGeneratorConfig Qt_4_6 BigEndian byte-compat"
# on success and exits 0.
# Note: the env script must be sourced before `set -u` -- it reads unset
# variables (e.g. EXTERNALS_DOWNLOAD_DIR) during its own setup.
source /mnt/ssd-disk/liyang/projects/krita-ci-env/env
set -euo pipefail

cd "$(dirname "$0")"

qt_prefix=${PK_QT_PREFIX:-/mnt/ssd-disk/liyang/projects/krita-ci-env/_install}
root="$(cd ../../../../.. && pwd)"

out_dir="${TMPDIR:-/tmp}/palette-bytecompat"
rm -rf "$out_dir"
mkdir -p "$out_dir"

# ---------------------------------------------------------------------------
# 1. Real-Qt fixture
# ---------------------------------------------------------------------------
g++ -std=c++17 -O2 -fPIC \
    -I"$qt_prefix/include" -I"$qt_prefix/include/QtCore" -I"$qt_prefix/include/QtGui" \
    palette_qt_fixture.cpp \
    -L"$qt_prefix/lib" -Wl,-rpath,"$qt_prefix/lib" \
    -lQt5Gui -lQt5Core \
    -o "$out_dir/palette_qt_fixture"

LD_LIBRARY_PATH="$qt_prefix/lib:${LD_LIBRARY_PATH:-}" "$out_dir/palette_qt_fixture" \
    > "$out_dir/qt.txt"

qt_hex="$(awk 'NR==1 && $1=="BLOB" {print $2; exit}' "$out_dir/qt.txt")"
if [ -z "$qt_hex" ]; then
    echo "FAIL: Qt fixture produced no BLOB line" >&2
    cat "$out_dir/qt.txt" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# 2. Pk fixture against production palettegeneratorconfig.cpp
# ---------------------------------------------------------------------------
# The fixture only exercises toByteArray()/fromByteArray(); generate() (the
# only function touching IndexColorPalette/KoColor) is unreferenced and is
# dropped by -ffunction-sections + --gc-sections, so no libs/image/pigment
# objects need to be linked.
g++ -std=c++17 -O2 -fwrapv -fno-operator-names -ffunction-sections -fdata-sections \
    -I"$root/plugins/filters/indexcolors" \
    -I"$(pwd)" \
    -I"$root/libs/pigment" \
    -I"$root/libs/global" \
    -I"$qt_prefix/include" \
    -I"$qt_prefix/include/Imath" \
    -I"$root/pk/string" -I"$root/pk/container" -I"$root/pk/variant" \
    -I"$root/pk/port" -I"$root/pk/pointer" -I"$root/pk/geometry" \
    -I"$root/pk/global" -I"$root/pk/log" -I"$root/pk/concurrent" \
    -I"$root/pk/image" -I"$root/pk/time" -I"$root/pk/config" \
    -I"$root/pk/flags" -I"$root/pk/color" -I"$root/pk/signal" \
    -I"$root/pk/sql" -I"$root/pk/xml" -I"$root/pk/namespace" \
    -I"$root/pk/port/graft/stubs" \
    palette_pk_fixture.cpp \
    palette_message_logger_stub.cpp \
    "$root/plugins/filters/indexcolors/palettegeneratorconfig.cpp" \
    "$root/pk/variant/PkDataStream.cpp" \
    "$root/pk/variant/PkDataStreamColor.cpp" \
    "$root/pk/variant/PkAuxTypes.cpp" \
    "$root/pk/color/PkColor.cpp" \
    "$root/pk/port/PkStream.cpp" \
    -Wl,--gc-sections \
    -o "$out_dir/palette_pk_fixture"

"$out_dir/palette_pk_fixture" "$qt_hex" > "$out_dir/pk.txt"

# ---------------------------------------------------------------------------
# 3. Diff
# ---------------------------------------------------------------------------
grep -q '^PALETTE_READ_SEMANTICS PASS$' "$out_dir/pk.txt" || {
    echo "FAIL: Pk read-back semantic verification did not pass" >&2
    cat "$out_dir/pk.txt" >&2
    exit 1
}

grep -v '^PALETTE_READ_SEMANTICS' "$out_dir/pk.txt" > "$out_dir/pk_nopass.txt"
if ! diff -u "$out_dir/qt.txt" "$out_dir/pk_nopass.txt"; then
    echo "FAIL: Qt vs Pk outputs differ (see diff above)" >&2
    exit 1
fi

# Belt and braces: the BLOB hex (Pk write) must equal the Qt blob hex (input).
# Note: pk.txt's first line is the PASS marker, so find the BLOB line anywhere.
pk_hex="$(awk '$1=="BLOB" {print $2; exit}' "$out_dir/pk.txt")"
if [ "$pk_hex" != "$qt_hex" ]; then
    echo "FAIL: Pk write hex != Qt blob hex" >&2
    exit 1
fi

echo "GRAFT PASS PaletteGeneratorConfig Qt_4_6 BigEndian byte-compat"
