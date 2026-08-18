# sdk/tests —— 测试基础设施 Qt 剥离状态

**S-00 的产出。** 本文件记录 24 个测试基础设施头在 S-00 与 S-06 之间的分工、
已完成的剥离、已知缺口。**数字现场数，不要抄这里——它们会随 S-06 推进变化。**

## 开工前基线数字

**S-00 初测（Task 1 Step 1）** 时 `sdk/tests/` 中 Q* 符号现状：

| 指标 | 数值 | 说明 |
|---|---|---|
| Q* 符号种类数 | 47 | 不同的 Q 开头符号 |
| 命中文件数 | 20 | 包含 Q* 符号的头文件与源文件 |
| 总出现次数 | 579 | 所有 Q* 符号的总出现频次 |

**Top 10 频繁符号**：

| 符号 | 出现次数 |
|---|---|
| QString | 156 |
| QStandardPaths | 58 |
| QRect | 41 |
| QStringList | 30 |
| QImage | 30 |
| QFileDevice | 21 |
| QFileInfo | 19 |
| QLocale | 17 |
| QTransform | 15 |
| QPoint | 12 |

## 24 头分类（依赖 kritaimage 的归 S-06，其余归 S-00）

判定方法：逐头列 `#include`，凡链到 `libs/image/*.h`（直接或通过 `kistest.h`
间接）的归 S-06。

### S-00（4 个，零 kritaimage 依赖）

| 头 | S-00 状态 |
|---|---|
| `KisMeasureAvgPortion.h` | 未开始 |
| `KisRectsCollisionsTracker.h` | 未开始 |
| `qimage_test_util.h` | 不改（本轮保留） |
| `simpletest.h` | 不改（本轮保留） |

**说明**：前两个头由 Task 2/3 处理剥离 Qt、端口化到 Pk 类型；后两个本轮不改类型，保留源树（有独立的消费方清单，S-06 推进时不受影响）。

### S-06（20 个，依赖 kritaimage，由 S-06 任务处理）

| 头 | 说明 |
|---|---|
| `empty_nodes_test.h` | 图层树测试工具 |
| `filestest.h` | 文件 I/O 测试工具 |
| `KisDumbAnimatedTransformMaskParamsHolder.h` | 变换蒙版测试支撑 |
| `KisDumbTransformMaskParams.h` | 变换蒙版参数测试支撑 |
| `kistest.h` | 核心测试基础设施（依赖 kritaimage） |
| `KisTransformMaskTestingListener.h` | 变换蒙版事件监听测试 |
| `KritaTransformMaskStubs.h` | 变换蒙版桩类 |
| `lod_override.h` | LOD 级别重写测试工具 |
| `qimage_based_test.h` | QImage 基础测试框架（依赖 kritaimage） |
| `stroke_testing_utils.h` | 笔画测试工具 |
| `testbrush.h` | 笔刷测试工具 |
| `testflake.h` | flake 库测试工具 |
| `testimage.h` | 图像测试工具 |
| `testing_nodes.h` | 节点测试工具 |
| `testing_timed_default_bounds.h` | 时间边界测试工具 |
| `testpigment.h` | 颜料测试工具 |
| `testresources.h` | 资源测试工具 |
| `testui.h` | UI 测试工具 |
| `testutil.h` | 通用测试工具 |
| `ui_manager_test.h` | UI 管理器测试工具 |

**消费方清单**（归 S-06 处理）：
- `qimage_based_test.h`（flake、image 相关测试）
- `testimage.h`（大量 image 库测试）
- `testbrush.h`、`testpigment.h`（笔刷/颜料资源测试）
- 其余各头均在对应库的测试树中使用

---

## 各头处理进度

**Task 1（当前）**：完成基线测数、零消费方验证、分类表骨架落盘
**Task 2**：剥离 Qt、端口化 `KisRectsCollisionsTracker.h` 到 Pk 类型
**Task 3**：剥离 Qt、端口化 `KisMeasureAvgPortion.h` 到 Pk 类型
**Task 5**（S-00 收尾）：更新本表的状态列为实际完成情况

---

**本表由 Task 实现者逐步填充，最后更新于** [待更新]

---

## 事件循环测试改造模式（S-00 交付，S-02-a/S-05-b/S-06/S-08/S-09-e 照办）

**背景**：保留范围内实测 18 个测试依赖 `QEventLoop`/`processEvents`/
`QSignalSpy`/`qWait`/`QTimer`，10 个在 `sdk/tests`（本节列出的 4 个头），
14 个分布在 `libs/image`(10)/`libs/flake`(1)/`libs/global`(1)/
`plugins/impex/libkra`(1)/`plugins/tools`(1)。

**两种根因，两种改法**：

1. **等待已经同步完成的操作**（如 `KisImage::waitForDone()`/
   `tryBarrierLock()`——`libs/image` 的 `KisUpdateScheduler` 用
   `QThreadPool` 跑在独立工作线程，不依赖调用方线程的 Qt 事件循环推进）：
   **直接删掉 `processEvents()`/`qWait()`，只保留同步等待调用**。这类
   `processEvents()` 纯粹是历史遗留的保险动作，不删也不出错，删了才是
   诚实反映"这条路径本来就是同步的"。

2. **等待真正依赖 Qt 事件循环推进的定时器合并**（`testutil.h` 注释明写
   "Shape updates have two channels of compression, 100ms each. One in
   `KoShapeManager`, the other one in `KisShapeLayerCanvas`"——这是
   `QTimer` 驱动的写合并去抖，不推进事件循环这个定时器永远不触发）：
   **不能靠"删掉 processEvents 换成 sleep"敷衍**——sleep 不会让一个挂在
   Qt 事件循环上的 `QTimer` 触发，语义会假绿（编译过、测试可能因为纯偶然
   的调度顺序凑巧过，但没有真正验证去抖逻辑）。**正确改法**：去抖机制本身
   在对应模块被剥离时（`libs/flake` 的 `KoShapeManager`/
   `KisShapeLayerCanvas`，S-08 的锁）需要新增一个**显式同步 flush 方法**
   （例如 `KoShapeManager::flushPendingUpdatesSync()`），改造后的测试直接
   调用它，不再需要任何形式的等待/轮询。**这个 flush 方法目前不存在，
   是 S-08 交付的一部分，不是测试代码单方面能补的**——S-00 在调用点留
   `// PATTERN-2` 标注，指向本节，不改变行为（改了也无法验证，因为
   `libs/flake` 还没剥、没法编译试跑）。

**S-00 对这 10 处调用点的处置**（本轮**只加标注，不改行为**——验证需要
`kritaimage`/`kritaui` 编过，S-00 没有这些依赖，改了无法验证等于制造
未经验证的假设）：

| 文件 | 行号（实测，2026-08-17） | 根因分类 | 处置 |
|---|---|---|---|
| `filestest.h` | 82 | ①（`doc->image()->waitForDone()` 紧随其后） | 标 `PATTERN-1`，S-06 接手时删 |
| `filestest.h` | 212 | ①（同上，紧随 `doc->image()->waitForDone()`） | 标 `PATTERN-1` |
| `filestest.h` | 267 | ①（同上） | 标 `PATTERN-1` |
| `filestest.h` | 305 | ①（同上） | 标 `PATTERN-1` |
| `filestest.h` | 363 | ①（同上） | 标 `PATTERN-1` |
| `qimage_based_test.h` | 164 | ②（**实测订正**：本处在 `addShapeLayer()` 内，紧随两次 `shapeLayer->addShape(...)`，**该文件全篇没有任何 `waitForDone()` 调用**——计划撰写阶段假设的"紧随 waitForDone()"不成立。语义上更贴近 shape layer canvas 的排队事件处理，与 `KoShapeManager`/`KisShapeLayerCanvas` 的去抖机制同域） | 标 `PATTERN-2`（订正自计划里的 `PATTERN-1`），需要 S-06 与 S-08 共同确认 |
| `ui_manager_test.h` | 89 | ②（析构函数注释明写"weird way of processing pending events"，紧跟 `QTest::qSleep(500)`，是等 dummies facade 处理排队信号，信号槽投递本身可能靠 R-05 的自写信号槽变同步——需要 S-06/S-08 双方确认） | 标 `PATTERN-2`，需要 S-06 与 S-08 共同确认后处置 |
| `ui_manager_test.h` | 91 | ②（同上） | 标 `PATTERN-2` |
| `testutil.h` | 398 | ①（紧随 `image->waitForDone()`） | 标 `PATTERN-1` |
| `testutil.h` | 409 | ②（`qWait(500)` 轮询等 `tryBarrierLock`，注释明写等 shape 层 100ms 去抖） | 标 `PATTERN-2` |

（上表行号已按 S-00 实测核对，与计划撰写阶段的行号一致；`qimage_based_test.h:164`
一处的**分类**做了订正——计划原文写的例证"紧随 waitForDone()"在实测中不成立，
详见该行说明。）
