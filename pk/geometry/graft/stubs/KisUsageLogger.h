#pragma once
// ============================================================================
// 试接垫片 —— **不是 R-03 的交付物**。`KisUsageLogger` 归 **R-08（日志与调试设施）**。
//
// 真品 libs/global/KisUsageLogger.{h,cpp} 是一个写 usage log 文件的单例
// （QScopedPointer 私有实现 + 日志轮转 + 系统信息采集），11 个静态成员。
//
// 唯一的真实调用点（口径：两个试接目标的被测源与测试源全文）：
//   libs/global/KisRectsGrid.cpp:23
//     KisUsageLogger::log(QString("Invalid grid configuration. Grid size: %1, "
//                                 "log grid size: %2. Resetting to 64 and 6")
//                         .arg(gridSize, m_logGridSize));
// 它在构造函数的防御分支里（gridSize 不是 2 的幂时），默认构造 gridSize=64
// 走不到 —— 但**要编过、要链接过**，所以垫片必须给出真实签名 + 一份定义
// （定义在 graft_stubs.cpp）。
//
// ⚠ 这一处调用点曾压出 R-01 的一个缺口（`PkString` 缺 `arg(int,int)`，
// 原本靠已删除的 `stubs/QString` 垫一个继承子类补上）。R-13 已把这个重载
// 补进 `pk/string/PkString.h` 正式实现，缺口已关闭，`#include <QString>`
// 现在直接经 `-I` 顺序落到 `pk/string/compat/QString`（见
// pk/geometry/README.md「已关闭（R-21 T1）」）。
// ============================================================================
#include <QString>

class KisUsageLogger
{
public:
    static void log(const QString &message);
};
