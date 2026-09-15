# pk/color —— PkColor（对齐 Qt 5.15.7 QColor）

独立薄壳工程（R 线），零 Qt 依赖。`class PkColor` 按真 Qt 5.15.7 `QColor` 的
内部模型与转换算法实现，行为由「真 Qt 对拍」（oracle）与探针实测语义锚定。

## 范围表（判据①，保留范围实测）

| Qt 成员 | 保留范围用量 | 实现 |
|---|---|---|
| `QColor()` 默认构造（无效，alpha=65535） | 用 | ✅ |
| `QColor(int r,int g,int b,int a=255)` | 用（越界→无效） | ✅ |
| `QColor(Qt::GlobalColor)` 20 项表 | 用 | ✅ |
| `QColor(const char*/QString)` 命名色 | 用（含 6 位 hex） | ✅ |
| `red/green/blue/alpha()` | 用 | ✅ |
| `redF/greenF/blueF/alphaF()` | 用 | ✅ |
| `setRed/setGreen/setBlue/setAlpha/setAlphaF()` | 用（越界截断，非 Rgb 先转 Rgb） | ✅ |
| `setRgb(4参)/setRgb(QRgb)/setRgba(QRgb)` | 用 | ✅ |
| `setRgbF`（rgb 越界→ExtendedRgb） | 用 | ✅ |
| `hue/saturation/value()` (HSV) | 用（灰=-1） | ✅ |
| `hslHue/hslSaturation/lightness()` (HSL) | 用（灰=-1） | ✅ |
| `setHsv/setHsl`（改 spec；h 超界回绕 h%360） | 用 | ✅ |
| `setHsvF/setHslF`（越界静默返回） | 用 | ✅ |
| `name()` / `name(NameFormat)` | 用（HexRgb/HexArgb） | ✅ |
| `setNamedColor()`（SVG 命名色 / #RGB / #RRGGBB / #AARRGGBB） | 用 | ✅ |
| `lighter/darker(factor)` | 用（HSV 模型，factor<=0 不变，<100 交叉调用） | ✅ |
| `operator==/!=` | 用（**比较 alpha**，见偏离 4） | ✅ |
| `isValid()/spec()` | 用 | ✅ |
| `rgba()/rgb()` | 用（rgb() 恒 alpha=FF；无效色 rgba=0xff000000） | ✅ |
| `toRgb/toHsv/toHsl/convertTo(Spec)` | 用（对拍） | ✅ |
| `Spec` 枚举（Invalid/Rgb/Hsv/Cmyk/Hsl/ExtendedRgb） | 用（对拍） | ✅ |
| `setRgba64(a,r,g,b)` | 用 | ✅ |
| `fromRgb/fromRgb(QRgb)/fromRgba/fromRgbF/fromHsv/fromHsvF/fromHsl/fromHslF` | 用（对拍） | ✅ |
| `fromString` | **Qt 5.15 不存在**（Qt 6.7+） | ❌ 不实现（偏离 1） |
| CMYK 系 | 0 | ❌ 不实现（偏离 2） |
| `convertTo(Cmyk/ExtendedRgb)` | 0（范围表判不实现） | ❌ 返回无效色（偏离 2） |
| QBrush/QPen/QGradient 等 QColor 邻型 | 0（本任务） | ❌ 归后续 R 线任务 |

## 偏离登记（对齐 spec 提示与真实 Qt 的差）

1. **`fromString` 不实现** —— Qt 5.15 没有 `QColor::fromString`（Qt 6.7+ 才有），
   spec 提示误引。解析入口是 `setNamedColor`/构造。登记为对 spec 提示的订正。

2. **CMYK 系不实现；`convertTo(Cmyk/ExtendedRgb)` 返回无效色** —— 范围表保留用量
   0（判据①）。对拍里这两支是**唯一**的 mismatch，各登记一条 DIFFTAG
   （`convertTo.cmyk.deviation` / `convertTo.xrgb.deviation`），谓词与偏离理由同宽：
   比 `spec()`（真 Qt 转成功、pk 返回 Invalid）。

3. **`toArgb32()` 是 PkColor 扩展** —— 真 Qt 5.15 qcolor.h 没有该方法（Qt 6 有）。
   为 Krita 移植期调用方提供，内联 `== rgba()`。不是对齐目标，是赠品。

4. **`operator==` 比较 alpha** —— brief「真 Qt 事实 6：== 忽略 alpha」是**错的**。
   真 Qt qcolor.cpp 2954-2981 比较 `cspec/alpha/hue(Hsv%36000)/green/blue/pad`
   （Hsl 支有 `%36000` + 50 容差；ExtendedRgb 支用模糊比较）。按真 Qt 实现，
   `PkColor(255,0,0,255)==PkColor(255,0,0,128)` 为**假**（测试 `equalityAlphaMatters`
   + 对拍 eq 组覆盖不同 alpha 输入，两侧一致）。

5. **`Qt::green=(0,255,0)`、`Qt::darkYellow=(128,128,0)` 有效** —— brief 探针
   54-55 行把 **SVG 命名色** 当成了 **GlobalColor**：SVG 表里 `"green"=(0,128,0)`、
   `"darkYellow"` 不在表内无效；但 GlobalColor 表里 `Qt::green=QRGB(0,255,0)`、
   `Qt::darkYellow=QRGB(128,128,0)`（真 Qt qcolor.cpp GlobalColor 构造表）。
   两组都按真 Qt 实现（测试 `global_color_values`/`namedColorData` + 对拍 gc 组，
   mismatch=0）。

6. **ExtendedRgb 分量用 `float`（32bit）** —— 真 Qt 5.15 用 `qfloat16`（16bit
   half）。对拍谓词为此放宽（float 分量容差 5e-4、int 分量容差 2），并实测最大
   偏差：本次输入集 `maxFloatDev=0 maxIntDev=0`（选用的越界输入都落在 half 精确
   表示范围内）。将来遇到 half 不可表示的越界浮点输入会有 ±1 级差，属本偏离。

7. **`lighter()/darker()` 对 ExtendedRgb 的连带行为** —— 真 Qt 的 lighter/darker
   对 ExtendedRgb 色走 `toExtendedRgb()` 返回有效色，pk 的 `convertTo(ExtendedRgb)`
   返回无效色（偏离 2 的同源后果）。**当前 PkColor API 下不可达**：没有
   `toExtendedRgb`/`fromRgbF(qfloat16)` 入口，`fromRgbF`（double/float 重载）都产生
   spec=Rgb（真 Qt 实测）。将来若引入 qfloat16 支持，需同时修 lighter/darker 的
   ExtendedRgb 分支。

8. **`std::ostream operator<<(std::ostream&, const PkColor&)` 落位在类型所有者（R-87）** ——
   真 Qt 把对应的值文本（`QTest::toString<T>` 特化）放在**测试库** `QtTest/qtest.h`；
   本仓把它放在**类型所有者**（`PkColor.h` 声明 + `PkColorTestString.cpp` 定义），
   因为 `pk/test` 被 R-82 占着（一个字节都不能动），且 `pk/test/PkTestCompare.h:77-82`
   的注释表明这条 SFINAE 通道本来就是给「类型自己出 ostream 运算符」设计的。
   **这是本仓第一处 `std::ostream operator<<`**（实测 `pk/` 全树此前 0 处）。
   ⚠ **带动一条头文件纪律**：`PkColor.h` 因此新增了它**唯一一条系统头** `#include <iosfwd>`。
   把本头 include 进某个 namespace 的 TU（`oracle/difftest_color.cpp` 的 `namespace pkoracle`、
   对拍/试接类 TU）时，`<iosfwd>`（或任何会带出它的系统头）必须**先在 namespace 之外**落地一次，
   否则会造出 `pkoracle::std` 遮住 `::std`。本仓既有先例见 `pk/geometry/oracle/geometry_difftest.cpp:119-120`
   与 `pk/color/oracle/difftest_color.cpp` 顶部那条同义注释；`pk/geometry` 侧同型的坑在 R-87 实测过
   （`rectf_macro_proof.cpp` 的匿名 namespace 造出 `(anonymous)::std`，直接编译失败）。
   本任务没改对拍/试接 TU（不在 R-87 范围），**实测本头的系统头清单已被 `difftest_color.cpp`
   的全局 include 块覆盖**（逐字复刻其 include 清单 + `namespace pkoracle { #include "PkColor.h" }`
   的 syntax-only 编译 exit=0，见 R-87 报告）。

9. **`PkColor` 的失败值文本是「超出 Qt」，不是「对齐 Qt」（R-87，Q2-a）** ——
   实测真 Qt 5.15.7：`QCOMPARE(QColor, QColor)` 判红时**根本不打 Actual/Expected 行**，
   `QTest::toString<QColor>` 返回 `nullptr`、退化成 `<unprintable>`（探针见
   `R-87.md` §1 探针① 原始输出）。**Qt 对 `QColor` 没有任何 `toString` 文本**。
   ⇒ 本仓给的值文本 `PkColor(ARGB 1, 1, 0, 0)` 是**我们的选择**，Qt **没有对应物**，
   不作对齐断言（对齐目标是「值文本必须存在且可读」，不是「文本与 Qt 逐字相同」）。
   文本形态取自 Qt 对 `QColor` **唯一存在的**文本：`QDebug operator<<(QDebug, const QColor&)`
   （`QtGui/qcolor.h:56-57`，定义在 `qcolor.cpp`）——原文逐字照抄，只把类型名 `QColor`
   换成 `PkColor`：
   - `!isValid()` → `PkColor(Invalid)`
   - `Rgb` → `PkColor(ARGB <alphaF>, <redF>, <greenF>, <blueF>)`
   - `ExtendedRgb` → `PkColor(Ext. ARGB <alphaF>, <redF>, <greenF>, <blueF>)`
   - `Hsv` → `PkColor(AHSV <alphaF>, <hsvHueF>, <hsvSaturationF>, <valueF>)`
   - `Hsl` → `PkColor(AHSL <alphaF>, <hslHueF>, <hslSaturationF>, <lightnessF>)`
   ⚠ 两处与 Qt 原文的**实现层**差别（文本本身仍逐字对齐）：① PkColor 没有
   `hueF/saturationF/valueF/hslHueF/hslSaturationF` 这套 getter，改用 `getHsvF()`/`getHslF()`
   取同名分量（`Hsv` 支 Qt 也是走 `getHsvF`，一致）；② **Cmyk 支无法复现** ——
   PkColor 没有 `cyanF/magentaF/yellowF/blackF`（偏离 2：CMYK 系不实现，实测用量 0），
   该支退化成 `PkColor(Cmyk->ARGB <...>)`，即先 `toRgb()` 再按 Rgb 形态打。
   这条分支在本仓当前**不可达**（没有任何入口能造出 `spec()==Cmyk` 的有效色），
   留它是为了让分支穷尽、将来真引入 CMYK 时文本有个明确落点。
   ⚠ **与 `pk/geometry` 侧的分工**：`PkRect` 那条是**真缺口**（真 Qt `QTest::toString<QRect>`
   打值，Pk 此前不打），文本逐字照抄 `QtTest/qtest.h:168-173`；`PkColor` 这条才是**超出**。
   两条都被 `pk/color/tests/run_tests.sh`（经 ctest 的 `test_pkcolor_comparable`）
   与 `pk/geometry/tests/run_tests.sh`（`test_pkgeometry_comparable`）钉在收尾路径上。

10. **新运算符的连带：`PkDebugIsOstreamable<PkColor>` 由 0 变 1（R-87 修复轮登记）** ——
   偏离 8 那条 `std::ostream operator<<(std::ostream&, const PkColor&)` 落地后，
    `pk/log/PkDebug.h:43-49` 的 SFINAE 探针命中，`PkDebugIsOstreamable<PkColor>` 由
    `false` 变 `true` ⇒ `qDebug() << pkColor` 由「分支三」的 `<unprintable>` 改走
    「分支二」打偏离 9 那条值文本。**方向上这是朝真 Qt 收敛，不是偏离**：真 Qt 的
    `QDebug operator<<(QDebug, const QColor&)` 本来就打值（探针文本
    `QColor(ARGB 1, 1, 0, 0)`，见 `PkColorTestString.cpp` 文件头）。本条登记的是这条
    **连带变更本身**（0→1 与消息通道的改道），不是文本形态——文本形态见偏离 9。
    - **可观测性：当前不可观测**。全树零处同时「内联编 `PkColor.cpp`」+「用 `PkDebug`
      流 `PkColor`」（实测 `grep -rln PkColor.h pk/` 再筛 `PkDebug.h`，只命中
      `pk/CMakeLists.txt`、`pk/geometry/CMakeLists.txt` 两份 CMakeLists，无 TU）——
      是「现在测不到」，**不是「无影响」**。
    - ⚠ **对照组 `PkRect` 没有这条连带**：`PkRect` 另有自由
      `PkDebug operator<<(PkDebug, const PkRect&)`（`pk/geometry/PkGeometryDebug.cpp:107`），
      重载决议仍选中它，`dbg << rect` 文本一字未漂（`test_pkgeometry_debugstream` 全绿）。
      **两边不同形**，别以为 PkColor 的连带在 PkRect 上同样存在。

## 判据③ 口径（零 Qt 符号）

`nm -u -C /tmp/r27-color-build/test_pkcolor | grep -i qt` → **无输出**
（`-C` 反修饰不能省）。test_pkcolor 只链 pkcolor（→ pkstring/pkglobal/pktest），
零 Qt。

⚠ **R-87 起收尾口径换强判据**：`pk/color/tests/run_tests.sh`（新写）用的是
`nm -u -C <libpkcolor.a> | grep -E '\bQ[A-Z][A-Za-z0-9_]*\b'` —— 上面那行
`grep -i qt` 是**作废口径**（R线-spec 2026-09-15 判「不构成证据」：R-70 实测强判据
命中 14、旧口径漏 10）。上面那行保留为 R-27 当时的历史记录，**新脚本按强口径来**
（`pk/test`、`pk/geometry` 那两份至今仍是旧口径，各自的任务去修）。

对拍侧 `oracle/difftest_color` 链接真 Qt —— 那是判据② 的工具，不是交付物，不适用
判据③。

## 对拍（判据②）

```bash
cd oracle && ./run_oracle.sh
```

单 TU 双侧：真 `<QColor>`（全局作用域）+ `PkColor.h` 连同其 PkString/PkGlobal
依赖链包进 `namespace pkoracle`（std 系统头先全局 include，防 `pkoracle::std`）。
`-I` 绝不进 compat/（防垫片合并两侧）。

结果（Qt 5.15.7，ci-env）：

```
DIFF total=54155 mismatch=2
DIFFTAG convertTo.cmyk convertTo.cmyk.deviation 1
DIFFTAG convertTo.xrgb convertTo.xrgb.deviation 1
```

全部 mismatch 落在偏离登记 2，无未登记差异。

## 试接（graft）

```bash
./graft/graft_check.sh
```

真实调用点零改动试接：`<QColor>` 解析到 compat/QColor 垫片（→ PkColor）。
EXPECT_PASS 1 个（`libs/pigment/KoChannelInfo.h`，QColor 作默认实参/成员/返回值），
EXPECT_FAIL 11 个登记各卡住的依赖与归属任务（export 宏归 S 批次、QMetaType 归
R-06、QDebug 归 R-08、QPointF 归 R-21/R-22）。driver 复刻调用点形状，链接 pkcolor
跑绿。exit 0，源树零改动自证。

## 工程形态

- 独立 `project(pkcolor CXX)`，零 Qt，无 `find_package(Qt)`。
- 消费：`pk/global`（PkGlobal.h 标量）、`pk/string`（PkString）、`pk/container`
  （经 PkString）、`pk/test`（PK_* harness）。
- 文件布局：
  - `PkColor.h/.cpp` —— 核心实现（对齐真 Qt qcolor.cpp，注释标 5.15 行号）
  - `compat/QColor`（`#define QColor PkColor`）、`compat/QtGlobal`（转发
    pk/global/compat/QtGlobal）
  - `tests/` —— PK_* harness，114 例全绿
  - `oracle/` —— 真 Qt 对拍（run_oracle.sh + difftest_color.cpp）
  - `graft/` —— 真实生产头试接（graft_check.sh + instantiate_color.cpp）
- 构建（macOS 上必须给部署目标 13.3，理由见下）：
  ```bash
  cmake -S pk/color -B pk/color/build -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=13.3 && cmake --build pk/color/build
  pk/color/build/test_pkcolor
  ```
  ⚠ `-DCMAKE_OSX_DEPLOYMENT_TARGET=13.3` **只在这台 macOS + source 过
  `krita-ci-env/env` 时必要**：env 把 `MACOSX_DEPLOYMENT_TARGET` 设成 `10.15`，
  而 `pk/string/PkString_format.cpp:408/:743` 用 `std::to_chars` 的浮点重载
  （macOS ≥ 13.3）⇒ 报 `'to_chars' is unavailable: introduced in macOS 13.3`。
  **pkcolor PUBLIC 链 pkstring** ⇒ 这条躲不掉。同一个坑与同一个解法在
  `pk/render/tests/run_tests.sh:62-72`、`pk/image/oracle/run_oracle.sh:143`、
  `pk/geometry/README.md`「判据② 的工具」一节都有记录。
- 收尾入口：`pk/color/tests/run_tests.sh`（R-87 新写；此前 pk/color 没有这个脚本，
  收尾入口就是 CMakeLists 里的 `add_test`）。它 configure+build 后走 ctest 跑
  `test_pkcolor` + `test_pkcolor_comparable`（R-87 判据），再跑上面那条强判据③。
  `oracle/run_oracle.sh` 与 `graft/graft_check.sh` **刻意不串进去**（链真 Qt，属判据② 工具；
  要串请另立任务）。
