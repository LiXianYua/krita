#pragma once
// 顶掉 libs/global/config-debug.h.cmake（CMake configure_file 的产物，本
// worktree 里没有真实生成过）。kis_debug.cpp 的 backtrace 分支要
// QByteArray/QLatin1String/QLatin1Char/QString::number/QString::fromLatin1，
// 全都在 PkString 的用量表之外。置 0 之后 HAVE_BACKTRACE 分支整段消失
// （预处理器直接跳过，不需要真的实现那些符号），kisBacktrace() 退化成
// "返回空串"，__methodName 与 21 个 Q_LOGGING_CATEGORY 原样编过。
//
// 这是 graft 自己的构建期胶水，不是对 Krita 源树的改动。内容与
// pk/log/tests/graft/stubs/config-debug.h 逐字一致（同一族脚手架，压的是
// 同一段 kis_debug.cpp，各 graft 任务各自持有一份自包含副本）。
#define HAVE_BACKTRACE 0
