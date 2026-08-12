#pragma once
#include <string>

#include "PkLogLevel.h"

// 建/取一个具名 spdlog logger 并设最低级别。幂等：同名 logger 已存在时只
// set_level，不重建（避免打断已有的 sink 配置）。
void PkLogEnsureLogger(const char *categoryName, PkLogLevel minLevel);

// 记一条日志：先分发给全部已注册 sink（不受 spdlog 的 minLevel 过滤影响，
// Flutter 侧订阅要看见每一条），再交给同名 spdlog logger（受 minLevel 过滤）。
// level == PkLogFatal 时落盘之后 std::abort()——对应 qFatal 的"终止进程"语义，
// 45 个调用点依赖它不返回。
void PkLogEmit(PkLogLevel level, const PkLogContext &ctx, const std::string &message);
