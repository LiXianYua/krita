#!/usr/bin/env bash
source /mnt/ssd-disk/liyang/projects/krita-ci-env/env
set -euo pipefail

cd "$(dirname "$0")/../../../.."
build_dir=${PK_VARIANT_BUILD:-build-r31-variant}
out_dir="$build_dir/palette-graft"
mkdir -p "$out_dir"

g++ -std=c++17 -O2 -fwrapv \
    -Ipk/variant -Ipk/color -Ipk/port -Ipk/string -Ipk/container -Ipk/geometry \
    -Ipk/global -Ipk/namespace -Ipk/time \
    pk/variant/tests/graft/palette_generator_shape.cpp \
    "$build_dir/libpkvariant.a" "$build_dir/libpkcolor.a" "$build_dir/libpkstring.a" \
    "$build_dir/libpktime.a" "$build_dir/libpkgeometry.a" "$build_dir/libpkport.a" \
    "$build_dir/libpkzip.a" "$build_dir/libminizip-ng.a" -lz \
    -o "$out_dir/palette_generator_shape"

"$out_dir/palette_generator_shape"
