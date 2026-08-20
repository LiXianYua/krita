#!/usr/bin/env bash
# pk/variant 与真 Qt5 的逐输入对拍。
#
# 架构：oracle.cpp 包含 Qt 侧（<QVariant>），pk_side.cpp 包含 Pk 侧（PkVariant.h）。
# 两个 TU 永不同时包含 Qt 头与 Pk 头，彻底避免 qAbs/qRound 的重定义冲突。
# 中间通过 C 桥接（pk_side.h）连接，链接时靠 C 链接符号。
#
# 退出码：0 = 成功跑完；非 0 = FAIL。
source /mnt/ssd-disk/liyang/projects/krita-ci-env/env
set -euo pipefail
cd "$(dirname "$0")/../../.." || exit 1

QT=${PK_QT_PREFIX:-/mnt/ssd-disk/liyang/projects/krita-ci-env/_install}
SRC=pk/variant/oracle

[ -f "$QT/include/QtCore/QVariant" ] || { echo "找不到真 Qt5 的头：$QT/include/QtCore/QVariant" >&2; exit 1; }
[ -f "$QT/lib/libQt5Core.so" ] || { echo "找不到真 Qt5 的库：$QT/lib/libQt5Core.so" >&2; exit 1; }

mkdir -p pk/variant/build

# ── 编译 Qt 侧 TU（oracle.cpp） ──────────────────────────────────────────────────
# 只包含 Qt 头，不包含 Pk 头。
echo "编译 Qt 侧 oracle.cpp..."
g++ -std=c++17 -O2 -fPIC -DQT_NO_DEBUG \
    -I"$QT/include" -I"$QT/include/QtCore" -I"$QT/include/QtGui" \
    -I"$SRC" \
    -c "$SRC/oracle.cpp" -o pk/variant/build/oracle_qt.o

# ── 编译 Pk 侧 TU（pk_side.cpp）──────────────────────────────────────────────────
# 只包含 Pk 头，不包含 Qt 头。
# -I pk/variant（PkVariant.h 所在）、-I pk/string、-I pk/geometry、-I pk/container
echo "编译 Pk 侧 pk_side.cpp..."
g++ -std=c++17 -O2 -fPIC -fwrapv \
    -I pk/variant -I pk/string -I pk/geometry -I pk/container \
    -c "$SRC/pk_side.cpp" -o pk/variant/build/pk_side.o

# ── 链接 ─────────────────────────────────────────────────────────────────────────
echo "链接 oracle..."
# pk_side.o 调用了 PkVariant/PkString/PkGeometry 的符号，需链上对应的静态库。
# 从 cmake 构建目录（krita/build-ci）拿 .a 文件。
CMAKE_BUILD=${PK_VARIANT_BUILD:-build-r31-variant}
g++ -std=c++17 -O2 \
    pk/variant/build/oracle_qt.o pk/variant/build/pk_side.o \
    "$CMAKE_BUILD/libpkvariant.a" \
    "$CMAKE_BUILD/libpkstring.a" \
    "$CMAKE_BUILD/libpkgeometry.a" \
    "$CMAKE_BUILD/libpktime.a" \
    -L"$QT/lib" -Wl,-rpath-link,"$QT/lib" -Wl,-rpath,"$QT/lib" \
    -lQt5Core -lQt5Gui \
    -o pk/variant/build/oracle

# ── ldd 确认 ────────────────────────────────────────────────────────────────────
echo "ldd 确认真 Qt5 依赖："
ldd pk/variant/build/oracle | grep -E "libQt5Core" || { echo "FAIL: 对拍二进制未链上真 Qt5" >&2; exit 1; }

# ── 运行 ─────────────────────────────────────────────────────────────────────────
echo "运行 oracle..."
LD_LIBRARY_PATH="$QT/lib:$LD_LIBRARY_PATH" \
    QT_QPA_PLATFORM=offscreen \
    QT_PLUGIN_PATH="$QT/lib/plugins" \
    timeout 120 pk/variant/build/oracle | tee pk/variant/build/oracle.out

# ── 摘要 ─────────────────────────────────────────────────────────────────────────
echo ""
echo "===== 结果摘要 ====="
grep -E '^DIFF ' pk/variant/build/oracle.out || echo "未找到 DIFF 行"
grep -E '^DIFFTAG ' pk/variant/build/oracle.out || echo "未找到 DIFFTAG 行"
echo "Done."
