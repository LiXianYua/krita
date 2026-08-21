#!/usr/bin/env bash
# coexist_qt_probe.sh —— 编+跑 coexist_qt_probe.cpp 的两种模式（real Qt / 无 Qt）。
# R-34「让位给真 Qt」守卫的共存证据：两个模式都 exit 0 才算过。
#
# 用法：bash pk/signal/tests/coexist_qt_probe.sh
# 依赖：krita-ci-env（source 后 PATH/LD_LIBRARY_PATH 指向 _install，含 Qt 5.15.7）。
# 注意：先 source env 再 set -u——env 在 fresh shell 里有未绑定变量引用，set -u
# 在 source 前激活会当场炸。
ENV=/mnt/ssd-disk/liyang/projects/krita-ci-env/env
[ -f "$ENV" ] && . "$ENV"
set -uo pipefail
QT=/mnt/ssd-disk/liyang/projects/krita-ci-env/_install
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PK_SIGNAL_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PK_CONCURRENT_DIR="$(cd "$PK_SIGNAL_DIR/../concurrent" && pwd)"
PK_NAMESPACE_DIR="$(cd "$PK_SIGNAL_DIR/../namespace" && pwd)"
PROBE="$SCRIPT_DIR/coexist_qt_probe.cpp"
TMP="$(mktemp -d /tmp/pk-coexist-qt.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

echo "== 模式 A：real Qt + pk/signal compat 同 TU =="
# 环境漂移处理（2026-08-21 实测）：ci-env 的 Qt 5.15.7 按 ICU 70 链接，而
# _install/lib 里现在是 ICU 72——直接 -lQt5Core 会 undefined reference to
# ucnv_*/ucol_*。ICU 70 在这台机器上只存在于 snap 挂载路径。这里自动探测
# libicuuc.so.70 所在目录，找不到就退回 -fsyntax-only（共存是编译期性质，
# static_assert 在编译期就钉住；不链接也能给出共存证据）。
ICU70_DIR=""
for d in "$QT/lib" \
         /snap/gnome-42-2204/*/usr/lib/x86_64-linux-gnu \
         /home/liyang/projects/docs/krita-alignment/paint_app/tools/krita-gt/.cache/squashfs-root/usr/lib; do
    if [ -f "$d/libicuuc.so.70" ]; then ICU70_DIR="$d"; break; fi
done

if [ -n "$ICU70_DIR" ]; then
    echo "（找到 ICU 70：$ICU70_DIR）"
    g++ -fPIC -std=c++17 "$PROBE" -o "$TMP/coexist_realqt" \
      -I"$QT/include" -I"$QT/include/QtCore" -I"$QT/include/QtGui" \
      -I"$QT/include/QtWidgets" -I"$QT/include/QtTest" \
      -I"$PK_SIGNAL_DIR" -I"$PK_CONCURRENT_DIR" \
      -DQT_CORE_LIB -DQT_GUI_LIB -DQT_WIDGETS_LIB -DQT_TESTLIB_LIB \
      -L"$QT/lib" -L"$ICU70_DIR" -lQt5Test -lQt5Widgets -lQt5Gui -lQt5Core \
      -Wl,-rpath-link,"$QT/lib:$ICU70_DIR" -Wl,-rpath,"$QT/lib:$ICU70_DIR" \
      -Wl,--disable-new-dtags
    r=$?
    if [ $r -ne 0 ]; then echo "模式 A 编译失败（exit=$r）" >&2; exit $r; fi
    LD_LIBRARY_PATH="$QT/lib:$ICU70_DIR:${LD_LIBRARY_PATH:-}" "$TMP/coexist_realqt"
    r=$?
    if [ $r -ne 0 ]; then echo "模式 A 运行失败（exit=$r）" >&2; exit $r; fi
    echo "模式 A OK"
else
    echo "（未找到 ICU 70，退回 -fsyntax-only 编译期共存证据）"
    g++ -fPIC -std=c++17 -fsyntax-only "$PROBE" \
      -I"$QT/include" -I"$QT/include/QtCore" -I"$QT/include/QtGui" \
      -I"$QT/include/QtWidgets" -I"$QT/include/QtTest" \
      -I"$PK_SIGNAL_DIR" -I"$PK_CONCURRENT_DIR" \
      -DQT_CORE_LIB -DQT_GUI_LIB -DQT_WIDGETS_LIB -DQT_TESTLIB_LIB
    r=$?
    if [ $r -ne 0 ]; then echo "模式 A 编译失败（exit=$r）" >&2; exit $r; fi
    echo "模式 A OK（-fsyntax-only）"
fi

echo "== 模式 B：无 Qt =="
g++ -fPIC -std=c++17 "$PROBE" -o "$TMP/coexist_noqt" \
  -I"$PK_SIGNAL_DIR" -I"$PK_CONCURRENT_DIR" -I"$PK_NAMESPACE_DIR"
r=$?
if [ $r -ne 0 ]; then echo "模式 B 编译失败（exit=$r）" >&2; exit $r; fi
"$TMP/coexist_noqt"
r=$?
if [ $r -ne 0 ]; then echo "模式 B 运行失败（exit=$r）" >&2; exit $r; fi
echo "模式 B OK"

echo "coexist 探针：两种模式全过"
