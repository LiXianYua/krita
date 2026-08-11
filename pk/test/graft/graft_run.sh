#!/usr/bin/env bash
# 真实 Krita 测试类试接：源树零改动，只在构建目录的副本上做 D-23 机械改名。
set -eu
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

BUILD=pk/test/build/graft
SED=pk/test/graft/rename.sed
CXX=${CXX:-g++}
rc=0

run_one() {
    local name="$1" srcdir="$2" hdr="$3" src="$4" incdir="$5"
    local work="$BUILD/$name"
    rm -rf "$work"; mkdir -p "$work"

    # ① 复制 —— 源树一个字节都不动
    cp "$srcdir/$hdr" "$srcdir/$src" "$work/"

    # ② D-23 机械改名，只此一项改动
    sed -i -f "$SED" "$work/$hdr" "$work/$src"

    # ③ 生成 binder（替代 moc 的测试发现）。.inc 而非 .cpp：产物全是类内定义
    #    （隐式 inline），只能被 #include，不能作为独立翻译单元编译。
    python3 pk/test/pk_test_moc.py "$work/$hdr" -o "$work/binder.inc"

    # ③.5 driver.cpp：PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须在同一
    #     翻译单元里看见它的完整定义（Task 5 报告里的 ODR 硬规则）。真实测试类
    #     的 .cpp 只允许 rename.sed 的机械改名，不能往里加 #include —— 所以这层
    #     "把两者塞进同一个 TU" 的粘合只能由 graft 自己的 driver.cpp 来做，
    #     它不是复制自源树的文件，是本 harness 拥有的构建期胶水。
    printf '#include "%s"\n#include "binder.inc"\n' "$src" > "$work/driver.cpp"

    # ④ 编译链接。PK_TEST_NO_QT_MACRO_ALIASES 关掉 compat 里的 QCOMPARE 别名，
    #    让 sed 漏改的地方编译期就炸 —— 那正是 D-23 想要的效果。
    "$CXX" -std=c++17 -DPK_TEST_NO_QT_MACRO_ALIASES \
        -I pk/test -I pk/test/compat -I pk/test/graft/stubs \
        -I "$incdir" -I "$srcdir" -I "$work" \
        "$work/driver.cpp" \
        pk/test/build/libpktest.a -o "$work/$name" 2>"$work/compile.log" || {
            printf '  试接编译失败: %s\n' "$name"
            sed 's/^/    /' "$work/compile.log" | head -60
            rc=1
            return
        }

    # ⑤ 跑
    if "./$work/$name" >"$work/run.log" 2>&1; then
        printf '  试接跑绿: %s (%s)\n' "$name" "$srcdir"
        grep -E '^(PASS|FAIL|Totals)' "$work/run.log" | sed 's/^/    /'
    else
        printf '  试接跑挂: %s\n' "$name"
        sed 's/^/    /' "$work/run.log" | head -40
        rc=1
        return
    fi

    # ⑥ 判据③：产物不得有 Qt 未定义符号
    local undef
    undef=$(nm -u "$work/$name" 2>/dev/null | grep -i qt || true)
    if [ -n "$undef" ]; then
        printf '  试接产物含 Qt 符号: %s\n%s\n' "$name" "$undef"
        rc=1
    else
        printf '    nm -u %s | grep -i qt: 无输出\n' "$name"
    fi
}

# libpktest.a 必须已经建好（run_tests.sh 前面几步的产物）。
if [ ! -f pk/test/build/libpktest.a ]; then
    printf '  libpktest.a 不存在，先跑 pk/test/tests/run_tests.sh 的构建步骤\n' >&2
    exit 1
fi

run_one KisValueCacheTest  libs/global/tests  KisValueCacheTest.h  KisValueCacheTest.cpp  libs/global
run_one TestKoIntegerMaths libs/pigment/tests TestKoIntegerMaths.h TestKoIntegerMaths.cpp libs/pigment

# 源树零改动自证
if ! git diff --quiet -- libs/global/tests libs/pigment/tests; then
    printf '  源树被改动了 —— 试接必须零改动\n' >&2
    git diff --stat -- libs/global/tests libs/pigment/tests >&2
    rc=1
fi

exit "$rc"
