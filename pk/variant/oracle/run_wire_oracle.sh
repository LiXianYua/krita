#!/usr/bin/env bash
source /mnt/ssd-disk/liyang/projects/krita-ci-env/env
set -euo pipefail

cd "$(dirname "$0")/../../.."
qt_prefix=${PK_QT_PREFIX:-/mnt/ssd-disk/liyang/projects/krita-ci-env/_install}
build_dir=${PK_VARIANT_BUILD:-build-r31-variant}
out_dir="$build_dir/wire-oracle"
mkdir -p "$out_dir"

g++ -std=c++17 -O2 -fPIC \
    -I"$qt_prefix/include" -I"$qt_prefix/include/QtCore" \
    pk/variant/oracle/wire_fixture_oracle.cpp \
    -L"$qt_prefix/lib" -Wl,-rpath-link,"$qt_prefix/lib" -Wl,-rpath,"$qt_prefix/lib" \
    -lQt5Core -o "$out_dir/wire_fixture_oracle"

ldd "$out_dir/wire_fixture_oracle" > "$out_dir/ldd.txt"
grep -q libQt5Core "$out_dir/ldd.txt"
LD_LIBRARY_PATH="$qt_prefix/lib:${LD_LIBRARY_PATH:-}" \
    "$out_dir/wire_fixture_oracle" > "$out_dir/fixtures.txt"

g++ -std=c++17 -O2 -fwrapv \
    -Ipk/variant -Ipk/port -Ipk/string -Ipk/container -Ipk/geometry -Ipk/global -Ipk/time \
    pk/variant/oracle/wire_pk_checker.cpp pk/port/PkStream.cpp \
    "$build_dir/libpkvariant.a" "$build_dir/libpkgeometry.a" \
    "$build_dir/libpkstring.a" "$build_dir/libpktime.a" \
    -o "$out_dir/wire_pk_checker"

"$out_dir/wire_pk_checker" "$out_dir/fixtures.txt"
