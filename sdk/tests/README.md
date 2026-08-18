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
