# pk/test —— QTest 语义兼容层 + moc 替代生成器

零 Qt 依赖的 QTest 兼容测试层（宏名 `PK_*`）+ 一个替代 moc 做测试发现的代码生成器
（`pk_test_moc.py`）。独立 `project(pktest)` 薄壳工程，不接入 Krita 主构建。

## 1. API 范围表（17 项，一项不多一项不少）

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
| 16 | `QTest::currentDataTag` | `PkTest::currentDataTag` | 15 |
| 17 | `QTest::qFail` | `PkTest::qFail` | 1 |

### 这张表是 17 项，D-30 记的是 15 项

`docs/迁移执行计划.md` D-30 ① 写的是「API 面实测 15 个」，但从未逐项枚举是哪 15 个；
本表是实测导出的，逐项列全之后是 **17 项**，多出来的两项：

- `QTest::currentDataTag`（15 处真实调用点，分布在 6 个文件：
  `libs/flake/tests/TestSvgParser.cpp`、`TestSvgTextShape.cpp`、
  `libs/image/tests/kis_clone_layer_test.cpp`、
  `kis_liquify_transform_worker_test.cpp`、
  `plugins/color/lcms2engine/tests/TestIccFromColorimetryConversion.cpp`、
  `plugins/paintops/libpaintop/tests/kis_linked_pattern_manager_test.cpp`）
- `QTest::qFail`（R-11 复评补登，1 处真实调用点：
  `libs/pigment/tests/TestColorConversionSystem.cpp:85`，`QVERIFY2`/`QCOMPARE`
  等断言宏在 Qt 里内部就是靠它记失败，这里是**唯二**被真实调用点直接调用、
  不经宏的 `QTest::` 成员之一——另一个是下面 §2 判定排除的 `QTest::qCompare`）

计数口径：`git ls-files '*.cpp' '*.h' '*.cc'` 里路径含 `tests/`、`benchmarks/`、
`sdk/tests/` 的文件，逐 token `grep -oh -w` 统计，用量 > 0 且属于 QTest 语义面的
条目各计一项；宏与 `QTest::` 成员函数不分开计。

**决策文档只有人能改**——这里只报差异，未修改 D-30，也没有为了凑 15 项把
`currentDataTag` 藏起来：这张表同时是 S-00 全量 sed 的规则来源
（`graft/rename.sed`），藏一项就是 sed 漏一项。

### 附带交付但不计入 16 项的三个垫片

不是 QTest API，是 Krita 自己的测试宏/基类，试接绕不过：

| 名字 | 用量 | 为什么必须给 |
|---|---|---|
| `SIMPLE_TEST_MAIN` | 223 | `sdk/tests/simpletest.h` 定义，试接目标 `KisValueCacheTest` 用它 |
| `KISTEST_MAIN` | 127 | `sdk/tests/kistest.h` 定义。本任务只给一个转发到 `PK_TEST_MAIN` 的最小垫片，资源目录那套归 S0 |
| `QObject` / `Q_OBJECT` / `Q_SLOTS` | 354 个 `private Q_SLOTS:` | 测试类的基类与访问控制。只给测试用最小垫片，真正的 `PkObject` 归 R-05 |

## 2. 明确排除的 9 项 —— 每项都有理由与归属

| Qt 名 | 用量 | 排除理由 | 归属 |
|---|---|---|---|
| `QBENCHMARK` | 135 | D-30 原文：「`QBENCHMARK`——benchmark 与单元测试分开，M0 的基线用的就是 `benchmarks/` 那套，不走兼容层」 | 不实现 |
| `QBENCHMARK_ONCE` | 14 | 同上 | 不实现 |
| `qWait` | 168 | 事件循环。D-30：约 11 个依赖事件循环的测试「逐个改成同步断言」 | S0 |
| `qSleep` | 36 | 同上 | S0 |
| `QSignalSpy` | 50 | 信号槽。R-11 spec 明写「它不阻塞在 R-05 上」，反过来 R-11 也不实现 R-05 的东西 | R-05 |
| `QTest::keyClicks` | 12 | GUI 键盘事件模拟，要的是 `QWidget` + 事件循环，零 Qt 之后没有可模拟的对象。唯一调用点 `libs/widgetutils/tests/kis_parse_spin_boxes_test.cpp` 测的是 `QDoubleSpinBox` 子类，属 D3 判定「随 `libs/ui` 一起删除的 GUI 测试」 | 随 GUI 测试删除 |
| `QTest::currentTestFunction` | 4 | 返回当前测试函数名。唯一调用点 `plugins/paintops/libpaintop/tests/kis_linked_pattern_manager_test.cpp` 拿它拼临时资源文件名——那条路径依赖的是 S0 才交付的资源目录，在资源系统剥完之前实现这个取值器没有可验证的调用点 | S0 |
| `QTest::currentTestFailed` | 1 | 返回当前测试函数是否已失败。唯一调用点 `libs/image/tests/kis_mask_similarity_test.cpp` 用它决定失败时是否落盘对比图，是**诊断产物**而非判定逻辑，去掉不改变任何测试的通过/失败结论 | 不实现 |
| `QTest::qCompare` | 1 | R-11 复评补登。唯一调用点是 `sdk/tests/testutil.h:59` 的 `KIS_COMPARE_FLT` 宏（`libs/*` 全仓 24 处调用该宏），但该宏体自己还依赖 `qreal`/`qRound`——这两个 Qt 标量类型本表 §3 已经记过「零真实调用点不预先实现，归 R-02/R-03」，`qCompare` 现在有了调用点，但依赖它的宏本身还编不过，实现了也没有可跑的真实调用点验证。等 `qreal`/`qRound` 落地后随 `KIS_COMPARE_FLT` 一起补 | R-02/R-03（跟随 `qreal`/`qRound`） |

未出现在 Krita 里、因此不实现（实测计数为 0）：`QTRY_COMPARE`、`QTRY_VERIFY`、
`QWARN`、`QFETCH_GLOBAL`。

**`QTest::` 命名空间成员这一部分，本次复评已重新核实完整**：按本节开头同样的
口径（`git ls-files '*.cpp' '*.h' '*.cc'`，路径含 `tests/`/`benchmarks/`/
`sdk/tests/`，排除 `pk/` 自身，本次重新枚举命中 796 个文件——与本文档别处的
805、复评员的 816 有几个百分点出入，来源同 §1 已说明的文件集边界问题，量级
一致）对全部 `\bQTest::[A-Za-z_][A-Za-z0-9_]*` 做过一次去重枚举：命中的 12
个不同成员——`addColumn`/`addRow`/`newRow`/`currentDataTag`/`qExec`/`qFail`
（本表 §1，6 项）、`qWait`/`qSleep`/`keyClicks`/`currentTestFunction`/
`currentTestFailed`/`qCompare`（本表 §2，6 项）——与两张表的登记逐一对上，
没有第 13 个 `QTest::` 成员。

**不带命名空间前缀的 `Q*` 测试宏这一部分，本次复评没有重新逐项核实**——
沿用的是本文档原有的口径与结论。用同样的口径顺手扫了一遍作为交叉检查时，
额外发现一个**本次未处理**的缺口：`QCOMPARE_NE`（Qt 6.4+ 新增的比较宏族之一，
`QCOMPARE_EQ`/`_LT`/`_LE`/`_GT`/`_GE` 的同族成员，语义上仍属 QTest 的一部分，
只是不带 `QTest::` 前缀所以没被上面那条枚举命中），唯一调用点
`libs/ui/tests/kis_coordinates_converter_test.cpp`（4 处）。这不是 R-11 复评
点名的两项之一，登记与实现判断留给下一次任务处理，这里只诚实记下"发现了但
没修"，不把它悄悄归零。

综上：`QTest::` 命名空间成员这一类，**没有第三类「既没实现也没登记」的条目**
（已验证）；`Q*` 宏这一类，**已知还有 `QCOMPARE_NE` 未登记**（本轮范围之外，
留给后续）。新增或删减任何一项都要同时改这两张表与 `graft/rename.sed`。

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
  之后，`compat/QObject` 应改指过去。`Q_UNUSED` 不在垫片里——它在两个试接目标、
  被测头与全部 harness 自测里零调用点，按调用点现补即可。
- **`compat/QtGlobal` 只给了测试路径上真正用到的两项**：`qAbs`（真实调用点
  见该文件头部注释）与 `qFuzzyCompare`/`qFuzzyIsNull`（R-11 API 范围表第 15
  项）。`qint8..quint64`/`qreal` 这批标量 typedef 与 `qMin`/`qMax` 不在这里：
  零真实调用点的东西不预先实现（线级 spec 判据①），这批类型归 R-02（容器）/
  R-03（几何）交付。
- **`PkTestDataRow::operator<<` 没有 Qt 那条非模板重载**：Qt 的
  `QTestData& operator<<(QTestData&, const char*)`（`qtestdata.h:81-86`）会把
  字符串字面量自动转成 `QString` 再存；pk 目前只有 `operator<<` 那一个模板，
  字符串字面量走的是模板推导（数组类型，`std::any` 里存的是 decay 后的
  `const char*`，不是任何字符串类型）。S-00 全量接入时，凡是数据行喂字符串
  字面量、且对应列是 `QString`（或将来的 pk 字符串类型）的调用点都会撞上
  ——**实测 `addColumn<QString>` 全仓 89 处**（本次复评重新核过，口径：
  `xargs grep -c 'addColumn<QString>'` 对 §1 口径下的文件集求和），**数据行
  喂字符串字面量约 329 行**（复评员给出的数字，未重新核实其精确 grep 口径，
  量级上与 89 处 `QString` 列合理对应）。归属：等 R-02 的字符串类型落地后，
  给 `PkTestDataRow` 补一条 `operator<<(const char*)` 非模板重载，把字符串
  字面量转成那时候的 pk 字符串类型再存。
- **本轮明确不修、留给后续的三条**（不是缺陷分类里的"未知"，是已定位、已决定不在
  R-11 处理的）：
  - 生成的 `.inc` 里 `#include` 写的是**绝对路径**，对 ccache 不友好（同一份源码
    换个 checkout 路径就是不同的编译输入）。S-00 做全量接入时若命中缓存命中率
    问题，改成相对于生成目录的路径。
  - 命令行过滤器**丢弃 `fn:tag` 里的 tag 段**：只按函数名过滤，`-functionName:rowTag`
    会跑该函数的全部数据行。Qt 支持按行过滤。
  - `PkTestTable::current()` 与 `PkTestCase::beginRun()` 的清理策略不对称：数据表由
    runner 在每个数据驱动函数前显式 `clear()`，计数由 `beginRun()` 归零，两者没有
    统一的"一次 run 的生命周期"入口。一个进程连续 qExec 多个测试类时能跑对，
    但这是两条各自正确的路径，不是一条。

## 4. 生成器全量扫描：实测数字与口径

```
全量扫描（已排除 pk/ 自身）：410 个测试头，发现 367 个测试类 / 2264 个测试函数，生成器失败 0 次
```

- 口径：`git ls-files '*.h' | grep -E '(^|/)(tests|benchmarks)/' | grep -v '^pk/'`
  （**含** `benchmarks/`，**只扫 `.h`**，不含定义在 `.cpp` 里的测试类，
  **排除 `pk/` 自身**）。
- **排除 `pk/` 的理由**：`pk/test/tests/generator_cases/*.h`（本任务自己构造的
  生成器自测输入）与 `pk/string/tests/**`（另一个 R 线任务的测试头）路径里
  也含 `tests/`，会被同一条 `git ls-files` 扫进去，但它们不是 Krita 的真实
  测试面，混进来会让这组数字对不上"扫的是 Krita 测试头"这个判据的本意。
- 对照 D-30「341 个测试类 / 约 2168 个测试函数」：类数 **+7.6%**（367 vs 341），
  函数数 **+4.4%**（2264 vs 2168）——仍在计划给的 ±10% 容忍带内，量级一致。
  已知的口径差异来源：D-30 未声明是否含 `benchmarks/`、是否含定义在 `.cpp` 里的
  测试类；本项统计只扫 `.h`。类名不含 `test`/`Test` 字样的 27 个条目全部是
  `benchmarks/` 下的 `*Benchmark` 类，没有误收辅助 `QObject` 子类，也没有零测试
  函数的空 binder。
- 生成器在全部 410 个头上零崩溃。
- **这两个数字是被断言守着的**，不只是打印出来看：`selftest_generator.sh` 结尾
  断言 `classes >= 341 && funcs >= 2168`（下限取 D-30 的数字），低于就非零退出。
  只打印不判定的话，把生成器改成"对 `libs/` 下的头一律返回 0 个类"，脚本照样
  打印一行漂亮数字然后 exit 0。下限同时抓两种事故：生成器退化，与 Krita 侧测试
  面真的变小；实测值高于下限，正常波动不会误报。

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
`kritapigment`）复制到构建目录，在副本上跑 `rename.sed`（D-23 的 17 条实现规则
+ 1 条 `QTest::qCompare` 排除但仍改名的规则，见 §6，唯一允许的改动），编译时
定义 `PK_TEST_NO_QT_MACRO_ALIASES` 关掉 `compat/QTest`
里的 `QCOMPARE→PK_COMPARE` 一类别名——sed 若漏改一处，编译立刻报
`'QCOMPARE' was not declared`，这样试接才真正证明 D-23 的 sed 是机械可行的，
而不是靠别名把漏改的地方悄悄编过。结尾 `git diff --quiet` 自证源树零改动。

**关于两条 `nm -u | grep -i qt`**：`run_tests.sh` 查的是 `libpktest.a`，
`graft_run.sh` 查的是试接出来的可执行文件。**只有前者有判别力**——静态库允许留
未定义符号，真混进 Qt 依赖就会在那里现形；后者是静态链接产物，链接行里根本没有
任何 Qt 库，真有未定义的 Qt 符号会在链接期就失败，跑不到 `nm` 那一步，因此那条
断言**恒真**。留着它是因为判据要求这种形式的证据，别把它当成"我们查过了"。

## 6. S-00 要接手什么

- **`pk/test/graft/rename.sed` 就是 D-23 全量 sed 的规则表**：18 条规则——17 条与
  §1 的实现表一一对应，另 1 条是 §2 判定排除但仍需要改名的 `QTest::qCompare`
  （原因见该行的表内说明与 sed 文件里那条规则上方的注释）。规则集本身对匹配
  顺序不敏感（各条模式后面都跟着硬边界，不存在互相吃掉前缀的情况，见
  `rename.sed` 文件头注释），S-00 可以直接复用这份表对全仓跑 sed。§1 的表加
  一项，这里就要加一条；§2 新增排除项若有真实调用点，也要加一条。
- **⚠ 不要把「小写 `q` 前缀的自由函数一律改成 `pk`」当成通则**——本表里
  `qFuzzyCompare → pkFuzzyCompare`（规则 15）会让人这么以为，但 **`pk/container` 的
  `qHash` / `qMakePair` 是明确不改名的**（见 `pk/container/PkHashFunctions.h` 类头与
  `PkContainerAlgo.h` 的「名字保持小写 q 前缀、不改名」）。

  **区别在约束不在风格**：`qFuzzyCompare` 的调用点反正都要 sed，改名零代价；而
  **Krita 全仓 18 处自定义 `unsigned int qHash(const X &)` 重载在 S 线是原样保留的**，
  `PkHash`/`PkSet` 靠 ADL 找它们，改了名就找不到。

  改错的代价是**编译期报错**（`PkHashFunctions.h` 的兜底只有指针模板与枚举模板，
  接不住类类型），不是静默退化——**但那是白跑一轮再回滚**。判断标准：
  **这个名字有没有「保留侧代码里已经存在、且不打算改的重载」在依赖它**。

- **`QEXPECT_FAIL` 的第三参数不用改名**：Qt 的调用点写的是**裸** `Continue`/`Abort`
  （实测全仓零处写 `QTest::Continue`），命名空间由宏体自己拼。`PK_EXPECT_FAIL`
  照抄了这条，所以 sed 只改宏名就够，不要额外去动第三参数。
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
