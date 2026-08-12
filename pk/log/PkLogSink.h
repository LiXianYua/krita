#pragma once
#include "PkLogLevel.h"

// (分类, 级别, 正文, 上下文) 的旁路订阅者——Flutter 侧接收日志走这条通道，
// 不经过 spdlog（PkLogEmit 先派发给 sink 再喂 spdlog，语义钉在
// tests/test_sink.cpp 的两个用例里：removed sink 立刻停止接收）。
using PkLogSinkFn = void (*)(PkLogLevel level, const PkLogContext &ctx,
                             const char *message, void *userData);

// 注册一个 sink，返回句柄（单调递增，>=1）。线程安全。
int PkLogAddSink(PkLogSinkFn fn, void *userData);

// 按句柄注销。句柄不存在（已注销或从未存在）时是空操作。线程安全。
void PkLogRemoveSink(int handle);

// 内部管道：把一条日志分发给全部已注册 sink。只供 PkLogBackend.cpp 调用——
// 不是给业务代码用的公开接口，但注册表是这个翻译单元私有的，只能声明在这。
void PkLogDispatchToSinks(PkLogLevel level, const PkLogContext &ctx, const char *message);
