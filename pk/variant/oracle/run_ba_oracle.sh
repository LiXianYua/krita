#!/usr/bin/env bash
# pk/variant PkByteArray 与真 Qt5 QByteArray 的逐输入对拍。
#
# 架构：ba_oracle.cpp 包含 Qt 侧（<QByteArray>），ba_pk_side.cpp 包含 Pk 侧
# （PkAuxTypes.h）。两 TU 永不同时包含 Qt 头与 Pk 头（PkGlobal.h 的 qAbs/qRound
# 与 Qt qglobal.h 同名冲突），中间靠 C 桥接（ba_pk_side.h）连接，链接时靠
# C 链接符号。
#
# 对拍源 include 路径**绝不能给 compat/**：一旦混入 pk/variant/compat 且存在
# compat/QByteArray 垫片，两侧会解析成同一个类型，oracle 无声退化成自比。
# ba_oracle.cpp 顶部有 static_assert(!std::is_same<QByteArray, PkByteArray>::value)
# 在编译期兜住。
#
# 退出码：0 = 成功跑完；非 0 = FAIL。
set -euo pipefail
cd "$(dirname "$0")/../../.." || exit 1

QT=${PK_QT_PREFIX:-/mnt/ssd-disk/liyang/projects/krita-ci-env/_install}
PK_BUILD=${PK_BUILD:-/tmp/r26-pkvariant}
SRC=pk/variant/oracle

[ -f "$QT/include/QtCore/QByteArray" ] || { echo "找不到真 Qt5 的头：$QT/include/QtCore/QByteArray" >&2; exit 1; }
[ -f "$QT/lib/libQt5Core.so" ] || { echo "找不到真 Qt5 的库：$QT/lib/libQt5Core.so" >&2; exit 1; }
[ -f "$PK_BUILD/libpkvariant.a" ] || { echo "找不到 $PK_BUILD/libpkvariant.a —— 先按 brief Step 6 构建 pk/variant" >&2; exit 1; }

mkdir -p pk/variant/build

# ── 编译 Qt 侧 TU（ba_oracle.cpp）─────────────────────────────────────────────────
# 只包含 Qt 头，不包含 Pk 头。
echo "编译 Qt 侧 ba_oracle.cpp..."
g++ -std=c++17 -O2 -fPIC -DQT_NO_DEBUG \
    -I"$QT/include" -I"$QT/include/QtCore" \
    -I"$SRC" \
    -c "$SRC/ba_oracle.cpp" -o pk/variant/build/ba_oracle_qt.o

# ── 编译 Pk 侧 TU（ba_pk_side.cpp）──────────────────────────────────────────────────
# 只包含 Pk 头，不包含 Qt 头。PkAuxTypes.h 经 PkPoint.h/PkSize.h 需要 pk/geometry。
echo "编译 Pk 侧 ba_pk_side.cpp..."
g++ -std=c++17 -O2 -fPIC -fwrapv \
    -I pk/variant -I pk/geometry \
    -c "$SRC/ba_pk_side.cpp" -o pk/variant/build/ba_pk_side.o

# ── 链接 ─────────────────────────────────────────────────────────────────────────
echo "链接 ba_oracle..."
# ba_pk_side.o 调 PkByteArray（libpkvariant.a）；libpkvariant.a 的 PkAuxTypes.o 又
# 引用 PkPoint/PkSize（libpkgeometry.a），PkVariant.o 引用 PkString（libpkstring.a）。
g++ -std=c++17 -O2 \
    pk/variant/build/ba_oracle_qt.o pk/variant/build/ba_pk_side.o \
    "$PK_BUILD/libpkvariant.a" \
    "$PK_BUILD/libpkstring.a" \
    "$PK_BUILD/libpkgeometry.a" \
    -L"$QT/lib" -Wl,-rpath-link,"$QT/lib" -Wl,-rpath,"$QT/lib" \
    -lQt5Core \
    -o pk/variant/build/ba_oracle

# ── ldd 确认 ────────────────────────────────────────────────────────────────────
echo "ldd 确认真 Qt5 依赖："
ldd pk/variant/build/ba_oracle | grep -E "libQt5Core" || { echo "FAIL: 对拍二进制未链上真 Qt5" >&2; exit 1; }

# ── 运行 ─────────────────────────────────────────────────────────────────────────
echo "运行 ba_oracle..."
LD_LIBRARY_PATH="$QT/lib:${LD_LIBRARY_PATH:-}" \
    QT_QPA_PLATFORM=offscreen \
    QT_PLUGIN_PATH="$QT/lib/plugins" \
    timeout 120 pk/variant/build/ba_oracle | tee pk/variant/build/ba_oracle.out

# ── 摘要 ─────────────────────────────────────────────────────────────────────────
echo ""
echo "===== 结果摘要 ====="
grep -E '^DIFF ' pk/variant/build/ba_oracle.out || echo "未找到 DIFF 行"
grep -E '^DIFFTAG ' pk/variant/build/ba_oracle.out || echo "未找到 DIFFTAG 行"
echo "Done."
