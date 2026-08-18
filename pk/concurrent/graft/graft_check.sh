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

check_allowed_to_fail() {
    # 与 check_pass 相同但允许失败——不是"只验证语法"（check_pass 本身也只做
    # -fsyntax-only），区别只在于失败时不设 fail=1。
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
check_allowed_to_fail libs/image/kis_updater_context.h

# R-24: 24 处 moveToThread 真实调用点试接（零改动，-fsyntax-only）。
# 这些文件依赖各自巨大的 libs/image 等边界，绝大多数预期卡在本任务未交付
# 的其他依赖上——这是诊断性试接，不是 pass/fail 判据，逐文件结论记进
# pk/concurrent/README.md。
MOVETOTHREAD_INC=(
    -include pk/concurrent/graft/stubs/QApplication
    -include pk/signal/compat/QObject
    -I pk/signal -I pk/signal/compat -I pk/concurrent
    -I libs/global -I libs/image -I libs/flake -I libs/canvas -I libs/impex
)
check_movetothread() {
    local f="$1"
    local out
    out=$("$CXX" -std=c++17 -fsyntax-only "${MOVETOTHREAD_INC[@]}" -I "$(dirname "$f")" "$f" 2>&1)
    if [ $? -eq 0 ]; then
        printf '  moveToThread graft OK（编到底，含 moveToThread 那一行）: %s\n' "$f"
    else
        local first_error
        first_error=$(printf '%s\n' "$out" | grep -m1 "error:")
        printf '  moveToThread graft 卡住: %s\n    %s\n' "$f" "$first_error"
    fi
}
for f in libs/canvas/kis_gui_context_command.cpp \
         libs/canvas/opengl/kis_texture_tile_info_pool.h \
         libs/flake/flake/kis_shape_layer.cc \
         libs/flake/flake/kis_shape_selection.cpp \
         libs/flake/KoShapeManager.cpp \
         libs/global/KisDeleteLaterWrapper.cpp \
         libs/global/kis_thread_safe_signal_compressor.cpp \
         libs/image/kis_image.cc \
         libs/image/kis_memory_statistics_server.cpp \
         libs/image/kis_node.cpp \
         libs/image/kis_processing_visitor.cpp \
         libs/image/KisSafeBlockingQueueConnectionProxy.cpp \
         libs/image/KisSafeNodeProjectionStore.cpp \
         libs/image/KisSelectionUpdateCompressor.cpp \
         libs/image/lazybrush/kis_colorize_mask.cpp \
         libs/impex/KisCloneDocumentStroke.cpp; do
    check_movetothread "$f"
done

exit $fail
