# pk/flags — `PkFlags<Enum>` 类型安全枚举位标志

## 范围表

### 保留范围 31 文件（`Q_DECLARE_FLAGS` 声明点）

| 文件 | 枚举类型 | 类型 |
|---|---|---|
| `libs/global/KisTransformComponents.h` | `KisAlgebra2D::KisTransformComponents` | namespace 嵌套 enum |
| `libs/global/KoUnit.h` | `KoUnit::ListOptions` | class 嵌套 enum |
| `libs/flake/KoPathPoint.h` | `KoPathPoint::PointProperties / PointTypes` | class 嵌套 enum |
| `libs/flake/KoShapeSavingContext.h` | `ShapeSavingOptions` | plain enum |
| `libs/flake/KoSnapGuide.h` | `Strategies` | plain enum |
| `libs/flake/KoZoomMode.h` | `Modes` | plain enum |
| `libs/flake/text/KoSvgText.h` | `HangingPunctuations / TextDecorations / TextSpaceTrims` | enum class 族 |
| `libs/flake/text/KoSvgTextShape.h` | `DebugElements` | enum class |
| `libs/image/commands/kis_image_layer_add_command.h` | `Flags` | plain enum |
| `libs/image/KisAnimAutoKey.h` | `AutoCreateKeyframeFlags` | plain enum |
| `libs/image/kis_base_rects_walker.h` | `SubtreeVisitFlags` | plain enum |
| `libs/image/KisFakeRunnableStrokeJobsExecutor.h` | `Flags` | plain enum |
| `libs/image/kis_image_animation_interface.h` | `SwitchTimeAsyncFlags` | plain enum（仅 DECLARE，无 OPERATORS） |
| `libs/image/kis_layer_utils.h` | `MergeFlags` | plain enum |
| `libs/image/KisLodPreferences.h` | `PreferenceFlags` | plain enum |
| `libs/image/kis_merge_walker.h` | `Flags` | plain enum |
| `libs/image/KisNodeAdditionFlags.h` | `KisNodeAdditionFlags` | enum class |
| `libs/image/kis_processing_applicator.h` | `ProcessingFlags` | plain enum |
| `libs/image/KisProjectionUpdateFlags.h` | `KisProjectionUpdateFlags` | enum class |
| `libs/image/kis_refresh_subtree_walker.h` | `Flags` | plain enum |
| `libs/image/KisRenderPassFlags.h` | `KisRenderPassFlags` | enum class |
| `libs/image/kis_transaction.h` | `Flags` | plain enum |
| `libs/image/KisUpdaterContextSnapshotEx.h` | `KisUpdaterContextSnapshotEx` | plain enum（无 None 成员） |
| `libs/impex/KisDocument.h` | `OpenFlags` | plain enum |
| `libs/impex/KisImportExportUtils.h` | `SaveFlags` | plain enum |
| `libs/pigment/KoColorConversionSystem_p.h` | `NodeCapabilities` | plain enum |
| `libs/pigment/KoColorConversionTransformation.h` | `ConversionFlags` | plain enum |
| `libs/resources/KoResourcePaths.h` | `SearchOptions` | plain enum |
| `plugins/paintops/libpaintop/kis_current_outline_fetcher.h` | `Options` | plain enum |
| `plugins/paintops/libpaintop/KisTextureOptionData.h` | `KisBrushTextureFlags` | plain enum |
| `plugins/paintops/libpaintop/strokes/freehand_stroke.h` | `Flags` | plain enum |

**全仓（含 tests/benchmarks/已删目录外）**：35 文件（差额 4 落在 D 线已删目录或 `libs/input/` 等非保留范围）。

### API 表

| Qt 成员 | 保留范围用量 | 状态 |
|---|---|---|
| 默认构造 `QFlags()` | 用 | ✅ 实现 |
| `QFlags(Enum)` 隐式转换 | 用 | ✅ 实现 |
| `QFlags(QFlag)` | 用（`PkFlag`） | ✅ 实现 |
| `QFlags(std::initializer_list<Enum>)` | 0 | ✅ 保留（isAffine 先例，R线-spec §判据①） |
| `operator&=(int/uint/Enum)` | 用 | ✅ 实现 |
| `operator|=(QFlags/Enum)` | 用 | ✅ 实现 |
| `operator^=(QFlags/Enum)` | 用 | ✅ 实现 |
| `operator Int()` | 用 | ✅ 实现 |
| `operator|(QFlags/Enum)` | 用 | ✅ 实现 |
| `operator^(QFlags/Enum)` | 用 | ✅ 实现 |
| `operator&(int/uint/Enum)` | 用 | ✅ 实现 |
| `operator~()` | 0（`setFlag` 内部用） | ✅ 保留 |
| `operator!()` | 0 | ✅ 保留（isAffine 先例） |
| `testFlag(Enum)` | 158 处 | ✅ 实现（含 `flag==0` 精确语义） |
| `setFlag(Enum, bool=true)` | 51 处 | ✅ 实现 |
| `QIncompatibleFlag` / `Q_DECLARE_INCOMPATIBLE_FLAGS` | 0 | ❌ 不实现（判据①，非方法） |

## 偏离登记

按 R线-spec「对齐口径：默认全对齐」，以下偏离已逐条论证：

| 偏离 | 理由 |
|---|---|
| `operator!()` 保留但零用量 | R线-spec §判据①「零用量时留着并登记，不要删」（isAffine 先例）。一行代码，留着省 S 线返工。 |
| `initializer_list` 构造保留但零用量 | 同上。 |
| `QIncompatibleFlag` / `Q_DECLARE_INCOMPATIBLE_FLAGS` 不实现 | 判据①「一项不多一项不少」——0 用量，且是独立宏不是方法。 |

## 缺口（已闭合）

### Qt 命名空间枚举常量（`Qt::KeyboardModifier` 等）

**已闭合（R-27 pk/namespace 交付，2026-08-19）**：`Qt::KeyboardModifier` / `Qt::MouseButton` / `Qt::Key` / `Qt::AspectRatioMode` 等 Qt 命名空间枚举族已由 `pk/namespace`（R-27 Task 2）交付，位值对齐 Qt 5.15 qnamespace.h。试接证据：`pk/namespace/graft/instantiate_namespace.cpp` 用真实 `QFlags<Qt::KeyboardModifier>` 调用形状编译跑绿。

4 个直接使用 `QFlags<Qt::KeyboardModifier>` 的真实生产 `.cpp`（`libs/flake/KoToolBase.cpp` / `KoToolProxy.cpp` / `KoShapeRubberSelectStrategy.cpp` / `plugins/tools/defaulttool/DefaultTool.cpp`）在 `pk/namespace/graft/graft_check.sh` 登记，**仍 EXPECT_FAIL 但归因已从「Qt 枚举」移到 S 线依赖墙**（`QDebug`→R-08 pk/log、`kritaflake_export`/`KoInteractionTool`→S-08 剥 flake）——这些文件到不了 QFlags 行，卡在更上游的未交付依赖，归 S 线打通。

## 判据③口径

本模块 `pk/flags/` 是纯头文件模板（`PkFlags<Enum>` 全部 inline），没有可编成 `.a` 的库目标。判据③「`nm -u <产物> | grep -i qt` 无输出」因此没有产物可查——改由单测二进制 `test_pkflags` 代查：

```bash
nm -u -C /tmp/r20-build/test_pkflags | grep -i qt && echo "HAS-QT" || echo "NO-QT-SYMBOLS"
```

`test_pkflags` 只链 `pktest` + `pkflags`（头文件），混进真 Qt 符号会在 nm 现形。

**oracle 程序按设计链真 Qt，它带 Qt 符号是对的，不在判据③统计内**（R线-spec「工程形态」表）。

## 试接降级说明

真实压 QFlags 的测试类（`TestCompositeOpInversion.cpp` 等）在 `libs/pigment`/`libs/resources`/`libs/flake` 等大模块，编它们需要该模块自己的 CMake 导出头 + `QTransform`/`QColor`/`QMetaType`/`QImage` 等几十个未交付类型（依赖墙），本任务 `locks=pk/flags` 物理编不过。

降级路径（R线-spec「依赖墙挡住真实测试类时」四条件全部满足）：
1. `graft/instantiate.cpp` 逐行复刻真实调用点形状（`KisNodeAdditionFlags` 的操作 + 校验值来自真 Qt 探针）
2. 4 个纯声明头（`KisNodeAdditionFlags.h`/`KisProjectionUpdateFlags.h`/`KisRenderPassFlags.h`/`KisUpdaterContextSnapshotEx.h`）零改动编译通过
3. driver 显式标注「不是真实测试文件，是复刻调用点形状的 driver」
4. 指名依赖墙：S-06（kritaimage）/S-03（pigment）/S-05（flake）把对应大模块剥完 Qt 后即可补真实测试类跑绿

## 工程形态

- **目录**：`pk/flags/`，与 `locks: [pk/flags]` 一一对应
- **命名**：全局 `Pk` 前缀，不引入 C++ namespace
- **C++ 标准**：C++17
- **compat 垫片**：`pk/flags/compat/QFlags`（无扩展名）+ `#define`
- **对拍源**：`pk/flags/oracle/`，独立工程
- **构建**：独立 `project()` 薄壳工程
- **测试**：R-11 的 PK_* harness
- **性能**：不预先优化