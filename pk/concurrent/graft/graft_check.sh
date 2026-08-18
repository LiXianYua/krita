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
    -I pk/concurrent/compat -I pk/concurrent
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

check_pass libs/global/KisUpgradeToWriteLocker.h
exit $fail
