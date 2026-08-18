#!/usr/bin/env bash
# Task 4（R-10）：真实 libs/image/tiles3/tests/kis_lockless_stack_test.cpp（.h 同）
# 经 pk/concurrent + pk/container + pk/log + pk/string + pk/test 垫片编过、链
# 过、跑绿——源树零改动。这是 R-10 全部交付物（PkMutex/PkAtomic*/PkThreadPool/
# PkRunnable/PkWaitCondition/PkSemaphore）与兄弟库真实协同工作的主证据，不是
# 自证的自测。
#
# 结构照抄 pk/log/tests/graft/kis_debug_build.sh + graft_run.sh 的手法：
# 依赖静态库缺了就地建 → 生成 binder（替代 moc）→ 用一个不提交的 driver.cpp
# 把"真实测试源 + binder"接进同一个 TU（ODR 硬规则，见 pk/test/CMakeLists.txt:
# 74-79）→ 编译（真实测试 .cpp 原地引用，一个字节不改）→ 链 → 跑 → nm -u 自证
# 判据③ → git diff 自证源树零改动。
set -eu
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

WORK=pk/concurrent/graft/build/kis_lockless_stack_test
STUBS=pk/concurrent/graft/stubs
CXX=${CXX:-g++}
mkdir -p "$WORK"

# ---- 依赖静态库：缺了就地建（各库独立薄壳工程，产物落各自 build/）----
build_lib() {
    # R-10 final review I2 修正：configure 只在 build 目录不存在时做（避免
    # 重复 cmake -S/-B），但 `cmake --build` 每次都无条件跑——它是增量构建，
    # 源码没变时近乎零成本；旧版把 build 挂在同一个 [ ! -f "$dir/build/$lib" ]
    # 判断里，导致库的 .a 一旦存在过一次，之后改了该库源码重跑本脚本会静默
    # 复用陈旧的预构建产物，而不是重新编译。
    local dir="$1"
    echo "== 建 $dir =="
    [ -d "$dir/build" ] || cmake -S "$dir" -B "$dir/build" -DCMAKE_BUILD_TYPE=Debug >/dev/null
    cmake --build "$dir/build" -j"$(nproc)" >/dev/null
}
build_lib pk/concurrent libpkconcurrent.a
build_lib pk/container  libpkcontainer.a
build_lib pk/string     libpkstring.a
build_lib pk/log        libpklog.a
build_lib pk/test       libpktest.a

# spdlog 静态库文件名随构建类型漂（Debug: libspdlogd.a，Release: libspdlog.a，
# 见 pk/log/tests/graft/kis_debug_build.sh 同款注释）——两个名字都探一遍。
SPDLOG_LIB=""
for cand in pk/log/build/libspdlogd.a pk/log/build/libspdlog.a; do
    if [ -f "$cand" ]; then SPDLOG_LIB="$cand"; break; fi
done
if [ -z "$SPDLOG_LIB" ]; then
    printf 'graft_run_lockless_stack.sh: 找不到 spdlog 静态库（试过 libspdlogd.a\n' >&2
    printf '  与 libspdlog.a，pk/log/build 下都没有）。\n' >&2
    exit 1
fi

# ---- 生成 binder（替代 moc 的测试发现）----
# 直接读真实测试头，一个字节不改——pk_test_moc.py 纯文本扫描，不需要编译。
python3 pk/test/pk_test_moc.py \
    libs/image/tiles3/tests/kis_lockless_stack_test.h \
    -o "$WORK/binder.inc"

# ---- driver.cpp：graft 自己的编译期胶水，不提交、不改测试源 ----
# 三行分别补的坑（都是真 Qt 靠某条本仓库没复刻的透传链带进来的东西，
# 不是调用点自己的疏漏，详情见本脚本旁 README/报告里的"依赖闭包探测"一节）：
#   · <numeric>   —— stressTestBulkPop() 用 std::accumulate，测试文件自己
#                     没 #include <numeric>，真 Qt 环境下靠某条头透传进来。
#   · QBENCHMARK  —— D-30 已裁决：benchmark 与单元测试分开，pk/test 不实现
#                     QBENCHMARK（pk/test/README.md 用量表）。试接的目的是
#                     证正确性，不是量性能，这里把它压成空宏——
#                     `QBENCHMARK { X }` 展开成 `{ X }`，块执行恰好一次；
#                     真 Qt 的 QBENCHMARK 会为了统计计时反复执行该块多次，
#                     所以本试接的实际运行时间天然比"跑一次真 Qt 环境基线"
#                     短得多——NUM_CYCLES/NUM_THREADS 一次没少，只是没有
#                     计时迭代器带来的重复执行倍数。
#   · binder.inc  —— PkTestBinder<KisLocklessStackTest> 特化必须与
#                     qExec<KisLocklessStackTest> 实例化处同一个 TU
#                     （ODR 硬规则），SIMPLE_TEST_MAIN 展开出的 main() 就在
#                     测试源里，所以 binder 必须接在它之后、同一个 driver.cpp。
cat > "$WORK/driver.cpp" <<'EOF'
#include <numeric>
#define QBENCHMARK
#include "kis_lockless_stack_test.cpp"
#include "binder.inc"
EOF

# 第二条 -include 是 kis_debug.h 自己的坑：它从没写过 #include <QString>，
# 真 Qt 靠 <QDebug>/<QLoggingCategory> 的透传把它带进来，我们的 compat/QDebug
# 与 compat/QLoggingCategory 都不做这层透传——同
# pk/log/tests/graft/kis_debug_build.sh 的手法，编译参数，不是对调用点的改动。
INC=(
    -include "$STUBS/QtGlobal"
    -include pk/string/compat/QString
    -include pk/concurrent/compat/QMutex
    -include pk/concurrent/compat/QAtomicInt
    -include pk/concurrent/compat/QAtomicPointer
    -include pk/concurrent/compat/QThreadPool
    -include pk/concurrent/compat/QRunnable
    -include pk/container/compat/QList
    -include pk/container/compat/QVector
    -I pk/concurrent -I pk/concurrent/compat
    -I pk/container -I pk/container/compat
    -I pk/log -I pk/log/compat
    -I pk/string -I pk/string/compat
    -I pk/test -I pk/test/compat
    -I pk/signal -I pk/signal/compat
    -I "$STUBS"
    -I libs/global
    -I libs/image/tiles3 -I libs/image/tiles3/tests
    -I "$WORK"
)
# kis_debug.h:158 的 `#ifndef QT_NO_DEBUG #undef Q_ASSERT #define Q_ASSERT(...)`
# 只在没定义 QT_NO_DEBUG 时才重定义 Q_ASSERT，同 kis_debug_build.sh 的理由。
DEFS=(-DQT_NO_DEBUG)

echo "== 编 driver.cpp（真实测试源原地引用，零改动）=="
"$CXX" -std=c++17 "${DEFS[@]}" "${INC[@]}" -c "$WORK/driver.cpp" -o "$WORK/driver.o"

echo "== 编 kis_debug.cpp（原地，libs/global 一字不动）=="
"$CXX" -std=c++17 "${DEFS[@]}" "${INC[@]}" -c libs/global/kis_debug.cpp -o "$WORK/kis_debug.o"

echo "== 链 =="
LIBS=(
    pk/concurrent/build/libpkconcurrent.a
    pk/container/build/libpkcontainer.a
    pk/log/build/libpklog.a
    pk/string/build/libpkstring.a
    "$SPDLOG_LIB"
    pk/test/build/libpktest.a
)
"$CXX" -std=c++17 "$WORK/driver.o" "$WORK/kis_debug.o" "${LIBS[@]}" -lpthread \
    -o "$WORK/kis_lockless_stack_test"

echo "== 跑（stressTest* 系列有 500000x10 / 10000000x3 规模的循环，属预期耗时）=="
"$WORK/kis_lockless_stack_test"

echo "== nm -uC 无 Qt 符号自证（判据③）=="
# R-10 final review I3 修正：裸 `nm -u | grep -i qt` 对 Itanium 名字修饰后的
# 符号基本失效——QString::number(int) 修饰后是 _ZN7QString6numberEi，
# 小写化不含 "qt" 子串，grep 直接漏判。改用 -C 反修饰 + 匹配 Qt 类名/命名空间
# 的实际形状：大写 Q 紧跟大写字母开头的标识符（QSomething）、qt_ 前缀的自由
# 函数、或 Qt:: 命名空间引用。
undef=$(nm -uC "$WORK/kis_lockless_stack_test" 2>/dev/null \
    | grep -E '(^|[^[:alnum:]_])(Q[A-Z][A-Za-z0-9_]*|qt_|Qt::)' || true)
if [ -n "$undef" ]; then
    printf 'nm -uC 产物里有 Qt 符号：\n%s\n' "$undef" >&2
    exit 1
fi
printf 'nm -uC kis_lockless_stack_test | grep -E Q类名/qt_/Qt::: 无输出\n'

echo "== 源树零改动自证（libs/image/tiles3、libs/global）=="
if ! git diff --quiet -- libs/image/tiles3 libs/global; then
    printf 'git diff --quiet 非空，源树被改动了！\n' >&2
    git diff --stat -- libs/image/tiles3 libs/global >&2
    exit 1
fi
printf 'git diff --quiet -- libs/image/tiles3 libs/global：干净\n'
