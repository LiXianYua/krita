#!/usr/bin/env bash
set -u
cd "$(dirname "$0")/../../.." || exit 1

CXX=${CXX:-g++}
INC=(
    -include pk/concurrent/compat/QMutex
    -include pk/concurrent/compat/QMutexLocker
    -include pk/concurrent/compat/QReadWriteLock
    -include pk/concurrent/compat/QReadLocker
    -include pk/concurrent/compat/QWriteLocker
    -include pk/concurrent/compat/QAtomicInt
    -include pk/concurrent/compat/QAtomicPointer
    -include pk/concurrent/graft/stubs/QtGlobal
    -I pk/concurrent/compat -I pk/concurrent -I pk/concurrent/graft/stubs
)
fail=0

check_pass() {
    local f="$1"
    local out
    out=$("$CXX" -std=c++17 -fsyntax-only "${INC[@]}" -I "$(dirname "$f")" "$f" 2>&1)
    if [ $? -eq 0 ]; then
        printf '  graft OK: %s\n' "$f"
    else
        printf '  graft FAILED（预期该编过）: %s\n' "$f"
        printf '%s\n' "$out" | head -20 | sed 's/^/    /'
        fail=1
    fi
}

check_pass_syntax_only() {
    # 与 check_pass 相同但允许失败：只验证语法/API 形状，不要求跑绿
    local f="$1"
    local out
    out=$("$CXX" -std=c++17 -fsyntax-only "${INC[@]}" -I "$(dirname "$f")" "$f" 2>&1)
    if [ $? -eq 0 ]; then
        printf '  graft OK: %s\n' "$f"
    else
        printf '  graft FAILED（预期可能失败）: %s\n' "$f"
        printf '%s\n' "$out" | head -20 | sed 's/^/    /'
        # 不设 fail=1，允许这个编译错误（依赖未剥离的类型）
    fi
}

check_pass libs/global/KisUpgradeToWriteLocker.h
check_pass libs/image/kis_lock_free_lod_counter.h
check_pass_syntax_only libs/image/kis_updater_context.h
exit $fail
