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

**测试规模的口径**（数字随代码变，改完重跑以实际值为准；下面的计数脚本口径是
「先去掉 `//` 注释，再数 `PK_VERIFY|PK_VERIFY2|PK_COMPARE` 的出现次数」——
Task 1 报的 117/93 把 `test_global.cpp:42` **注释里**的一条 `PK_COMPARE` 数进去了，
正确值是 116/92）：

| 口径 | 数 | 怎么数的 |
|---|---:|---|
| 测试函数 | 38 | `tests/cases/*.h` 里 `private Q_SLOTS:` 下声明的 slot 个数 = `global_case.h` 12 + `point_case.h` 26 |
| 断言（展开后） | 313 | `test_global.cpp` 116（= 直接写在函数体里的 92 + 共享宏 `PK_CHECK_COEXIST_PROBE` 体内 12 条被两个 coexist 测试函数各展开一次 = 24）+ `test_point.cpp` 197（无共享宏） |
| `static_assert` | 31 | `PkPoint.cpp` 28（布局 / constexpr 能力，只有在一个 TU 里才落得了地）+ `oracle/geometry_difftest.cpp` 3 |
| 运行输出 `Totals` 行 | 14 + 28 | harness 的口径：每个测试类的 slot 数 + `initTestCase` + `cleanupTestCase`，**不是**测试函数数，也不是断言数 |
| 翻译单元 | 6 | `test_main` `test_global` `test_point` `coexist_test_first` `coexist_geometry_first` `point_macro_proof` |
| 对拍比对次数 | 2 648 280 | `run_oracle.sh` 输出的 `DIFF total=`，其中 `mismatch=3` 全是 canary（见下） |

## 现在有什么

| 文件 | 内容 |
|---|---|
| `PkGlobal.h` / `PkGlobal.cpp` | `qreal` `qAbs` `qMin` `qMax` `qBound` `qRound` `qFuzzyCompare` `qFuzzyIsNull` `qIsNaN` `qInf`，以及**宏改写不到**的 `pkQtFuzzyCompare` / `pkQtFuzzyIsNull`（见下） |
| `PkPoint.h` / `PkPoint.cpp` | `PkPoint`（两个 `int`）与 `PkPointF`（两个 `qreal`），逐字抄自 `qpoint.h` |
| `compat/QtGlobal` `compat/QPoint` `compat/QPointF` | `#define` 垫片，无扩展名 |
| `tests/` | `test_global.cpp`（12 函数 / 116 断言）、`test_point.cpp`（26 函数 / 197 断言）、两个共存 TU `coexist_*.cpp`、宏改写探针 `point_macro_proof.cpp` |
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
| 5 | Qt 的宏映射到 C++17：`Q_DECL_CONSTEXPR`→`constexpr`、`Q_DECL_RELAXED_CONSTEXPR`→`constexpr`、去掉 `Q_CORE_EXPORT` / `Q_DECLARE_TYPEINFO` / `QT_WARNING_*` | 分别是可见性属性、容器移动优化提示、`-Wfloat-equal` 的诊断压制。**都不进入可观察行为**，且 2 648 280 次逐输入对拍零真实差异。`Q_DECL_RELAXED_CONSTEXPR` 在 C++14 起就是 `constexpr`，`PkPoint.cpp` 用 `static_assert` 钉住"放宽 constexpr 真的能在编译期改状态"。 |
| 6 | **运算符按 Qt 头文件全集实现**，不按实测调用点裁剪 | 运算符无法按调用点 grep 归属（`a + b` 认不出类型），因此**没有实测用量数字**可依。这违反判据①「一项不多」的字面要求，**登记在案请 reviewer 判**。理由：运算符是值类型的基本语法面，漏一个就让调用点编不过，而"哪些漏了"在替换之前无法测量。范围 = `qpoint.h` 里为 `QPoint`/`QPointF` 声明的全部运算符，一个不多一个不少。 |
| 7 | `PkPointF::isNull()` 直接写 `xp == 0.0 && yp == 0.0`，不引入 `qIsNull` 这个名字 | `qglobal.h:925-928` 的 `qIsNull(double d)` 就是 `d == 0.0`，逐字等价（实测 `QPointF(-0.0,-0.0).isNull() == true`、`5e-324` 为 `false`，两侧一致）。不把 `qIsNull` 提进 `compat/` 是因为它在保留范围内实测 **0 调用点**，导出去才是违反判据①。 |

## 覆盖度缺口

「说不出覆盖不到什么的，说明还没想清楚」：

- **`PkGlobal.h` 那 10 项标量工具本身没有进自动对拍**，它们的期望值来自人工跑真
  Qt 探针（`tests/test_global.cpp`）。`oracle/` 是经由 `PkPoint`/`PkPointF` **间接**
  压到 `qRound`/`qAbs`/`qFuzzy*` 的（`toPoint`、`operator*`、`operator==` 都调它们），
  但 `qMin`/`qMax`/`qBound`/`qIsNaN`/`qInf` 一次都没被对拍碰到。后续 Task 可以在
  `geometry_difftest.cpp` 里加一节直接对拍标量工具，成本很低。
- **对拍程序用 `-fwrapv` 编译。** `manhattanLength`/`operator+`/`dotProduct` 在
  `INT_MIN`/`INT_MAX` 上是**有符号溢出 UB**（Qt 自己就这么写，替代品照抄），不加
  `-fwrapv` 时 `-O2` 会拿"溢出不可能发生"去推导取值范围——实测的表现是
  `std::to_string` 打印溢出后的负数时**段错误**。加上之后 `-O0`/`-O0 -fwrapv`/
  `-O2 -fwrapv` 三种编法的 `total=2648280 mismatch=3` **逐字相同**，说明 `-fwrapv`
  只是挡住优化器、没有改变取值。**但这意味着：涉及整数溢出与 `int(inf)` 这类 UB 的
  断言（单测里也有几条）是"在本机、本编译器、这组旗标下与 Qt 一致"，不是语言保证。**
  换平台或换 `-O` 等级时这几条要重测；它们钉的是"和 Qt 一样"，Qt 换了行为它们也该跟着换。
- **浮点→int 越界（`int(inf)`、`int(1e308)`）`-fwrapv` 管不着**，是另一类 UB。
  实机上两侧都编成同一条 `cvttsd2si`，取值一致（`+inf`→`INT_MIN`、`-inf`→`0`）；
  这是实测事实，不是保证。
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
- **`oracle/` 的输入是 token 全组合，不是穷举。** Point 族用 21 个 double token
  与 20 个 int token 做二元点（21²/20² 个点）、二元 API 再做两点全组合，加上
  三层的标量参数组合，合计 2 648 280 次比对。**每个 API 摊到的输入数差别很大**：
  二元 API 各约 20 万次，而 `toPoint`/`isNull`/`transposed` 这些一元 API 只有约
  485 次（441 个 token 点 + 44 个手挑点）。一元 API 上的窄边界 bug 只能靠 token
  集里有没有那个形态挡住 —— 注入 `toPoint` 截断实测抓到 236 处，够用，但这是
  **输入集选得对**，不是穷举保证。
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
