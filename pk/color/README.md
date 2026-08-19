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

## 判据③ 口径（零 Qt 符号）

`nm -u -C /tmp/r27-color-build/test_pkcolor | grep -i qt` → **无输出**
（`-C` 反修饰不能省）。test_pkcolor 只链 pkcolor（→ pkstring/pkglobal/pktest），
零 Qt。

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
- 构建：
  ```bash
  cmake -S . -B /tmp/r27-color-build && cmake --build /tmp/r27-color-build
  /tmp/r27-color-build/test_pkcolor
  ```
