#!/usr/bin/env bash
source /mnt/ssd-disk/liyang/projects/krita-ci-env/env
set -euo pipefail

cd "$(dirname "$0")/../../.."
qt_prefix=${PK_QT_PREFIX:-/mnt/ssd-disk/liyang/projects/krita-ci-env/_install}
build_dir=${PK_VARIANT_BUILD:-build-r31-variant}
out_dir="$build_dir/bare-bool-oracle"
mkdir -p "$out_dir"

g++ -std=c++17 -O2 -fPIC \
    -I"$qt_prefix/include" -I"$qt_prefix/include/QtCore" \
    pk/variant/oracle/bare_bool_qt_oracle.cpp \
    -L"$qt_prefix/lib" -Wl,-rpath-link,"$qt_prefix/lib" -Wl,-rpath,"$qt_prefix/lib" \
    -lQt5Core -o "$out_dir/qt_oracle"

g++ -std=c++17 -O2 -fwrapv \
    -Ipk/variant -Ipk/port -Ipk/string -Ipk/container -Ipk/geometry -Ipk/global -Ipk/time \
    pk/variant/oracle/bare_bool_pk_checker.cpp \
    "$build_dir/libpkvariant.a" "$build_dir/libpkstring.a" "$build_dir/libpktime.a" \
    "$build_dir/libpkgeometry.a" "$build_dir/libpkport.a" "$build_dir/libpkzip.a" \
    "$build_dir/libminizip-ng.a" -lz -o "$out_dir/pk_checker"

ldd "$out_dir/qt_oracle" > "$out_dir/ldd.txt"
grep -q libQt5Core "$out_dir/ldd.txt"
LD_LIBRARY_PATH="$qt_prefix/lib:${LD_LIBRARY_PATH:-}" "$out_dir/qt_oracle" > "$out_dir/qt.txt"
"$out_dir/pk_checker" > "$out_dir/pk.txt"

# R-line oracle contract: mismatches are data for review, not process errors.
# The first four fields encode the input shape and form the DIFFTAG.
awk '
    NR == FNR { qt[NR] = $0; qt_count = NR; next }
    { pk[FNR] = $0; pk_count = FNR }
    END {
        total = qt_count > pk_count ? qt_count : pk_count
        mismatch = 0
        for (i = 1; i <= total; ++i) {
            if (qt[i] != pk[i]) {
                ++mismatch
                source = qt[i] != "" ? qt[i] : pk[i]
                count = split(source, fields, " ")
                if (count >= 4) {
                    tag = fields[1] "/" fields[2] "/" fields[3] "/" fields[4]
                    gsub(/=/, "-", tag)
                } else {
                    tag = "line-" i
                }
                tags[i] = tag
            }
        }
        printf "DIFF total=%d mismatch=%d\n", total, mismatch
        for (i = 1; i <= total; ++i) {
            if (tags[i] != "") printf "DIFFTAG bare-bool %s 1\n", tags[i]
        }
    }
' "$out_dir/qt.txt" "$out_dir/pk.txt"
