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
grep -i qt` 必须无输出（判据③）→ 自证改动只落在 `pk/geometry/` 前缀内（`locks`）。

最后那条用 `git status --porcelain -- . ':(exclude)pk/geometry'` **非空即失败**，
不解析 porcelain 的输出文本。按列切文本的写法在两种真实情形下会失灵，两种都实测
复现过：改名行 `R  pk/geometry/x -> pk/other/x` 切出来仍以 `pk/geometry/` 开头，
越界改名被放过；含空格/非 ASCII 的路径 git 默认加引号转义，切出来以 `"` 开头，
合规改动被误判越界。

单测走 R-11 的 PK_* harness：`pk/test` 由 `add_subdirectory(... EXCLUDE_FROM_ALL)`
拉进来，**只用不改**。测试类声明必须在独立 `.h` 里（`pk_test_moc.py` 只扫 `.h`）。

**测试规模的口径**（数字随代码变，改完重跑以实际值为准）：

| 口径 | 数 | 怎么数的 |
|---|---:|---|
| 测试函数 | 12 | `tests/cases/global_case.h` 里 `private Q_SLOTS:` 下声明的 slot 个数 |
| 断言 | 117 | `tests/test_global.cpp` 里 `PK_VERIFY`/`PK_COMPARE` 展开后的条数 = 直接写在函数体里的 93 条 + 共享宏 `PK_CHECK_COEXIST_PROBE`（体内 12 条）被两个 coexist 测试函数各展开一次 |
| 运行输出 `Totals` 行 | 14 | harness 的口径：12 个 slot + `initTestCase` + `cleanupTestCase` 两个自动函数，**不是**测试函数数，也不是断言数 |
| 翻译单元 | 4 | `test_main.cpp` `test_global.cpp` `coexist_test_first.cpp` `coexist_geometry_first.cpp` |

## 现在有什么

| 文件 | 内容 |
|---|---|
| `PkGlobal.h` / `PkGlobal.cpp` | `qreal` `qAbs` `qMin` `qMax` `qBound` `qRound` `qFuzzyCompare` `qFuzzyIsNull` `qIsNaN` `qInf` |
| `compat/QtGlobal` | `<QtGlobal>` 垫片，是 `pk/test/compat/QtGlobal` 的超集 |
| `tests/` | `test_global.cpp`（12 个测试函数 / 117 条断言）+ 两个共存 TU `coexist_*.cpp` |

几何类型（`PkPoint`/`PkSize`/`PkRect`/`PkRectF`/`PkTransform`）、`graft/` 试接、
`oracle/` 逐输入对拍由后续 Task 交付。

## 与 `pk/test/compat/QtGlobal` 的共存

两份同名垫片会在试接时同时出现在编译行上（`-I pk/test/compat -I pk/geometry/compat`）。
**"两份共用同一个 include guard 宏名让后来者空转"这条路走不通**：`pk/test` 那份用的是
`#pragma once`，认的是文件身份，两个不同文件各自都会落地一次；而那份文件不在 R-03 的
`locks` 里，不能改。

代之以两个机制，完整说明在 `PkGlobal.h` 顶部，两种 include 顺序各有一个 TU
（`tests/coexist_test_first.cpp` / `coexist_geometry_first.cpp`）编译并核对取值。
**这两个 TU 能编过本身就是断言的一半**——去掉任一机制它们立刻编译失败。

**第三种顺序**（先直接 `#include "../PkGlobal.h"`、之后才撞上 `pk/test` 那份垫片）
**不覆盖**：Krita 源码写的是 `#include <QRect>` / `#include <QtGlobal>`，一定经过
compat 垫片，落在上面两种里；够得到第三种顺序的只有手写库头路径的 TU，而那种 TU
只存在于本目录自己的测试里（唯一消费者就是测它自己的那个 TU，自我指涉）。
判据①「一项不多一项不少」——没有真实场景就不做。Task 7 试接时若真撞上这个形态，
把 `pk/geometry/compat/QtGlobal` 的那两行 `__has_include` 抄进库头即可，代价很小。

**另一半断言必须包含零侧语义。** 这两条路径上 `qAbs`/`qFuzzyCompare`/`qFuzzyIsNull`
都让位给了 `pk/test` 的实现，而 `pk/test` 不在 R-03 的 `locks` 里、R-11 随时可能动它。
探针只取非零点时，给 `pkFuzzyCompare` 注入一个「任一侧为 0 就走 `fuzzyIsNull`」的
分支（真实的对 Qt 偏离）两个 TU 会全绿——所以 `PkCoexistProbe` 里有
`fuzzyZeroA`/`fuzzyZeroB` 两个字段，钉住 `qFuzzyCompare(0.0, 1e-300)` 与反向都是
`false`（真 Qt 5.15.7 实测）。

## Qt 语义里必须照抄、看着像 bug 的地方

都已在 `tests/test_global.cpp` 里逐条钉住，防止后来者"顺手修正"：

- **`qRound` 对负半值向 +∞ 取整**，不是"远离零"：`qRound(-0.5) == 0`、
  `qRound(-1.5) == -1`、`qRound(-2.5) == -2`。实测真 Qt 5.15.7 确认。
- **`int(d + 0.5)` 让 `qRound(0.49999999999999994)` 得 1**（和落在 1.0 中点上、
  ties-to-even 有关），不是 0。
- **`qFuzzyCompare` 的右端取 `qMin(|p1|, |p2|)`**，所以任何一侧是 0 时永远返回 false
  ——`qFuzzyCompare(0.0, 1e-300)` 与反向都是 false。这与 `pk/test` 的
  `pkFloatingCompare` 不同（那边对 0 走 `pkFuzzyIsNull` 分支），别混为一谈。
  是 `qMin` 不是 `qMax` 这一点靠**判别输入**钉住，不能只用 `|p1| ≈ |p2|` 的用例
  （那片区域里 `qMin ≈ qMax`，换成 `qMax` 一条都不会红）：
  `qFuzzyCompare(999999999999.5, 1000000000000.5)` 与 float 的
  `qFuzzyCompare(99999.5f, 100000.5f)` 在真 Qt 下都是 false，`qMax` 变体下都是 true。
- **`qAbs(-0.0)` 返回 `-0.0`，不是 `+0.0`**：条件写的是 `t >= 0`，`-0.0 >= 0` 为真，
  于是原样返回。`signbit(qAbs(-0.0)) == 1`、`1.0/qAbs(-0.0) == -inf`（真 Qt 5.15.7
  实测）。改成 `t > 0` 会把零号规范成 `+0.0`——那是会经 `1/x`/`atan2`/`copysign`
  扩散的真实行为差异。`PK_COMPARE(qAbs(0), 0)` 对它免疫（`-0.0 == 0.0`），
  必须用 `std::signbit` 直接查符号位。
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
| 4 | `pk/test/compat/QtGlobal` 先进 TU 时，`qAbs`/`qFuzzyCompare`/`qFuzzyIsNull` 用的是它那份实现 | 写法不同、语义等价，两处差异已逐条核对：`t >= T(0)` vs `t >= 0` 对全部算术类型等价（含 `-0.0`，两边都原样返回 `-0.0`）；`std::fabs`/`std::fmin` vs `qAbs`/`qMin` 只在实参含 NaN 时取值不同，而那种情况下两边的 `<=` 都被 NaN 拉成 false。**无行为差异。** 这条不是口头断言：`tests/coexist.h` 的探针把让位路径上的取值（含零侧语义 `fuzzyZeroA`/`fuzzyZeroB`）逐条钉住，`pk/test` 漂离 Qt 时两个 coexist TU 会变红。 |

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
- **共存只覆盖两种 include 顺序**（见上「与 `pk/test/compat/QtGlobal` 的共存」）。
  第三种顺序没有真实场景，故意不做；真撞上的表现是响亮的编译错误
  （`qAbs` 重定义），不是静默错行为。
- **让位路径上的取值校验只有探针里那 10 项**（`PkCoexistProbe` 的字段）。
  `pk/test` 若在这 10 项之外的地方漂离 Qt（例如 `pkFuzzyIsNull` 的阈值），
  coexist TU 不会发现。`oracle/` 落地后应把标量工具纳入自动逐输入对拍，
  这才是根治。
