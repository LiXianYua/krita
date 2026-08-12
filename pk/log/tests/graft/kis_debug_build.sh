#!/usr/bin/env bash
# Task 5：真实 libs/global/kis_debug.h / kis_debug.cpp 零改动经 pk/log + pk/string
# 垫片编过、跑通分类过滤。libs/global 下的文件不复制、不 sed——这两个文件不是
# 测试，D-23 改名表跟它们无关，原地编译。全部胶水在这个脚本与 stubs/ 里。
set -eu
cd "$(dirname "$0")/../../../.." || exit 1     # → fork 仓库根

WORK=pk/log/tests/graft/build
mkdir -p "$WORK"

CXX=${CXX:-g++}
STUBS=pk/log/tests/graft/stubs

# pk/string：kis_debug.cpp 里的函数返回 QString（经 compat/QString 垫片 =
# PkString），要链 libpkstring.a。独立薄壳工程，不接入 Krita 主构建。
if [ ! -f pk/string/build/libpkstring.a ]; then
    cmake -S pk/string -B pk/string/build -DCMAKE_BUILD_TYPE=Debug >/dev/null
    cmake --build pk/string/build -j"$(nproc)" >/dev/null
fi

# pk/log：run_tests.sh 通常已经建过 libpklog.a；独立跑本脚本时兜底自己建。
if [ ! -f pk/log/build/libpklog.a ]; then
    cmake -S pk/log -B pk/log/build -DCMAKE_BUILD_TYPE=Debug >/dev/null
    cmake --build pk/log/build -j"$(nproc)" >/dev/null
fi

# 注意：这里的 -I pk/string / -I pk/string/compat 只是 graft 这条编译行的事——
# pk/log 库本身（pk/log/CMakeLists.txt 编出来的 libpklog.a）不 include/链接
# pk/string，Task 2 定的硬约束没被这个脚本破坏。
INC=(-I pk/log -I pk/log/compat -I pk/string -I pk/string/compat -I "$STUBS" -I libs/global)

# 第一次跑到这一步会报 "QString does not name a type" / "missing binary
# operator before token (" —— kis_debug.h 本身从没写过 #include <QString>，
# 全靠真 Qt 的 <QDebug>/<QLoggingCategory> 透传把它带进来；我们的 compat/QDebug
# 与 compat/QLoggingCategory 都不做这层透传（各自只 include 自己对应的
# PkXxx.h）。与 pk/string/tests/graft/graft_check.sh 同一个手法：用 -include
# 把 QString 垫片提到翻译单元最前面——这是编译参数，不是对调用点的改动。
FORCE_INCLUDE=(-include pk/string/compat/QString)

# -DQT_NO_DEBUG：kis_debug.h:158 的
#   #if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#     #ifndef QT_NO_DEBUG
#       #undef Q_ASSERT / #define Q_ASSERT(cond) ... errKrita.noquote() << kisBacktrace() ...
# pk/string/compat/QString 把 QT_VERSION 定义成 QT_VERSION_CHECK(0,0,0)，
# 于是外层条件恒真，内层 #ifndef QT_NO_DEBUG 那段就会激活，把 Q_ASSERT 整个
# 重定义掉。QT_NO_DEBUG 是 Qt 自己的 release-build 语义（NDEBUG 的 Qt 版本），
# 不是我们造的开关——用它跳过这段重定义，理由是"这段代码本来就只在
# debug 构建里生效"，而不是拿一个自造的宏硬压过去。
DEFS=(-DQT_NO_DEBUG)

echo "== 编 kis_debug.cpp（原地，libs/global 一字不动）=="
"$CXX" -std=c++17 "${DEFS[@]}" "${FORCE_INCLUDE[@]}" -c libs/global/kis_debug.cpp -o "$WORK/kis_debug.o" "${INC[@]}"

echo "== 编 driver_kis_debug.cpp =="
"$CXX" -std=c++17 "${DEFS[@]}" "${FORCE_INCLUDE[@]}" -c pk/log/tests/graft/driver_kis_debug.cpp -o "$WORK/driver_kis_debug.o" "${INC[@]}"

echo "== 链 =="
"$CXX" -std=c++17 "$WORK/kis_debug.o" "$WORK/driver_kis_debug.o" \
    pk/log/build/libpklog.a pk/string/build/libpkstring.a pk/log/build/libspdlogd.a \
    -o "$WORK/kis_debug_check"

echo "== 跑 =="
"$WORK/kis_debug_check"

echo "== nm -u 无 Qt 符号自证 =="
undef=$(nm -u "$WORK/kis_debug_check" 2>/dev/null | grep -i qt || true)
if [ -n "$undef" ]; then
    printf 'nm -u kis_debug_check 里有 Qt 符号：\n%s\n' "$undef" >&2
    exit 1
fi
printf 'nm -u kis_debug_check | grep -i qt: 无输出\n'

echo "== 源树零改动自证（libs/global）=="
if ! git diff --quiet -- libs/global; then
    printf 'git diff --quiet -- libs/global 非空，libs/global 被改动了！\n' >&2
    git diff -- libs/global >&2
    exit 1
fi
printf 'git diff --quiet -- libs/global：干净\n'
