#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.." || exit 1

QT_ROOT=${PK_QT_PREFIX:-/mnt/ssd-disk/liyang/projects/krita-ci-env/_install}
SRC=pk/geometry/oracle/painterpath_pathops_difftest.cpp
OUT=pk/geometry/build/pathops_difftest
LOG=pk/geometry/build/pathops_difftest.out
COVERAGE_LOG=pk/geometry/build/pathops_difftest.coverage
INVALID_STDOUT=pk/geometry/build/pathops_difftest.invalid-stdout
DEV=pk/geometry/oracle/pathops.deviation

[ -f "$QT_ROOT/include/QtGui/QPainterPath" ] || { echo "missing real Qt QPainterPath" >&2; exit 1; }
[ -f "$QT_ROOT/lib/libQt5Gui.so.5" ] || { echo "missing real Qt5Gui" >&2; exit 1; }

mkdir -p pk/geometry/build
g++ -std=c++17 -O2 -fPIC -DQT_NO_DEBUG \
    -I"$QT_ROOT/include" -I"$QT_ROOT/include/QtCore" -I"$QT_ROOT/include/QtGui" \
    -Ipk/geometry -Ipk/global -Ipk/container -o "$OUT" "$SRC" \
    -L"$QT_ROOT/lib" -Wl,-rpath-link,"$QT_ROOT/lib" -Wl,-rpath,"$QT_ROOT/lib" \
    -lQt5Gui -lQt5Core

printf 'ldd %s | grep -i qt:\n' "$OUT"
LD_LIBRARY_PATH="$QT_ROOT/lib" ldd "$OUT" | grep -i qt
for lib in libQt5Gui.so.5 libQt5Core.so.5; do
    LD_LIBRARY_PATH="$QT_ROOT/lib" ldd "$OUT" | grep -q "$lib" || {
        echo "oracle did not link $lib" >&2
        exit 1
    }
done

LD_LIBRARY_PATH="$QT_ROOT/lib" "$OUT" > "$LOG" 2> "$COVERAGE_LOG"
if grep -Ev '^(DIFF total=[0-9]+ mismatch=[0-9]+|DIFFTAG .+ [0-9]+)$' "$LOG" > "$INVALID_STDOUT"; then
    echo "oracle stdout contains records outside DIFF/DIFFTAG" >&2
    head -30 "$INVALID_STDOUT" >&2
    exit 1
fi
if grep -Ev '^FAMILY [a-z0-9-]+$' "$COVERAGE_LOG" > /dev/null; then
    echo "oracle coverage stream contains invalid records" >&2
    head -30 "$COVERAGE_LOG" >&2
    exit 1
fi
for family in empty move-only consecutive-moves zero-length-line rectangle ellipse open-polyline closed-polyline nested-rings disjoint-compound bow-tie star adversarial-cubic close-same-line close-different-line fuzzy-close-normalization edit-last-element add-polygon; do
    grep -qx "FAMILY $family" "$COVERAGE_LOG" || { echo "missing family $family" >&2; exit 1; }
done

[ ! -s "$DEV" ] || { echo "$DEV must be empty without an approved deviation" >&2; exit 1; }
line=$(grep '^DIFF total=' "$LOG")
[ "$(grep -c '^DIFF total=' "$LOG")" -eq 1 ] || { echo "expected exactly one DIFF line" >&2; exit 1; }
printf '%s\n' "$line"
mismatch=$(printf '%s\n' "$line" | sed -E 's/.*mismatch=([0-9]+).*/\1/')
if [ "$mismatch" -ne 0 ]; then
    grep '^DIFFTAG ' "$LOG" | head -30 >&2 || true
    echo "pathops oracle mismatch=$mismatch" >&2
    exit 1
fi
if grep -q '^DIFFTAG ' "$LOG"; then
    echo "DIFFTAG present with mismatch=0" >&2
    exit 1
fi
