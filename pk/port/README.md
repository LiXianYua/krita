# pk/port —— 零 Qt 端口层

独立 `project(pkport CXX)` 薄壳工程，不接入 Krita 主构建。目前有 R-12 Task 1
交付的 `PkStream`（对应 Qt 的 `QIODevice`）、Task 3 交付的 `PkEventSink`（状态
通知端口，对应 `libs/image/kis_node_graph_listener.h` 等）、Task 4 交付的
`PkResourceStorage`（资源定位与目录枚举端口，对应 `QDir`/`QDirIterator`/
`QStandardPaths` 里 `libs/resources/KisStoragePlugin.h` 承担的那部分职责）与
Task 5 交付的 `PkFontProvider`（「拿到字体」端口——发现/枚举/匹配/回退，
**不含度量**，对应 `libs/flake/text/KoFontRegistry.cpp`/`KoFFWWSConverter.cpp`
里由 fontconfig 承担的那部分职责）。跑法见 `tests/run_tests.sh`。

## `PkFontProvider`——拿到字体（不含度量）

端口只负责「拿到字体」：发现（配置与字体目录）/ 枚举（系统全部字体）/
匹配（按 `PkFontQuery` 排序候选、取最佳单个）/ 回退（候选家族链里放通用家族，
走同一条匹配路径,不是独立方法）。**度量不在端口里**——四个目标平台
（fontconfig/Android/DirectWrite/CoreText）拿到字体文件之后都走同一份
FreeType 完成度量，按端口判据（"这个能力在不同平台有没有不同实现"）不该
进端口。**端口不返回 `FT_Face`**，返回字体文件标识
（`FontHandle`：路径 + face index），FreeType 打开在端口之外。

实测范围（`Fc*` 160 处出现 / 69 处真实调用 / 38 个不同函数，见
`.superpowers/sdd/R-12/task-5-report.md`）：**不是 1:1 映射**，覆盖
②配置与路径（8 个不同函数：`initialize`/`addFontFile`/`addFontDirectory`/
`fontDirectories`/`rebuildFontSet`）+ ⑤匹配（4 个：`sortedMatches`/
`bestMatch`）+ ⑥字体集枚举（2 个：`allFonts`）+ ⑦字符集（`coversCodepoint`）
+ ⑧语言集（并入 `FontEntry::languages`，见 `allFonts()`）。
④模式构造与属性读写（32 处、占比最大的一族）整族归零，换成 `PkFontQuery`
结构体（family/weight/slant/width/size/pixelsize/lang 七个固定属性——评审
I-5 删掉了零调用点的 `charset` 字段，从八个收窄为七个）。
①初始化/生命周期的 4 个 `FcXxxDestroy`、③目录列表迭代 `FcStrList`、
`FcFontRenderPrepare`（零命中）、`FcWeightToOpenType`/`FcWeightFromOpenType`
（纯数学，调用方自行内联）、`FcPatternHash`（调用方内部缓存键）——全部
排除，登记不是遗漏，逐项理由见 `PkFontProvider.h` 类头注释。

**一处判断，不是实测**：`PkFontQuery::families` 在
`getCssDataForPostScriptName()`（KoFontRegistry.cpp:1197-1234）那个调用点
实际查的是 `FC_POSTSCRIPT_NAME` 而不是 `FC_FAMILY`——查询侧 7 字段预算里
没有单独的 postscript 字段，选择让 `families[0]` 兼载这个语义（由具体实现
决定按哪个 fontconfig 属性精确匹配），见该字段注释。

**评审修复后 `FontEntry` 的字段分工**（C-2/I-6）：`sortedMatches()`/
`allFonts()` 路径只读 `handle`/`familyName`/`languages`；`bestMatch()`
路径（`getCssDataForPostScriptName()` 唯一调用点）读
`familyName`/`postScriptName`/`weight`/`width`/`slant`，不读
`handle`/`languages`——两条路径共用同一个结构体，但字段语义按调用路径钉死
在类头注释里，不留"由实现决定"的空子。`sortedMatches()` 的过滤契约（I-1）：
非缩放位图字体、且 `query.pixelSize` 与候选自身像素大小不相等时，实现必须
在返回前自己内部排除，不能指望调用方再做一遍。

## `PkResourceStorage`——资源定位与目录枚举

两族 API：

- **目录枚举族**：`listEntries(path, nameFilters, kind, recursive)` 返回一个
  惰性 `EntryIterator`（`hasNext()`/`next()`/`url()`/`lastModified()`），覆盖
  `QDirIterator`（6 处构造，全部递归+可选 glob 过滤+只要文件）与
  `QDir::entryList`/`entryInfoList`（9 处，非递归，7 处要文件、2 处要目录）
  两种实测形态；`exists()`/`mkpath()`/`remove()`/`absolutePath()`。
- **资源定位族**：`platformDir(PlatformDir)`，7 个 kind
  （AppData/AppLocalData/GenericData/GenericConfig/Cache/Home/Pictures）。
- **路径拼接**：`joinPath`/`cleanPath`/`relativePath` 三个静态纯字符串工具。

实测口径、逐方法调用次数、样本核验见 `.superpowers/sdd/R-12/task-4-report.md`。

**`EntryIterator::lastModified()` 用 `int64_t` 毫秒（Unix epoch，UTC），不
是前置声明占位。** `QDateTime` 归 R-16（未交付），任务硬约束要求这两选一
并登记理由：`lastModified()` 是纯虚数值返回，任何具体子类都必须能给出一个
可编译的返回值——"只声明不定义"这条对 `PkStream.h` 那三个按值返回类对象
的方法安全（不 override 就不会被实例化触发链接），但对纯虚数值返回不安全
（虚函数表决议阶段就报错，不是"调用才炸"），所以选了可以立刻编译、可测试
的整型时间戳。`QDateTime`（R-16）落地后如果需要更丰富的时区/日历语义，
再加一个转换方法，不改这个字段本身。

### 已知缺口（登记，不是遗漏）

| 缺口 | 原因 | 由谁补 |
|---|---|---|
| `QDir::rmpath`（1 处，`KoResourcePaths.cpp:282`） | 评审 M-4：调用点数与 `remove()` 一样都是 1 处、返回值都未被检查——排除理由不能是"1 处/没检查"（那对 `remove()` 同样成立却被留下了），真正的区别是**能力必要性**：`remove()` 删单个文件是 `mkpath()`（建目录）的对称能力，属于存储抽象必须能做的核心 CRUD；`rmpath()` 只是 `mkpath` **已经失败之后**的兜底清理，它是否执行不影响任何后续正确性——最坏结果只是留几个空目录，不是"资源定位与目录枚举"要保证的东西 | 需要时再加，不阻塞任何已知消费者 |
| `QDir::tempPath`（3 处，`kis_image_config.cpp` 的 swap 目录） | 不在上级给定的 7 个 `PlatformDir` kind 里（`QStandardPaths` 没有直接对应的 TempLocation 命中，`QDir::tempPath()` 是独立的 OS 临时目录概念）——按给定范围未纳入 | 若后续任务需要 swap 目录定位，另开一个 kind 或专门的 API |
| `ResourceIterator::type()`（先例字段） | 目录枚举没有"逐条目反查类型"的真实调用点，过滤发生在查询级（`EntryKind` 参数），按"范围上界=实测"不加 | 无——如果出现真实需要，那时再加 |
| `TagIterator` / `resource()`（`KoResourceSP` 加载） | Task 4 范围是"资源定位与目录枚举"，不含资源加载/版本化/标签，没有测量材料 | 归属未来批次（资源加载相关端口） |

### 与 `task-4-brief.md` 口径不同的两处（已用实测数覆盖）

- `QDirIterator` 带 name filter + `QDir::Files` 的构造：brief 写"4 处"，
  逐处 `sed` 核对全部 6 处构造的完整参数列表后确认是 **5 处**
  （`KisFolderStorage.cpp` 的两处构造都带，不是一处）。
- `PlatformDir` 的 kind 数：`task-4-brief.md` 写"5 个位置"，上级任务 prompt
  与本任务实测都是 **7 个**（含 `KoResourcePaths.cpp:192` 经
  `mapTypeToQStandardPaths()` 间接命中的 `CacheLocation`）——按实测数与上级
  prompt 走，`task-4-brief.md` 的"5"是过期口径。

## 等 R-02 交付的符号

以下三个 `PkStream` 成员**只声明，不定义**，链接期未使用则不报错、被调用则
响亮报错（`undefined reference`）——这是刻意的设计，不是遗漏，理由见
`PkStream.h` 里「一个刻意的设计」那段头注释。`PkByteArray`（R-02 交付）落地后
需要给它们补实现：

| 符号 | 签名 |
|---|---|
| `PkStream::readAll()` | `PkByteArray readAll()` |
| `PkStream::peek(pk_int64)` | `PkByteArray peek(pk_int64 maxSize)` |
| `PkStream::readLine()` | `PkByteArray readLine()` |

## 等 R-03 交付的类型：`PkEventSink::imageUpdated()`

`PkEventSink::imageUpdated(const PkRect &rect)`（来源：`kis_image.h:818`
`sigImageUpdated(const QRect &)`）**已经给了真实的空函数体实现**（在
`PkEventSink.cpp` 里），不是像 `PkStream::readAll()` 那三个一样只声明不定义
——原因见 `PkEventSink.h` 里这个方法的注释：它是 `virtual` 的，若像
`PkByteArray` 那三个方法一样留空不定义，任何没有 override 它的子类在被实例
化时都会在**虚函数表**上出现未决议符号，链接期必炸（不是「被调用才炸」，
是「实例化就炸」），这条路对虚函数不安全。空函数体不触碰 `rect`，不需要
`PkRect` 的完整定义，两者在「只前置声明、不 include」这一点上是一致的，只
是「声明是否配一个定义」这一步因为虚函数表的缘故必须不同。

代价：`PkRect` 完整定义交付前，没法在测试里构造一个实参去调用这个方法，
`pk/port/tests/test_eventsink.cpp` 因此没有覆盖它的调用链路，只覆盖了另外
8 个事件。`PkRect`（R-03）落地后要给这个方法补一条真正传值调用的测试用例。

## 后续消费者

`PkStream` 的消费者是 S-01（`kritastore`，`KoStoreDevice` 等）与其他需要文件/
内存/zip 适配器的批次；`PkEventSink` 的消费者是防腐层（转发到 Flutter）与
测试记录器——两者本任务都只出接口，不出任何具体适配器/生产实现。
