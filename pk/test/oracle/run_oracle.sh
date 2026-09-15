#!/usr/bin/env bash
# run_oracle.sh —— R-77 Task 1 的真 Qt 语义探针（macOS 版）。
#
# 形态照 pk/flags/oracle/run_oracle.sh 与 pk/color/oracle/run_oracle.sh，
# 差异只在平台：那两份是 Linux（g++ + libQt5Core.so + rpath-link）；
# 本机是 macOS（Apple clang + QtCore.framework + -F$QT/lib）。
#
# 真 Qt 5.15.7 在 CI 前缀里：$PREFIX/lib/QtCore.framework。
# 探针只读，除了在 $TMPDIR 下自造几个探针文件（自删）。
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${1:-$HERE/build}"

# 真 Qt 前缀：优先取 CI 环境变量，否则用本机已知位置。
: "${PK_QT_PREFIX:=/Users/liyang/Developer/projects/krita-ci-env/_install}"
QT_LIB="$PK_QT_PREFIX/lib"
if [ ! -d "$QT_LIB/QtCore.framework" ]; then
    echo "找不到真 QtCore.framework：PK_QT_PREFIX=$PK_QT_PREFIX" >&2; exit 1
fi

mkdir -p "$OUT"
CXXFLAGS="-std=c++17 -fPIC -O1 -F$QT_LIB -I$QT_LIB/QtCore.framework/Headers"
c++ $CXXFLAGS "$HERE/probe_file_shims.cpp" -o "$OUT/probe_file_shims" \
    -framework QtCore -Wl,-rpath,"$QT_LIB"

# 证明真的链到了真 Qt（把 otool 输出也留一份，报告里当证据）
echo "### otool -L $OUT/probe_file_shims (证明链的是真 Qt5.15.7 框架)"
otool -L "$OUT/probe_file_shims" | sed 's/^/    /'

echo
echo "### probe 原始输出（HOME=$(mktemp -d) 隔离，模拟 ctest 口径）"
_HOME="$(mktemp -d)"
HOME="$_HOME" "$OUT/probe_file_shims"
rm -rf "$_HOME"
