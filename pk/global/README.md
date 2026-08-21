# pk/global —— 零 Qt 的标量地基（R-18）

独立 `project(pkglobal)` 薄壳工程，**不接入 Krita 主构建**，不改根 `CMakeLists.txt`。
交付的是整个剥离项目里所有「标量」的**唯一权威**：`qreal` / 整数别名 / 数值工具 /
`Q_UNUSED` / `Q_ASSERT` / `Qt` 的两个标量枚举。全局作用域、无 C++ namespace（compat
垫片靠 `#define` 改写工作；唯一例外是 `namespace Qt { enum … }`，那是调用点写
`Qt::KeepAspectRatio` 限定名所必需的，论证见下）。

对齐口径：**与 Qt 的任何行为差异默认都是缺陷。** 参照物是真 Qt 5.15.7
（`/mnt/ssd-disk/liyang/projects/krita-ci-env/_install`，`QT_VERSION_STR "5.15.7"`）。
想记成"可接受偏离"的，逐条写进下面的偏离清单，由 reviewer 判。

**三条证据链**：`tests/`（构建 + 单测 + `nm -u` + locks）、`oracle/`（链真 Qt5 的逐输入
对拍）、`graft/`（真实测试类试接 + 标量调用点 driver）。下面是 2026-08-18 收口时
**现场重跑**的数字——**数字随代码变，改完重跑以实际值为准，不要照抄本文件**。

## 怎么跑

```bash
./pk/global/tests/run_tests.sh      # 四条自证：构建 + 单测 + nm -u + locks
./pk/global/oracle/run_oracle.sh    # 与真 Qt5 逐输入对拍
./pk/global/graft/graft_run.sh      # 试接：真实测试类零改动跑绿 + 标量 driver
```

### `run_tests.sh` —— 四条自证

1. 配置并构建独立工程（`pkglobal` 静态库 + `test_pkglobal`，复用 pk/test 的 harness）。
2. 跑 `test_pkglobal`。
3. **判据③**：`nm -u libpkglobal.a | grep -i qt` 必须无输出。
4. **locks**：`git status --porcelain -- . ':(exclude)pk/global' ':(exclude)pk/geometry'`
   非空即失败——改动必须落在两个前缀内（R-18 含折叠任务，geometry 侧也在允许内）。

> **判据③的判别力边界。** `nm -u` 查的是替代品本体 `build/libpkglobal.a`。**静态库允许
> 留未定义符号**，真引了 Qt 的话符号会挂在 `.a` 里，所以这条有判别力。反过来，
> `graft/graft_run.sh` 里对**静态链接出来的可执行文件**跑的同形状 `nm -u` 是**恒真、没有
> 判别力**的（真引了 Qt 在链接期就已失败，走不到 `nm`）。它留着只是因为判据要求这种
> 形式的证据。`oracle/` 按设计就要链真 Qt，那边 `ldd` 看得到 `libQt5Core` 才是对的。

**收口时重跑（2026-08-18）**：`Totals: 30 passed, 0 failed`（`PkGlobalCase` 18 +
`PkInttypesCase` 7 + `PkMacrosCase` 5，每个类 = slot 数 + `initTestCase` +
`cleanupTestCase`）；`nm -u` 无输出；locks 全落在 `pk/global/` 与 `pk/geometry/` 内。

**测试规模的口径**（数字随代码变，改完重跑以实际值为准）。计数口径：**先去掉 `//` 注释
再数** `PK_VERIFY|PK_VERIFY2|PK_COMPARE` 的**出现次数**（注释里写着的断言不算，
`grep -c` 按行数会系统性少算——同一行多条断言只算一行）：

| 口径 | 数 | 怎么数的 |
|---|---:|---|
| 测试函数（slot） | 24 | `global_case.h` 16 + `inttypes_case.h` 5 + `macros_case.h` 3 |
| 断言（**写在源里的**） | 165 | 去注释后出现次数：`test_global.cpp` 130 + `test_inttypes.cpp` 29 + `test_macros.cpp` 6 |
| 断言（**展开后**） | 175 | 与上一行差 10：`test_global.cpp` 的共享宏 `PK_CHECK_GLOBAL_COEXIST_PROBE`（体内 5 条）被三个 coexist 测试函数各展开一次，源里只写了一遍。**两个数都要给**，只给一个必然被读成另一个 |
| 运行输出 `Totals` 行 | 18 / 7 / 5 | harness 口径：每个类的 slot 数 + `initTestCase` + `cleanupTestCase`，**不是** slot 数也不是断言数。三类合计 30 |
| `static_assert` | 29 | `test_inttypes.cpp` 的 typedef 编译期断言（`sizesMatchStdInt`/`signednessMatchesNames`/`symmetricAliasesMatch`/`cCompatibleAliasesMatch` 的静态断言面） |
| 翻译单元 | 7 | `test_main` `test_global` `test_inttypes` `test_macros` + 三个 `coexist_*.cpp` |

### `run_oracle.sh` —— 与真 Qt5 逐输入对拍

链真 Qt5（`ldd` 必须看得到 `libQt5Core`）→ 跑 → 把每条 `DIFFTAG` 与
`oracle/global.deviation` 双向核对。**未声明的差异**、**已声明 tag 的计数漂移**、
**canary 消失**三者任一出现就 FAIL。方法论与 tag 的两条硬规则写在
`oracle/global_difftest.cpp` 的文件头。

**收口时重跑（2026-08-18）**：

```
对拍结论：total=17604 mismatch=11 tag=12（其中 canary 11）
run_oracle.sh: 通过 —— 全部差异都已声明，canary 齐全
```

- `mismatch=11` = **11 条 canary**（比较管道自证，故意不相等的比对，走真实 `rec()`/
  `same_*`/`tag` 路径）+ **1 行 Q_ASSERT 登记**（`assert_gate`，编译期语义差异，不是运行
  期可比——对拍程序直接打一行 `DIFFTAG Q_ASSERT assert_gate 1` 充当登记）。
- **真实行为差异预期为零**：修复轮随 `PkGlobal.h` 修正消除了 `qFloor`/`qCeil` 在 ±inf
  上的分家（对非有限值原样返回 `int(v)`，与 Qt 的 `int(floor(±inf))` 一致）。判别力靠
  canary + 注入自证撑着，不靠 `total` 这个数字本身。
- 规则三机器闸门：`APISEEN 24 个（期望 24）`——每个已实现的重载都有自己的 `rec()`。
- **geometry 基线不回归**（Task 3 折叠的回归检查）：`./pk/geometry/oracle/run_oracle.sh`
  仍 `total=154358778 mismatch=25498`（23 条已声明 `mapRect` 偏离 + 3 条 canary），全部
  已声明、无未声明 tag。折叠后 geometry 经转发头拉 pk/global 标量，这份基线证明转发没
  引入漂移。

### `graft/graft_run.sh` —— 试接

红线与 R-03 相同：被测源与测试源**一个字节都不许改**，唯一允许的改动是 `rename.sed`
对构建目录里那份**副本**做的 D-23 机械改名。依赖 `pk/global/build/libpktest.a` 先由
`run_tests.sh` 产出。

**收口时重跑（2026-08-18）**：

- 目标① `TestKoIntegerMaths`（`libs/pigment/tests`，target kritapigment）——真实测试类
  **跑绿**：`Totals: 5 passed, 0 failed`；`nm -u` 无输出。
- 目标② `driver_global_scalars`（复刻 `KisLager.h`/`KisZug.h`/`kis_algebra_2d.h` 调用
  形状的 driver，**降级路径**，四条要求全满足见 driver 文件头）——**跑绿**：
  `Totals: 14 passed, 0 failed`；`nm -u` 无输出。
- 自证：`rename.sed` 与 `pk/test/graft/rename.sed` 逐字一致；`git diff --quiet` 覆盖
  5 个文件（两个真实测试源 + 三个真实调用点头）——**源树零改动**。

## 交付面清单

### API 表

| 类目 | 名字 | 语义 / 来源 |
|---|---|---|
| `qreal` | `typedef double qreal` | `qglobal.h:279-283`；`QT_COORD_TYPE` 未定义（桌面/Android）即 double |
| 定长整数 | `qint8`/`quint8`/`qint16`/`quint16`/`qint32`/`quint32`/`qint64`/`quint64` | `qglobal.h:232-257`；64 位在 Linux 是 `long long` |
| **对称别名** | `qlonglong`（= `qint64`）/ `qulonglong`（= `quint64`） | **0 调用点**，为对称性照抄 Qt 登记（见偏离清单 1） |
| C 兼容别名 | `uchar`/`ushort`/`uint`/`ulong` | `qglobal.h:273-276`；`ulong` Qt 实有此名，照抄（见偏离清单 2） |
| 绝对值 | `qAbs(const T &t)` 模板 | `qglobal.h:657-658`；条件 `t >= 0`（`-0.0` 原样返回） |
| 极值 | `qMin`/`qMax`/`qBound` 模板 | `qglobal.h:660-677`；返回 `const T&`（照抄签名） |
| 取整 | `qRound(double)`/`qRound(float)` → `int` | `qglobal.h:660-663`；负半值向 +∞、`int(d+0.5)` 进位 |
| 模糊比较 | `qFuzzyCompare(double/float)`/`qFuzzyIsNull(double/float)` → `bool` | `qglobal.h:900-917`；公式住内部名 `pkQtFuzzy*`（宏改写不到），对外名字只转发 |
| 向 -∞/+∞ 取整 | `qFloor(qreal)`/`qCeil(qreal)` → `int` | `qmath.h:68-76`；非有限值原样返回 `int(v)`（同 Qt `int(floor(±inf))`） |
| 2 的幂 | `qNextPowerOfTwo(quint32)` → `quint32` | `qmath.h:247-258`；返回**严格大于** v 的最小 2 的幂，`v==0→1` |
| NaN/∞ | `qIsNaN(double/float)`/`qInf()`/`qQNaN()` | `qnumeric.h:48-59`；std::isnan / std::numeric_limits |
| 内部 | `pkQtFuzzyCompare`/`pkQtFuzzyIsNull` | 公式**唯一**真身，`qFuzzy*` 只是转发（`#define` 打架防护） |
| 内部 | `pk_qt_assert(const char*,const char*,int)` | `PkGlobal.cpp` 实现：fprintf + abort；`Q_ASSERT` 触发路径 |
| 宏 | `Q_UNUSED(x)` | `qglobal.h:117` |
| 宏 | `Q_ASSERT(cond)` | `qglobal.h:855-859`；关闭条件**偏离**（见偏离清单 3） |
| Qt 枚举 | `namespace Qt { enum AspectRatioMode }` / `enum Axis` | `qnamespace.h:1235-1239` / `1386-1390`，**全项目唯一真 namespace** |

### 0 用量登记

| 名字 | 实测 | 处置 |
|---|---|---|
| `qlonglong` / `qulonglong` | **0 调用点**（保留范围口径） | **登记**为对称别名照抄 Qt，不删（见偏离清单 1） |
| `qRound64` | **0 调用点** | 判据①「一项不多」，不做 |
| `qIsNull`/`qIsInf`/`qIsFinite`/`qQNaN`(浮点系其余)/`qSNaN`/`qFpClassify`/`qFloatDistance`/`Q_INFINITY`/`Q_QNAN` 宏 | **0 调用点**（保留范围口径） | 一概不做，撞上先补实测用量再决定 |

> 口径：保留范围 = `git ls-files` ∩ 保留前缀 ∩ `.cpp`/`.h`/`.cc` − `tests/`|`benchmarks/` −
> `pk/`（`.cc` 算在内），详见 `pk/geometry/README.md` 的「API 范围」一节（同一个文件集）。

### 与 `pk/geometry/PkGlobal.h` 的关系（Task 3 已折叠）

- **`pk/global/PkGlobal.h` 是标量地基的唯一实现。**
- **`pk/geometry/PkGlobal.h` 是薄转发头**（`#include "../global/PkGlobal.h"`），不再自用
  定义。几何头 / `pk/geometry/compat/` / R-03 的 tests/oracle/graft 全经它命中
  pk/global 的标量与 `Qt` 枚举。
- 让位-to-pk/test **保留**：pk/test 那份 compat 先进 TU 时，`qAbs`/`qFuzzyCompare`/
  `qFuzzyIsNull` 让位给 pk/test 的实现（`PK_GLOBAL_SCALARS_FROM_PKTEST` 检测），其余
  无条件定义。机制全文在 `PkGlobal.h` 顶部「与 pk/test 的共存」一节。

## 偏离清单

**预期零行为偏离**——对拍下来 `mismatch=11` 全是 canary + 登记行。下面三条是**登记项**
（README 是给人读的汇总，`oracle/global.deviation` 是给 `run_oracle.sh` 读的机器闸门；
**两边都要有 Q_ASSERT gate 这一条**）：

| # | 偏离 | 理由 |
|---|---|---|
| 1 | **`qlonglong` / `qulonglong`：0 调用点对称别名，登记** | 实测 0 调用点（保留范围口径）。为与 Qt 头文件全集对齐而照抄（调用点若写 `qlonglong`，它必须存在）。这不是行为差异，是「一项不多」判据下的登记项：对称别名随 `qint64`/`quint64` 一起给，不单独实现。 |
| 2 | **`ulong`：Qt `qglobal.h:276` 实有此名，照抄，非偏离** | Qt 自己也提供 `typedef unsigned long ulong`。本头照抄（`PkGlobal.h:98` 注释登记）。**不是偏离**，登记为「照抄 Qt」，防后来者误判为「C 语义补充」而删掉。 |
| 3 | **Q_ASSERT gate：`QT_NO_DEBUG\|\|NDEBUG` vs Qt `QT_NO_DEBUG&&!QT_FORCE_ASSERTS`，登记偏离** | 本头以「任一宏定义即空转」为关闭条件；真 Qt 以 `QT_NO_DEBUG` 且非 `QT_FORCE_ASSERTS` 为关闭条件（光 `-DNDEBUG` 仍激活断言）。对拍两侧都 `-DQT_NO_DEBUG`，宏展开一致，此差异只在 `-DNDEBUG` 且无 `-DQT_NO_DEBUG` 的构建下显现。这是 Task 1 遗留、`global.deviation` 首行 `assert_gate` 已登记。 |

`oracle/global.deviation` 的当前内容：1 行 `Q_ASSERT	assert_gate` + 11 行
`canary`——**与上表第 3 条一致**，其余真实差异为零。

**让位给真 Qt（R-34，2026-08-21）**：real Qt 已进 TU（`QT_CORE_LIB` 定义）时，本头与 Qt 同名的一切（`qAbs`/`qRound`/`qMin`/`qMax`/`qBound`/`qIsNull`/`qFuzzyCompare`/`qFuzzyIsNull`/`qFloor`/`qCeil`/`qNextPowerOfTwo`/`qIsNaN`/`qInf`/`qQNaN` 与 `namespace Qt` 枚举族）让位，与真 Qt qglobal.h/qmath.h/qalgorithms.h/qnumeric.h/qnamespace.h 共存。`pkQtFuzzyCompare`/`pkQtFuzzyIsNull`（pk 自有名，geometry 头内部调用）保留。`qFloor`/`qCeil`/`qNextPowerOfTwo` 的守卫是 `!QT_CORE_LIB || !QMATH_H`（`!QALGORITHMS_H`）——qmath.h/qalgorithms.h 不被 qglobal.h 拉，real-Qt 在场但没 include `<QtMath>`/`<QtAlgorithms>` 的 TU 里 pk 版仍应可用；mixed TU 必须「Qt 头在前」。

## 覆盖度限制（照 R-01 先例写）

| 对拍覆盖不到什么 | 怎么补的 |
|---|---|
| `Q_ASSERT` / `Q_UNUSED` 无对拍（**宏**，编译后不可观测） | `tests/macros_case.h` 单测覆盖：`assertTrueIsNoop` / `assertFalseAborts`（fork 子进程验证 abort）/ `unusedDoesNotWarn`（`-Wall -Werror` 守卫） |
| `qreal` / 整数别名无行为对拍（**typedef**，无运行期可比） | `tests/inttypes_case.h` 编译期断言覆盖：`static_assert` 钉 `sizeof` / `signedness` / 对称别名 / C 兼容别名 |
| 跨 TU 的 Qt 符号 | 判据③ `nm -u libpkglobal.a | grep -i qt`（静态库允许留未定义符号，真混进 Qt 依赖会现形） |
| `qFuzzyCompare`/`qFuzzyIsNull` 的「`#define` 打架」与让位路径取值 | 三个 `tests/coexist_*.cpp` TU 各测一种 include 顺序，`test_global.cpp` 的 `PK_CHECK_GLOBAL_COEXIST_PROBE` 核对取值与真 Qt 一致 |
| 真实调用点「不改一字命中」 | `graft/` 试接 + 收口时的 `-fsyntax-only` 复核（见下） |

### 收口的 `-fsyntax-only` 复核（真实调用点不改一字命中）

判据②的完整含义不是「挑几个能编过的头证明垫片存在」，而是**真实 Krita 头文件零改动
语法编译**。收口时按 R-01/R-12 的 `graft_check.sh` 形态（`-include` 把垫片提前 + `-I`
指 compat 目录，全在编译参数上，**不碰源文件**）对 3 个真实 `#include <QtGlobal>` 文件
复核：

```
g++ -std=c++17 -fsyntax-only \
    -include pk/global/compat/QtGlobal \
    -I pk/global/compat -I pk/global -I pk/test/compat -I pk/test -I pk/geometry/compat \
    -I <kritaglobal_export.h 垫片目录> \
    -I "$(dirname "$f")" "$f"
```

| 文件 | 结果 | 说明 |
|---|---|---|
| `libs/global/kis_assert.h` | **PASS** | 只需 `<QtGlobal>` + `kritaglobal_export.h`（构建系统生成物，垫片置空 `KRITAGLOBAL_EXPORT`）。`KIS_ASSERT` 宏族定义体编过（宏未展开，`qt_noop` 未定义不构成语法错） |
| `libs/pigment/KoColorSpaceConstants.h` | **PASS** | 用 `quint8` / `qreal`（均来自 pk/global），加 `<climits>` 的 `UCHAR_MAX` |
| `libs/global/KisQStringListFwd.h` | **GAP** | 第 11 行 `#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))`——**QT_VERSION/QT_VERSION_CHECK 版本宏未交付**，报 `missing binary operator`。归 **S-批次**（版本宏）。缺的不是标量，是 Qt 版本宏面 |

另把 brief 点名的三个真实调用点也跑了，**全部预期 GAP**（依赖未交付的几何/Qt 类型，
**不强行编**）：

| 文件 | GAP 原因 | 归属 |
|---|---|---|
| `libs/global/kis_global.h` | `#include <KoConfig.h>`（构建生成物）→ `QPoint`/`QPointF`/`QRect`/`QLineF`/`QStringConverter` | 几何 **R-21/R-22**；`QString`/`QStringConverter` **R-01**；`KoConfig.h` 构建系统生成物 |
| `libs/global/KisLager.h` | `#include <QVariant>` → lager vendored 库（根 CMake FetchContent，本机无） | lager **归属未定**；`QVariant` **S-批次** |
| `libs/global/KisZug.h` | `#include <zug/transducer/map.hpp>`（vendored，本机无） | zug **归属未定** |

三条都已是 `graft/` driver 复刻过的调用形状，真实头的**完整**编译要等几何/Qt 类型
落地。**这一节不是「还有哪些功能没做」，是判据②的边界声明。**

## 与 `pk/test` / `pk/geometry` 两份 compat 的共存纪律

三份 `compat/QtGlobal` 同名（`pk/global`、`pk/test`、`pk/geometry`），试接时同进一个
TU。`#include <QtGlobal>` 只命中 `-I` 里靠前的一份，靠两条机制共存：

- **每个 compat 垫片必须先包 `compat/QtGlobal`**，再包各自的 Pk 头。pk/global 的
  `compat/QtGlobal` 是超集链：自己 `__has_include` 先拉 `pk/test/compat/QtGlobal` 再拉
  `pk/geometry/compat/QtGlobal`，然后才包 `PkGlobal.h`——于是**任意顺序进同一 TU，
  `PkGlobal.h` 总在最后落地、总能让位**（检测到 `PK_GLOBAL_SCALARS_FROM_PKTEST` 后整段
  让位）。这也是对真 Qt「每个公开头先包 `qglobal.h`」的复刻。
- **geometry 的 compat 经转发头拉 pk/global 标量**：`pk/geometry/PkGlobal.h` 已是转发头，
  geometry 的 `compat/QtGlobal` 与类型垫片拿到的就是 pk/global 这份——折叠后不再有第二
  份公式可漂移。

三种 include 顺序各有一个 TU（`tests/coexist_global_first.cpp` /
`coexist_geometry_first.cpp` / `coexist_test_first.cpp`）编译并核对取值，**能编过本身就是
断言的一半**（重复定义 `qAbs`/`qRound` 是硬错误，`qFuzzyCompare` 的 `#define` 打架会当场
改写坏）。纪律的完整论证在 `PkGlobal.h` 顶部「与 pk/test 的共存」一节。

> ⚠ 另一个"共存"形态不覆盖：`PkGlobal.h` 的 `namespace Qt { enum … }` 与**真 Qt 的**
> `qnamespace.h` 同进一个 TU 时是**重定义硬错**。对拍是唯一有这个形态的编译行（靠把
> 替代品整包塞进 `namespace pkoracle` 绕开）。剥离完成后的 Krita 里不存在"真 Qt 与替代品
> 共存"的编译行，所以只登记不解决——表现是响亮的编译错误，不是静默错行为。

## 现在有什么

| 文件 | 内容 |
|---|---|
| `PkGlobal.h` | 标量地基唯一实现（交付面全部 + 让位机制 + `Qt` 枚举） |
| `PkGlobal.cpp` | `pk_qt_assert`（Q_ASSERT 触发路径：fprintf + abort） |
| `compat/QtGlobal` | `<QtGlobal>` 垫片（超集链 + 共存纪律） |
| `tests/` | `test_global.cpp`（16 函数）、`test_inttypes.cpp`（5）、`test_macros.cpp`（3）、三个 `coexist_*.cpp` 共存 TU、`coexist.h` 探针 |
| `oracle/` | `global_difftest.cpp`（对拍骨架 + 全部 `rec()`）、`run_oracle.sh`、`global.deviation`（1 行 Q_ASSERT + 11 行 canary）、`api_seen.expected`（24 行，规则三闸门） |
| `graft/` | `graft_run.sh`（两目标 + 自证）、`rename.sed`（pk/test 逐字副本）、`driver_global_scalars.cpp`（标量调用点 driver）、`stubs/`（当前空）、`README.md` |

## 数字随代码变

**本文件所有数字（`Totals`、`total=`/`mismatch=`、APISEEN、`static_assert` 数、断言数、
`nm -u` 结论）都是 2026-08-18 收口时的现场值。改完代码重跑三条证据链，以实际输出为准，
不要照抄本文件。** 数值的 SOT 只有一处：现场。
