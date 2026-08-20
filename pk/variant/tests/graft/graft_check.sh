#!/usr/bin/env bash
source /mnt/ssd-disk/liyang/projects/krita-ci-env/env
set -euo pipefail

cd "$(dirname "$0")/../../../.."
build_dir=${PK_VARIANT_BUILD:-build-r31-variant}
out_dir="$build_dir/graft"
mkdir -p "$out_dir"

g++ -std=c++17 -O2 -fwrapv \
    -Ipk/variant -Ipk/port -Ipk/string -Ipk/container -Ipk/geometry -Ipk/global -Ipk/time \
    pk/variant/tests/graft/kis_resource_cache_db_shape.cpp pk/port/PkStream.cpp \
    "$build_dir/libpkvariant.a" "$build_dir/libpkgeometry.a" \
    "$build_dir/libpkstring.a" "$build_dir/libpktime.a" \
    -o "$out_dir/kis_resource_cache_db_shape"

"$out_dir/kis_resource_cache_db_shape"
