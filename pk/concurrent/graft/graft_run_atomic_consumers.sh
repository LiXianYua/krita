#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")/../../.."

work=pk/concurrent/graft/build/atomic_consumers
mkdir -p "$work"
cxx=${CXX:-g++}
common=(-std=c++17 -pthread -Ipk/concurrent -Ipk/concurrent/compat -Ilibs/global -Ilibs/image/3rdparty/lock_free_map)

"$cxx" "${common[@]}" -Ipk/concurrent/graft/stubs pk/concurrent/graft/atomic_kis_shared_ptr.cpp -o "$work/kis_shared_ptr"
"$cxx" "${common[@]}" -include pk/concurrent/compat/QAtomicInt pk/concurrent/graft/atomic_qsbr.cpp -o "$work/qsbr"
"$cxx" "${common[@]}" pk/concurrent/graft/atomic_kis_lockless_stack.cpp -o "$work/kis_lockless_stack"

for proof in kis_shared_ptr qsbr kis_lockless_stack; do
    "$work/$proof"
    if nm -uC "$work/$proof" | grep -i qt; then
        echo "Qt symbol found in $proof" >&2
        exit 1
    fi
    echo "PASS atomic consumer: $proof"
done
