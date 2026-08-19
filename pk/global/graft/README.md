# pk/global/graft —— R-18 试接（graft）

试接判据②：**真实 Krita 测试类编过跑绿**，证明 `pk/global` 标量地基的 API 形状对得上
真实调用点。红线与 R-03 相同：被测源与测试源**一个字节都不许改**，唯一允许的改动是
`rename.sed` 对构建目录里那份**副本**做的 D-23 机械改名（QCOMPARE→PK_COMPARE 等）。

```
./pk/global/graft/graft_run.sh
```

依赖：`pk/global/build/libpktest.a` 必须先由 `pk/global/tests/run_tests.sh`（或 pk/test
的构建）产出（硬约束，缺了就报错退出，不自建）。产物落在 `pk/global/graft/build/`，
**不与 `pk/global/build/` 混用**。

## 两个目标

### 目标 ① TestKoIntegerMaths —— 真实测试类，跑绿

- 来源：`libs/pigment/tests/TestKoIntegerMaths.{h,cpp}`，target kritapigment；
  被测 `libs/pigment/KoIntegerMaths.h`（自包含，只 `<cstdint>` + 宏）。
- 依赖墙：`KoIntegerMaths.h` 自包含；`simpletest.h`/`<QObject>` 由 pk/test compat 提供。
  **无外部墙**。pk/test（R-11）已用同形态 graft 跑绿过它，harness 形态已验证。
- 零改动：复制 → `rename.sed` 机械改名（`QTEST_GUILESS_MAIN`→`PK_TEST_GUILESS_MAIN`、
  `QCOMPARE`→`PK_COMPARE`、`QVERIFY`→`PK_VERIFY`、`qAbs` 不动）→
  `pk_test_moc.py` 生成 binder → driver.cpp 粘合 → 编译链接 → 跑。

**证据（`graft_run.sh` 输出）**：

```
试接跑绿: TestKoIntegerMaths (libs/pigment/tests)
  PASS   : TestKoIntegerMaths::initTestCase()
  PASS   : TestKoIntegerMaths::UINT8Tests()
  PASS   : TestKoIntegerMaths::UINT16Tests()
  PASS   : TestKoIntegerMaths::conversionTests()
  PASS   : TestKoIntegerMaths::cleanupTestCase()
  Totals: 5 passed, 0 failed, 0 skipped
  nm -u TestKoIntegerMaths | grep -i qt: 无输出
```

### 目标 ② driver_global_scalars.cpp —— 标量调用点 driver（**降级路径**）

2026-08-18 裁决：`libs/global/KisLager.h`、`KisZug.h`、`kis_algebra_2d.h` 的真实测试类
依赖 lager/zug vendored 库（根 CMake FetchContent，本机无）与 `QPointF`/`QLineF` 几何
类型（归 R-21/R-22 未交付）——环境凑不齐，**不是代码写错**。走 driver 降级路径，
四条要求全满足：

1. **逐行复刻真实调用点的代码形状**（来源行号标在 driver 每个复刻块上方）：
   - `libs/global/KisLager.h:57` —— `[multiplier] (int, qreal value) { return qRound(value / multiplier); }`
   - `libs/global/KisZug.h:53` —— `map_equal<qreal>` 的 `qFuzzyCompare(x, value)`
   - `libs/global/KisZug.h:64` —— `map_round` 的 `qRound(x)`
   - `libs/global/kis_algebra_2d.h:325` —— `qMin(corner1.x(), corner2.x())` 等四参
   - `libs/global/kis_algebra_2d.h:339/344` —— `qMax(size.width(), size.height())` /
     `qMin(size.width(), size.height())`
   同样的函数、同样的参数类型/个数/顺序。几何部分用 `PkGraft*` 替代
   `QPointF`/`QSizeF`/`QRectF`（依赖墙②），但 `.x()/.y()/.width()/.height()` 返回
   `qreal` 不变，`qMin`/`qMax`/`qAbs` 的实参形态与真品逐字一致。
2. **校验值来源分两类**：`qRound(-1.5)==-1`、`qFuzzyCompare(1.0, 1.000000000001)
   ==false`（相对差 1.0000889e-12 > Qt 阈值 1e-12，**超阈为假**）、
   `qFuzzyCompare(1.0, 1.0000005)==false` 来自 Task 2 探针
   （`pk/global/oracle/global_difftest.cpp` 的输入宇宙 `kD[]`）；而
   `qMin(4.0,1.0)`/`qMax(3.0,7.0)`/`qAbs` 取**普通输入形态**（4.0/7.0 不在
   `kD[]` 里），其语义已由 oracle 在 `kD[]` 全集上逐位对拍过，driver 只验调用形状。
3. **driver 显式标注自己是替代品**：文件头注释写明「这不是真实测试文件，是复刻
   KisLager.h/KisZug.h/kis_algebra_2d.h 调用点形状的 driver」。
4. **指名依赖墙**：lager/zug 库（Krita 根 CMake FetchContent，本机无）与
   `QPointF`/`QLineF` 几何类型（归 R-21/R-22）。墙拆掉后理论上可补真实编译。

**证据（`graft_run.sh` 输出）**：

```
driver 跑绿: driver_global_scalars
  PASS: KisLager.h:56 scale_int_to_real getter: (3 * 2.0) == 6.0
  PASS: KisLager.h:57 scale_int_to_real setter: qRound(-3.0/2.0) == qRound(-1.5) == -1
  PASS: KisLager.h:57 scale_int_to_real setter: qRound(3.0/2.0) == qRound(1.5) == 2
  PASS: KisZug.h:53 map_equal<qreal>: qFuzzyCompare(1.0, 1.0) == true
  PASS: KisZug.h:53 map_equal<qreal>: qFuzzyCompare(1.0, 1.000000000001) == false
  PASS: KisZug.h:53 map_equal<qreal>: qFuzzyCompare(1.0, 1.0000005) == false
  PASS: KisZug.h:64 map_round: qRound(-1.5) == -1
  PASS: KisZug.h:64 map_round: qRound(1.5) == 2
  PASS: kis_algebra_2d.h:325 createRectFromCorners: qMin(3.0, -2.0) == -2.0
  PASS: kis_algebra_2d.h:325 createRectFromCorners: qMin(4.0, 1.0) == 1.0
  PASS: kis_algebra_2d.h:325 createRectFromCorners: qAbs(3.0 - (-2.0)) == 5.0
  PASS: kis_algebra_2d.h:325 createRectFromCorners: qAbs(4.0 - 1.0) == 3.0
  PASS: kis_algebra_2d.h:339 maxDimension: qMax(3.0, 7.0) == 7.0
  PASS: kis_algebra_2d.h:344 minDimension: qMin(3.0, 7.0) == 3.0
  Totals: 14 passed, 0 failed, 0 skipped
  nm -u driver_global_scalars | grep -i qt: 无输出
```

## -I 顺序（按 brief，别调）

```
-I $STUBS -I pk/global -I pk/global/compat -I pk/test -I pk/test/compat -I pk/geometry/compat
```

- `$STUBS`（`pk/global/graft/stubs/`）最前：本轮两个目标都不拉 Krita 内部头，
  槽位保留（将来加垫片就插同位）。
- `#include <QtGlobal>` 命中 `pk/global/compat/QtGlobal`（-I 里它最靠前），那份超集链
  自己 `__has_include` 拉 `pk/test/compat/QtGlobal` 与 `pk/geometry/compat/QtGlobal`，
  `PkGlobal.h` 检测到 `PK_GLOBAL_SCALARS_FROM_PKTEST` 后整段让位（`qAbs`/
  `qFuzzyCompare`/`qFuzzyIsNull` 用 pk/test 那份，其余标量本头自供）。
- 目标① 的 `"KoIntegerMaths.h"` 引号 include 先按包含者所在目录找（测试源已复制进
  构建目录，那里没有），回落 `-I libs/pigment`。
- `-DPK_TEST_NO_QT_MACRO_ALIASES` **不关**：rename.sed 漏改的 QCOMPARE 编译期爆炸。

实测（`g++ -H` 追踪）：
- 目标① 的 `<QObject>` → `pk/test/compat/QObject` → `"QtGlobal"` →
  `pk/test/compat/QtGlobal`（pk/test 那份）。
- 目标② 的 `<QtGlobal>` → `pk/global/compat/QtGlobal` → `pk/test/compat/QtGlobal` +
  `pk/geometry/compat/QtGlobal`（经 geometry 转发头）→ `pk/global/PkGlobal.h`。

## 文件清单

```
pk/global/graft/graft_run.sh                 # 试接 runner（两个目标 + 两条自证）
pk/global/graft/rename.sed                   # pk/test/graft/rename.sed 的逐字副本（diff 自证）
pk/global/graft/driver_global_scalars.cpp    # 目标②：标量调用点 driver（降级路径）
pk/global/graft/stubs/README.md              # 垫片目录说明（当前为空，无需垫片）
pk/global/graft/README.md                    # 本文档
```

`pk/global/graft/build/` 不入版控（根 .gitignore 的 `build` 模式覆盖）。

## 自证

1. `rename.sed` 与 `pk/test/graft/rename.sed` 逐字一致（`diff -q`，脚本第 0 步）。
2. 源树零改动：`git diff --quiet` 覆盖 5 个文件 —— 目标① 的测试源
   `libs/pigment/tests/TestKoIntegerMaths.{h,cpp}`，目标② 的三个真实调用点头
   `libs/global/KisLager.h`、`KisZug.h`、`kis_algebra_2d.h`。
3. 判据③：两个产物 `nm -u <exe> | grep -i qt` 无输出（对静态链接可执行文件这条
   恒真，见脚本内注释；真正有判别力的是 `tests/run_tests.sh` 对 `libpkglobal.a` 那条）。

## 自审发现

- driver 初稿有一行 `template <> struct PkGraftPointTypeTraits<PkGraftPointF_dummy_never>;`
  引用了未声明类型，属笔误，已删（主模板 + `PkGraftPointF` 特化足够）。
- `graft_run.sh` 目标②段初稿在顶层用了 `local undef`（bash 的 `local` 只能用在函数
  内），已改为普通变量赋值。
- 两次重跑结果一致（idempotent）：`libpkglobal.a` 自建、work 目录 `rm -rf` 重建。

## 遗留与边界

- 目标② 是降级路径，**不是真实测试类**。lager/zug 库与 `QPointF`/`QLineF` 几何类型
  交付后，应补真实测试类编译（对应 libs/global/tests 的 KisLagerTest / KisZugTest 与
  几何测试），本 driver 届时退役。
- 本轮 `stubs/` 为空：两个目标都不拉 Krita 内部头。将来目标变多拉到 `kis_debug.h`
  一类时，按 R-03 `pk/geometry/graft/stubs/` 的模式在 `pk/global/graft/stubs/` 补。
