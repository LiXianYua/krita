#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../../.." && pwd)
out="${1:-$root/build-r30-task2/graft-eventloop}"

"${CXX:-c++}" -std=c++17 -pthread \
    -I"$root/pk/concurrent" \
    "$root/pk/concurrent/graft/eventloop_process_events.cpp" \
    "$root/pk/concurrent/PkEventLoop.cpp" \
    "$root/pk/concurrent/PkThreadCallQueue.cpp" \
    "$root/pk/concurrent/PkThread.cpp" \
    "$root/pk/concurrent/PkSemaphore.cpp" \
    -o "$out"
"$out"
