# sdk/tests —— 测试基础设施 Qt 剥离状态

**S-00 的产出。** 本文件记录 24 个测试基础设施头在 S-00 与 S-06 之间的分工、
已完成的剥离、已知缺口。**数字现场数，不要抄这里——它们会随 S-06 推进变化。**

## 开工前基线数字

**S-00 初测（Task 1 Step 1）** 时 `sdk/tests/` 中 Q* 符号现状：

| 指标 | 数值 | 说明 |
|---|---|---|
| Q* 符号种类数 | 47 | 不同的 Q 开头符号 |
| 命中文件数 | 20 | 包含 Q* 符号的头文件与源文件 |
| 总出现次数 | 568 | 所有 Q* 符号的总出现频次（订正：Task 1 原记 579 有笔误，Task 4 实现者与 Task 6 派发方两次独立复核均为 568，种类数 47 与文件数 20 不变） |

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
| `KisMeasureAvgPortion.h` | 已剥离（零 Qt，nm -u 自证） |
| `KisRectsCollisionsTracker.h` | 已剥离（零 Qt，nm -u 自证） |
| `qimage_test_util.h` | 阻塞，见「已知缺口」 |
| `simpletest.h` | 阻塞，见「已知缺口」 |

**说明**：前两个头由 Task 2/3 剥离 Qt、端口化到 Pk 类型，Task 4 用薄壳探针
`S00TestInfraProbe` 编过并以 `nm -u ... | grep -i qt` 为空自证符号层零 Qt；
后两个头本轮不改类型，保留源树——阻塞原因与归属见下方「已知缺口」章节。

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

**Task 1**：完成基线测数、零消费方验证、分类表骨架落盘
**Task 2**：剥离 Qt、端口化 `KisRectsCollisionsTracker.h` 到 Pk 类型
**Task 3**：剥离 Qt、端口化 `KisMeasureAvgPortion.h` 到 Pk 类型
**Task 4**：加薄壳探针 `S00TestInfraProbe`，`nm -u` 自证符号层零 Qt
**Task 5**：定义事件循环测试改造模式，标注 `sdk/tests` 内 10 处调用点（不改行为）
**Task 6**（S-00 收尾）：补「已知缺口」章节 + 五族像素比对入口剥离前后对照表 +
本表状态列回填为实际完成情况

---

**本表由 Task 实现者逐步填充，最后更新于 Task 6（2026-08-17）**

---

## 事件循环测试改造模式（S-00 交付，S-02-a/S-05-b/S-06/S-08/S-09-e 照办）

**背景**：保留范围内实测 18 个测试依赖 `QEventLoop`/`processEvents`/
`QSignalSpy`/`qWait`/`QTimer`，4 个在 `sdk/tests`（本节列出的 4 个头，
对应下表 10 处调用点——同一个测试可能命中同一个头的多处调用点，
所以"测试数"与"调用点数"不是同一个口径，10 不是测试数），14 个不在
`sdk/tests` 下（10 个在 `libs/image` + 1 个 `libs/flake` + 1 个
`libs/global` + 1 个 `plugins/impex/libkra` + 1 个 `plugins/tools`；
Task 5 原文将"10 处调用点"误写为"10 个测试"，此处订正）。

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

---

## 已知缺口

**`qimage_test_util.h` 本轮未做类型替换**——`checkQImage`/`compareQImages`
系列的目标类型 `PkImage` 是 R-15，**当前 `NOT_STARTED`**（见 `docs/TASKS.md`
第 76 行）。即使造一个过渡实现，23+ 消费方（`libs/ui`/`plugins/impex` 等）
都还是 Qt target，函数签名换了当场编不过——这些不在 S-00 `locks` 内，改了
是越界。**归属**：等 R-15 交付后，由消费这份工具的目标所在的 S 批次接手
（多数消费方在 `libs/ui`，尚未指派 S 编号；`plugins/impex/*` 系列已有
S-05-a/S-05-b/待定 后续批次）。**五族入口的语义在此期间完全不变**——本
文件没有改动 `checkQImage` 的任何一行代码或默认参数值。

**`simpletest.h` 本轮未做类型替换**——`SIMPLE_TEST_MAIN`/`SIMPLE_MAIN_IMPL`
创建 `QApplication`、调 `KisSynchronizedConnectionBase::
setAutoModeForUnittestsEnabled`、`QTest::qExec`。Pk 世界的测试进程要不要
有一个"应用对象"、事件循环残留怎么处理，是跨 R-05（自写信号槽）/R-10
（Q-8 并发与线程模型）才能回答的设计问题，S-00 单独回答不了也验证不了
（这两个 R 任务都 `NOT_STARTED`）。**归属**：R-05 的事件模型细节 + R-10
定型后补。

**`KoProgressProxy` 无需新端口**——R-12 §1.2 已确认它已经是端口形态
（D0.5 `git mv` 到 `libs/global/KoProgressProxy.h`，零 `Q_OBJECT`，唯一
Qt 依赖是 2 处 `const QString&` 形参，R-01 的 `compat/QString` 顶得住）。
`sdk/tests/testutil.h` 里 `#include <KoProgressProxy.h>` 是 S-06 锁内的
头，S-00 未修改。**这一条无需后续任务再起单独工作，登记为"已满足"。**

**`plugins/metadata` 的 `kritaui` 边卡在 `testui.h`**——D-02-f 交接给 S-00
的一条：`plugins/metadata/tests/CMakeLists.txt:6` 链 `kritaui`，走
`kis_exif_test.cpp:12` `#include "testui.h"`。`testui.h` 只是
`#include "kistest.h"` 一行转发，本身零 `kritaui` 依赖——它属于 S-00
分类表里的 S-06 组（依赖 kritaimage，经 `kistest.h` 传递），**不在
S-00 本轮的 4 个可动头之列**。这条边真正的断点在 `kis_exif_test.cpp`
本身用没用到 `kistest.h` 里 UI 相关的东西，S-00 判断不了，转交 S-06
处理时核对。

**`QDebug` 残留不是真 Qt 残留，是宏别名手法的正常表现**——Task 4 发现剥离
后 `QDebug` 这个符号在 `sdk/tests/` 里的出现次数没变（5 次），原因是
`KisMeasureAvgPortion.h` 用 `#include "compat/QDebug"` 这种别名手法
（`pk/log/compat/QDebug` 里 `#define QDebug PkDebug`）——源码层正则匹配的
是符号文本 `QDebug`，天然看不穿宏定义，所以文本计数"没变"，但实际类型
已经是 `PkDebug`。这一点已被 Task 4 的符号层 `nm -u ... | grep -i qt` 交叉
验证过（该薄壳探针链接产物为空，不含 Qt 符号）——**无害的预期内现象，
不是剥漏了**。下一个只看源码层文本计数的人不要被"QDebug 还在"误导。

**`ENTER_FUNCTION()` 是一条既有的隐式 include-order 依赖**——
`KisRectsCollisionsTracker.h:42` 用了 `ENTER_FUNCTION()` 宏，但该文件本身
并没有 `#include` 定义它的头。这个宏定义在 `libs/global/kis_debug.h`
（该头目前仍是真 Qt 实现，尚未被剥离——是 S-02-a 的锁）。当前之所以能编
过，是因为消费方（该头的 include 者）在别处已经间接 `#include` 了
`kis_debug.h`——**这不在 S-00 范围内处理**，只在此记录，供将来剥
`kis_debug.h` 的任务（S-02-a）知道：改掉 `ENTER_FUNCTION()` 的定义或去掉
该头的 Qt 实现时，`KisRectsCollisionsTracker.h` 这一处隐式依赖也要一并
核对。

---

## 五族像素比对入口：剥离前后对照

| 族 | 定义处 | S-00 锁内？ | 默认容差剥离前 | 默认容差剥离后 |
|---|---|---|---|---|
| `checkQImage`/`checkQImagePremultiplied`/`checkQImageExternal` | `qimage_test_util.h` | 是 | `fuzzy=0, fuzzyAlpha=-1→0, maxNumFailingPixels=0` | **不变**（本轮未改类型，见「已知缺口」） |
| `compareQImages`/`compareQImagesPremultiplied` | `qimage_test_util.h` | 是 | `0, 0, 0` | **不变** |
| `QImageBasedTest::check*` | `qimage_based_test.h` | 否（S-06） | `baseFuzzyness=0` + `root`/`blur1`/`shape` 隐式 `+1` | 不适用（S-00 未触碰此文件的比对逻辑，仅 Task 5 加了 1 处注释） |
| `TestUtil::testFiles` | `filestest.h` | 否（S-06） | `fuzzy=0, maxNumFailingPixels=0` | 不适用（同上，Task 5 仅加注释） |
| `utils::StrokeTester` | `stroke_testing_utils.cpp` | 否（S-06） | `m_baseFuzziness=1` | 不适用（S-00 未触碰） |

**结论**：五族像素比对入口的默认容差与失败判定逻辑，S-00 全程**零改动**——
前两族因 R-15 阻塞保持原类型未改；后三族本轮未锁定、未触碰。像素比对能力
未被削弱。
