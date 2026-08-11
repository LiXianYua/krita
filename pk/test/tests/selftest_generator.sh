#!/usr/bin/env bash
# 生成器（pk_test_moc.py）的行为测试：跑生成器、断言产物文本里发现结果正确，
# 再对全仓所有测试头跑一遍做全量扫描自证。
set -eu
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

BUILD=pk/test/build/gentest
mkdir -p "$BUILD"
python3 pk/test/pk_test_moc.py pk/test/tests/generator_cases/simple.h \
        -o "$BUILD/simple_binder.cpp"

# 产物必须发现：SimpleTest 有 2 个测试函数（testAlpha/testBeta），
# testBeta 的 dataName 是 testBeta_data，initTestCase 与 cleanup 进 fixture 槽；
# NestedBracesTest 有 1 个（testGamma）；NotATest 一个都没有。
#
# 用 if/then 而不是 `grep -q X f && { ...; exit 1; }`：后者在 set -e 下，grep
# 找不到时返回 1，整条 && 语句也返回 1，会误触发 set -e 退出。
if ! grep -q '"testAlpha"' "$BUILD/simple_binder.cpp"; then
    echo "漏了 testAlpha"; exit 1
fi
if ! grep -q '"testBeta"' "$BUILD/simple_binder.cpp"; then
    echo "漏了 testBeta"; exit 1
fi
if ! grep -q '"testBeta_data"' "$BUILD/simple_binder.cpp"; then
    echo "漏了 testBeta_data"; exit 1
fi
if ! grep -q '"testGamma"' "$BUILD/simple_binder.cpp"; then
    echo "漏了 testGamma"; exit 1
fi
if grep -q 'notDiscovered' "$BUILD/simple_binder.cpp"; then
    echo "误收 notDiscovered（NotATest 没有 Q_OBJECT，不该出现在产物里）"; exit 1
fi
if grep -q 'notATestFunction' "$BUILD/simple_binder.cpp"; then
    echo "误收 notATestFunction（在 private Q_SLOTS: 块结束之后）"; exit 1
fi
if grep -q -- '->helper' "$BUILD/simple_binder.cpp"; then
    echo "误收 helper（在 public: 里，不在 private Q_SLOTS: 块）"; exit 1
fi
if grep -q 'NotATest' "$BUILD/simple_binder.cpp"; then
    echo "误收 NotATest 本身（没有 Q_OBJECT，不该生成 PkTestBinder 特化）"; exit 1
fi

echo "generator selftest OK"

# 全量扫描自证：对全仓所有测试头跑一遍生成器，统计发现结果。
# 这不是"能编过"的证明，是"扫得动、不崩、数量合理"的证明。
: >"$BUILD/scan.tsv"
: >"$BUILD/scan.err"
HEADERS=$(git ls-files '*.h' | grep -E '(^|/)(tests|benchmarks)/')
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
printf '全量扫描：%d 个测试头，发现 %d 个测试类 / %d 个测试函数，生成器失败 %d 次\n' \
       "$headerCount" "$classes" "$funcs" "$failed"
[ "$failed" -eq 0 ] || exit 1
