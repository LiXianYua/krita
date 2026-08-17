#!/usr/bin/env bash
# 真实 Krita 测试类 KisSignalAutoConnectionTest 试接：源树零改动，只在构建目录副本上
# 做 D-23 机械改名（pk/test/graft/rename.sed）+ 老式宏脚手架 sed（rename_extra.sed）。
# 两个 sed 都只是「让试接能跑绿」的脚手架；老式宏正式转换归 S 批次。
set -eu
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

BUILD=pk/signal/build/graft
SED=pk/test/graft/rename.sed
SED_EXTRA=pk/signal/graft/rename_extra.sed
SRCDIR=libs/global/tests
INC=libs/global
CXX=${CXX:-g++}
rc=0

# 前置：依赖库必须已就绪。libpksignal.a 由本工程的 CMake 建；其余三个各自由
# 自己的 CMake 建（pk/test 的构建正是 pk/test/tests/run_tests.sh 开头做的事）。
# 不存在则提示对应自举命令，不静默假定已在。
boot_cmake_lib() {
    local src="$1" build="$2" target="$3" lib="$4"
    if [ ! -f "$lib" ]; then
        cmake -S "$src" -B "$build" -DCMAKE_BUILD_TYPE=Debug >/dev/null
        cmake --build "$build" --target "$target" -j"$(nproc)" >/dev/null
    fi
    [ -f "$lib" ] || { echo "依赖库缺失: $lib（$src 的 CMake 构建后仍未产出）" >&2; exit 1; }
}
# libpksignal.a：本工程的静态库。注意 CMake 目标名是 pksignal（无 lib 前缀）。
set +e
if [ ! -f pk/signal/build/libpksignal.a ]; then
    cmake -S pk/signal -B pk/signal/build -DCMAKE_BUILD_TYPE=Debug >/dev/null
fi
cmake --build pk/signal/build --target pksignal -j"$(nproc)" >/dev/null
set -e
[ -f pk/signal/build/libpksignal.a ] || { echo "依赖库缺失: pk/signal/build/libpksignal.a" >&2; exit 1; }
boot_cmake_lib pk/test      pk/test/build      pktest       pk/test/build/libpktest.a
boot_cmake_lib pk/string    pk/string/build    pkstring     pk/string/build/libpkstring.a
boot_cmake_lib pk/container pk/container/build pkcontainer  pk/container/build/libpkcontainer.a

work="$BUILD/KisSignalAutoConnectionTest"
rm -rf "$work"; mkdir -p "$work"

# ① 复制 —— 源树一个字节都不动
cp "$SRCDIR/KisSignalAutoConnectionTest.h" "$SRCDIR/KisSignalAutoConnectionTest.cpp" "$work/"
cp "$INC/kis_signal_auto_connection.h" "$work/"

# ② 两个 sed（D-23 机械改名 + 老式宏脚手架）
sed -i -f "$SED" "$work/KisSignalAutoConnectionTest.h" "$work/KisSignalAutoConnectionTest.cpp" "$work/kis_signal_auto_connection.h"
sed -i -f "$SED_EXTRA" "$work/KisSignalAutoConnectionTest.cpp"

# ③ 双生成器：信号定义（本任务）+ 测试发现（R-11）
python3 pk/signal/pk_signal_moc.py "$work/KisSignalAutoConnectionTest.h" -o "$work/signals.inc"
python3 pk/test/pk_test_moc.py "$work/KisSignalAutoConnectionTest.h" -o "$work/binder.inc"

# ④ driver.cpp：把信号定义、测试 binder、测试源塞进同一个 TU（ODR 硬规则见 R-11 README）
printf '#include "signals.inc"\n#include "%s"\n#include "binder.inc"\n' \
  "KisSignalAutoConnectionTest.cpp" > "$work/driver.cpp"

# ⑤ PkTestObject override 带来的联动：
#    本试接用 `-include pk/signal/graft/stubs/PkTestObject.h`（见下方编译行）把
#    PkTestObject 宏改写成 PkObject——这让 harness 头（PkTest.h/binder.inc）里声明的
#    PkTest::execPlan(PkTestObject*) 实际声明成 execPlan(PkObject*)。而
#    pk/test/build/libpktest.a 里 PkTestRunner.o 是**不用**这个宏编出来的（签名是
#    execPlan(PkTestObject*) 真品），两边符号对不上会链接失败。
#    解法：用同一个 preinclude 把 pk/test 的 4 个 .cpp 重编一份 override 版
#    libpktest.a，只落在构建目录，pk/test 源树一字节不动。（preinclude 机制的
#    完整推导见 stubs/PkTestObject.h 头注释。）
mkdir -p "$work/libpktest"
for f in PkTestCase PkTestCompare PkTestData PkTestRunner; do
    "$CXX" -std=c++17 -c "pk/test/$f.cpp" \
        -include pk/signal/graft/stubs/PkTestObject.h \
        -I pk/test \
        -o "$work/libpktest/$f.o" 2>"$work/libpktest/$f.log"
done
ar rcs "$work/libpktest/libpktest.a" "$work/libpktest/"*.o

# ⑥ 编译链接。
#    -include 预包含 stubs/PkTestObject.h：见该文件头注释，PkTestObject=PkObject。
#    include 优先级：stubs 最优先（simpletest.h/QSharedPointer 同目录引号 include），
#    pk/signal/compat 先于 pk/test/compat（driver 顶层的 <QObject> 解析到 PkObject）。
#    PK_TEST_NO_QT_MACRO_ALIASES 让 sed 漏改处编译期炸。
"$CXX" -std=c++17 -DPK_TEST_NO_QT_MACRO_ALIASES \
    -include pk/signal/graft/stubs/PkTestObject.h \
    -I pk/signal/graft/stubs \
    -I pk/signal -I pk/signal/compat \
    -I pk/test -I pk/test/compat \
    -I pk/string -I pk/string/compat \
    -I pk/container -I pk/container/compat \
    -I "$INC" -I "$SRCDIR" -I "$work" \
    "$work/driver.cpp" \
    pk/signal/build/libpksignal.a \
    "$work/libpktest/libpktest.a" \
    pk/string/build/libpkstring.a \
    pk/container/build/libpkcontainer.a \
    -o "$work/test" 2>"$work/compile.log" || {
        printf '  试接编译失败\n'
        sed 's/^/    /' "$work/compile.log" | head -60
        rc=1
    }

if [ "$rc" -eq 0 ]; then
    if "$work/test" >"$work/run.log" 2>&1; then
        printf '  试接跑绿: KisSignalAutoConnectionTest (%s)\n' "$SRCDIR"
        grep -E '^(PASS|FAIL|Totals)' "$work/run.log" | sed 's/^/    /'
    else
        printf '  试接跑挂\n'
        sed 's/^/    /' "$work/run.log" | head -40
        rc=1
    fi
fi

# ⑦ nm -u 无 Qt（对静态链接可执行文件恒真，真实判别力在 libpksignal.a 那条，见 Task 4）
if [ "$rc" -eq 0 ]; then
    undef=$(nm -u "$work/test" 2>/dev/null | grep -i qt || true)
    if [ -n "$undef" ]; then
        printf '  试接产物含 Qt 符号:\n%s\n' "$undef"; rc=1
    else
        printf '    nm -u 试接产物 | grep -i qt: 无输出\n'
    fi
fi

# ⑧ 源树零改动自证
if ! git diff --quiet -- libs/global/tests/kis_signal_auto_connection.h \
       libs/global/tests/KisSignalAutoConnectionTest.h \
       libs/global/tests/KisSignalAutoConnectionTest.cpp; then
    printf '  源树被改动 —— 试接必须零改动\n' >&2
    rc=1
fi

exit "$rc"