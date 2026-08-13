#!/usr/bin/env bash
# 复现 qt_semantics_probe.cpp 的原始输出（docs/superpowers/plans/R-04.md §0
# 那份贴出来的就是这条命令的产物）。探针配方见计划的 Global Constraints 一节，
# 这里逐字照抄，不自创。
#
# ⚠ -fPIC 不能省：Qt 是 -reduce-relocations 构建的，漏了会让每一个编译探针
# 失败，看起来像"Qt 没有这个 API"（本任务踩过一次：25 条编译矩阵全 FAIL，
# 实际只是缺 -fPIC）。
set -eu
cd "$(dirname "$0")"

QT=/mnt/ssd-disk/liyang/projects/kde-deps/usr
g++ -fPIC -std=c++17 qt_semantics_probe.cpp -o probe \
    -I"$QT/include/x86_64-linux-gnu/qt5" -I"$QT/include/x86_64-linux-gnu/qt5/QtCore" \
    -L"$QT/lib/x86_64-linux-gnu" -lQt5Core

LD_LIBRARY_PATH="$QT/lib/x86_64-linux-gnu" ./probe
