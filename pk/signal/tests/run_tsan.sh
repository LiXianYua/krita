#!/usr/bin/env bash
# final whole-branch review 顺手加（Recommendation）：固化 pk/signal 的 TSan
# 跑法——这是 C-1（PkThreadCallQueue 线程 id 复用）、C-2（postBlocking 异常
# 挂起）、I-1（PkObject::m_thread 跨线程读写数据竞争）三处修复最该用来验证
# 的手段。
#
# 用 -fsanitize=thread 在一个独立的临时目录里重新构建 pk/signal（同时把
# pk/concurrent 一起编进去，因为 pksignal 链接 pkconcurrent），跑
# test_pksignal，TSan 报错则脚本非零退出。
#
# 构建产物落在 /tmp 下的临时目录，不落进仓库——不需要碰 .gitignore（若改用
# 仓库内目录，bare "build" 这条既有规则只匹配名字恰好是 "build" 的目录，
# 不会匹配 "build-tsan" 这类名字，需要额外登记；用 /tmp 直接绕开这个问题）。
# 默认跑完清理，传 --keep 保留（保留时会打印路径）。
#
# 本机环境实测：TSan 运行时需要 `setarch -R`（关闭 ASLR），否则报
# "FATAL: ThreadSanitizer: unexpected memory mapping"（exit=66）。脚本对
# `setarch -R true` 做一次探测，能用就用，不能用就直接跑（不同机器可能不需要）。

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PK_SIGNAL_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

KEEP=0
if [[ "${1:-}" == "--keep" ]]; then
    KEEP=1
fi

BUILD_DIR="$(mktemp -d /tmp/pksignal-tsan.XXXXXX)"

cleanup() {
    if [[ "${KEEP}" -eq 1 ]]; then
        echo "TSan 构建目录已保留：${BUILD_DIR}"
    else
        rm -rf "${BUILD_DIR}"
    fi
}
trap cleanup EXIT

echo "== TSan 构建目录：${BUILD_DIR} =="

if ! command -v ccache >/dev/null 2>&1; then
    LAUNCHER_ARGS=()
else
    LAUNCHER_ARGS=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
fi

echo "== cmake 配置（-fsanitize=thread） =="
if ! cmake -S "${PK_SIGNAL_DIR}" -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" \
    "${LAUNCHER_ARGS[@]}" \
    > "${BUILD_DIR}/configure.log" 2>&1; then
    echo "cmake 配置失败，日志："
    cat "${BUILD_DIR}/configure.log"
    exit 1
fi

echo "== 构建 =="
if ! cmake --build "${BUILD_DIR}" > "${BUILD_DIR}/build.log" 2>&1; then
    echo "构建失败，日志尾部："
    tail -80 "${BUILD_DIR}/build.log"
    exit 1
fi

TEST_BIN="${BUILD_DIR}/test_pksignal"
if [[ ! -x "${TEST_BIN}" ]]; then
    echo "构建产物 ${TEST_BIN} 不存在"
    exit 1
fi

echo "== 探测本机是否需要 setarch -R（关闭 ASLR）=="
RUN_PREFIX=()
if setarch -R true >/dev/null 2>&1; then
    RUN_PREFIX=(setarch -R)
    echo "本机需要 setarch -R，已启用"
else
    echo "本机不需要 setarch -R（或该命令不可用），直接运行"
fi

echo "== 跑 test_pksignal（TSAN_OPTIONS=halt_on_error=0，尽量报出全部警告）=="
TSAN_OUTPUT="$(TSAN_OPTIONS="halt_on_error=0" "${RUN_PREFIX[@]}" "${TEST_BIN}" 2>&1)"
STATUS=$?

echo "${TSAN_OUTPUT}"

if echo "${TSAN_OUTPUT}" | grep -q "ThreadSanitizer"; then
    echo "== TSan 报错，见上方输出 =="
    exit 1
fi
if [[ "${STATUS}" -ne 0 ]]; then
    echo "== test_pksignal 非零退出（exit=${STATUS}），可能是测试本身失败 =="
    exit 1
fi

echo "== TSan 干净，test_pksignal 全过 =="
exit 0
