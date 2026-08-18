# pk/config —— Q-6（配置）+ Q-7（MIME 静态表）零 Qt 替代

本目录交付两个独立能力：

- **`PkConfigGroup` / `PkSharedConfig` / `PkConfigStore` / `PkConfigColor`**
  （Q-6）—— `KConfigGroup` / `KSharedConfig` 的零 Qt 替代。
- **`PkMimeDatabase`**（Q-7）—— `libs/koplugin/KisMimeDatabase` 的零 Qt 替代，
  37 条硬编码 MIME 表。

## 1. 没有真实磁盘持久化

`PkConfigGroup::sync()` 是空操作，只保证不抛异常/不崩——数据全程活在
`PkConfigStore::instance()` 这个进程内单例的 `std::map` 里，**进程一退出就
丢**。真实 `KConfigGroup`/`KSharedConfig` 会落盘到 `~/.config/*rc`，本任务不做
这件事（Global Constraints 明确划出范围）。

后续某个 S 批次要接手真实文件 I/O：把 `PkConfigStore` 的读写路径接到
`PkResourceStorage`/`PkStream`（两者都是 R-12 的接口，见
`pk/port/README.md` §5——`PkResourceStorage`/`PkStream` 目前只有接口，没有
具体的文件/内存/zip 适配器，適配器本身也是延后到 S-01 的缺口，不是本任务
遗留的新缺口）。

## 2. 序列化格式

一旦持久化落地，下面这些格式就会变成实际写到磁盘上的数据，改格式即改变格式
版本，需要谨慎：

| 类型 | 格式 |
|---|---|
| `bool` | 字面量 `"true"` / `"false"` |
| `int` | `std::to_string` 十进制 |
| `double` | `snprintf("%.17g", ...)`——17 位十进制有效数字，保证任意 IEEE754 double 精确往返（`std::to_string` 固定 6 位小数不是往返安全的：小量会截成 `"0.000000"`，与真的存 0 无法区分，见 `PkConfigGroup.cpp` 里 `formatDouble` 的注释） |
| `PkConfigColor` | `"r,g,b,a"` 十进制逗号分隔，每段范围 `[0,255]`，越界（如 `"300,-5,0,255"`）视为格式错误、退回 `defaultValue`，不做环绕/截断 |
| `PkPoint` | `"x,y"` 十进制逗号分隔 |
| `PkStringList` | `'\x1f'`（ASCII Unit Separator）拼接，元素本身几乎不可能包含它，所以不用逗号（会跟元素内容冲突） |

## 3. `PkConfigColor` 是临时的范围内代打

`PkConfigColor` 只是 `PkConfigGroup::readEntry/writeEntry(..., PkConfigColor)`
需要的一个 `(r,g,b,a)` 元组值类型——**不是通用颜色类型**。目前没有任何 R 任务
认领 `QColor` 的完整替代（Task 3/4 的试接报告都确认过：不在 R-03 几何范围，
不在 R-06 `PkVariant` 范围）。等某个后续任务交付真正的颜色类型后，
`PkConfigColor` 应该退役，改用那个类型——这与其他 R 任务里"临时局部代打类型
用完即退役"的一贯处置方式一致（同类先例见其他 R 任务对自己范围内临时 stub 的
处置说明）。

## 4. 已知限制

- **`PkConfigStore` 不是线程安全的**：单个未加锁的 `std::map`（group →
  key → value 两级）。目前本分支的测试/试接都是单线程用它，没暴露问题；但
  真实 `KSharedConfig::openConfig()` 在 KDE 里返回的是**按线程各一份**的实例，
  这里的全局单例在"多线程各自独立配置视图"这个形状上**不是等价替代**——
  未来有消费者依赖这个线程隔离语义时需要重新设计，不能假定现状够用。
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
├── PkConfigColor.h           readEntry/writeEntry(..., PkConfigColor) 的值类型（见 §3）
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
