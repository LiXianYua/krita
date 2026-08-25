#!/usr/bin/env bash
# transition_qt_probe.sh —— 编+跑 transition_qt_probe.cpp 的两种模式（real Qt / 无 Qt）。
# R-37「pk namespace Qt 墙让位守卫」的 transition TU 证据：两个模式都 exit 0 才算过。
#
# 用法：bash pk/namespace/tests/transition_qt_probe.sh
# 依赖：krita-ci-env（source 后 PATH/LD_LIBRARY_PATH 指向 _install，含 Qt 5.15.7）。
ENV=/mnt/ssd-disk/liyang/projects/krita-ci-env/env
[ -f "$ENV" ] && . "$ENV"
set -uo pipefail
QT=/mnt/ssd-disk/liyang/projects/krita-ci-env/_install
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PK_COLOR_DIR="$(cd "$SCRIPT_DIR/../../color" && pwd)"
PROBE="$SCRIPT_DIR/transition_qt_probe.cpp"

echo "== 模式 A：real Qt + PkColor.h（经 PkNamespace.h）同 TU =="
g++ -fPIC -std=gnu++17 -fsyntax-only "$PROBE" \
  -isystem "$QT/include" -isystem "$QT/include/QtCore" -isystem "$QT/include/QtGui" \
  -DQT_CORE_LIB -DQT_GUI_LIB -I "$PK_COLOR_DIR"
r=$?
if [ $r -ne 0 ]; then echo "模式 A 编译失败（exit=$r）" >&2; exit $r; fi
echo "模式 A OK"

echo "== 模式 B：无 Qt =="
g++ -fPIC -std=gnu++17 "$PROBE" -o /tmp/transition_noqt_$$ -I "$PK_COLOR_DIR"
r=$?
if [ $r -ne 0 ]; then echo "模式 B 编译失败（exit=$r）" >&2; exit $r; fi
/tmp/transition_noqt_$$
r=$?
rm -f /tmp/transition_noqt_$$
if [ $r -ne 0 ]; then echo "模式 B 运行失败（exit=$r）" >&2; exit $r; fi
echo "模式 B OK"

echo "transition 探针：两种模式全过"
