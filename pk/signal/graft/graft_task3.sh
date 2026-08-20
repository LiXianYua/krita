#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../../.." && pwd)
out=${TMPDIR:-/tmp}/pk-r30-task3-graft
c++ -std=c++17 -pthread -I"$root/pk/concurrent" "$root/pk/concurrent/graft/timer_to_post.cpp" "$root/pk/concurrent/PkTimer.cpp" "$root/pk/concurrent/PkThreadCallQueue.cpp" "$root/pk/concurrent/PkThread.cpp" "$root/pk/concurrent/PkSemaphore.cpp" -o "$out-timer"
c++ -std=c++17 -pthread -I"$root/pk/signal" -I"$root/pk/concurrent" "$root/pk/signal/graft/delete_later.cpp" "$root/pk/signal/PkObject.cpp" "$root/pk/concurrent/PkThreadCallQueue.cpp" "$root/pk/concurrent/PkThread.cpp" "$root/pk/concurrent/PkSemaphore.cpp" -o "$out-delete"
"$out-timer"
"$out-delete"
