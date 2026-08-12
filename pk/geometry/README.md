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

```bash
./pk/geometry/oracle/run_oracle.sh
```

它链**真 Qt5** 做逐输入对拍：定位真 Qt → 编译（`ldd` 必须看得到 `libQt5Core`）→ 跑 →
把每条 `DIFFTAG` 与 `oracle/geometry.deviation` 双向核对。出现未声明的差异就 FAIL。
方法论与 tag 的两条硬规则写在 `oracle/geometry_difftest.cpp` 的文件头。

**测试规模的口径**（数字随代码变，改完重跑以实际值为准）。计数口径：
**先去掉 `//` 注释再数** `PK_VERIFY|PK_VERIFY2|PK_COMPARE` 的出现次数——注释里
写着的断言不算，不去注释就会系统性多数（实测踩过）：

| 口径 | 数 | 怎么数的 |
|---|---:|---|
| 测试函数 | 36 | `tests/cases/*.h` 里 `private Q_SLOTS:` 下声明的 slot 个数 = `global_case.h` 12 + `point_case.h` 24 |
| 断言（展开后） | 315 | `test_global.cpp` 116（= 直接写在函数体里的 92 + 共享宏 `PK_CHECK_COEXIST_PROBE` 体内 12 条被两个 coexist 测试函数各展开一次 = 24）+ `test_point.cpp` 199（无共享宏） |
| `static_assert` | 29 | `PkPoint.cpp` 26（布局 / constexpr 能力，只有在一个 TU 里才落得了地）+ `oracle/geometry_difftest.cpp` 3 |
| 运行输出 `Totals` 行 | 14 + 26 | harness 的口径：每个测试类的 slot 数 + `initTestCase` + `cleanupTestCase`，**不是**测试函数数，也不是断言数 |
| 翻译单元 | 6 | `test_main` `test_global` `test_point` `coexist_test_first` `coexist_geometry_first` `point_macro_proof` |
| 对拍比对次数 | 35 569 662 | `run_oracle.sh` 输出的 `DIFF total=`，其中 `mismatch=3` 全是 canary（见下） |

**优化档矩阵**（`-fwrapv` 由 `CMakeLists.txt` 的 `target_compile_options(... PUBLIC)`
统一带上；`-fno-wrapv` 那一列是手工编译出来的对照，不是可用配置）：

| `-O` 档 | `-fno-wrapv` | `-fwrapv`（实际构建） |
|---|---|---|
| `-O0` | 全绿 | 全绿 |
| `-O1` | 全绿 | 全绿 |
| `-O2` | 全绿 | 全绿 |
| `-Os` | **红**：`pointManhattanLength()` | 全绿 |
| `-O3` | 全绿 | 全绿 |

`-Os` 那一格就是 `-fwrapv` 存在的理由（有符号整数溢出）。**另一类 UB（浮点→int
越界）`-fwrapv` 管不着**，靠 `tests/test_point.cpp` 的 `noFold()` 把转换压到运行期
——没有它时 `-O1`/`-O2` 会把 `int(2147483648.0)` 在编译期折成另一个答案，
`pointfToPointMatchesQt()` 变红，整套单测事实上锁死在 `-O0`。

## 现在有什么

| 文件 | 内容 |
|---|---|
| `PkGlobal.h` / `PkGlobal.cpp` | `qreal` `qAbs` `qMin` `qMax` `qBound` `qRound` `qFuzzyCompare` `qFuzzyIsNull` `qIsNaN` `qInf`，以及**宏改写不到**的 `pkQtFuzzyCompare` / `pkQtFuzzyIsNull`（见下） |
| `PkPoint.h` / `PkPoint.cpp` | `PkPoint`（两个 `int`）与 `PkPointF`（两个 `qreal`），逐字抄自 `qpoint.h` |
| `compat/QtGlobal` `compat/QPoint` `compat/QPointF` | `#define` 垫片，无扩展名 |
| `tests/` | `test_global.cpp`（12 函数 / 116 断言）、`test_point.cpp`（24 函数 / 199 断言）、两个共存 TU `coexist_*.cpp`、宏改写探针 `point_macro_proof.cpp` |
| `oracle/` | `geometry_difftest.cpp`（对拍骨架 + Point 族）、`run_oracle.sh`、`geometry.deviation` |

`PkSize`/`PkRect`/`PkRectF`/`PkTransform`、`graft/` 试接由后续 Task 交付；
它们往 `oracle/geometry_difftest.cpp` 里**加节**，不另起文件。

### `pkQtFuzzyCompare` / `pkQtFuzzyIsNull`：为什么几何类型不能写 `qFuzzy*`

`PkPointF::operator==` 照 Qt 抄的是 `qFuzzyIsNull` / `qFuzzyCompare` 这两个名字，
而在「`pk/test` 那份垫片先进 TU」这条**真实**的共存路径上，这两个名字是
`#define`（→ `pkFuzzyCompare` / `pkFuzzyIsNull`）——预处理器会把函数体当场改写到
`pk/test` 的实现上去，几何类型的相等语义于是静默换了一套，而 `pk/test` 不在 R-03 的
`locks` 里、R-11 随时可能动它。

所以公式住在 `pkQtFuzzy*` 这组名字里（宏改写不到），`qFuzzy*` 只是转发；
几何类型内部一律调 `pkQtFuzzy*`。**`oracle/` 覆盖不到这一条**（对拍的编译行里
根本没有 `pk/test` 的垫片，两边都走 Qt 公式），靠 `tests/point_macro_proof.cpp`
这个独立 TU 钉住：它把宏指向一对恒返回 `false` 的破坏版实现，再核对
`PkPointF::operator==` 的取值仍与真 Qt 一致。

**⚠ 被污染的 include 必须落进匿名 `namespace`（Task 3–6 抄这条纪律）。**
在预处理期改写过语义的 TU 会编出**同名同签名、函数体却不同**的 inline 实体
（`operator==(const PkPointF&, const PkPointF&)`、`qAbs<double>` …）。它们默认以
**弱符号**发射，链接器只保留一份，于是：探针调到的可能根本不是自己编出来的那份
（判别力归零：把 `pkQtFuzzy*` 改回 `qFuzzy*` 时单测与对拍会一起绿灯放过），
反过来破坏版也可能赢、把干净 TU 的断言变成假红。把整包 include 包进匿名
`namespace` 就压成内部链接，链接顺序再影响不了任何东西。三个 TU
（`point_macro_proof.cpp`、两个 `coexist_*.cpp`）都这么做，纪律只有一条：
**系统头留在 `namespace` 之外**，否则会造出 `(anonymous)::std`。

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
| 5 | Qt 的宏映射到 C++17：`Q_DECL_CONSTEXPR`→`constexpr`、`Q_DECL_RELAXED_CONSTEXPR`→`constexpr`、去掉 `Q_CORE_EXPORT` / `Q_DECLARE_TYPEINFO` / `QT_WARNING_*` | 分别是可见性属性、容器移动优化提示、`-Wfloat-equal` 的诊断压制。**都不进入可观察行为**，且 35 569 662 次逐输入对拍零真实差异。`Q_DECL_RELAXED_CONSTEXPR` 在 C++14 起就是 `constexpr`，`PkPoint.cpp` 用 `static_assert` 钉住"放宽 constexpr 真的能在编译期改状态"。 |
| 6 | **运算符按 Qt 头文件全集实现**，不按实测调用点裁剪 | 运算符无法按调用点 grep 归属（`a + b` 认不出类型），因此**没有实测用量数字**可依。这违反判据①「一项不多」的字面要求，**登记在案请 reviewer 判**。理由：运算符是值类型的基本语法面，漏一个就让调用点编不过，而"哪些漏了"在替换之前无法测量。范围 = `qpoint.h` 里为 `QPoint`/`QPointF` 声明的全部运算符，一个不多一个不少。 |
| 7 | `PkPointF::isNull()` 直接写 `xp == 0.0 && yp == 0.0`，不引入 `qIsNull` 这个名字 | `qglobal.h:925-928` 的 `qIsNull(double d)` 就是 `d == 0.0`，逐字等价（实测 `QPointF(-0.0,-0.0).isNull() == true`、`5e-324` 为 `false`，两侧一致）。不把 `qIsNull` 提进 `compat/` 是因为它在保留范围内实测 **0 调用点**，导出去才是违反判据①。 |

## 覆盖度缺口

「说不出覆盖不到什么的，说明还没想清楚」：

- **`PkGlobal.h` 那 10 项标量工具本身没有进自动对拍**，它们的期望值来自人工跑真
  Qt 探针（`tests/test_global.cpp`）。`oracle/` 是经由 `PkPoint`/`PkPointF` **间接**
  压到 `qRound`/`qAbs`/`qFuzzy*` 的（`toPoint`、`operator*`、`operator==` 都调它们），
  但 `qMin`/`qMax`/`qBound`/`qIsNaN`/`qInf` 一次都没被对拍碰到。后续 Task 可以在
  `geometry_difftest.cpp` 里加一节直接对拍标量工具，成本很低。
- **⚠ 整数溢出这一整类，我们比的是"钉死之后的行为"，`pkgeometry` 与对拍程序
  都用 `-fwrapv` 编译（`CMakeLists.txt` 里 `target_compile_options(pkgeometry
  PUBLIC -fwrapv)`，对拍在 `run_oracle.sh` 的 `CXXFLAGS_ORACLE` 里）。**
  `manhattanLength`/`operator+`/`dotProduct` 在 `INT_MIN`/`INT_MAX` 上是有符号
  溢出 —— **两侧（Qt 与替代品）都是 UB**，编译器有权给出任何结果。`-fwrapv` 把
  两侧一起钉成二补数回绕，之后再比，于是"逐输入一致"这句话才有确定的含义。
  **这不等于 Krita 发布构建里的行为**：Krita 不带 `-fwrapv`，同一段 Qt 代码在
  那里的取值由优化器自由裁量，可能与本目录测出来的不同。我们保证的是
  "在同一组旗标下两侧一致"，不是"和 Krita 线上跑出来的一样"，更不是语言保证。
  不加 `-fwrapv` 的实测后果：`-Os` 下 `pointManhattanLength()` 变红；对拍那边
  `-O2` 无 `-fwrapv` 时 `std::to_string` 打印溢出后的负数直接**段错误**。
  加上之后各 `-O` 档取值与 `-O0` 逐字相同（矩阵见上），说明它只挡优化器、不改取值。
- **浮点→int 越界（`int(inf)`、`int(2147483648.0)`）是另一类 UB，`-fwrapv` 管不着。**
  实机上运行期两侧都编成同一条 `cvttsd2si`，取值一致（`+inf`/`2147483648.0`→`INT_MIN`、
  `-inf`/`nan`→`0`）；但**编译期常量折叠给的是另一个答案**，所以单测里这批断言
  一律经 `noFold()`（一次 `volatile` 读）把转换压到运行期。这是实测事实，不是保证。
- **⚠ 换平台要重测，点名 Android / ARM。** 上面两条 UB 的取值
  （`int(inf)`、`int(2147483648.0)`、`qAbs(INT_MIN)` 回绕、`INT_MAX+1` 回绕）
  两侧一致，是 **x86-64 / GCC 13 / 本机**的实机事实：x86 的 `cvttsd2si` 越界时返回
  "整数不定值" `INT_MIN`，而 **AArch64 的 `fcvtzs` 是饱和语义**（正越界给 `INT_MAX`、
  `nan` 给 0），取值不同。R-03 的最终目标平台正是 Android/ARM，**这批断言与对拍
  结论都要在目标平台上重跑一遍**；重跑时要对齐的仍是"两侧一致"，不是本文件里
  写下的具体数字。
- **`PK_COMPARE` 对 `double` 走的是 `pk/test` 的模糊比较（相对 1e-12），不是位相等**
  ——R-11 harness 的能力边界，跨线，R-03 内不修。凡是主张"与 Qt 逐位一致"的断言
  一律用 `PK_VERIFY(sameBits(...))` 或 `std::signbit`，`test_point.cpp` 里已经这么做了。
  没有机器闸门拦住后来者误用 `PK_COMPARE` 比浮点。
- **`qHash` / `QDataStream operator<<>>` / `QDebug operator<<` 不实现**：前者是哈希
  容器的地基，归 R-02；后两者归 R-12 端口 / R-08 日志。保留范围（3 325 个文件）内实测：
  `QHash<QPoint*` / `QSet<QPoint*` / `QMap<QPoint*` **0 次**；
  Krita 自己在 `plugins/tools/tool_transform2/kis_mesh_transform_strategy.cpp:23` 写了
  `uint qHash(const QPoint &value)`（**自备的，不是 Qt 的**）；
  `QHash<QRect, ...>` **1 次**（`libs/image/kis_suspend_projection_updates_stroke_strategy.cpp:121`）
  ——**这是 R-02 要接的**，Task 4/5 交付 `PkRect` 时要再点一次名。
  `QDataStream` 配 `QPoint*` **0 次**；`qDebug() << QPoint*` 只有 1 处、还在注释里。
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
  coexist TU 不会发现。`oracle/` 已经落地，但**它跑的是不带 `pk/test` 垫片的编译行**，
  帮不到这一条；根治办法是把标量工具做成一节直接对拍（见上）并让 coexist 探针
  覆盖更多取值。
- **`oracle/` 的输入是全组合，不是穷举。** 两组输入集，两组都做**满**：
  手挑对抗集 44 个 double / 25 个 int，token 集 21 个 double / 20 个 int；
  一元 API 走 44²+21² = 2 377 个 double 点、25²+20² = 1 025 个 int 点，
  二元 API 走 44⁴+21⁴ = 3 942 577 组 double 两点、25⁴+20⁴ = 550 625 组 int 两点，
  带标量参数的 API 做三层，合计 35 569 662 次比对。
  **手挑集必须做全组合，不能只做索引轮转** —— 轮转时 44 个手挑值只产生 44 个点，
  `kHandD` 里那批 `kTokD` 没有的值（`0.5000000000000001`、`1e-323`、
  `2.2250738585072014e-308`、`-2147483649.0`、`4294967296.0`、`1.0+1e-13`、
  `2e-12`、`±0.25`、`±3.5` …）永远无法同时出现在 x 和 y 上；复评实测：注入一个
  只在 `x == y == 2147483648.0` 时触发的 `toPoint` 缺陷，轮转版**全绿放过**，
  全组合版抓到 1 处。**分量更多的族（Rect 8 个、Transform 18 个）做不了 44⁸，
  至少要做「手挑 × token」的交叉，保证每个分量位上都取得到手挑值。**
  即便如此，覆盖仍然靠**输入集选得对**（注入 `toPoint` 截断实测抓到 930 处，
  分布在 `half-boundary`/`near-half-ulp`/`out-of-int-range` 三个根因 tag 上），
  不是穷举保证。
- **对拍不覆盖预处理期的语义偷换。** `oracle/` 的编译行里没有 `pk/test` 的垫片，
  `qFuzzy*` 那两个 `#define` 永远不生效，所以「几何类型的 `==` 被改写到 `pk/test`
  的实现上」这类问题对拍看不见。靠 `tests/point_macro_proof.cpp` 单独钉住。
- **`graft/` 试接还没做**（Task 7）。现在证明 API 形状对不对的只有本目录自己的
  单测与对拍，没有一个真实 Krita 调用点编译过 `PkPoint`。

## 与决策文档 / 实施计划的差异（**只报告，不修改那些文档**）

1. **文件集规模：实测 3 325，`Qt替代品选型.md` 说 3 321。** 口径 =
   `git ls-files` ∩ 保留范围前缀 ∩ 扩展名 `.cpp`/`.h`/`.cc` − 路径含
   `tests/`|`benchmarks/` − `pk/`。差 4 已查清：文档那条口径用了大小写不敏感的
   `test` 子串过滤**整条路径**，误伤 `KisTransformMaskTestingInterface.{cpp,h}`
   （"Testing"）与 `ShapeRotateStrategy.{cpp,h}`。3 325 − 4 = 3 321，完全对上。
2. **⚠ 实施计划把 `dotProduct` 列进「族内全文 0 次、明确不实现」——实测证伪。**
   真实调用点：`plugins/tools/basictools/kis_tool_measure.cc:139` 的
   `QPointF::dotProduct(diff, offset)`。根因与计划里已经点名过的
   `QTransform::fromTranslate` 完全相同：那份用量导出的 `occ` 字段只数 `.name(`
   与 `->name(`，**静态调用落在别处**。（这个文件还是 `.cc`，只数 `.cpp` 的口径
   会再漏一次。）本 Task 已实现 `PkPoint::dotProduct` / `PkPointF::dotProduct`，
   单测与对拍都覆盖了。**Task 3–6 必须自己重跑一遍「0 次」清单，不要照抄计划**
   ——Size/Rect/Transform 族的「0 次」名单里大概率还有同类漏网（凡是 Qt 里声明成
   `static` 的成员都要单独查一遍）。
   **`dotProduct` 的完整实测**（口径：上面那 3 325 个文件，数
   `.dotProduct(` / `->dotProduct(` / `::dotProduct(` 三形态）：合计 **13 处 / 7 个文件**，
   全部是 `::` 形态。归属拆开是 `QPointF::dotProduct` **1 处**（`kis_tool_measure.cc:139`，
   本 Task 实现的就是它）、Krita 自备的 `KisAlgebra2D::dotProduct` **5 处**、
   `QVector2D::dotProduct` **4 处**、`QVector3D::dotProduct` **3 处**
   ——后 12 处都不是 `QPoint` 族，`QVector2D/3D` 还不在 R-03 交付范围内（见上文）。

3. **⚠ 实施计划把 `transposed` 列进 Point 族「有用量、必须实现」——实测证伪，
   本 Task 不实现它。** 口径同上（3 325 个文件，`.transposed(` / `->transposed(` /
   `::transposed(` 三形态）：`transposed` 合计 **5 处 / 2 个文件，全部是
   `QTransform::transposed()`**（`libs/global/kis_algebra_2d.cpp:935` 的
   `globalToLocal.transposed()`，`globalToLocal` 声明在同文件 :930 是 `QTransform`；
   加 `plugins/tools/tool_transform2/kis_free_transform_strategy_gsl_helpers.cpp:356-359`
   的 4 处，实参都是 `QTransform` 成员）。`QPoint`/`QPointF` 上 **0 处**。
   顺带查了形近的 `transpose`：**5 处 / 3 个文件，全部是 Eigen 矩阵**
   （`KisBezierUtils.cpp:1201`、`kis_algebra_2d.cpp:949` 与 :957 的注释、
   `kis_selection_filters.cpp:525`），与 Qt 无关。
   按判据①「一项不多」，Point 族的 `transposed` 删掉；**那个名字的调用点属于
   `QTransform`，归 Task 6 的 `PkTransform`**，到时候按这 5 处实现。
