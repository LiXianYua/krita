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
// 现场枚举出的负载类型全集（17 个）：
//   * S-17 已挂：`PkString` `PkStringList` `PkByteArray`；
//   * R-53 挂：`PkPoint` `PkPointF` `PkRect` `PkRectF` `PkSize` `PkSizeF`
//     `PkLine` `PkLineF`（`pk/geometry`）+ `PkDate` `PkTime` `PkDateTime`（`pk/time`）；
//   * R-53 裁决 A 挂：**`PkVariant` 自己**（`pk/variant/PkVariant.h`，
//     `class PkVariant` —— 它自己也是那些 std 容器负载的元素类型，见下）。
//
// `PkVariantList` / `PkVariantHash` / `PkVariantMap` 分别是
// `std::vector<PkVariant>` / `std::unordered_map<PkString, PkVariant>` /
// `std::map<PkString, PkVariant>` 的 **typedef**（`pk/variant/PkVariant.h:46-48`）。
// 这里原先写着「那三个只能靠 `pk/variant` 侧改设计」——**那句现在假了**：R-53 实测
// 发现属性会**透过模板实参传播**（见下），于是按裁决 A **给 `class PkVariant` 挂
// 这一行属性**，三个 typedef 就一起翻开了，**不需要改设计**。原因正是下面这条：
//
// ### 传播规则（R-53 实测）
//
// 一个**类模板特化**的 unique 位 = 该特化**全部模板实参**（**含** `std::less<K>` /
// `std::hash<K>` / `std::allocator<…>` 这类由库推导出来的实参）各自 unique 位的
// 「**与**」——只要有一个实参是 unique，整个特化就是 unique。成员类**不**传播；
// `using` / `typedef` 别名**不产生新类型**（它的 unique 位完全由被别名化的那个
// 特化的实参决定，给别名挂属性改不了它）。
//
// **这条有一处会咬人的边界——与上面同段读，别只读一半**：既然「与」覆盖**全部**
// 实参，那么**若将来把 `PkVariantHash` 的哈希策略换成一个自身 hidden 的 `PkHasher`
// （或把 `PkVariantMap` 的比较器换成 `PkLess`），这次翻开会重新失效**。那两处的
// 实测数据在 R-53 plan 的 §7.2、复现脚本在 §7.3（本机跑过、输出逐字一致）。原因：
// `std::hash<K>` / `std::less<K>` 自身的 unique 位是**从实参 `K` 推出来的**（K 挂了
// 属性 ⇒ 它们也 non-unique）；而 `PkHasher` / `PkLess` 是**单独的类型**、自身不带
// 属性 ⇒ unique ⇒ 按「与」把整个特化拉回 unique。**要改这两个容器的哈希/比较器
// 策略的人，先读这一条。**
//
// ### 不在覆盖内：UserType 分支（开放集）
//
// `PkVariant::fromValue<T>` / `setValue<T>` 的 **UserType 分支接受任意 `T`** ——
// 那可能是**调用方自己的类型**，不是 pk 的类型、挂不了属性。它的相等走
// `m_anyEqualAccessor`（同镜像内成对使用）；**跨镜像读用户类型 = 调用方自己的
// 责任**（要么别跨镜像传，要么调用方自己给 `T` 挂本宏并保证两侧 include 同一份
// 声明）。本宏**不**覆盖这一类，也不该覆盖。
//
// ⚠ 这套修法**没有机器防线**：新增一个能当负载的类型而忘了挂宏，就会静默重现同类
// 段错，而单测全绿（`libs/global` 的测试只走 `PkString` 负载）。防线是一条**跨镜像
// round-trip 测试**，它落在 **`pk/variant/tests/test_cross_image_payload.cpp`**（配套
// `cross_image_payload.h` / `cross_image_payload.cpp` / `cross_image_case.h`，由
// `pk/variant/CMakeLists.txt` 注册 SHARED 探针 `pkcrossimage` 与 exe
// `test_pk_cross_image`）。**两个镜像怎么造**：SHARED 探针 dylib 与测试 exe **各
// 静态链一份 pk 目标码** = 真镜像边界（薄壳原本没有镜像边界，这是自建的；§1.4 的
// 目录级 `-fvisibility=hidden` 保证 dylib 不导出 pk 符号、两镜像不被合并）。它
// **自带一条判别力断言**：对每个类型断言两镜像的 `type_info.__type_name` **互异**
// （`exeRaw != libRaw`）——若两镜像被 dyld 合并成一个，槽当场翻红，而不是「全绿却
// 毫无判别力」。**新增负载类型时把 T 加进那个 X-macro 清单即可**
// （`cross_image_payload.h` 两侧共用一份，不会漂）。
// ---------------------------------------------------------------------------

#if defined(__clang__) && __has_attribute(type_visibility)
#  define PK_TYPE_VISIBILITY __attribute__((type_visibility("default")))
#else
#  define PK_TYPE_VISIBILITY
#endif
