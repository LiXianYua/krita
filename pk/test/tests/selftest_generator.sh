#!/usr/bin/env bash
# 生成器（pk_test_moc.py）的行为测试：跑生成器、断言产物文本里发现结果正确，
# 再对全仓所有测试头跑一遍做全量扫描自证。
set -eu
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

BUILD=pk/test/build/gentest
mkdir -p "$BUILD"
python3 pk/test/pk_test_moc.py pk/test/tests/generator_cases/simple.h \
        -o "$BUILD/simple_binder.inc"

OUT="$BUILD/simple_binder.inc"

# 产物必须发现：SimpleTest 有 2 个测试函数（testAlpha/testBeta），
# testBeta 的 dataName 是 testBeta_data，initTestCase/initTestCase_data/cleanup
# 进 fixture 槽；NestedBracesTest 有 1 个（testGamma）；SignalBoundaryTest 有
# 1 个（realTest）；NotATest 一个都没有。
#
# 用 if/then 而不是 `grep -q X f && { ...; exit 1; }`：后者在 set -e 下，grep
# 找不到时返回 1，整条 && 语句也返回 1，会误触发 set -e 退出。
# 含 `-`/`{`/`(` 等 regex 特殊字符的 pattern 一律 `grep -qF --`（按字面串找，
# 不当正则解析），避免误报或语法错误。
if ! grep -q '"testAlpha"' "$OUT"; then
    echo "漏了 testAlpha"; exit 1
fi
if ! grep -q '"testBeta"' "$OUT"; then
    echo "漏了 testBeta"; exit 1
fi
if ! grep -q '"testBeta_data"' "$OUT"; then
    echo "漏了 testBeta_data"; exit 1
fi
if ! grep -q '"testGamma"' "$OUT"; then
    echo "漏了 testGamma"; exit 1
fi
if grep -q 'notDiscovered' "$OUT"; then
    echo "误收 notDiscovered（NotATest 没有 Q_OBJECT，不该出现在产物里）"; exit 1
fi
if grep -q 'notATestFunction' "$OUT"; then
    echo "误收 notATestFunction（在 private Q_SLOTS: 块结束之后）"; exit 1
fi
if grep -qF -- '->helper' "$OUT"; then
    echo "误收 helper（在 public: 里，不在 private Q_SLOTS: 块）"; exit 1
fi
if grep -q 'NotATest' "$OUT"; then
    echo "误收 NotATest 本身（没有 Q_OBJECT，不该生成 PkTestBinder 特化）"; exit 1
fi

# ---- 注释剥离：块注释 /* */ 与行尾/整行 // 注释里的 void foo(); 不能被误收。
# simple.h 的 private Q_SLOTS: 段里塞了 commentedOutBlock（/* */ 包住）与
# commentedOutLine（// 整行注释）——两个名字都不该出现在产物里。
if grep -qF -- 'commentedOutBlock' "$OUT"; then
    echo "误收 commentedOutBlock（在块注释 /* */ 里，说明注释没被剥掉）"; exit 1
fi
if grep -qF -- 'commentedOutLine' "$OUT"; then
    echo "误收 commentedOutLine（在行注释 // 里，说明注释没被剥掉）"; exit 1
fi
# 剥注释必须是单趟扫描：行注释里出现的 `/*` 不是块注释起点。两条互不知情的
# 正则先后 sub 时，`// 见 /* ...` 的 `/*` 会被当块起点一路吃到下一个 `*/`，
# 把中间的 testAfterBlockOpenInLineComment 静默吞掉。
if ! grep -q '"testAfterBlockOpenInLineComment"' "$OUT"; then
    echo "漏了 testAfterBlockOpenInLineComment（行注释里的 /* 被误当成块注释起点）"; exit 1
fi
if ! grep -q '"testAfterRealBlockComment"' "$OUT"; then
    echo "漏了 testAfterRealBlockComment"; exit 1
fi

# ---- 裸 Q_SIGNALS:（不带访问关键字）必须被当成块边界，不是槽的延续。
# SignalBoundaryTest：private Q_SLOTS: void realTest(); / Q_SIGNALS: void changed();
# / private: void notATest();——只有 realTest 该被收。
if ! grep -q '"realTest"' "$OUT"; then
    echo "漏了 realTest"; exit 1
fi
if grep -qF -- '->changed' "$OUT"; then
    echo "误收 changed（裸 Q_SIGNALS: 没被当成块边界，信号被当槽收了）"; exit 1
fi
if grep -qF -- '->notATest' "$OUT"; then
    echo "误收 notATest（在 Q_SIGNALS: 之后的 private: 里）"; exit 1
fi

# ---- initTestCase_data 必须精确匹配进 initTestCaseData() 这个 fixture 槽，
# 不能落进 dataFunctions() 的数据函数表（FIXTURES 集合的精确匹配要先于
# _data 后缀判断生效）。正例：initTestCaseData() 的单值访问器里出现
# `fn{"initTestCase_data"`；反例：dataFunctions() 的数组项（该行第一个
# 非空白字符就是 `{`）里不能出现它。
if ! grep -qF -- 'fn{"initTestCase_data"' "$OUT"; then
    echo "initTestCase_data 没有正确归进 fixture 槽（initTestCaseData 未生成访问器）"; exit 1
fi
if grep -qE '^ *\{"initTestCase_data"' "$OUT"; then
    echo "initTestCase_data 被误收进了数据函数表（dataFunctions 的列表项）"; exit 1
fi

echo "generator selftest OK"

# 全量扫描自证：对全仓所有测试头跑一遍生成器，统计发现结果。
# 这不是"能编过"的证明，是"扫得动、不崩、数量合理"的证明。
#
# 排除 pk/ 自己的路径：pk/test/tests/generator_cases/*.h（本任务的生成器自测
# 输入）与 pk/string/tests/**（另一个 R 线任务的测试头）路径里也含 tests/，
# 会被同一条 `git ls-files` 扫进去——但它们不是 Krita 的真实测试面，混进来
# 会让这组数字对不上"扫的是 Krita 测试头"这个判据的本意。
: >"$BUILD/scan.tsv"
: >"$BUILD/scan.err"
HEADERS=$(git ls-files '*.h' | grep -E '(^|/)(tests|benchmarks)/' | grep -v '^pk/')
failed=0
for h in $HEADERS; do
    if ! python3 pk/test/pk_test_moc.py "$h" -o /dev/null --stats >>"$BUILD/scan.tsv" 2>>"$BUILD/scan.err"; then
        failed=$((failed + 1))
        printf '  生成器在 %s 上失败\n' "$h" >&2
    fi
done
headerCount=$(echo "$HEADERS" | wc -l)
classes=$(awk -F'\t' '{c+=$2} END{print c+0}' "$BUILD/scan.tsv")
funcs=$(awk -F'\t'   '{f+=$3} END{print f+0}' "$BUILD/scan.tsv")
printf '全量扫描（已排除 pk/ 自身）：%d 个测试头，发现 %d 个测试类 / %d 个测试函数，生成器失败 %d 次\n' \
       "$headerCount" "$classes" "$funcs" "$failed"
[ "$failed" -eq 0 ] || exit 1

# 只打数字不判定的自证等于没自证：把生成器改成对 libs/ 下的头一律返回 0 个类，
# 上面那行照样打印、照样 exit 0。下限取 D-30 记的 341 类 / 2168 函数（实测值更
# 高：见 pk/test/README.md §4），低于就是出事了——生成器退化、或 Krita 侧真的
# 少了测试，两种都该拦下；正常波动在下限之上，不会误报。
MIN_CLASSES=341
MIN_FUNCS=2168
if [ "$classes" -lt "$MIN_CLASSES" ] || [ "$funcs" -lt "$MIN_FUNCS" ]; then
    printf '全量扫描低于下限：实测 %d 类 / %d 函数，下限 %d 类 / %d 函数（D-30 的口径）。\n' \
           "$classes" "$funcs" "$MIN_CLASSES" "$MIN_FUNCS" >&2
    printf '要么生成器退化了（该发现的类/函数没发现），要么 Krita 侧测试面真的变小了——先查清是哪一种。\n' >&2
    exit 1
fi
