# pk/config —— Q-6（配置）+ Q-7（MIME 静态表）零 Qt 替代

本目录交付两个独立能力：

- **`PkConfigGroup` / `PkSharedConfig` / `PkConfigStore`**
  （Q-6）—— `KConfigGroup` / `KSharedConfig` 的零 Qt 替代。颜色读写接
  `pk/color` 的 `PkColor`（QColor 替代，R-27 Task 1 交付）。
- **`PkMimeDatabase`**（Q-7）—— `libs/koplugin/KisMimeDatabase` 的零 Qt 替代，
  37 条硬编码 MIME 表。

## 1. 跨进程持久化契约

`PkConfigStore` 在首次使用时读取平台 `GenericConfig/kritarc`，与
`KoResourcePaths` 的已有资源目录配置共用同一文件：

- Linux/Unix：`$XDG_CONFIG_HOME/kritarc`，未设时为 `$HOME/.config/kritarc`；
- Windows：`%APPDATA%/kritarc`（缺失时回退用户目录）；
- macOS：`$HOME/Library/Preferences/kritarc`；
- Android：`$ANDROID_APP_DATA/kritarc`，然后按 `HOME`/`TMPDIR` 已有规则回退。
- Haiku：用户 settings 目录下的 `kritarc`。

`writeEntry`/`deleteEntry`/`deleteGroup` 先更新线程安全的进程内视图，并记录
顺序变更。`sync()` 在 `kritarc.lock` 的跨进程排他锁内重读最新文件、
合并本进程变更，写入 `kritarc.tmp.<pid>.<sequence>`，刷新文件后原子
替换目标，并在 POSIX 上刷新父目录。进程生命期单例的析构函数会
再做一次 RAII `sync()`，因此未显式调用 `sync()` 的现有选区/SVG 设置也会
在正常进程退出时持久化。

同步失败时不替换旧文件，不丢弃待写变更，进程内读仍返回已写值；
`PkConfigStore::sync()` 返回 `false` 供有错误通道的上层处理，兼容的
`PkConfigGroup::sync()` 保持 `void` API。已有 Pk 专属段损坏、重复或超过 16 MiB 时
按失败关闭处理：读取返回调用方 default，同步拒绝覆写原文件。

## 2. 序列化格式

底层文件框架为可共存的版本化 INI 段：每个 group 写成
`[PkConfig-v1:<group-utf8-hex>]`，每个条目写成
`<key-utf8-hex>=<value-utf8-hex>`。十六进制为小写 ASCII、两个字符表示一个
UTF-8 字节；空 group/key/value 编码为空字段。这使换行、`=`、`[]`和任意
Unicode 都可无歧义往返，且不会与现有 KConfig 段碰撞。非
`PkConfig-v1` 段和根级行在重写时保留。

一旦持久化落地，下面这些格式就会变成实际写到磁盘上的数据，改格式即改变格式
版本，需要谨慎：

| 类型 | 格式 |
|---|---|
| `bool` | 字面量 `"true"` / `"false"` |
| `int` | `std::to_string` 十进制 |
| `double` | `snprintf("%.17g", ...)`——17 位十进制有效数字，保证任意 IEEE754 double 精确往返（`std::to_string` 固定 6 位小数不是往返安全的：小量会截成 `"0.000000"`，与真的存 0 无法区分，见 `PkConfigGroup.cpp` 里 `formatDouble` 的注释） |
| `PkColor` | `"r,g,b,a"` 十进制逗号分隔，每段范围 `[0,255]`，越界（如 `"300,-5,0,255"`）视为格式错误、退回 `defaultValue`，不做环绕/截断 |
| `PkPoint` | `"x,y"` 十进制逗号分隔 |
| `PkStringList` | `'\x1f'`（ASCII Unit Separator）拼接，元素本身几乎不可能包含它，所以不用逗号（会跟元素内容冲突） |

## 3. 颜色类型：`PkConfigColor` 已退役，改接 `PkColor`

`PkConfigGroup::readEntry/writeEntry(..., PkColor)` 的 `(r,g,b,a)` 值类型是
`pk/color` 的 `PkColor`（QColor 替代，R-27 Task 1 交付）。R-09 时代的临时
范围内代打 `PkConfigColor`（一个 4×`uint8_t` 元组，分量越界被
`static_cast<uint8_t>` 悄悄截断）已删除。

语义变化一处：`PkColor` 越界输入构造出**无效色**（不是截断），但
`readEntry` 对越界分量的处置不变——显式判 `[0,255]`、越界视为格式错误、退回
`defaultValue`（见 `PkConfigGroup.cpp` `readEntry` 的注释），与「段数不对」
的兜底行为一致。

## 4. 已知限制

- `PkConfigStore` 是单个进程全局视图，所有 map/变更日志访问由同一把
  mutex 保护；不模拟 KDE 的每线程独立句柄。跨进程冲突以锁内重读+顺序
  变更合并解决，同时修改同一 key 时后取得锁的写入者获胜。
- **`PkStringList` 往返 `{""}`（单个空字符串元素）会退化成空列表**：这是
  `'\x1f'` 扁平分隔编码的固有行为（空字符串 join 出来的结果和"没有元素"在
  分隔符层面无法区分），不是 bug。低风险：实测的真实调用点都是插件 ID 黑
  名单，从不存这种元素。

## 5. Q-6 API 覆盖（本轮修复新增）

最终整分支评审发现 89 处保留调用点用到了原 API 覆盖不到的 4 种真实调用形式，
本轮已经补齐：

- **`readEntry<T>(key, def)` / `writeEntry<T>(key, value)` 显式模板实参形式**
  ——`PkConfigGroup.h` 新增两个成员模板，转发到既有的非模板重载。对于参数
  推导能精确匹配某个非模板重载的调用（包括不写 `<T>` 的既有调用形式），重载
  决议按标准规则优先选非模板精确匹配，两个模板不会被选中；只有显式写 `<T>`，
  或非模板重载因参数类型需要转换而不再是"唯一最佳"时（`quint32` 等小整型
  写入的三路 ambiguous 场景），才会走到模板。
- **`PkConfigGroup(config, "Group")` 两参构造函数**——真实调用点是
  `KConfigGroup cfg(KSharedConfig::openConfig(), "Group")`，不经过
  `.group(name)`。`config` 形参类型是 `PkSharedConfig*`，只做类型检查，不
  参与存储路径（底层永远是 `PkConfigStore` 的全局单例）。
- **`readEntry(key, const char*)` 精确匹配重载**——修掉一个静默类型错绑：
  没有这个重载时，`readEntry("k", "字面量")` 会靠标准布尔转换悄悄绑定到
  `bool` 重载，编译器不报错，调用方以为拿到字符串实际拿到的是 `bool`。
- **`PkSharedConfig::Ptr` / `PkSharedConfigPtr` 别名**——两者都是
  `PkSharedConfig*` 的裸指针别名，不是引用计数智能指针：`openConfig()` 本来
  就返回进程生命周期单例的裸指针，实测调用点没有一处对它做手动生命周期管理，
  裸指针别名与这个既有设计是同一件事。`compat/KSharedConfig` /
  `compat/ksharedconfig.h` 各自补了 `#define KSharedConfigPtr
  PkSharedConfigPtr`——`#define KSharedConfig PkSharedConfig` 只重写单个
  token，覆盖不到 `KSharedConfigPtr` 这个不同的自由 typedef 拼写。

## 6. 目录结构

```
pk/config/
├── PkConfigStore.{h,cpp}     进程内单例，group→key→value 两级字符串存储
├── PkConfigGroup.{h,cpp}     KConfigGroup 替代，类型化读写 + 编解码
│                             （颜色类型用 ../color/PkColor，见 §3）
├── PkSharedConfig.{h,cpp}    KSharedConfig 替代，openConfig() 单例 + group() 工厂
├── PkMimeDatabase.{h,cpp}    KisMimeDatabase 替代，37 条硬编码 MIME 表
├── compat/                   #include <KConfigGroup> 等零改动垫片
└── tests/
    ├── test_config_group.{h,cpp}   Q-6 自测
    ├── test_mime_database.{h,cpp}  Q-7 自测（37/37 条表逐条核对）
    └── graft/
        ├── graft_run.sh       Task 2：libs/command/KisCumulativeUndoData.cpp 零改动编译试接，跑绿
        ├── graft_check.sh     Task 4：Q-7 的 18 个真实消费者零改动编译试接，全 EXPECT_FAIL（逐条登记卡在哪个未交付类型上）
        ├── shim_check.sh      I-2：compat/KisMimeDatabase.h 垫片本身的端到端验证（真实消费者会走的路径此前从未被覆盖到）
        ├── shim_probe.cpp     shim_check.sh 用的探针
        └── compat_shims_probe.cpp   M-3：另外 3 个此前从未编译过的垫片（KConfigGroup/KSharedConfig/ksharedconfig.h）的存在性检查
```
