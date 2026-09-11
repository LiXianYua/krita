#pragma once

// ---------------------------------------------------------------------------
// PK_TYPE_VISIBILITY —— 让一个 pk 值类型的 **RTTI 身份跨镜像可比较**。
//
// ---- 为什么需要它（S-17 实测，2026-09-11）----
//
// `PkVariant` 把非 POD 负载存进 `std::any m_any`，读出来靠 `std::any_cast`
// （`pk/variant/PkVariant.cpp`、`PkVariantImpl.h`）。`any_cast` 判「这个 any 里
// 装的是不是 T」用的是 `std::type_info::operator==`。而 `std::type_info` 的相等
// **在 arm64 Apple 上没有「按类型名比较」的默认保障**：见下。
//
// `pk` 的库全是 STATIC，而 `-fvisibility=hidden` 是仓库级的（ECM），于是
// `typeid(PkString)` 在「测试可执行文件」与「libkritaglobal.dylib」里各有一份。
// **两份不是问题，「两份被判成不同类型」才是问题。**
//
// ---- 机制（libc++ `NonUniqueARMRTTIBit`，`typeinfo` 头原文）----
//
// Apple arm64 上 libc++ 用的是 `_LIBCPP_TYPEINFO_COMPARISON_IMPLEMENTATION == 3`：
//
//   「若一个类型的 RTTI 本会以默认可见性发射成 weak 符号，它会被改成 hidden，
//     并且**置上 non-unique 位**。否则，声明为 hidden 可见性的类型**一律被认为
//     拥有 unique RTTI**……跨已链接镜像的边界，这类类型因此被视为不同类型。」
//
//   `__eq`：地址相等 → 相等；**任一方是 unique → 不相等**；
//           两方都 non-unique → `strcmp(类型名) == 0`。
//
// 所以：`-fvisibility=hidden` 下 `PkString` 的 RTTI 被标成 **unique**，两个镜像
// 各判各的 ⇒ 地址不等 ⇒ **不相等** ⇒ `any_cast` 认不出跨镜像造的 `PkVariant`：
//   * 指针形 `any_cast<T>(&any)` 返回 `nullptr` ⇒ `*nullptr` ⇒ 段错
//   * 引用形 `any_cast<T&>(any)` 抛 `std::bad_any_cast`
//
// `type_visibility("default")` 正是那个「本会默认可见」的开关：它把该类型的 RTTI
// 从 unique 翻成 **non-unique**，于是 `__eq` 走 `strcmp` 分支，跨镜像判等成功。
//
// 实测（本机，arm64 macOS，clang 21）：加属性前后两份 `type_info` 仍各自是局部
// 符号（`nm -a` 全是 `s`），`&typeid` 地址**依旧不同**，但跨镜像 `any_cast`
// 由 `nullptr` 变成成功——即走的是 `strcmp` 分支，不是符号被合并。
//
// ---- 平台差异（决定本宏在各平台上是不是 no-op）----
//
// | 平台 | libc++ 实现 | 本宏 |
// |---|---|---|
// | Apple arm64（本机） | 3 `NonUniqueARMRTTIBit` | **有效，就是它在起作用** |
// | Apple x86_64 | 1 `Unique`（**只比地址，没有名字回退**） | **无效** |
// | Linux / GCC（libstdc++） | —— | 展开为空（GCC 无 `type_visibility`） |
// | Linux / clang（libstdc++） | —— | 属性会发出，但 libstdc++ 的 `operator==` 本来就带 `strcmp` 回退，语义上是 no-op |
//
// **Apple x86_64 那一格要记住**：impl 1 根本没有名字回退，所以「一个进程里两份
// pk」这件事在那条路上**没有可见性层面的解法**——只能靠「pk 全进程只有一份」或
// 「`PkVariant` 不再用 `any_cast` 做类型判等」。别以为挂了本宏就到处都能用。
//
// **Linux 那两行是读两个标准库头文件的实现推出来的，没有在本机复现**
// （本机没有 Linux 工具链）。唯一的旁证是 M0 基线在 Ubuntu/GCC 上录到
// `KoPropertiesTest` = `Passed`（`.exec/baseline/env.json` 的 `compiler` 写着
// `c++ (Ubuntu 13.3.0-…24.04.1)`）——同一份源码在 Linux 上不炸。
//
// ---- 为什么用 type_visibility 而不是 visibility ----
//
// `visibility("default")` 会把**成员函数符号**一起导出，等于把 pk 的 API 面掀开；
// 这里要的只是「类型身份」。`type_visibility` 只作用于 **RTTI / vtable**。
// 实测（`nm -m` 对比同一 TU 编译前后）：成员函数符号表逐条相同，导出符号数
// 64 → 64；对象布局也不变（`sizeof`：PkString 16、PkStringList 16、PkByteArray 24）。
//
// ---- 用在哪 ----
//
// **凡是可以当 `PkVariant::m_any` 负载的 pk 类，都要挂这个宏。**
// `PkVariantList`/`PkVariantHash`/`PkVariantMap` 是 `std::vector<PkVariant>` /
// `std::map` / `std::unordered_map` 的 **typedef**（`pk/variant/PkVariant.h:46-48`），
// **没有 pk 类可挂**——那三个只能靠 `pk/variant` 侧改设计。
//
// ⚠ 这套修法**没有机器防线**：新增一个能当负载的类型而忘了挂宏，就会静默重现
// 同类段错，而现有测试全绿（`libs/global` 的测试只走 `PkString` 负载，跨镜像
// 边界不经过 geometry/time/集合那几类）。补挂时请同时补一条**跨镜像 round-trip
// 测试**——那才是防线。
// ---------------------------------------------------------------------------

#if defined(__clang__) && __has_attribute(type_visibility)
#  define PK_TYPE_VISIBILITY __attribute__((type_visibility("default")))
#else
#  define PK_TYPE_VISIBILITY
#endif
