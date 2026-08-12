# pk/geometry —— 零 Qt 的几何 POD 类型与标量工具（R-03）

独立 `project(pkgeometry)` 薄壳工程，**不接入 Krita 主构建**，不改根 `CMakeLists.txt`。
全局 `Pk` 前缀、不引 C++ namespace（compat 垫片靠 `#define QRect PkRect` 工作，
而 Krita 里有 `class QRect;` 前置声明，套 namespace 这个技巧就废）。

对齐口径：**与 Qt 的任何行为差异默认都是缺陷。** 参照物是真 Qt 5.15.7
（`/mnt/ssd-disk/liyang/projects/krita-ci-env/_install`，`QT_VERSION_STR "5.15.7"`）。
想记成"可接受偏离"的，逐条写进下面的偏离清单，由 reviewer 判。

## 怎么跑

```bash
./pk/geometry/tests/run_tests.sh
```

它做四件事：配置并构建独立工程 → 跑 `test_pkgeometry` → `nm -u libpkgeometry.a |
grep -i qt` 必须无输出（判据③）→ `git status --porcelain` 自证改动只落在
`pk/geometry/` 前缀内（`locks`）。

单测走 R-11 的 PK_* harness：`pk/test` 由 `add_subdirectory(... EXCLUDE_FROM_ALL)`
拉进来，**只用不改**。测试类声明必须在独立 `.h` 里（`pk_test_moc.py` 只扫 `.h`）。

## 现在有什么

| 文件 | 内容 |
|---|---|
| `PkGlobal.h` / `PkGlobal.cpp` | `qreal` `qAbs` `qMin` `qMax` `qBound` `qRound` `qFuzzyCompare` `qFuzzyIsNull` `qIsNaN` `qInf` |
| `compat/QtGlobal` | `<QtGlobal>` 垫片，是 `pk/test/compat/QtGlobal` 的超集 |
| `tests/` | `test_global.cpp`（14 个测试函数）+ 三个共存 TU `coexist_*.cpp` |

几何类型（`PkPoint`/`PkSize`/`PkRect`/`PkRectF`/`PkTransform`）、`graft/` 试接、
`oracle/` 逐输入对拍由后续 Task 交付。

## 与 `pk/test/compat/QtGlobal` 的共存

两份同名垫片会在试接时同时出现在编译行上（`-I pk/test/compat -I pk/geometry/compat`）。
**"两份共用同一个 include guard 宏名让后来者空转"这条路走不通**：`pk/test` 那份用的是
`#pragma once`，认的是文件身份，两个不同文件各自都会落地一次；而那份文件不在 R-03 的
`locks` 里，不能改。

代之以三个机制，完整说明在 `PkGlobal.h` 顶部，三种 include 顺序各有一个 TU
（`tests/coexist_test_first.cpp` / `coexist_geometry_first.cpp` /
`coexist_pkglobal_first.cpp`）编译并核对取值。**这三个 TU 能编过本身就是断言的一半**
——去掉任一机制它们立刻编译失败。

## Qt 语义里必须照抄、看着像 bug 的地方

都已在 `tests/test_global.cpp` 里逐条钉住，防止后来者"顺手修正"：

- **`qRound` 对负半值向 +∞ 取整**，不是"远离零"：`qRound(-0.5) == 0`、
  `qRound(-1.5) == -1`、`qRound(-2.5) == -2`。实测真 Qt 5.15.7 确认。
- **`int(d + 0.5)` 让 `qRound(0.49999999999999994)` 得 1**（和落在 1.0 中点上、
  ties-to-even 有关），不是 0。
- **`qFuzzyCompare` 的右端取 `qMin(|p1|, |p2|)`**，所以任何一侧是 0 时永远返回 false
  ——`qFuzzyCompare(0.0, 1e-300)` 与反向都是 false。这与 `pk/test` 的
  `pkFloatingCompare` 不同（那边对 0 走 `pkFuzzyIsNull` 分支），别混为一谈。
- **`qBound(min, val, max)` 在 `min > max` 时返回 `min`**（实现是
  `qMax(min, qMin(max, val))`），不是断言失败也不是 `max`。
- **`qMin`/`qMax`/`qBound` 返回 `const T&`**：实参是字面量时返回的引用只在那条
  full-expression 内有效，跨语句用就悬垂。测试里对字面量调用先拷进具名变量。

## 偏离清单

| # | 偏离 | 理由 |
|---|---|---|
| 1 | `qFuzzyCompare`/`qFuzzyIsNull` 去掉 Qt 原文的 `static` / `Q_REQUIRED_RESULT` / `Q_DECL_UNUSED` | `static` 只影响链接性（每 TU 一份内部副本），另两个是编译器诊断属性。**无行为差异。** |
| 2 | 不做 `qIsNaN(float)` / `qIsFinite` / `qIsInf` 等 `qnumeric.h` 的其余成员 | 用量表只点名 `qIsNaN`(19)/`qInf`(1)，且实参都是 `double`；`float` 实参隐式提升到 `double`，取值一致。**少做一项，不是行为差异。** |
| 3 | 不实现 `qRound64` | 实测 0 调用点（R-03 用量表）。判据①「一项不多」。 |
| 4 | `pk/test/compat/QtGlobal` 先进 TU 时，`qAbs`/`qFuzzyCompare`/`qFuzzyIsNull` 用的是它那份实现 | 写法不同、语义等价，两处差异已逐条核对：`t >= T(0)` vs `t >= 0` 对全部算术类型等价；`std::fabs`/`std::fmin` vs `qAbs`/`qMin` 只在实参含 NaN 时取值不同，而那种情况下两边的 `<=` 都被 NaN 拉成 false。**无行为差异。** |

## 覆盖度缺口

「说不出覆盖不到什么的，说明还没想清楚」：

- **本 Task 的期望值来自人工跑真 Qt 探针，不是自动逐输入对拍。** `oracle/`
  尚未落地；它交付时要把这 10 项标量工具一并纳入自动对拍，别只对拍几何类型。
- **`qint8`..`quint64` 那批整数 typedef 不在 `compat/QtGlobal` 里**——R-03 的用量表
  没点名它们，归 R-02（容器）。真实调用点出现时按那条线补。
- **`qglobal.h`/`qnumeric.h` 的其余成员一概不做**（`qIsNull`、`qIsInf`、`qIsFinite`、
  `qQNaN`、`qSNaN`、`qFpClassify`、`qFloatDistance`、`Q_INFINITY`/`Q_QNAN` 宏……）。
  用量表只点名了上面那 10 项；后续 Task 或 S 线撞上别的成员时，**先补实测用量再决定**，
  不要顺手加。
- **`qFuzzyCompare` 只有 `double`/`float` 两个重载**（和 Qt 一样）。Qt 另有
  `QPointF`/`QSizeF`/`QRectF`/`QTransform` 等类型的同名重载，散在各自的头文件里，
  归对应类型的 Task，不在本文件。
- **共存机制③（`PK_GEOMETRY_WITH_PKTEST_COMPAT`）是个要人主动打开的开关。**
  漏打开的表现是响亮的编译错误（`qAbs` 重定义），不是静默错行为，因此接受。
