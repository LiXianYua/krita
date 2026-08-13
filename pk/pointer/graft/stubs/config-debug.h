#pragma once
// graft 自己的构建期胶水，不是对 Krita 源树的改动——顶掉
// libs/global/config-debug.h.cmake（CMake configure_file 的产物，本 worktree
// 里没有真实生成过）。置 HAVE_BACKTRACE 0，kis_debug.cpp 的 backtrace 分支
// （要 QByteArray/QLatin1String/QLatin1Char/QString::number/QString::fromLatin1，
// 全在 PkString 用量表之外）整段被预处理器跳过，kisBacktrace() 退化成返回空串。
// 抄自 pk/log/tests/graft/stubs/config-debug.h（R-08，内容逐字相同）。
#define HAVE_BACKTRACE 0
