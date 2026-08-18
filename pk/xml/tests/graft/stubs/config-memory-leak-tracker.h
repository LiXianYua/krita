#pragma once
// graft 自己的构建期胶水，不是对 Krita 源树的改动——顶掉
// libs/global/config-memory-leak-tracker.h.cmake（CMake configure_file 的产物，
// 本 worktree 里没有真实生成过）。故意留空、不定义 HAVE_MEMORY_LEAK_TRACKER：
// kis_shared_ptr.h 的 `#ifndef HAVE_MEMORY_LEAK_TRACKER ... Q_UNUSED(sp); #else
// ...` 因此总是走 Q_UNUSED 分支，kis_memory_leak_tracker.cpp（有 QMutex/QHash/
// QGlobalStatic）不需要编、不需要链。
// 抄自 pk/pointer/graft/stubs/config-memory-leak-tracker.h（内容逐字相同）。
