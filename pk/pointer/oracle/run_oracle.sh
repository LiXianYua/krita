#!/bin/bash
# pk/pointer 与真 Qt5 的逐输入对拍：编两侧 → ldd 自证真的链上了 Qt → 跑。
#
# **判据自己编译、自己执行**：这样"这份输出是不是它产生的"不需要推断，一条 g++
# 加一条运行就把整类 attestation 问题关死了。退出码恒为 0（即使 mismatch>0，
# 差异该不该存在是 reviewer 判的，不是这个脚本判的）。
set -euo pipefail
QT=/mnt/ssd-disk/liyang/projects/kde-deps/usr
HERE="$(cd "$(dirname "$0")" && pwd)"

# ⚠ -I 只给 pk/pointer 与 Qt，**绝不给 pk/pointer/compat**——垫片一旦被拉进来，
# 两侧会解析成同一个类型，对拍失去意义（pointer_difftest.cpp 顶部的
# #ifdef QSharedPointer / #error 与 main.cpp 的 static_assert 兜底，但这里先不写）。
QT_INCS=(-I"$QT/include/x86_64-linux-gnu/qt5" -I"$QT/include/x86_64-linux-gnu/qt5/QtCore")
PK_INCS=(-I"$HERE" -I"$HERE/..")

echo "--- 编译 Qt 侧 ---"
g++ -fPIC -std=c++17 -O1 -c "$HERE/pointer_difftest.cpp" -o /tmp/oracle_qt.o -DORACLE_QT_SIDE \
    "${PK_INCS[@]}" "${QT_INCS[@]}"

echo "--- 编译 Pk 侧 ---"
g++ -fPIC -std=c++17 -O1 -c "$HERE/pointer_difftest.cpp" -o /tmp/oracle_pk.o \
    "${PK_INCS[@]}"

# main.cpp 是裁决 R2 补的第三个 TU（main() 独立成 TU，两次编译 pointer_difftest.cpp
# 才不会撞出两份 main）。它同时 #include <QSharedPointer> 与 "PkSharedPointer.h"
# 放形态契约第一条自证（static_assert 两个真类型不是同一个），因此也要两组 -I。
echo "--- 编译 main.cpp ---"
g++ -fPIC -std=c++17 -O1 -c "$HERE/main.cpp" -o /tmp/oracle_main.o \
    "${PK_INCS[@]}" "${QT_INCS[@]}"

echo "--- 链接 ---"
g++ -fPIC /tmp/oracle_qt.o /tmp/oracle_pk.o /tmp/oracle_main.o -o /tmp/pointer_oracle \
    -L"$QT/lib/x86_64-linux-gnu" -lQt5Core

echo "--- ldd 自证（形态契约第二条：必须看到 libQt5Core.so.5）---"
LD_LIBRARY_PATH="$QT/lib/x86_64-linux-gnu" ldd /tmp/pointer_oracle | grep -i qt

echo "--- 对拍 ---"
LD_LIBRARY_PATH="$QT/lib/x86_64-linux-gnu" /tmp/pointer_oracle
