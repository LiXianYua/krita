#!/usr/bin/env bash
# 判据②：真实 Krita 测试类试接。源树零改动，只在构建目录的副本上做 D-23 机械改名。
#
# 目标：libs/image/tests/kis_fill_interval_map_test.cpp
#       ← 被测 libs/image/floodfill/kis_fill_interval_map.cpp（target kritaimage）
#
# 它压到的容器形状（选它而不选别的，理由见 ../../README.md §9）：
#   QHash<int, QMap<int, KisFillInterval>>  ← **嵌套关联容器**
#   QMap<int, KisFillInterval>              ← 有序映射，值是自定义 POD
#   QStack<KisFillInterval>                 ← fetchAllIntervals() 的返回类型
#   迭代器语义：it->field（解引用得 value 而非 pair）、*it、++it、it++、
#               it != end、rowMap->insert(...)（对 QHash::iterator 用
#               operator-> 拿到里层 QMap 再调它的方法）、erase(it) 返回下一个、
#               PK_COMPARE(range.beginIt, range.endIt)（迭代器互比）
#
# ── 与 pk/test/graft/graft_run.sh 的**一处结构差异** ────────────
# R-11 那份只编一个 TU（测试 .cpp + binder + driver），因为它选的被测类是
# header-only。容器族没有 header-only 的被测类，所以这里必须把被测实现
# `kis_fill_interval_map.cpp` 一起编进同一条命令行。
# **这是 harness 的形状差异，不是对测试源/实现源的改动** —— 那个 .cpp 是直接从
# 源树里按原路径编的，连副本都没做。
#
# ── pk/test/graft/ 是只读依赖 ────────────────────────────────
# 本脚本**调用**它的 rename.sed 与 pk_test_moc.py，不修改它们。改名规则表只有
# 一份（D-23），两条试接链路共用。
set -eu
cd "$(dirname "$0")/../../../.." || exit 1     # pk/container/tests/graft → fork 仓库根

BUILD=pk/container/build/graft
SED=pk/test/graft/rename.sed
STUBS=pk/container/tests/graft/stubs
CXX=${CXX:-g++}
rc=0

# 两个静态库必须已经建好（cmake --build pk/container/build 的产物）。
for lib in pk/container/build/libpkcontainer.a pk/container/build/libpktest.a; do
    if [ ! -f "$lib" ]; then
        printf '  %s 不存在，先构建：cmake --build pk/container/build\n' "$lib" >&2
        exit 1
    fi
done

run_one() {
    local name="$1" srcdir="$2" hdr="$3" src="$4" implsrc="$5" incdir="$6"
    local work="$BUILD/$name"
    rm -rf "$work"; mkdir -p "$work"

    # ① 复制 —— 源树一个字节都不动
    cp "$srcdir/$hdr" "$srcdir/$src" "$work/"

    # ② D-23 机械改名，**唯一**允许对副本做的改动。改动条数打印出来存证：
    #    手工改一个字就说明 API 形状不对，而那正是判据②要抓的东西。
    sed -i -f "$SED" "$work/$hdr" "$work/$src"
    printf '  机械改名（%s）：\n' "$src"
    diff -u "$srcdir/$src" "$work/$src" | grep -E '^[+-][^+-]' | sed 's/^/    /' || true
    printf '  机械改名（%s）：%s\n' "$hdr" \
        "$(diff -q "$srcdir/$hdr" "$work/$hdr" >/dev/null && echo '无改动' || echo '见下')"
    diff -u "$srcdir/$hdr" "$work/$hdr" | grep -E '^[+-][^+-]' | sed 's/^/    /' || true

    # ③ 生成 binder（替代 moc 的测试发现）。.inc 而非 .cpp：产物全是类内定义
    #    （隐式 inline），只能被 #include，不能作为独立翻译单元编译。
    python3 pk/test/pk_test_moc.py "$work/$hdr" -o "$work/binder.inc"

    # ③.5 driver.cpp：PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须在同一
    #     翻译单元里看见它的完整定义。真实测试的 .cpp 只允许机械改名、不能往里
    #     加 #include —— 这层粘合只能由 graft 自己拥有的 driver.cpp 来做。
    printf '#include "%s"\n#include "binder.inc"\n' "$src" > "$work/driver.cpp"

    # ④ 编译链接。
    #    -DPK_TEST_NO_QT_MACRO_ALIASES 关掉 compat 里的 QCOMPARE 别名，让 sed
    #    漏改的地方**编译期就炸** —— 那正是 D-23 想要的效果，不是绕过。
    #
    #    -I 顺序即解析顺序，每一条都对应一类 include：
    #      $STUBS            ← boost/operators.hpp · kritaimage_export.h ·
    #                          kis_global.h · kis_assert.h · kis_debug.h ·
    #                          QScopedPointer（试接脚手架，不是交付件）
    #      pk/container/compat ← <QMap> <QHash> <QStack> <QVector> <QList> ...
    #      pk/container        ← 上面那些垫片 #include 的 Pk*.h 本体
    #      pk/string           ← PkStringHash.h → PkString.h（<QHash>/<QSet> 复刻
    #                            Qt 的传递性带进来的；**不需要链 pkstring**，
    #                            qHash(PkString) 是 inline 且本试接零实例化）
    #      pk/test, pk/test/compat ← PK_* harness 与 <QObject>/<QTest>/<simpletest.h>
    #      $incdir, $srcdir, $work ← 调用点自己的 include 根
    "$CXX" -std=c++17 -DPK_TEST_NO_QT_MACRO_ALIASES \
        -I "$STUBS" \
        -I pk/container/compat -I pk/container -I pk/string \
        -I pk/test -I pk/test/compat \
        -I "$incdir" -I "$srcdir" -I "$work" \
        "$work/driver.cpp" "$implsrc" \
        pk/container/build/libpkcontainer.a \
        pk/container/build/libpktest.a \
        -o "$work/$name" 2>"$work/compile.log" || {
            printf '  试接编译失败: %s\n' "$name"
            sed 's/^/    /' "$work/compile.log" | head -60
            rc=1
            return
        }

    # ⑤ 跑
    if "./$work/$name" >"$work/run.log" 2>&1; then
        printf '  试接跑绿: %s (%s + %s)\n' "$name" "$srcdir/$src" "$implsrc"
        sed 's/^/    /' "$work/run.log"
    else
        printf '  试接跑挂: %s\n' "$name"
        sed 's/^/    /' "$work/run.log" | head -40
        rc=1
        return
    fi

    # ⑥ 判据③：产物不得有 Qt 未定义符号。
    #    对静态链接的可执行文件这条断言接近恒真（链接行里没有任何 Qt 库，真出现
    #    未定义的 Qt 符号会在链接期就失败）。真正有判别力的是对 libpkcontainer.a
    #    的那条 —— 静态库允许留未定义符号，那里查得出来。
    local undef
    undef=$(nm -u "$work/$name" 2>/dev/null | grep -i qt || true)
    if [ -n "$undef" ]; then
        printf '  试接产物含 Qt 符号: %s\n%s\n' "$name" "$undef"
        rc=1
    else
        printf '    nm -u %s | grep -i qt: 无输出\n' "$name"
    fi
}

run_one KisFillIntervalMapTest \
        libs/image/tests \
        kis_fill_interval_map_test.h \
        kis_fill_interval_map_test.cpp \
        libs/image/floodfill/kis_fill_interval_map.cpp \
        libs/image

# 源树零改动自证。判据②的字面要求，也是"手工改了源就说明 API 形状不对"这条
# 判别力的来源 —— 没有它，试接跑绿说明不了任何事。
if ! git diff --quiet -- libs/image; then
    printf '  源树被改动了 —— 试接必须零改动\n' >&2
    git diff --stat -- libs/image >&2
    rc=1
else
    printf '  源树零改动自证: git diff --quiet -- libs/image 通过\n'
fi

exit "$rc"
