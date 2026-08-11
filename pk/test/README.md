# pk/test —— QTest 语义兼容层 + moc 替代生成器

零 Qt 依赖的 QTest 兼容测试层（宏名 `PK_*`）+ 一个替代 moc 做测试发现的代码生成器
（`pk_test_moc.py`）。独立 `project(pktest)` 薄壳工程，不接入 Krita 主构建。

## 1. API 范围表（15 项，一项不多一项不少）

口径：`git ls-files '*.cpp' '*.h' '*.cc'` 取路径含 `tests/`、`benchmarks/`、
`sdk/tests/` 的 805 个文件，逐 token 做 `grep -oh -w` 统计。与
`docs/迁移执行计划.md` R34 的官方数字有 ±4% 出入（例：`QCOMPARE` 5017 vs 4978），
因为 R34 未声明它的文件集边界；两者量级一致、排序完全一致，不影响范围判定。

决策依据：`docs/迁移执行计划.md` R34（约第 1082 行）与 D-30 ①（约第 684 行）。
**不在 `docs/Qt替代品选型.md`**——后者全文无 QTest 条目，作为文档差异回报，
未在本任务里修改任何决策文档。

| # | Qt 名 | PK 名 | 实测用量 |
|---|---|---|---|
| 1 | `QCOMPARE` | `PK_COMPARE` | 5017 |
| 2 | `QVERIFY` | `PK_VERIFY` | 4512 |
| 3 | `QVERIFY2` | `PK_VERIFY2` | 252 |
| 4 | `QFETCH` | `PK_FETCH` | 450 |
| 5 | `QFAIL` | `PK_FAIL` | 134 |
| 6 | `QSKIP` | `PK_SKIP` | 5 |
| 7 | `QEXPECT_FAIL` | `PK_EXPECT_FAIL` | 65 |
| 8 | `QTest::addColumn<T>` | `PkTest::addColumn<T>` | 434 |
| 9 | `QTest::addRow` | `PkTest::addRow` | 539 |
| 10 | `QTest::newRow` | `PkTest::newRow` | 269 |
| 11 | `QTEST_MAIN` | `PK_TEST_MAIN` | 2 |
| 12 | `QTEST_APPLESS_MAIN` | `PK_TEST_APPLESS_MAIN` | 2 |
| 13 | `QTEST_GUILESS_MAIN` | `PK_TEST_GUILESS_MAIN` | 18 |
| 14 | `QTest::qExec` | `PkTest::qExec` | 3 |
| 15 | `qFuzzyCompare` | `pkFuzzyCompare` | 28 |

### 附带交付但不计入 15 项的三个垫片

不是 QTest API，是 Krita 自己的测试宏/基类，试接绕不过：

| 名字 | 用量 | 为什么必须给 |
|---|---|---|
| `SIMPLE_TEST_MAIN` | 223 | `sdk/tests/simpletest.h` 定义，试接目标 `KisValueCacheTest` 用它 |
| `KISTEST_MAIN` | 127 | `sdk/tests/kistest.h` 定义。本任务只给一个转发到 `PK_TEST_MAIN` 的最小垫片，资源目录那套归 S0 |
| `QObject` / `Q_OBJECT` / `Q_SLOTS` | 354 个 `private Q_SLOTS:` | 测试类的基类与访问控制。只给测试用最小垫片，真正的 `PkObject` 归 R-05 |

## 2. 明确排除的 5 项 —— 每项都有决策文档依据

| Qt 名 | 用量 | 排除理由 | 归属 |
|---|---|---|---|
| `QBENCHMARK` | 135 | D-30 原文：「`QBENCHMARK`——benchmark 与单元测试分开，M0 的基线用的就是 `benchmarks/` 那套，不走兼容层」 | 不实现 |
| `QBENCHMARK_ONCE` | 14 | 同上 | 不实现 |
| `qWait` | 168 | 事件循环。D-30：约 11 个依赖事件循环的测试「逐个改成同步断言」 | S0 |
| `qSleep` | 36 | 同上 | S0 |
| `QSignalSpy` | 50 | 信号槽。R-11 spec 明写「它不阻塞在 R-05 上」，反过来 R-11 也不实现 R-05 的东西 | R-05 |

未出现在 Krita 里、因此不实现（实测计数为 0）：`QTRY_COMPARE`、`QTRY_VERIFY`、
`QWARN`、`QFETCH_GLOBAL`。

## 3. 覆盖度缺口

「说不出覆盖不到什么的，说明还没想清楚」——逐条列清楚：

- **数据驱动族（`addColumn`/`newRow`/`addRow`/`PK_FETCH`，实测 1692 处调用）
  没有真实 Krita 调用点试接，只有 `tests/selftest_data.cpp` 的 harness 自测覆盖**。
  原因：实测 `libs/*/tests/` 里用 `addColumn` 的 30 个文件，依赖最轻的
  `libs/global/tests/KisGlobalTest.cpp` 也要 `kis_global.h` → `QRect`/`QPoint`/
  `QtGlobal`，落在 R-03 几何的范围里，R-11 拿不到。没有为了补这个缺口去实现
  R-03 的几何类型。
- **`initTestCase_data()` 的全局数据行笛卡尔积没实现**。实测用量为 0
  （`QFETCH_GLOBAL` 计数为 0），生成器仍识别并登记这个槽（`initTestCaseData`），
  runner 把它当普通 fixture 调一次，不展开笛卡尔积。
- **`#if 0` 包住的测试函数声明会被生成器照收，不做特殊处理**。这会多登记一个
  不存在的成员函数，编译期立刻报"没有这个成员"——是响亮的失败，不是静默漏测，
  因此接受，未在生成器里加 `#if`/`#endif` 识别。
- **生成器只扫 `.h`**：测试类若定义在 `.cpp` 里（例如把类声明和实现都写在同一个
  `.cpp` 里，不建头）则发现不到，需要人工确认 Krita 测试是否有这种写法后再补。
- **`compat/QObject` 不是 R-05 的 `PkObject`**：没有信号槽、没有对象树、没有属性
  系统，只是"测试类有个带虚析构的公共基类"这一件事。R-05 交付真正的对象系统
  之后，`compat/QObject` 应改指过去。
- **`compat/QtGlobal` 只给了测试路径上真正用到的标量**：`qint8..quint64`、
  `qreal`、`qAbs`/`qMin`/`qMax`、`qFuzzyCompare`/`qFuzzyIsNull` 的别名。不实现
  `QtGlobal` 的其余部分——多实现一项就是一项没有调用点、没有测试压力、却要跟着
  Qt 语义走的负债。R-02/R-03 交付各自类型时，这里的重叠部分应让给它们。

## 4. 生成器全量扫描：实测数字与口径

```
全量扫描：419 个测试头，发现 371 个测试类 / 2273 个测试函数，生成器失败 0 次
```

- 口径：`git ls-files '*.h' | grep -E '(^|/)(tests|benchmarks)/'`（**含**
  `benchmarks/`，**只扫 `.h`**，不含定义在 `.cpp` 里的测试类）。
- **这个口径没有排除 `pk/` 自己的测试头**：`pk/test/tests/generator_cases/*.h`
  （本任务自己构造的生成器自测输入）与 `pk/string/tests/**`（另一个 R 线任务的
  测试头）路径里也含 `tests/`，会被同一条 `git ls-files` 扫进去——数字比
  Task 5 报告的 416/367/2264（当时 `pk/` 下只有 2 个自测头）略高，差额来自
  Task 6/7 新增的 `compat_shape_case.h` 等自测输入，**不是 Krita 真实测试面变了**。
  严格的"纯 Krita 全仓视角"应该排除 `pk/` 前缀重新统计，本任务未做这一步——
  差额对判定没有影响（见下一条容忍带），留给需要精确数字的人自己排除。
- 对照 D-30「341 个测试类 / 约 2168 个测试函数」：类数 **+8.8%**（371 vs 341），
  函数数 **+4.8%**（2273 vs 2168）——仍在计划给的 ±10% 容忍带内，量级一致，
  未进一步深挖差异来源。已知的口径差异来源：D-30 未声明是否含 `benchmarks/`、
  是否含定义在 `.cpp` 里的测试类、是否排除 `pk/` 自身；本项统计只扫 `.h`。
- 生成器在全部 419 个头上零崩溃。

## 5. 怎么跑

```bash
# harness 自测（选自身 4 类断言/比较/expect-fail/skip/数据驱动/compat）
# + 生成器行为测试 + 全量扫描自证 + 两个真实 Krita 测试类试接，一条命令跑完全部：
./pk/test/tests/run_tests.sh

# 只跑真实测试类试接（判据②，中心判据）：
./pk/test/graft/graft_run.sh
```

`graft_run.sh` 把 `libs/global/tests/KisValueCacheTest.{h,cpp}`（target
`kritaglobal`）与 `libs/pigment/tests/TestKoIntegerMaths.{h,cpp}`（target
`kritapigment`）复制到构建目录，在副本上跑 `rename.sed`（D-23 的 15 条机械改名，
唯一允许的改动），编译时定义 `PK_TEST_NO_QT_MACRO_ALIASES` 关掉 `compat/QTest`
里的 `QCOMPARE→PK_COMPARE` 一类别名——sed 若漏改一处，编译立刻报
`'QCOMPARE' was not declared`，这样试接才真正证明 D-23 的 sed 是机械可行的，
而不是靠别名把漏改的地方悄悄编过。结尾 `git diff --quiet` 自证源树零改动。

## 6. S-00 要接手什么

- **`pk/test/graft/rename.sed` 就是 D-23 全量 sed 的规则表**：15 条规则，
  顺序有讲究（`QVERIFY2` 必须排在 `QVERIFY` 前，`QTEST_APPLESS_MAIN`/
  `QTEST_GUILESS_MAIN` 必须排在 `QTEST_MAIN` 前），S-00 可以直接复用这份表对
  全仓跑 sed。
- **`PK_TEST_NO_QT_MACRO_ALIASES` 的用法**：`compat/QTest` 里的
  `QCOMPARE→PK_COMPARE` 一类别名默认打开，给 S-00 分批 sed 期间的过渡用——
  改名之前的调用点也能先编过。全量 sed 跑完之后应该定义这个宏，把别名关掉，
  让残留的未改名调用点（如果有）编译期报错，而不是被别名悄悄兼容掉。
- **`kritatestsdk` 那 24 个头还没剥**：R-11 只给了两个试接目标绕过去的最小垫片
  （`compat/simpletest.h`、`compat/kistest.h` 的最小转发），`sdk/tests/` 目录下
  完整的测试 SDK（资源目录、`KoTestConfig.h`、`KisSynchronizedConnection` 等）
  没有被替代，S-00 需要的时候要单独立项。
- **`compat/QObject` 会传递 include `compat/QtGlobal`**——这不是随手加的，是
  Task 7 试接 `TestKoIntegerMaths` 时真实压出来的：该测试用 `qAbs` 但自己没
  `#include <QtGlobal>`，靠的是真实 Qt 里 `<QObject>` 传递 include `qglobal.h`
  这条隐式行为。S-00 做全量替换时如果遇到类似"某个标量工具函数没有对应
  include 却能编过"的调用点，先查是不是在吃这条传递性，不要当成漏 include 去改。
