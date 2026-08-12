# pk/port —— 零 Qt 端口层

独立 `project(pkport CXX)` 薄壳工程（`pk/port/CMakeLists.txt`），不接入 Krita 主构建。
R-12 交付五个端口的**接口定义**（不含产品级实现/适配器）+ Q-5 的一个具体实现（`PkZipArchive`
底层直接接 minizip-ng，因为 zip 归档格式本身没有"跨平台不同实现"这件事，端口判据在这里天然
收敛成一个实现）：

| 端口/组件 | 对应 Qt 概念 | 来源 Task | 头文件 |
|---|---|---|---|
| `PkStream` | `QIODevice` | Task 1（`compat/QIODevice` 垫片：Task 2） | `pk/port/PkStream.h` |
| `KoProgressProxy` | （已是端口形态，见 §1.2） | Task 2（登记 + 编译试接证明） | `libs/widgetutils/KoProgressProxy.h`（**不在 `pk/port/` 下**） |
| `PkEventSink` | `libs/image/kis_node_graph_listener.h` 等信号面 | Task 3 | `pk/port/PkEventSink.h` |
| `PkResourceStorage` | `QDir`/`QDirIterator`/`QStandardPaths` 里 `KisStoragePlugin` 承担的那部分 | Task 4 | `pk/port/PkResourceStorage.h` |
| `PkFontProvider` | `libs/flake/text/KoFontRegistry.cpp`/`KoFFWWSConverter.cpp` 里 fontconfig 承担的那部分（不含度量） | Task 5 | `pk/port/PkFontProvider.h` |
| `PkZipArchive`（Q-5） | `QuaZip`（`libs/store/KoQuaZipStore.cpp`） | Task 6 | `pk/port/zip/PkZipArchive.h` |

跑法见 §4。全部 5 个测试套件当前 **82 passed, 0 failed**（22+11+21+16+12，实测数字见 §4 原始输出，
非历史记录——测试数会随后续 fix round 变化，报告前一律重新跑）。

---

## 1. 各端口 API 范围表——逐项标明来源

判据：**范围上界 = 实测用量表，一项不多一项不少**。以下每一项都能在对应头文件的方法/字段
注释里找到同样的来源引用——本节是那些注释的索引与汇总，不是另一套独立口径。

### 1.1 `PkStream`（对应 `QIODevice`）

**范围依据不是"Qt token 计数"，是"保留范围内唯一的子类"**：`libs/store/KoStoreDevice.h`
是保留范围内唯一 override `readData()`/`writeData()` 的 `QIODevice` 子类（`task-1-brief.md`
判断依据）。`read()` 公开 / `readData()` 保护纯虚这个模板方法拆分，就是为了让 `KoStoreDevice`
的移植是"零改动"——拆分没了这条就不成立。

**行为对齐来源**：`pk/port/probe/probe_qiodevice.cpp` + `probe_qdatastream_openmode.cpp`
（真链 Qt 5.15.13，见 `pk/port/probe/README.md`），不是读文档/凭常识实现的。

| 方法/字段 | 来源 |
|---|---|
| `OpenModeFlag` 各位值 | 位值照抄真 Qt `QIODevice::OpenModeFlag`，探针 §3.9 实测确认 |
| `read()`/`readData()` 拆分 | `KoStoreDevice` 是唯一子类，见上 |
| EOF 返回 0 而非 -1；`errorString()`/`error()` 在 EOF 后仍是"无错误" | 探针 §1（`probe_qiodevice.cpp` 对 `QFile`/`QBuffer`/`pipe(2)` 上的 `QFile` 三种设备实测） |
| 短读不补零（一次转发一次 `readData`/`writeData`） | 探针 §2 |
| 顺序设备 `pos()` 恒为 0，基类不为其推进游标 | 探针 §3（真事故复现：`isSequential()` 改 `true` 后 `readAll()` 死循环吐 64MB） |
| `peek()`：不动 `pos()`，不足 `n` 返回剩余 | 探针（`PkStream.h` 头注释 1-3 条） |
| `atEnd()` == `bytesAvailable()==0`，未 open 时恒 `true` | 探针；fix round I-1 修正 |
| `ungetChar()`：非顺序设备允许 `pos()` 到 -1、写入后失效 | 探针 §3.8；fix round C-1/C-2 修正 |
| `write()` 拒绝负 `pos`、只读设备返回 -1 | fix round C-1；测试 `testWriteAtNegativePosReturnsMinusOne` |
| `errorString()` 默认文案 `"Unknown error"` | **不是探针条目①-⑧覆盖的范围，按"已知的 Qt 默认"处理，未实测初始值**（`PkStream.h` 私有字段注释显式标注） |
| `readAll()`/`peek(pk_int64)`/`readLine()`（`PkByteArray` 重载） | **只声明不定义**，等 R-02，见 §3 |

`compat/QIODevice`（Task 2）：`#define QIODevice PkStream`（不是 `using`——`kis_meta_data_io_backend.h:13`
的 `class QIODevice;` 前置声明会撞上"redeclared as different kind of entity"，宏改写整行则前置声明
照样成立，成因与 `pk/string/compat/QString` 对 `class QString;` 的处理完全相同）。

测试：22 个（`pk/port/tests/test_stream.cpp`）。

### 1.2 `KoProgressProxy`——**R-12 没有为它新建头文件**

`libs/widgetutils/KoProgressProxy.h`（34 行）**已经**满足端口形态：纯抽象类，零 `Q_OBJECT`
（实测 `grep -c Q_OBJECT` = 0），唯一类型级 Qt 依赖是 2 处 `const QString&`（`setFormat`/
`setAutoNestedName` 形参，实测 `grep -c 'const QString'` = 2）。

**为什么不新建端口头**：`docs/实施边界-构建目标视图.md` 的 **D0.5** 已经定了"进度族 4 个头 +
`KoProperties.h` `git mv` 到 `libs/global`"——R-12 再造一个头会与 D0.5 撞车。R-12 对它的交付是：

1. **编译试接的证明**：`graft_check.sh` 的 2 个 `EXPECT_PASS`——`libs/widgetutils/KoProgressProxy.h`
   与 `libs/widgetutils/KoFakeProgressProxy.h`，只靠 `compat/QIODevice` + `compat/QString`
   （R-01）就该编过（实测编过，见 §4）。**这两个文件本身没有 `QIODevice` 用量**——它们证明
   的是 R-01 的 `QString` 垫片，不是 `PkStream`（见 §3 已知缺口第 5 条）。
2. **本节登记**：作为交接件的一部分，让 S-* 批次知道这个类不用等 R-12 出新头，直接消费
   `libs/widgetutils/KoProgressProxy.h` 现状即可（D0.5 落地后路径会变成 `libs/global/`）。

### 1.3 `PkEventSink`——零 Qt 依赖的状态通知端口

先例：`libs/image/kis_node_graph_listener.h`（124 行，Krita 自己已做过一次"去 QObject 化"，
类注释原话 "we don't want our nodes to be QObjects..."）。设计模式照抄它的三条：
①`virtual` + 空默认实现，不用 `= 0` ②成对命名只用于天然带"之前/之后"两阶段的操作
③接口自身可带状态（本类目前未用到这条，无材料证明需要）。

**当前头文件是 10 个事件**（`pk/port/PkEventSink.h` 实测 `grep -c '^    virtual void'` = 10；
`.superpowers/sdd/R-12/progress.md` 记的"9 个事件"是评审 I-4 补 `layersChanged()` **之前**
的旧数字，本文档以代码实测为准，见「发现的不一致」）：

| # | 方法 | 分类 | 来源 |
|---|---|---|---|
| 1 | `aboutToAddANode` | ①图层树变更 | `kis_node_graph_listener.h:45` |
| 2 | `nodeHasBeenAdded`（含 `dontActivateNode` 位） | ①图层树变更 | `kis_node_graph_listener.h:50` + `kis_image_signal_router.h:94` `emitNodeHasBeenAdded()`；`dontActivateNode` 来源 `KisNodeAdditionFlags.h:13` 唯一位 `DontActivateNode`，消费点 `kis_dummies_facade_base.cpp:159` |
| 3 | `aboutToRemoveANode` | ①图层树变更 | `kis_node_graph_listener.h:55` + `kis_image_signal_router.h:109` |
| 4 | `nodeHasBeenRemoved` | ①图层树变更 | `kis_node_graph_listener.h:60` |
| 5 | `aboutToMoveNode` | ①图层树变更 | `kis_node_graph_listener.h:66` |
| 6 | `nodeHasBeenMoved` | ①图层树变更 | `kis_node_graph_listener.h:72` |
| 7 | `nodeChanged` | ①图层树变更 | `kis_node_graph_listener.h:74` + `kis_image_signal_router.h:29/89` |
| 8 | `layersChanged`（评审 I-4 补） | ①图层树变更 | `KisImage::notifyLayersChanged()`（`kis_image.cc:1793`）+ `kis_image_change_layers_command.cpp:28,38`（拼合/撤销整棵图层栈替换）；消费者 `kis_dummies_facade_base.cpp:99`（图层树镜像，漏这个会在拼合/撤销后静默过期） |
| 9 | `imageUpdated(const PkRect&)` | ②区域重绘完成 | `kis_image.h:818` `sigImageUpdated(const QRect&)` |
| 10 | `historyStateChanged` | ③撤销栈变更 | `libs/command/kis_undo_store.h:64`——本端口最大收益点：`KisUndoStore` 除这一个信号外 6 个方法全是纯虚，通知面挪出去后端口版本能彻底摆脱 `QObject` |

`kis_image_signal_router.h` 另外 4 个 `emitXxx()`（`emitNotification`/`emitNotifications`
系列图像属性变更调度、`emitRequestLodPlanesSyncBlocked`、`emitNotifyBatchUpdateStarted/Ended`）
**不在三类分组之内，本任务不做**——见 §2。

**`nodeHasBeenAdded` 省略了完整 `KisNodeAdditionFlags`**：原型是 `QFlags<KisNodeAdditionFlag>`，
本任务硬约束只允许前置声明 `KisNode`/`PkRect` 两个类型，`DontActivateNode` 是该 flags 里
**唯一一位**，收窄成 `bool` 形参——这是范围裁剪，不是遗漏，但确实丢了"这是不是真的只有一位、
以后会不会加位"这个信息，`QFlags` 归口问题见 §3。

测试：11 个（9 个事件测试 + init/cleanup，`pk/port/tests/test_eventsink.cpp`）。`imageUpdated`
未被任何用例调用——见 §3。

### 1.4 `PkResourceStorage`——资源定位与目录枚举

**范围口径**：`git ls-files`（含 `.cc`），`libs/{image,brush,pigment,global,store,resources,
flake,command,psd,psdutils,metadata,impex,color,surfacecolormanagementapi,koplugin,version,
multiarch}` + `plugins/{paintops,filters,generators,impex,color,tools,flake,assistants,
metadata}`，**排除路径含 `tests`/`benchmarks` 的文件**。逐处调用点核验见
`.superpowers/sdd/R-12/task-4-report.md`。

| Qt 来源 | 实测用量 | PK 覆盖形态 |
|---|---|---|
| `QDir` | 34 文件 / 141 处 token（107 处真实调用：24 静态 + 83 实例；其余 34 处是声明/`#include`/标志位实参/构造不链式调用/注释/疑似前置声明，逐处人工分类） | `listEntries`（非递归分支）/`exists`/`mkpath`/`remove`/`absolutePath`/`platformDir`（Home 分支）/`joinPath`/`cleanPath`/`relativePath` |
| `QDirIterator` | 5 文件 / 6 处构造，全部带 `Subdirectories`；5 处额外带 name filter + `QDir::Files`（**不是 `task-4-brief.md` 写的 4 处**——`KisFolderStorage.cpp` 两处构造都带，按实测数走） | `listEntries`（递归分支） |
| `QStandardPaths` | 7 个 kind：`AppData`/`AppLocalData`/`GenericData`/`GenericConfig`/`Home`/`Pictures` 直接命中，`Cache` 经 `KoResourcePaths.cpp:192` `mapTypeToQStandardPaths()` 间接命中（**不是 `task-4-brief.md` 写的 5 个位置**） | `platformDir(PlatformDir)` |
| `QDir::absoluteFilePath`/`filePath`/`cd`/`cdUp`/`path` | 3+2+8+4+5 = 22 处，全部只做"拼路径/读拼接结果"，`cd`/`cdUp` 8 处调用点都不依赖 `QDir` 对象的可变"当前目录"状态 | `joinPath`（静态纯字符串工具） |
| `QResource` | 保留范围**零命中** | 不实现，见 §2 |

`EntryKind::Files/Directories` 来源：`entryList`/`entryInfoList` 里 7 处要文件 + 全部 6 处
`QDirIterator` 构造要文件（共 13 处）vs `QDir::Dirs|NoDotAndDotDot`（`KoJsonTrader.cpp:64,98`，
2 处，非递归取子目录）。

测试：21 个（`pk/port/tests/test_resourcestorage.cpp`）。

### 1.5 `PkFontProvider`——拿到字体（不含度量）

**范围口径**：`libs/flake/text/KoFontRegistry.cpp`（124 处 `Fc*` 调用）+
`KoFFWWSConverter.cpp`（22 处）+ `KoFontLibraryResourceUtils.h`（12 处，RAII 模板 destroy
函数指针实参）+ `KoFFWWSConverter.h`（2 处，类型别名）——**合计 160 处出现 / 69 处真实调用 /
38 个不同函数**，逐处核验见 `.superpowers/sdd/R-12/task-5-report.md`。**不是 1:1 映射
fontconfig 全部函数**。

| 分族 | 覆盖的 Fc 函数 | 实测调用 | PK 覆盖形态 |
|---|---|---|---|
| ②配置与路径 | `FcConfigCreate`/`FcConfigParseAndLoad`/`FcConfigSetCurrent`/`FcConfigGetCurrent`/`FcConfigAppFontAddDir`/`FcConfigAppFontAddFile`/`FcConfigGetFontDirs`/`FcConfigBuildFonts`（8 个不同函数） | 12 处 | `initialize`/`addFontFile`/`addFontDirectory`/`fontDirectories`/`rebuildFontSet` |
| ⑤匹配 | `FcConfigSubstitute`/`FcDefaultSubstitute`/`FcFontSort`/`FcFontMatch`（4 个） | 8 处 | `sortedMatches`/`bestMatch` |
| ⑥字体集枚举 | `FcObjectSetBuild`/`FcFontList`（2 个） | 2 处 | `allFonts` |
| ⑦字符集 | `FcCharSetHasChar` | （`facesForCSSValues()` 逐 grapheme 回退匹配用） | `coversCodepoint` |
| ⑧语言集 | `FcPatternGetLangSet`+`FcLangSetGetLangs` | （WWS 家族归并用） | 并入 `FontEntry::languages`（`allFonts()`） |

`PkFontQuery` 七个固定属性（`families`/`weight`/`slant`/`width`/`size`/`pixelSize`/`lang`）
替代族④"模式构造与属性读写"（`FcPatternCreate`/`AddString`/`AddInteger`/`AddDouble`/
`AddWeak`/`GetString`/`GetInteger`/`GetDouble`/`GetBool`/`GetCharSet`/`Hash`，32 处，
占比最大的一族）——**整族归零**，评审 I-5 删掉零调用点的 `charset` 字段后从 8 个收窄为 7 个。

测试：16 个（`pk/port/tests/test_fontprovider.cpp`）。**`PkFontProvider` 10/10 方法都是纯虚，
`.cpp` 只有构造/析构 6 行**——这 16 个用例全部跑在测试自建的 `FakeFontProvider` 上，验证的是
"这个接口形状能不能被实现出预期行为"（接口可实现性），不是"某个具体实现（fontconfig/
Android/DirectWrite/CoreText 适配器）对不对"（实现正确性）——那些适配器不在本任务交付范围
内。`PkEventSink` 的 11 个用例同理：默认实现全是空函数体，测试同样跑在自建的假实现上。这对
"只出接口"的交付合理且不可避免，但 §4 的"82 passed"容易被误读成 82 个用例都在验证已交付
实现的正确性，这里点明一句。

### 1.6 `PkZipArchive`（Q-5，替换 QuaZip）

能力面 1:1 对应 `libs/store/KoQuaZipStore.cpp` + `libs/resources/KisResourceStorage.cpp:140`
**这 2 个文件**——保留范围内全部 QuaZip 消费者，一项不多（`task-6-brief.md`"能力面"表）。
底层 minizip-ng **4.0.7**（`pk/port/zip/CMakeLists.txt` `GIT_TAG` 钉死，实测走
`FetchContent`——dev 包未装，`find_package(minizip-ng)` 不命中）。

| 方法 | 来源 |
|---|---|
| `Mode { Read, Write }` | `KoQuaZipStore::init()` 行 156：`open(mode==Write ? mdCreate : mdUnzip)` |
| `openFile(path)` | `KoQuaZipStore(const QString&, ...)` 行 43-52 |
| `openStream(PkStream*)` | `KoQuaZipStore(QIODevice*, ...)` 行 54-60——不接管 stream 生命周期，调用前 stream 必须已 open |
| `close()` | `doFinalize()` 行 181-191 + 析构行 62-93 |
| `setDataDescriptorWritingEnabled` | `init()` 行 150（真实调用点只传 `false`，setter 仍暴露） |
| `setZip64Enabled` | `init()` 行 151；minizip-ng 无归档级开关，per-entry 应用（`true→MZ_ZIP64_FORCE`，`false→MZ_ZIP64_AUTO`） |
| `setAutoClose` | `init()` 行 154，只影响 `openStream()` 打开的归档 |
| `entryCount()` | `init()` 行 176-177 `getEntriesCount()` |
| `locateEntry(name)` | `openRead()` 行 238 `setCurrentFile(fixedPath)` |
| `entryNames()` | `directoryList()` 行 126-138（`QuaZipDir` 也基于同一份平铺条目名单做前缀判断，不是独立能力） |
| `openEntryForRead()` | `openRead()` 行 243-248，返回值本身就是一个 `PkStream` |
| `openEntryForWrite(name, perms, compressed)` | `openWrite()` 行 193-216 + `setCompressionEnabled()` 行 95-104（两档：`Z_DEFAULT_COMPRESSION`/`Z_NO_COMPRESSION`，压缩方法恒 `Z_DEFLATED`） |
| `lastError()`/`isOk()` | `doFinalize()` 行 189 `getZipError()==ZIP_OK` |

**不实现加密**：`QuaZip::setPassword` 保留范围内实测 **0 处**调用。**同一时刻只能有一个条目
开着**：继承自真 QuaZip 的约束（`KoQuaZipStore.cpp:76-86` 析构注释引用 `QuaZipFile` 文档），
不是本类新引入的限制。

测试：12 个（`pk/port/tests/test_zip.cpp`），含一条端到端"写 zip→读回→内容一致，且条目流
真的是 `PkStream`"，另有评审 I-1（`openStream()` 拒绝未 `open()` 的 stream）与 I-2
（`entryNames()` 不破坏 `locateEntry()` 定位的条目）两条契约回归。

---

## 2. 明确排除的项目——每项都有理由与归属

| 项目 | 用量 | 排除理由 | 归属 |
|---|---|---|---|
| `QIODevice::reset()` | 保留范围内实测 0 处 | 零调用点，按"范围上界=实测"不做 | 不实现 |
| `QResource` | 保留范围内实测 0 处 | 同上 | 不实现 |
| `PkFontProvider` 度量（ascent/descent/glyph 尺寸/hinting） | — | 端口判据是"这个能力在不同平台有没有不同实现"；四个目标平台拿到字体文件后都走**同一份** FreeType 完成度量，不该进端口。**这与 `docs/Qt替代品选型.md`「用途」列的措辞有出入，本文只报告，不改决策文档** | 度量在端口之外，调用方拿 `FontHandle` 后自己走 FreeType |
| `Fc*` 族①（`FcConfigDestroy`/`FcPatternDestroy`/`FcFontSetDestroy`/`FcCharSetDestroy`） | 全仓零直接调用，仅作 RAII 模板 destroy 函数指针实参 | 具体实现的资源生命周期是实现细节 | 不实现 |
| `Fc*` 族③（`FcStrList*`，10 处） | 唯一用途是遍历 `FcConfigGetFontDirs()`/`FcPatternGetLangSet()` 结果 | 折叠进对应端口方法直接返回 `vector`，不单独暴露链表迭代器类型 | 不实现（已并入 `fontDirectories()`/`FontEntry::languages`） |
| `FcFontRenderPrepare` | 零命中 | 不复刻 pattern-merge 语义——`sortedMatches()` 返回的每个 `FontEntry` 就是候选本身 | 不实现 |
| `FcWeightToOpenType`/`FcWeightFromOpenType` | 2 处（`KoFontRegistry.cpp:395,1222`） | 纯数学分段线性插值，无 I/O 无状态，不是端口能力 | 调用方自行内联 |
| `FcPatternHash` | 2 处（`KoFontRegistry.cpp:406,422`） | 调用方自己拿它做候选结果缓存键，是实现细节 | 不实现 |
| `QuaZip::setPassword`（加密） | 保留范围内实测 0 处 | 见 §1.6 | 不实现 |
| `QDir::rmpath` | 1 处（`KoResourcePaths.cpp:282`） | 评审 M-4：不能拿"1 处/未检查返回值"当排除理由（`remove()` 同样成立却被留下）——真正区别是**能力必要性**：`remove()` 是 `mkpath()` 的对称能力，属核心 CRUD；`rmpath()` 只是 `mkpath` 失败后的兜底清理，做不做不影响任何后续正确性 | 需要时再加，不阻塞任何已知消费者 |
| `QDir::tempPath` | 3 处（`kis_image_config.cpp` swap 目录） | 不在给定的 7 个 `PlatformDir` kind 里，`QStandardPaths` 没有直接对应的 `TempLocation` 命中 | 若后续需要 swap 目录定位，另开 kind 或专门 API |
| `ResourceIterator::type()`（先例字段） | — | 目录枚举的过滤发生在查询级（`EntryKind` 参数），141 处 `QDir` 调用点没有一处"拿到 entry 后反查类型" | 不加，按"范围上界=实测" |
| `TagIterator`/`resource()`（`KoResourceSP` 加载） | — | Task 4 范围是"资源定位与目录枚举"，不含资源加载/版本化/标签，没有测量材料 | 归属未来批次（资源加载相关端口） |
| `kis_image_signal_router.h` 另外 4 个 `emitXxx()` | — | 图像属性变更调度器 / 画布 LOD 同步 / 批量重绘调度，都不是"图层树变更/区域重绘完成/撤销栈变更"三类之一 | 不在 `PkEventSink` 范围（这条边界评审承认"不是 100% 无歧义"，见 `task-3-report.md` 判断 3） |
| `KisNodeGraphListener` 的 `graphSequenceNumber()`/`nodeCollapsedChanged()`/`notifySelectionChanged()`/`invalidateAllFrames()`/`keyframeChannelHasBeenAdded()`/`keyframeChannelAboutToBeRemoved()` | — | 材料没有单独点名要求这些方法 | 按"范围上界=实测"不做 |
| `PkEventSink` 跨线程投递 | — | 本端口是**同步**通知；跨线程生命周期保证用不上 | **Q-8/R-10**（"投递到指定线程"是另一件事） |

---

## 3. 覆盖度缺口——「说不出覆盖不到什么的，说明还没想清楚」

- **`PkStream::readAll()`/`peek(pk_int64)`/`readLine()` 只声明不定义**。`PkByteArray` 归 R-02。
  刻意的设计：这三个是按值返回一个不完整类型，先把"符号存在、签名是什么"这个形状钉下来，
  不定义不会静默变错，会在**链接期**响亮报错。不要为了编过去自己造一个 `PkByteArray` 或
  注释掉这三行 → **归 R-02**。
- **`graft_check.sh` 全部 12 个 `EXPECT_FAIL` 行的卡点与归属**（实测 12 处，不是
  `task-7-brief.md` 里写的 13 处——`grep -c '^check_expect_fail '` 精确匹配调用行，实测
  12，本文档以此为准，见「发现的不一致」）：

  | 头文件 | 卡在 | 归属 |
  |---|---|---|
  | `libs/store/KoStore.h` | `QByteArray` | R-02 |
  | `libs/store/KoStoreDevice.h` | `QByteArray` | R-02 |
  | `libs/store/KoDirectoryStore.h` | `QByteArray` | R-02 |
  | `libs/image/tiles3/swap/kis_abstract_compression.h` | `QtGlobal` | R-02/R-03 |
  | `libs/brush/kis_png_brush.h` | `QImage` | R-15 |
  | `libs/brush/kis_svg_brush.h` | `QImage` | R-15 |
  | `libs/image/kis_composite_progress_proxy.h` | `QList` | R-02 |
  | `libs/command/kis_undo_store.h` | `QObject` | R-05 |
  | `libs/resources/KisStoragePlugin.h` | `QScopedPointer` | R-04 |
  | `libs/image/kis_node_graph_listener.h` | `QScopedPointer` | R-04 |
  | `libs/pigment/resources/KoCachedGradient.h` | `expected class-name`（工程头闭包，非 Qt 缺口） | — |
  | `libs/flake/resources/KoFontFamily.h` | `KoResource\.h`（工程头闭包，非 Qt 缺口） | — |

- **判据②目前只有一个真实见证者**：保留范围内引用 `QIODevice` 的头文件共 141 个（实测数字
  见 `task-1-report.md`/`task-2-report.md`），靠现有垫片能编过的只有
  `libs/metadata/kis_meta_data_io_backend.h` **一个**（其余全卡在上表的 R-02/R-03/R-04/
  R-05/R-15）；另两个 `EXPECT_PASS` 文件（`KoProgressProxy.h`/`KoFakeProgressProxy.h`）
  各含 **0 处** `QIODevice`，它们证的是 R-01 的 `QString` 垫片，不是本端口——**如实写，
  不要美化成"3 个头文件证明了 QIODevice 垫片"**。
- **`QFlags`/`Q_DECLARE_FLAGS`（实测 `Q_DECLARE_FLAGS` 49 文件、`QFlags` 26 文件，均排除
  `tests`/`benchmarks`，本文档口径：`git ls-files '*.h' '*.cpp' '*.cc' | grep -v -E
  '(^|/)(tests|benchmarks)/' | xargs grep -l <token>`）在 R 线任务表里查不到归口**——
  `PkEventSink::nodeHasBeenAdded` 已经踩到这个缺口（`KisNodeAdditionFlags` 只给了单独一位
  `bool`，见 §1.3），**撞上停下报告，不自己发明替代品**。这是本任务发现但未解决的空白，
  由后续任务或人决定归属。
- **`QFont::toString()`/`fromString()` 是 `.kpp` 文字笔刷预设的持久化格式**（实测：写
  `libs/brush/kis_text_brush.cpp:284` `e.setAttribute("font", m_font.toString())`，读
  `libs/brush/kis_text_brush_factory.cpp:34` `font.fromString(data.textBrush.font)`），
  **不做格式兼容会静默丢失已有预设**（不报错、不崩溃，打开后笔刷没了）→ 归 **S-07-a 或后续
  字体任务**，**认领时要先实测 `QFont::toString()` 的确切格式**（Qt 私有序列化，版本间会变，
  本任务未实测这个格式字符串本身）。
- **`PkEventSink` 的跨线程投递不在本端口**（端口是同步通知）→ 归 **Q-8/R-10**。见 §2。
- **`KoFFWWSConverter::addFontFromPattern` 需要整个 `FcCharSet`**（`KoFFWWSConverter.cpp:
  331-337`）喂 `addSupportedLanguagesByFile()`，端口只有逐码点的 `coversCodepoint()`
  （族⑦）——**没有"整个字符集"这个粒度，这一处调用方必须重构**（要么改成逐码点查询驱动
  `addSupportedLanguagesByFile()`，要么改它的输入形状去接受一个逐码点查询谓词，具体做法
  留给消费本端口的批次决定）。评审 M-5 记录。
- **`PkEventSink::imageUpdated()` 没有真实调用链路测试**：`PkRect` 归 R-03（未交付），
  `tests/test_eventsink.cpp` 只验证"默认实现存在、类可以被实例化"，验证不了传值调用。
  `PkRect` 落地后要补一条真正传值调用的测试用例。
- **`PkResourceStorage::cleanPath()` 越根 `".."` 处理**：`QDir::cleanPath` 不折叠越过根的
  `".."`，原样保留（`"/.."` → `"/.."`，探针 `probe_qdir.cpp` 实测）——这是评审 C-1 修复后
  的当前行为，登记是因为它此前一度按文档/常识实现、与真 Qt 相反，R 线"实测优先"硬规则在
  这里真的抓到过一次偏差。
- **`joinPath()` 空目录/空 leaf 有意收窄，不跟 `QDir::filePath()` 的隐式"当前目录"语义**
  （评审 M-1）：真链 Qt 5.15.13 探针（`probe_qdir.cpp`「joinPath() 与真 Qt 的 7 处未登记
  分歧」，2026-08-12 实测）——`joinPath("","a")` 真 Qt `"./a"` 本实现 `"a"`；
  `joinPath("","")` 真 Qt `"."` 本实现 `""`；`joinPath("/root/","")` 真 Qt `"/root"`
  （去掉尾部斜杠）本实现 `"/root/"`。**没有已知调用点会传空目录**，本端口没有"当前目录"这个
  隐式概念，跟着 QDir 的默认值走只会让空目录拼出带 `.` 前缀的结果——保持简单收窄。测试
  `test_resourcestorage.cpp` `testJoinPathTrailingLeadingSlashCombinations()` 把这 3 个
  形态锁死为当前行为。另探针额外核验的 4 个同类形态（`dir="/root/"`，leaf 分别是
  `"a/"`/`"./a"`/`"../a"`/`"a/b"`）与真 Qt **完全一致**，不在收窄范围内。
- **`FontEntry::weight`/`width`/`slant` 默认值改成越界哨兵**（评审 I-4，此前是 `400`/`100`/
  `Slant::Normal`——三个都是合法可信的真实取值，`sortedMatches()`/`allFonts()` 路径按 §1.5
  分工表不填这几个字段时，误读会拿到"每个字体都是 Regular 400"这种静默错数据，不会有编译期
  /运行期信号）：现在是 `-1`/`-1`/`Slant::Unknown`，`weight`/`width` 的合法取值空间都是正
  整数，`-1` 落在区间外；`Slant` 新增 `Unknown` 枚举值，仅用于结果侧默认值，`PkFontQuery::
  slant`（查询侧）不受影响、默认仍是 `Normal`。`test_fontprovider.cpp`
  `testAllFontsReturnsFamilyNameAndLanguagesForEveryRegisteredFont()` 断言未填字段确实是
  哨兵值。
- **`entryNames()` 不缓存**：真 QuaZip 只在 Read 模式缓存，Write 模式每次重扫；本类两种
  模式都不缓存——功能等价，只是少一层调用开销优化，**这是判断，不是从实测口径直接导出的**。
- **`openEntryForWrite` 的 `unixPermissions` 是原始 `uint32_t`，不是枚举**：真实调用点固定
  传 `0444`，放开成参数是延续"QuaZip 本身是可读写的 setter 就不额外收窄"这条原则，**不是
  从额外调用点验证过"权限真的会变"**。
- **`mz_zip_file.modified_date` 用 `time(nullptr)`（写入时刻）**：语义上等价于
  `QuaZipNewInfo` 默认的"构造那一刻"，但**本仓库没有 vendor QuaZip 源码可以直接核对这一行**，
  未实测材料核对这个默认值。
- **`PkFontQuery::families` 兼载 `getCssDataForPostScriptName()` 的 `FC_POSTSCRIPT_NAME`
  查询**：这一处实际按 PostScript 名精确匹配，7 字段预算里没有单独 postscript 字段，选择让
  `families[0]` 兼载——**这是判断，不是实测出来的映射关系**。
- **`sortedMatches()`/`bestMatch()`/`allFonts()` 共用同一个 `FontEntry` 类型，但两条路径实际
  读的字段不是同一组**（评审 I-6 已在类头注释钉死分工，见 §1.5），共用类型本身是接口一致性
  的设计取舍，不是每个字段在每条路径上都有实测出处。

---

## 4. 怎么跑

```bash
./pk/port/tests/run_tests.sh    # 建工程 + 跑 test_pkport + nm -u 零 Qt 自证
./pk/port/graft/graft_check.sh  # 判据②：候选头文件试接（EXPECT_PASS/EXPECT_FAIL）
```

**`run_tests.sh` 真实输出**（本次评审修复实测，2026-08-12，R-12 全分支最终评审 6 条修复
落地后重跑）：

```
********* Start testing of PkStreamTestCase *********
PASS   : PkStreamTestCase::initTestCase()
PASS   : PkStreamTestCase::testEofReturnsZeroNotMinusOne()
PASS   : PkStreamTestCase::testShortReadReturnsActualCount()
PASS   : PkStreamTestCase::testPeekDoesNotMovePos()
PASS   : PkStreamTestCase::testAtEndIsBytesAvailableZero()
PASS   : PkStreamTestCase::testUngetCharShadowsRealByte()
PASS   : PkStreamTestCase::testSkipToEofReturnsRemaining()
PASS   : PkStreamTestCase::testReadLineTinyMaxSizeReturnsMinusOne()
PASS   : PkStreamTestCase::testUnopenedDeviceReadWriteReturnsMinusOne()
PASS   : PkStreamTestCase::testSequentialDevicePosStaysZero()
PASS   : PkStreamTestCase::testWriteInvalidatesUngetBuffer()
PASS   : PkStreamTestCase::testUngetCharAtPosZero()
PASS   : PkStreamTestCase::testAtEndTrueWhenUnopened()
PASS   : PkStreamTestCase::testMaxSizeEdgeCasesMatchRealQt()
PASS   : PkStreamTestCase::testMultipleUngetCharLifoOrder()
PASS   : PkStreamTestCase::testPeekAcrossUngetBufferAndUnderlyingData()
PASS   : PkStreamTestCase::testReadLineNewlineAndEof()
PASS   : PkStreamTestCase::testWriteToReadOnlyDeviceReturnsMinusOne()
PASS   : PkStreamTestCase::testWriteAtNegativePosReturnsMinusOne()
PASS   : PkStreamTestCase::testBytesAvailableDoesNotDoubleCountUngetForNonSequential()
PASS   : PkStreamTestCase::testReadLineEofReturnsMinusOneAllScenarios()
PASS   : PkStreamTestCase::cleanupTestCase()
Totals: 22 passed, 0 failed, 0 skipped
********* Finished testing of PkStreamTestCase *********
********* Start testing of PkEventSinkTestCase *********
PASS   : PkEventSinkTestCase::initTestCase()
PASS   : PkEventSinkTestCase::testMinimalOverrideCompilesAndDispatches()
PASS   : PkEventSinkTestCase::testNoOverrideAtAllStillCallable()
PASS   : PkEventSinkTestCase::testAddPairCalledInOrder()
PASS   : PkEventSinkTestCase::testRemovePairCalledInOrder()
PASS   : PkEventSinkTestCase::testMovePairCalledInOrder()
PASS   : PkEventSinkTestCase::testMixedEventsPreserveCallOrder()
PASS   : PkEventSinkTestCase::testNodePointerIdentityIsPreserved()
PASS   : PkEventSinkTestCase::testNodeHasBeenAddedForwardsDontActivateNodeFlag()
PASS   : PkEventSinkTestCase::testLayersChangedIsIndependentEventPreservingOrder()
PASS   : PkEventSinkTestCase::cleanupTestCase()
Totals: 11 passed, 0 failed, 0 skipped
********* Finished testing of PkEventSinkTestCase *********
********* Start testing of PkResourceStorageTestCase *********
PASS   : PkResourceStorageTestCase::initTestCase()
PASS   : PkResourceStorageTestCase::testIteratorHasNextNextUrlLastModified()
PASS   : PkResourceStorageTestCase::testIteratorHasNextFalseWhenEmpty()
PASS   : PkResourceStorageTestCase::testListEntriesNonRecursiveFilesExcludesSubdirAndItsContents()
PASS   : PkResourceStorageTestCase::testListEntriesRecursiveDescendsIntoSubdirectories()
PASS   : PkResourceStorageTestCase::testListEntriesDirectoriesKindReturnsOnlyDirs()
PASS   : PkResourceStorageTestCase::testListEntriesNameFilterMatchesGlobSuffix()
PASS   : PkResourceStorageTestCase::testListEntriesEmptyNameFiltersMeansNoFiltering()
PASS   : PkResourceStorageTestCase::testExistsTrueForKnownPathFalseOtherwise()
PASS   : PkResourceStorageTestCase::testMkpathCreatesAllMissingParents()
PASS   : PkResourceStorageTestCase::testRemoveDeletesFileAndFailsForMissingOrDir()
PASS   : PkResourceStorageTestCase::testAbsolutePathPassesThroughAbsoluteResolvesRelative()
PASS   : PkResourceStorageTestCase::testPlatformDirReturnsValuePerKindNotSharedAcrossKinds()
PASS   : PkResourceStorageTestCase::testJoinPathTrailingLeadingSlashCombinations()
PASS   : PkResourceStorageTestCase::testJoinPathAbsoluteLeafIgnoresDir()
PASS   : PkResourceStorageTestCase::testCleanPathCollapsesSlashesAndDotSegments()
PASS   : PkResourceStorageTestCase::testCleanPathKeepsDotDotPastAbsoluteRoot()
PASS   : PkResourceStorageTestCase::testRelativePathComputesDotDotClimb()
PASS   : PkResourceStorageTestCase::testRelativePathSameDirectoryYieldsDot()
PASS   : PkResourceStorageTestCase::testRelativePathTargetIsAncestorOfBaseYieldsTrailingSlash()
PASS   : PkResourceStorageTestCase::cleanupTestCase()
Totals: 21 passed, 0 failed, 0 skipped
********* Finished testing of PkResourceStorageTestCase *********
********* Start testing of PkFontProviderTestCase *********
PASS   : PkFontProviderTestCase::initTestCase()
PASS   : PkFontProviderTestCase::testSortedMatchesByFamilyNameReturnsRegisteredFont()
PASS   : PkFontProviderTestCase::testSortedMatchesReturnsEmptyWhenFamilyUnknown()
PASS   : PkFontProviderTestCase::testSortedMatchesFallsBackToLaterFamilyInList()
PASS   : PkFontProviderTestCase::testSortedMatchesReturnsEmptyWhenNoFamilyMatchesAndNoFallbackRegistered()
PASS   : PkFontProviderTestCase::testSortedMatchesOrdersCandidatesByWeightDistance()
PASS   : PkFontProviderTestCase::testCoversCodepointTrueForRegisteredCharAndFalseForOther()
PASS   : PkFontProviderTestCase::testSortedMatchesFiltersNonScalableBitmapFontsWithMismatchedPixelSize()
PASS   : PkFontProviderTestCase::testBestMatchReturnsFirstSortedCandidate()
PASS   : PkFontProviderTestCase::testBestMatchReturnsFalseWhenNoMatch()
PASS   : PkFontProviderTestCase::testBestMatchReturnsPostScriptNameWeightWidthSlant()
PASS   : PkFontProviderTestCase::testAllFontsReturnsFamilyNameAndLanguagesForEveryRegisteredFont()
PASS   : PkFontProviderTestCase::testInitializeRecordsConfigSearchPath()
PASS   : PkFontProviderTestCase::testAddFontDirectoryReflectedInFontDirectories()
PASS   : PkFontProviderTestCase::testRebuildFontSetReturnsTrueAndCanBeCalledRepeatedly()
PASS   : PkFontProviderTestCase::cleanupTestCase()
Totals: 16 passed, 0 failed, 0 skipped
********* Finished testing of PkFontProviderTestCase *********
********* Start testing of PkZipArchiveTestCase *********
PASS   : PkZipArchiveTestCase::initTestCase()
PASS   : PkZipArchiveTestCase::testEndToEndWriteThenReadBackViaPkStream()
PASS   : PkZipArchiveTestCase::testEntryCountReflectsEntriesWritten()
PASS   : PkZipArchiveTestCase::testEntryNamesListsAllEntries()
PASS   : PkZipArchiveTestCase::testCompressionLevelAffectsArchiveSize()
PASS   : PkZipArchiveTestCase::testOpenFromPkStreamRoundTrips()
PASS   : PkZipArchiveTestCase::testLocateMissingEntryFailsAndReportsError()
PASS   : PkZipArchiveTestCase::testCannotOpenSecondEntryWhileFirstStillOpen()
PASS   : PkZipArchiveTestCase::testZip64EnabledStillRoundTrips()
PASS   : PkZipArchiveTestCase::testOpenStreamRejectsUnopenedStream()
PASS   : PkZipArchiveTestCase::testEntryNamesDoesNotDisturbLocatedEntry()
PASS   : PkZipArchiveTestCase::cleanupTestCase()
Totals: 12 passed, 0 failed, 0 skipped
********* Finished testing of PkZipArchiveTestCase *********
nm -u libpkport.a libpkzip.a | grep -i qt: 无输出
```

5 个测试套件合计 **82 passed, 0 failed, 0 skipped**（22+11+21+16+12——评审 I-1/I-2 各给
`PkZipArchiveTestCase` 添了一条回归，zip 从 10 个变成 12 个，其余四个套件数量不变）。

**`graft_check.sh` 真实输出**（本次任务实测，退出码 0）：

```
  graft OK: libs/metadata/kis_meta_data_io_backend.h
  graft OK: libs/widgetutils/KoProgressProxy.h
  graft OK: libs/widgetutils/KoFakeProgressProxy.h
  graft 按登记失败: libs/store/KoStore.h（卡在 QByteArray → R-02）
  graft 按登记失败: libs/store/KoStoreDevice.h（卡在 QByteArray → R-02）
  graft 按登记失败: libs/store/KoDirectoryStore.h（卡在 QByteArray → R-02）
  graft 按登记失败: libs/image/tiles3/swap/kis_abstract_compression.h（卡在 QtGlobal → R-02/R-03）
  graft 按登记失败: libs/brush/kis_png_brush.h（卡在 QImage → R-15）
  graft 按登记失败: libs/brush/kis_svg_brush.h（卡在 QImage → R-15）
  graft 按登记失败: libs/image/kis_composite_progress_proxy.h（卡在 QList → R-02）
  graft 按登记失败: libs/command/kis_undo_store.h（卡在 QObject → R-05）
  graft 按登记失败: libs/resources/KisStoragePlugin.h（卡在 QScopedPointer → R-04）
  graft 按登记失败: libs/image/kis_node_graph_listener.h（卡在 QScopedPointer → R-04）
  graft 按登记失败: libs/pigment/resources/KoCachedGradient.h（卡在 expected class-name → 工程头闭包，非 Qt 缺口）
  graft 按登记失败: libs/flake/resources/KoFontFamily.h（卡在 KoResource\.h → 工程头闭包，非 Qt 缺口）
```

**关于 `nm -u | grep -i qt`**：查的是 `libpkport.a` 静态库——静态库允许留未定义符号，真混进
Qt 依赖会在这里现形；`test_pkport` 可执行文件是静态链接产物，链接期就会失败，跑不到 `nm`
这一步，同 `pk/test/README.md` §5 的说明。

探针跑法见 `pk/port/probe/README.md`（需要真链 Qt 5.15.13，不参与本工程构建，产物不进版控）。

---

## 5. 后续批次要接手什么

`PkStream` 的消费者是 **S-01**（`kritastore`：`KoStore`/`KoStoreDevice`/`KoDirectoryStore`
等）与其他需要文件/内存/zip 适配器的批次；`PkZipArchive`（Q-5）同样归 **S-01**——
`KoQuaZipStore` 与 `KoStoreDevice` 是同一个子系统里的姊妹类。`PkEventSink` 的消费者是
防腐层（转发到 Flutter）与测试记录器。**S-00** 是全仓机械 sed 接入批次（同
`pk/test/README.md` §6 的 S-00，不是 pk/port 专属的另一个批次）。R-12 只出接口，
不出任何具体适配器/生产实现。

> **`S-02-b`/`S-06`/`S-08` 的确切子系统边界本任务无法核实**：本 worktree 结构性隔离，
> 不含 `docs/迁移执行计划.md`（见本仓库 `CLAUDE.md`「你正在做什么」），拿不到这三个批次
> 编号的官方定义范围。下面按各端口在代码里能查到的自然消费者写，**已用来源标注哪些是
> 代码证据、哪些是按批次编号顺序的推断**——核对 `docs/迁移执行计划.md` 后如与本节不符，
> 以决策文档为准，并请回来更新本节。

### S-00：全量 sed 接入

对 `pk/port` 没有类似 `pk/test/graft/rename.sed` 的规则文件——`QIODevice` 只是一个类型名的
`#define`，不是 17 个宏那种批量改名。S-00 对 `pk/port` 的动作是**把 `-include
pk/port/compat/QIODevice` 这条编译参数注入到全量构建里**，不是文本替换；候选头文件表就是
`graft_check.sh` 本身（3 `EXPECT_PASS` + 12 `EXPECT_FAIL`）——R-02/R-04/R-05/R-15 等依赖
陆续落地后，这张表里的 `EXPECT_FAIL` 应该逐条转成 `EXPECT_PASS`，脚本会在第一时间报出来
（见 §4 关于「缺口已消失」的自证机制），S-00 不需要另建一份独立的候选清单。

### S-01：`kritastore`（推断，代码证据充分）

接手 `PkStream` + `PkZipArchive`。

- **接口在哪**：`pk/port/PkStream.h`、`pk/port/zip/PkZipArchive.h`。
- **参考适配器该实现成什么样**：`KoStoreDevice` 改写为 `PkStream` 子类，只 override
  `readData()`/`writeData()` 并调用 `setOpenMode()`（这正是 `PkStream` 保留"公开转发层 +
  受保护纯虚"拆分的唯一理由）；`KoQuaZipStore` 改写为基于 `PkZipArchive` 的实现，
  `openStream()` 接手"zip 建在 PkStream 之上"这条能力（原 `QuaZip(QIODevice*)` 构造路径）。
- **已知坑**：
  - `PkStream::readAll()`/`peek(pk_int64)`/`readLine()` 只声明不定义，等 R-02 落地——
    S-01 若先于 R-02 启动，这三个符号在真链接期会报 `undefined reference`，属预期。
  - `KoStore.h`/`KoStoreDevice.h`/`KoDirectoryStore.h` 三个头目前全卡在 `QByteArray`
    （R-02），graft_check 现在是 `EXPECT_FAIL`，S-01 实际开工前应该先确认 R-02 状态。
  - `PkZipArchive::openEntryForRead()`/`openEntryForWrite()` 返回的 `PkStream*` 是 `new`
    出来的，调用方必须 `delete`（对齐 `KoQuaZipStore::Private::currentFile` 的所有权）。

### S-02-b：资源存储（推断，未核实批次编号对应关系）

接手 `PkResourceStorage`。

- **接口在哪**：`pk/port/PkResourceStorage.h`。
- **参考适配器该实现成什么样**：对应 `libs/resources/KisStoragePlugin.h`
  两个现存实现（`KisFolderStorage`/`KisBundleStorage`）应该分别改写为 `EntryIterator`
  的具体子类；桌面走真实 POSIX 目录，Android 需要一份基于 `AAssetManager` 的独立实现
  （端口判据的支点就在这里——两边没有共用的目录概念）。
- **已知坑**：
  - `KisStoragePlugin.h` 本身卡在 `QScopedPointer`（R-04），graft_check 现在是
    `EXPECT_FAIL`。
  - `cleanPath()` 越根 `".."` 的行为已按真 Qt 探针改正（§3），不是文档推断的行为。
  - `EntryIterator::lastModified()` 是 `int64_t` 毫秒（Unix epoch UTC），不是
    `QDateTime`——`QDateTime` 归 R-16，落地后如需更丰富的时区/日历语义再加转换方法，
    不改这个字段本身。
  - `rmpath()`/`TagIterator`/`resource()`（`KoResourceSP` 加载）不在本端口范围内，见 §2。

### S-06：字体/文本（推断，未核实批次编号对应关系）

接手 `PkFontProvider`。

- **接口在哪**：`pk/port/PkFontProvider.h`。
- **参考适配器该实现成什么样**：桌面走 fontconfig（`initialize()` 的 `configSearchPath`
  对应 `/etc/fonts` 探测），Android/DirectWrite/CoreText 各自一份实现，但都在拿到
  `FontHandle` 后走同一份 FreeType 做 `FT_New_Face` + 度量（度量不在本端口，见 §2）。
- **已知坑**：
  - `KoFFWWSConverter::addFontFromPattern` 需要整个 `FcCharSet`，端口只有逐码点的
    `coversCodepoint()`——**这一处调用方必须重构**，见 §3。
  - `PkFontQuery::families` 兼载 `FC_POSTSCRIPT_NAME` 查询是判断不是实测，见 §3。
  - `FontEntry` 的字段分工按调用路径钉死（`sortedMatches()`/`allFonts()` 不读
    `postScriptName`/`weight`/`width`/`slant`，`bestMatch()` 不读 `handle`/`languages`）
    ——不要假设"结构体里的字段都会被填"。**评审 I-4：不填的字段读到的是越界哨兵
    （`weight`/`width` = `-1`，`slant` = `Slant::Unknown`），不是"看起来合法"的
    `400`/`100`/`Normal`**，误用可以按值域检出，见 §3。
  - `.kra` 文字图层的 `.kpp` 预设持久化用 `QFont::toString()`/`fromString()`，本端口不管
    这件事（见 §3，归 S-07-a）。

### S-08：图层树镜像/通知转发（推断，未核实批次编号对应关系）

接手 `PkEventSink`。

- **接口在哪**：`pk/port/PkEventSink.h`。
- **参考适配器该实现成什么样**：`libs/command/kis_undo_store.h`（`KisUndoStore`）与
  `libs/image/kis_node_graph_listener.h` 的具体实现改成持有一个 `PkEventSink*`，在原来
  发 Qt 信号的地方改成调用对应事件方法；防腐层再实现一个 `PkEventSink` 子类转发到 Flutter，
  `kis_dummies_facade_base.cpp` 风格的图层树镜像消费者需要覆盖全部 8 个"图层树变更"事件
  （含 `layersChanged()`，遗漏会导致镜像在拼合/撤销后静默过期）。
- **已知坑**：
  - `nodeHasBeenAdded` 的 `dontActivateNode` 是从完整 `KisNodeAdditionFlags` 收窄成的
    单个 `bool`——`QFlags` 归口在 R 线任务表里查不到，见 §3，如果以后这个 flags 加了
    第二位，这里需要重新设计。
  - `imageUpdated()` 依赖 `PkRect`（R-03），未交付前只有空实现、无真实调用链路测试。
  - 跨线程投递不在本端口（同步通知），归 **Q-8/R-10**——不要假设这个类线程安全。

---

## 发现的不一致（本次任务核实中发现，供后续核对）

1. `PkEventSink` 的事件数：`.superpowers/sdd/R-12/progress.md` 与 `task-7-brief.md` 主
   prompt 都写"9 个事件（图层树变更 7 个）"，实测（`grep -c '^    virtual void'
   pk/port/PkEventSink.h`）是 **10 个（图层树变更 8 个）**——评审 I-4 补
   `layersChanged()` 之后旧数字没有跟着更新。本文档 §1.3 已按实测数字写。
2. `graft_check.sh` 的 `EXPECT_FAIL` 行数：`task-7-brief.md` 写"13 个"，精确匹配
   `check_expect_fail` 调用行（`grep -c '^check_expect_fail '`，排除函数定义那一行的
   前缀匹配误报）实测是 **12 个**。本文档 §3 已按实测数字写。
