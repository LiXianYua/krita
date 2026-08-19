# pk/namespace —— Qt 命名空间枚举族（R-27 Task 2）

独立 `project(pknamespace)` 薄壳工程，**不接入 Krita 主构建**，不改根 `CMakeLists.txt`。
纯头文件（`PkNamespace.h`），无 `.a` 产物。交付的是真 Qt 5.15.7 `qnamespace.h` 里
`namespace Qt` 的枚举族的替代品：`KeyboardModifier` / `MouseButton` / `Key` /
`AlignmentFlag` / `BrushStyle` / `CursorShape` / `GlobalColor` … 与 pk/flags 的
`PkFlags<Enum>` 模板（`PK_DECLARE_FLAGS`）配合复刻 `Q_DECLARE_FLAGS` 的复数类型。

对齐口径：**与 Qt 的任何位值差异默认都是缺陷。** 参照物是真 Qt 5.15.7
（`/home/liyang/projects-ssd/krita-ci-env/_install`，`QT_VERSION_STR "5.15.7"`），
位值探针证据在 `.superpowers/sdd/R-27/probe_qnamespace.out`。想记成"可接受偏离"的，
逐条写进下面的偏离清单，由 reviewer 判。

## 与 pk/global 的分权（R-18 已交付，只读）

`PkGlobal.h` 已定义 `namespace Qt` 的 `AspectRatioMode` / `Axis` 两个枚举。
**本头不重定义这两个**——重定义会与 R-18 的 `namespace Qt` 同名枚举硬错。同一 TU
同时 include 两份时，两个枚举集合在同一个 `namespace Qt` 里**并集可见**（C++ 允许
同名 namespace 多次打开，只要枚举名不重复）——这正好构成完整 Qt 枚举集。
`tests` 的 `coexistWithGlobalEnums` / `coexistAxisEnum` 两条探针钉住这一点。

## 范围表（2026-08-18 收口）

| 枚举 | 覆盖 | 备注 |
|---|---|---|
| `KeyboardModifier` | 全 8 值 | 位值照抄 qnamespace.h:98-108；`NoModifier=0x0` … `KeyboardModifierMask=0xfe000000` |
| `Modifier` | 全 6 值 | 快捷键短名 `META/SHIFT/CTRL/ALT`，`UNICODE_ACCEL=0` |
| `MouseButton` | 全 36 值 | `NoButton`..`ExtraButton24`，`AllButtons=0x07ffffff`，`MaxMouseButton=ExtraButton24`，`MouseButtonMask=0xffffffff` |
| `Orientation` | 全 2 值 | `Horizontal=0x1` `Vertical=0x2` |
| `FocusPolicy` | 全 5 值 | `StrongFocus=11` `WheelFocus=15` |
| `SortOrder` | 全 2 值 | |
| `SplitBehaviorFlags` | 全 2 值 | `SplitBehavior` 复数（`SkipEmptyParts` 保留范围 40 处） |
| `AlignmentFlag` | 7 值 | 只实现保留范围有用量的成员；`AlignCenter=0x84`；`Alignment` 复数 |
| `TextFlag` | 全 13 值 | `TextWordWrap=0x1000`（保留范围 10 处）；真 Qt **没有** TextFlag 的复数 QFlags |
| `ImageConversionFlag` | 全 19 值 | `AutoColor=0` `ColorOnly=0x3` `PreferDither=0x40`；`ImageConversionFlags` 复数 |
| `Key` | 29 值（裁剪） | 只实现保留范围用量 > 0 + brief 要求的 `Key_A`；见偏离登记 |
| `PenStyle` | 全 7 值 | |
| `PenCapStyle` | 全 3 值 | `FlatCap=0` `SquareCap=0x10` `RoundCap=0x20` |
| `PenJoinStyle` | 全 4 值 | `MiterJoin=0` `BevelJoin=0x40` `RoundJoin=0x80` `SvgMiterJoin=0x100` |
| `BrushStyle` | 全 19 值 | `TexturePattern=24`（见偏离登记） |
| `CursorShape` | 全 23 值 | `LastCursor=DragLinkCursor` |
| `TextFormat` | 全 3 值 | `PlainText=0` `RichText=1` `AutoText=2` |
| `DateFormat` | 4 值 | `TextDate=0` `ISODate=1` `RFC2822Date=8` `ISODateWithMs=9` |
| `TimeSpec` | 全 3 值 | `LocalTime=0` `UTC=1` `OffsetFromUTC=2` |
| `ScrollBarPolicy` | 全 3 值 | |
| `CaseSensitivity` | 全 2 值 | |
| `ConnectionType` | 全 5 值 | `UniqueConnection=0x80` |
| `FillRule` | 全 2 值 | |
| `ClipOperation` | 全 3 值 | `IntersectClip` 保留范围 5 处 |
| `TransformationMode` | 全 2 值 | |
| `LayoutDirection` | 全 3 值 | |
| `CheckState` | 全 3 值 | 支持 `Qt::CheckState::Unchecked` 限定语法（plain enum 名限定，C++17） |
| `ItemDataRole` | 12 值 | `UserRole=0x0100`（保留范围 213 处）；`DisplayRole=0`..`CheckStateRole=10` |
| `ItemFlag` | 全 10 值 | `ItemFlags` 复数（保留范围 8 处） |
| `TimerType` | 全 3 值 | `PreciseTimer=0` `CoarseTimer=1` `VeryCoarseTimer=2` |
| `GlobalColor` | 全 20 值 | `color0=0`..`transparent=19`；QColor 构造依赖序号 |
| `AspectRatioMode` / `Axis` | **不定义** | 已由 pk/global（R-18）提供，分权见上 |

保留范围内用量为 0、**登记缺口但不实现**的成员（判据①）：

| 枚举 | 缺口成员 |
|---|---|
| `AlignmentFlag` | `AlignLeading` `AlignTrailing` `AlignJustify` `AlignAbsolute` `AlignBaseline` `AlignHorizontal_Mask` `AlignVertical_Mask` |
| `CursorShape` | `BitmapCursor` `CustomCursor` |
| `TextFormat` | `MarkdownText` |
| `TimeSpec` | `TimeZone` |
| `DateFormat` | deprecated 的 `SystemLocaleDate` `LocaleDate` `DefaultLocaleShort` 等 2-7 号位 |
| `Key` | 真 Qt 全量 500+ 值，只实现用量 > 0 的 29 个（见偏离登记） |

## 偏离登记

1. **`MaxMouseButton = ExtraButton24`（brief 示例误作 `TaskButton`）。** 真 Qt 5.15.7
   里 `MaxMouseButton = ExtraButton24`（`0x04000000`）。`libs/input/kis_stroke_shortcut.cpp:36`
   的 `std::log2((int)Qt::MaxMouseButton)` 依赖这一位；`kis_shortcut_configuration.cpp` 的
   `BOOST_PP_REPEAT_FROM_TO(4,25,EXTRA_BUTTON)` 实例化 `ExtraButton4..ExtraButton24`——
   故 `ExtraButton4-24` 全量照抄，`MaxMouseButton` 对齐真 Qt。oracle 钉住。
2. **`TexturePattern = 24`（brief 示例暗示 18）。** 真 Qt 5.15.7 里
   `LinearGradientPattern=15` 后留出 18-23 空档，`TexturePattern=24`。oracle 钉住。
3. **`Key` 裁剪**：真 Qt 全量 500+ 值，本头只实现保留范围用量 > 0 的 29 个 + brief
   测试要求的 `Key_A`。规律登记在头注释：可打印键 = ASCII/Unicode 码点，特殊键 =
   `0x01000000` 基址 + 键码。保留范围内用到再补。oracle 只对拍已实现的成员。
4. **`CheckState` 是 plain enum 而非 `enum class`**：照抄 Qt 形态。Krita 用
   `Qt::CheckState::Unchecked` 限定语法（`libs/global/KisMessageBoxWrapper.cpp:27`），
   C++17 允许 plain enum 名限定访问。
5. **`Rotate*Cursor` 不实现**：`DefaultTool.cpp` 里 `Qt::RotateNCursor` 的用法被注释
   掉（实际代码用 `QTransform().rotate(N)`），且真 Qt 没有这些枚举值。

## 三条证据链

- **`tests/`**：`run_tests.sh` —— 构建 + 单测 + **判据③** + locks。
- **`oracle/`**：`run_oracle.sh` —— 链真 Qt5 逐值 static_assert（269 项全对齐）。
- **`graft/`**：`graft_check.sh` —— 4 个 `QFlags<Qt::KeyboardModifier>` 调用形状
  driver 跑绿 + 4 个真实文件 EXPECT_FAIL 锁外登记。

### 判据③口径

本模块是纯头文件（`pknamespace` 是 INTERFACE 目标），**没有 `.a` 库产物**可查
`nm -u`。判据③改由**单测二进制 `test_pknamespace` 代查**：它只链 `pktest` + 本模块头，
真混进 Qt 依赖会在 `nm` 现形。

```bash
nm -u -C pk/namespace/build/test_pknamespace | grep -i qt    # 必须无输出
```

⚠ **`-C` 不能省**：demangle 前的符号名带版本后缀（`Qt_5`），不 demangle 的
`grep -i qt` 会漏掉非 Qt 前缀的真实依赖（同 brief 的判据③要求）。

### `run_tests.sh` 收口时重跑（2026-08-18）

```
Totals: 35 passed, 0 failed, 0 skipped（PkNamespaceCase：33 slot + init + cleanup）
nm -u -C test_pknamespace | grep -i qt: 无输出
git status --porcelain: 改动全部落在 pk/namespace/ 前缀内
```

**测试规模的口径**（数字随代码变，改完重跑以实际值为准）：

| 口径 | 数 | 怎么数的 |
|---|---:|---|
| 测试函数（slot） | 33 | `namespace_case.h` 33 个 `void ...();` |
| 断言（写在源里的） | 270 | `test_namespace.cpp` 的 `PK_COMPARE|PK_VERIFY` 出现次数（本文件无注释内断言、无共享宏展开，源内 = 展开后） |
| 运行输出 `Totals` 行 | 35 | harness 口径：slot 数 + `initTestCase` + `cleanupTestCase`，**不是**断言数 |
| 枚举项 `static_assert`（oracle） | 269 | `difftest_namespace.cpp` 的 `PKN_CHECK` 条数（含 PkGlobal.h 的 6 项） |

### `run_oracle.sh` —— 与真 Qt5 逐值对拍

枚举对拍的判别力**全在编译期**：`difftest_namespace.cpp` 里每个枚举项一条
`static_assert(static_cast<long long>(::Qt::X) == static_cast<long long>(pkoracle::Qt::X))`。
编译过了 = 位值全对齐；编译失败 = 有位值不一致。运行期只打一条 `DIFF total=… mismatch=0`
契约行。`ldd` 必须看得到 `libQt5Core`（`-lQt5Core` 显式给，链不上就 FAIL）——枚举全
编译期内联，链不链 Qt 库运行结果都一样，所以必须靠 ldd 证明两侧真的各链各的。

**收口时重跑（2026-08-18）**：`DIFF total=269 mismatch=0`，`run_oracle.sh: 通过`。

## 锁外处置

**4 个 `QFlags<Qt::KeyboardModifier>` graft**：`libs/flake/KoToolBase.cpp:152`、
`libs/flake/KoToolProxy.cpp:77`、`libs/flake/tools/KoShapeRubberSelectStrategy.cpp:26`、
`plugins/tools/defaulttool/defaulttool/DefaultTool.cpp:145`。这 4 个文件使用
`QFlags<Qt::KeyboardModifier>`——既需要 pk/flags 的 `PkFlags` 模板（R-20 已交付，
锁已释放），也需要本任务的 `Qt::KeyboardModifier` 枚举。它们落在 `libs/flake` /
`plugins/tools`（S 线范围），**不在本任务 locks 内**。

R-27 任务定义②要求「交付后 R-20 的 4 个 QFlags graft EXPECT_FAIL 改 check_pass」，
但 `pk/flags` 归 R-20 所有、已 VERIFIED 释放锁。**本任务不能动 `pk/flags`**。处置：

1. 本任务在 `pk/namespace/graft/` 内独立建了试接证据：`graft_check.sh` 的 EXPECT_PASS
   driver 复刻 4 个真实调用形状（空 `QFlags<Qt::KeyboardModifier>()` 实参、
   `Qt::LeftButton` 单枚举转复数、`Qt::KeyboardModifiers` 默认实参），跑绿。
2. 4 个真实 `.cpp` 各自登记 FIRST 阻塞的**其它**未交付依赖（都到不了 QFlags 行）：
   `KoToolBase.cpp` → `QDebug`（R-08 pk/log）；`KoToolProxy.cpp` /
   `KoShapeRubberSelectStrategy.cpp` → `kritaflake_export`（S-08 剥 flake）；
   `DefaultTool.cpp` → `KoInteractionTool`（S-08 剥 flake）。
3. **报主会话补锁 `pk/flags`**：获准后把 `pk/flags/graft/graft_check.sh` 的 4 条
   `check_expect_fail` 改成 `check_pass`（届时编译行需含 `-I pk/namespace`），重跑
   graft 全绿。实现者未改 `pk/flags` 任何文件。

## 怎么跑

```bash
bash pk/namespace/tests/run_tests.sh     # 构建 + 单测 + 判据③ + locks
bash pk/namespace/oracle/run_oracle.sh   # 与真 Qt5 逐值 static_assert 对拍
bash pk/namespace/graft/graft_check.sh   # 调用形状 driver + 4 文件 EXPECT_FAIL 锁外登记
```
