#pragma once
// 顶掉 libs/global/config-memory-leak-tracker.h.cmake（CMake configure_file
// 的产物，本 worktree 里没有真实生成过）。跟同目录 config-debug.h 同一个手法：
// 故意留空、不定义 HAVE_MEMORY_LEAK_TRACKER。
//
// kis_memory_leak_tracker.h 自己会在 #ifndef Q_OS_LINUX / #ifdef NDEBUG 两个
// 分支里 #undef 它——对一个从没 #define 过的宏 #undef 是合法的空操作，不影响
// 结果：宏始终未定义。kis_shared_ptr.h:184-188 / :196-200 的
// `#ifndef HAVE_MEMORY_LEAK_TRACKER ... Q_UNUSED(sp); #else ...` 因此总是走
// Q_UNUSED 分支，一个 tracker 符号都不引用——kis_memory_leak_tracker.cpp（有
// QMutex/QHash/QGlobalStatic）不需要编、不需要链。
//
// 这是 graft 自己的构建期胶水，不是对 Krita 源树的改动。
