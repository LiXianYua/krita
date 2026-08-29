# pk/geometry —— 零 Qt 的几何 POD 类型与标量工具（R-03）

独立 `project(pkgeometry)` 薄壳工程，**不接入 Krita 主构建**，不改根 `CMakeLists.txt`。
全局 `Pk` 前缀、不引 C++ namespace（compat 垫片靠 `#define QRect PkRect` 工作，
而 Krita 里有 `class QRect;` 前置声明，套 namespace 这个技巧就废）。

对齐口径：**与 Qt 的任何行为差异默认都是缺陷。** 参照物是真 Qt 5.15.7
（`/mnt/ssd-disk/liyang/projects/krita-ci-env/_install`，`QT_VERSION_STR "5.15.7"`）。
想记成"可接受偏离"的，逐条写进下面的偏离清单，由 reviewer 判。

**交付面**：七个类型 `PkPoint` / `PkPointF` / `PkSize` / `PkSizeF` / `PkRect` /
`PkRectF` / `PkTransform`，加 `PkGlobal.h` 的十项标量工具，加 `compat/` 八个 `#define`
垫片。三条证据链：`tests/`（PK_* 单测）、`oracle/`（链真 Qt5 的逐输入对拍）、
`graft/`（两个真实 Krita 测试类零改动编译并跑绿）。

## 怎么跑

```bash
./pk/geometry/tests/run_tests.sh
```

它做四件事：配置并构建独立工程 → 跑 `test_pkgeometry` → `nm -u libpkgeometry.a |
grep -i qt` 必须无输出（判据③）→ 自证改动只落在 `pk/geometry/` 前缀内（`locks`）。

> **判据③的判别力边界，别搞反。** `nm -u` 那条查的是**替代品本体**
> `build/libpkgeometry.a`。**静态库允许留未定义符号**，所以在 `.a` 上这条有判别力：
> 真引了 Qt 的话符号会挂在那里。反过来，`graft/graft_run.sh` 里对**静态链接出来的
> 试接可执行文件**也跑了一条同形状的 `nm -u`——那条**恒真、没有判别力**（真引了
> Qt，链接期就已经失败，走不到 `nm`）。`graft_run.sh:183-186` 的注释里已经披露了
> 这一点，本文件再说一次：**不要把试接那条 `nm -u` 当成"我们查过了"**。
> 它留着只是因为判据要求这种形式的证据。`oracle/` 更不在此列——对拍**按设计就要
> 链真 Qt**，那边 `ldd` 看得见 `libQt5Core` 才是对的。

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
把每条 `DIFFTAG` 与 `oracle/geometry.deviation` 双向核对。**未声明的差异**、
**已声明 tag 的计数漂移**、**canary 消失**三者任一出现就 FAIL。
方法论与 tag 的两条硬规则写在 `oracle/geometry_difftest.cpp` 的文件头。

> `geometry.deviation` 是**四列** tab 分隔：`<api>`、`<tag>`、`<期望计数>`、`<≥20 码点理由>`。
> 第三列是**额度闸门**：没有它的话"键在清单里"就等于**无限额度** —— 实测同一个
> tag 下差异翻 2.1 倍照样 exit=0（见覆盖度缺口）。

**测试规模的口径**（数字随代码变，改完重跑以实际值为准；下面这一版是 Task 9
收口时现场重测的）。计数口径：**先去掉 `//` 注释再数**
`PK_VERIFY|PK_VERIFY2|PK_COMPARE` 的出现次数——注释里写着的断言不算，不去注释
就会系统性多数（实测踩过）：

| 口径 | 数 | 怎么数的 |
|---|---:|---|
| 测试函数 | 208 | `tests/cases/*.h` 里 `private Q_SLOTS:` 下声明的 slot 个数 = `global_case.h` 13 + `point_case.h` 24 + `size_case.h` 31 + `rect_case.h` 38 + `rectf_case.h` 46 + `transform_case.h` 56 |
| 断言（**写在源里的**） | 1 350 | 去注释后 `PK_VERIFY`/`PK_VERIFY2`/`PK_COMPARE` 的出现次数：`test_global.cpp` 108 + `test_point.cpp` 199 + `test_size.cpp` 214 + `test_rect.cpp` 275 + `test_rectf.cpp` 264 + `test_transform.cpp` 290 |
| 断言（**展开后**） | 1 362 | 与上一行差 12：`test_global.cpp` 的共享宏 `PK_CHECK_COEXIST_PROBE`（体内 12 条）被两个 coexist 测试函数各展开一次，源里只写了一遍。**两个数都要给**，只给一个必然被读成另一个 |
| `static_assert` | 151 | `PkPoint.cpp` 26 + `PkSize.cpp` 42 + `PkRect.cpp` 49 + `PkTransform.cpp` 14 + `oracle/geometry_difftest.cpp` 20（布局 / 枚举取值 / constexpr 能力 / **noexcept 面**，只有在一个 TU 里才落得了地） |
| 运行输出 `Totals` 行 | 15 / 26 / 33 / 40 / 48 / 58 | harness 的口径：每个测试类的 slot 数 + `initTestCase` + `cleanupTestCase`，**不是**测试函数数，也不是断言数。六个类合计 220 |
| 翻译单元 | 13 | `test_main` `test_global` `test_point` `test_size` `test_rect` `test_rectf` `test_transform` + 三个 `coexist_*` + 三个 `*_macro_proof` |
| 对拍比对次数 | 154 358 778 | `run_oracle.sh` 输出的 `DIFF total=`，`mismatch=25 498`（= 3 条 canary + 23 条已声明偏离共 25 495 次，全部落在 `T::mapRect` 的透视裁剪那一支，见偏离清单 21）。拆开：Point 族 35 569 662 + Size 族 63 189 837 + Rect/RectF 两族 26 578 866 + Transform 族 29 020 413 |
| 规则三 map 的声明数 | 56 / 56 / 62 / 64 / 43 | `point_api.map` / `size_api.map` / `rect_api.map` / `rectf_api.map` / `transform_api.map` 的非注释行数，与对应头文件类体里的纯声明逐条对账（不一致即 FAIL）。`api_seen.expected` **303 行（同口径：非注释非空行；裸 `wc -l` 是 311，差的 8 行是注释）** |

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
| `PkSize.h` / `PkSize.cpp` | `PkSize`（两个 `int`）与 `PkSizeF`（两个 `qreal`），逐字抄自 `qsize.h`；两个 `scaled(const Pk*&, mode)` 照 Qt 的形态放在 `.cpp` 里（`QSize::scaled` 定义在 `qsize.cpp`） |
| `PkRect.h` / `PkRect.cpp` | `PkRect`（**四个 `int` 边界坐标 `x1/y1/x2/y2`**）与 `PkRectF`（**四个 `qreal` 的左上角 + 宽高 `xp/yp/w/h`** —— ⚠ **两者内部表示不同，Qt 就是这么不对称的**），逐字抄自 `qrect.h`；`normalized` / `operator\|` / `operator&` / `contains` / `intersects` 两族各一套、外加 `PkRectF::toAlignedRect`，照 Qt 的形态放在 `.cpp` 里（Qt 那些编在 `libQt5Core.so`，本机没有 `qrect.cpp` 源码，它们是**靠对拍逐输入逼出来的**） |
| `PkTransform.h` / `PkTransform.cpp` | `PkTransform`（3×3 齐次矩阵 + 惰性 `m_type`/`m_dirty` 缓存），逐字抄自 `qtransform.h` 与上游 `v5.15.7-lts-lgpl` 的 `qtransform.cpp`。**行向量约定**、**`TransformationType` 是位标志不是 0..5**、**惰性缓存是可观测语义**——三条各错一条整族全错，头文件顶部逐条列了。**R-21 T1** 补了 `map(const PkLineF&)`/`operator*(const PkLineF&, const PkTransform&)`（顺带解开，见「Line 族与 Margins 族」一节） |
| `PkLine.h` / `PkLine.cpp` / `PkMargins.h` / `PkMargins.cpp`（**R-21 T1**） | `PkLine`/`PkLineF`（逐字抄自 `qline.h`；`length`/`angle`/`setAngle`/`angleTo`/`unitVector`/`intersects`/`fromPolar` 七个 out-of-line 成员靠独立差分脚本对真 Qt 逐输入逼出公式）与 `PkMargins`/`PkMarginsF`（逐字抄自 `qmargins.h`，全部 inline）。详见下面「Line 族与 Margins 族（R-21 T1）」 |
| `PkPolygon.h` / `PkPolygon.cpp`（**R-21 T2**） | `PkPolygon`/`PkPolygonF`（**继承** `PkVector<PkPoint>`/`PkVector<PkPointF>` 不是包一层，boost range 概念逼出来的硬约束），逐字抄自 `qpolygon.h` + 上游 `v5.15.7-lts-lgpl` 的 `qpolygon.cpp`。`PkPolygonF` 实现 `containsPoint`（射线穿越/环绕数）、`boundingRect`、`translate`/`translated`、`isClosed`、`toPolygon`、`PkPolygonF(const PkRectF&)`；`united`/`intersected`/`subtracted`/`intersects` 四个 QPainterPath 依赖成员明确不实现（R-22）。详见「Polygon 族（R-21 T2）」 |
| `PkRegion.h` / `PkRegion.cpp`（**R-21 T5**） | `PkRegion`（矩形表 + 两趟相邻合并，**不逐位对齐 Qt 的扫描线划分**、只保证覆盖面积正确，同 KisRegion 的设计）。详见「Region 族（R-21 T5）」 |
| `PkPathClipper_p.h` / `PkPathClipper.cpp.inc`（**R-39 T1**） | 从 Qt 5.15.7 `qtbase/src/gui/painting/qpathclipper_p.h` 与 `qpathclipper.cpp` 移植的私有 winged-edge 路径布尔引擎；保留原始 Qt LGPL-3.0/GPL-2.0/GPL-3.0 license block。`PkPainterPath.cpp` 单点 include `.inc`，公开 `united` / `intersected` / `subtracted` / `simplified`、路径 `contains` / `intersects` 及 `& | + -` 别名。类型替换只涉及 QPainterPath/QPointF/QLineF/QRectF/QBezier 与私有容器，核心拓扑/交点/绕数决策保持上游形态。 |
| `PkMatrix4x4.h` / `PkMatrix4x4.cpp`（**R-21 T4**） | `PkMatrix4x4`（float 4×4 列主序矩阵 + 惰性 flagBits），逐字抄自 `qmatrix4x4.h` + 上游 `v5.15.7-lts-lgpl` 的 `qmatrix4x4.cpp`。`inverted()` 用 double 中间量的伴随矩阵求逆。详见「Matrix4x4 族（R-21 T4）」 |
| `PkVectorND.h` / `PkVectorND.cpp`（**R-21 T3**） | `PkVector2D`/`PkVector3D`/`PkVector4D`（N 维 float 向量），逐字抄自 `qvector2d.h`/`qvector3d.h`/`qvector4d.h` + 上游 `v5.15.7-lts-lgpl` 的 `qvectornd.cpp`。**float/double 精度不对称**（length double 累加、lengthSquared float 累加、dotProduct float 累加）与 **`qIsNull`（精确零）vs `qFuzzyIsNull`（模糊）语义分家**都是反汇编真 `libQt5Gui.so.5` 实测钉死的照抄语义。详见「VectorND 族（R-21 T3）」 |
| `compat/QtGlobal` `compat/QPoint` `compat/QPointF` `compat/QSize` `compat/QSizeF` `compat/QRect` `compat/QRectF` `compat/QTransform` `compat/QLine` `compat/QLineF` `compat/QMargins` `compat/QMarginsF`（后四个 **R-21 T1**）`compat/QPolygon` `compat/QPolygonF`（**R-21 T2**）`compat/QVector2D` `compat/QVector3D` `compat/QVector4D`（**R-21 T3**）`compat/QMatrix4x4`（**R-21 T4**）`compat/QRegion`（**R-21 T5**） | `#define` 垫片，无扩展名，共 **19 个**。垫片形态一致：每个都把同族名字一起给（Qt 的转发头也是这样，包任一个都能拿到全族名字）。Task 5 补齐了 `QRectF`（偏离 16 已消）。**每个垫片都必须先包 `compat/QtGlobal` 再包各自的 Pk 头**（见「与 `pk/test/compat/QtGlobal` 的共存」，Task 7/8 的试接把这条压成了硬纪律） |
| `tests/` | `test_global.cpp`（13 函数）、`test_point.cpp`（24）、`test_size.cpp`（31）、`test_rect.cpp`（38）、`test_rectf.cpp`（46）、`test_transform.cpp`（56）、`test_line.cpp`（29，R-21 T1）、`test_margins.cpp`（22，R-21 T1）、`test_polygon.cpp`（23，R-21 T2）、`test_vectornd.cpp`（28，R-21 T3）、`test_matrix4x4.cpp`（23，R-21 T4）、`test_region.cpp`（18，R-21 T5）、**三个**共存 TU `coexist_*.cpp`、**三个**宏改写探针 `point_macro_proof.cpp` / `size_macro_proof.cpp` / `rectf_macro_proof.cpp`（口径：函数个数按 `cases/*_case.h` 的 `private Q_SLOTS:` 声明数，不含 harness 自带的 `initTestCase`/`cleanupTestCase`；`run_tests.sh` 打的 `Totals: N passed` 是 N = 函数数 + 2） |
| `oracle/` | `geometry_difftest.cpp`（对拍骨架 + Point / Size / Rect / RectF / Transform / Line / Margins / Polygon / VectorND / Matrix4x4 / Region **十一族**）、`run_oracle.sh`、`geometry.deviation`、**`api_seen.expected` 与 `point_api.map` / `size_api.map` / `rect_api.map` / `rectf_api.map` / `transform_api.map` / `line_api.map` / `margins_api.map` / `polygon_api.map` / `vectornd_api.map` / `matrix4x4_api.map` / `region_api.map`（规则三的机器闸门，十一族各一份，见下）** |
| `oracle/painterpath_pathops_difftest.cpp` / `run_pathops_oracle.sh`（**R-39 T1**） | 同一 TU 内保持真 `QPainterPath` 与 `pkoracle::PkPainterPath` 为不同类型，不使用 `compat/`。组合矩形、椭圆、开放/闭合折线、嵌套环、离散复合、自交蝴蝶结和星形，在 OddEven/Winding 下逐输入比较具名运算、四个运算符别名、路径关系、网格成员关系、bounds/fill/empty 与规范化元素签名；`pathops.deviation` 默认必须为空。脚本通过 `ldd` 强制确认真实链接 Qt5Gui/Qt5Core。 |
| `graft/` | 真实调用点试接（判据②）：`graft_run.sh` 拿 **两个真实 Krita 测试类零改动**编译并跑绿——`KisRectsGridTest`（`libs/global/tests`）与 `KisFourPointInterpolatorTest`（`libs/image/tests`），分属两个不同 target。`stubs/` 是把不属于 R-03 的上游依赖顶住的最小垫片（清单与归属见下面「`graft/` 的 stub 清单」），`rename.sed` 做 `QTest`→`PK_*` 的机械改写，`git diff --quiet` 自证源树零改动 |

### 规则三的机器闸门

方法论规则三是「**每个已实现的重载都要有自己的 `rec()`**」。Task 3 复评实测过它的
反面：`PkSizeF::scale(qreal,qreal,mode)` 少写一条 `rec()`，把那个重载整个改坏之后
**93 630 039 次比对一条都没红、`run_oracle.sh` 退出码 0 放行**。没有机器闸门时这条
规则只能靠人手列对照表。

现在它是机器对账的：对拍程序末尾多打一批 `APISEEN <name>` 行（不影响
`DIFF`/`DIFFTAG` 的输出契约），`run_oracle.sh` 做三向核对 ——
① `APISEEN` 集合必须等于 `oracle/api_seen.expected`；
② **三个头文件的类体里每一条声明**都要在对应的 `oracle/<族>_api.map` 里有一行
   （反之亦然）；
③ 各 map 里每个标签都要真的出现在 `APISEEN` 里。
声明指纹由脚本从头文件机械解析（去注释、**去内联函数体**、去形参名、去默认实参），
不是人手抄的；键带类名前缀，因为 `PkPoint`/`PkPointF` 这类孪生类有大量同名同参声明。

**②③ 覆盖全部五族**。**只靠 `api_seen.expected` 是自证循环**：那份清单的内容来自
对拍程序自己打出的 `APISEEN`，**用 `rec()` 去证明 `rec()` 没漏证明不了任何东西**。
五族都接进同一个解析器之后判据才统一：**头文件的类体声明是独立来源**。规模
（口径：`*_api.map` 的非注释非空行数，与解析器从头文件类体机械数出的声明数逐条对账）：

| map | 头文件 | 类 | 声明 / 行 |
|---|---|---|---|
| `oracle/point_api.map` | `PkPoint.h` | `PkPoint`、`PkPointF` | 56 |
| `oracle/size_api.map` | `PkSize.h` | `PkSize`、`PkSizeF` | 56 |
| `oracle/rect_api.map` | `PkRect.h` | `PkRect` | 62 |
| `oracle/rectf_api.map` | `PkRect.h` | `PkRectF` | 64 |
| `oracle/transform_api.map` | `PkTransform.h` | `PkTransform` | 43 |

**扩过去当场抓到一个真实缺口**：`PkPoint()` 与 `PkPointF()` 两个默认构造
**从 Task 2 起就没被对拍比过**（Size 族有 `S::defaultCtor`/`SF::defaultCtor`，
Point 族没有对应物）。已补两条 `rec()`（`cmp_point_constants`），不是写进注释了事。

**闸门自证**（注入实验，两族各一次，`run_tests.sh` 与 `run_oracle.sh` 都跑）：

| 注入 | `run_tests.sh` | `run_oracle.sh` |
|---|---|---|
| `PkRect.h` 加已实现的 `areaHint()`，不写 `rec()` | 退出码 0 全绿 | **FAIL**（闸门②，`PkRect.h 声明 63 条 / rect_api.map 62 行`），`DIFF` 行逐字不变 |
| `PkPoint.h` 加已实现的 `chebyshevLength()`，不写 `rec()` | 退出码 0 全绿 | **FAIL**（闸门②，`PkPoint.h 声明 57 条 / point_api.map 56 行`），`DIFF` 行逐字不变 |

两组都是 Task 3 那个洞的形状：**新成员压根没被比到，所以 `mismatch` 一动不动**，
只有闸门看得见 —— 没有闸门时第二组是**完全看不见的**。

### `Qt::AspectRatioMode`：**全项目唯一一个真 `namespace`**

`QSize::scaled` 的签名里有 `Qt::AspectRatioMode`，而调用点写的就是
`Qt::KeepAspectRatio` 这个限定名 —— 不套 `namespace Qt` 对不上。与「全局 `Pk`
前缀、不引 namespace」那条不冲突：那条针对的是**我们自己的类型**（compat 垫片靠
`#define QRect PkRect`，而 Krita 里有 `class QRect;` 前置声明）。

它放在 `PkGlobal.h` 而不是 `compat/QtGlobal`：`PkSize.h` 的成员签名要用它，而
**几何头不许依赖 `compat/`**（对拍的 `-I` 里绝不能有 compat）。放 `PkGlobal.h`
之后 `compat/QtGlobal` 与 `compat/QSize` 都能经它拿到，与 Qt 的形态一致
（`qsize.h` 自己 `#include <QtCore/qnamespace.h>`）。

实测用量（口径：保留范围 3 325 个文件）：类型名 `Qt::AspectRatioMode` **22 次**、
`Qt::IgnoreAspectRatio` **12 次**、`Qt::KeepAspectRatio` **18 次**、
`Qt::KeepAspectRatioByExpanding` **3 次** —— 都 > 0，所以没有退化成"只留默认参数
所需的定义"。取值实测真 Qt 5.15.7 为 **0/1/2**、`sizeof==4`、底层类型**无符号**
（`PkSize.cpp` 用 `static_assert` 钉住，对拍再用一条跨侧 `static_assert` 核对两侧
取值一致）。`qnamespace.h` 的其余几百个枚举一概不做（判据①）。

⚠ 副作用：一个 TU 若同时包含 `PkGlobal.h` 与**真 Qt 的** `qnamespace.h`，
`Qt::AspectRatioMode` 会重定义。对拍是唯一有这个形态的地方，它把替代品整包塞进
`namespace pkoracle`（于是那份是 `pkoracle::Qt::AspectRatioMode`），已经解决。
剥离完成后的 Krita 里不存在"真 Qt 与替代品共存"的编译行。

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

### ⚠ 被污染的 include 与匿名 `namespace`：**一条规则加一个必须记住的例外**

**问题**：在预处理期改写过语义的 TU 会编出**同名同签名、函数体却不同**的 inline
实体（`operator==(const PkPointF&, const PkPointF&)`、`qAbs<double>` …）。它们默认以
**弱符号**发射，链接器只保留一份，于是：探针调到的可能根本不是自己编出来的那份
（判别力归零：把 `pkQtFuzzy*` 改回 `qFuzzy*` 时单测与对拍会一起绿灯放过），
反过来破坏版也可能赢、把干净 TU 的断言变成假红。

**判据：这个 TU 会不会 odr-use「编在别处的非 inline 成员」。**
**不是**「带进来的东西有没有类型定义」——那个更宽的条件被本目录自己的样板证伪过，
下面有实验。

| 本 TU 是否 odr-use 编在别处的非 inline 成员 | 落匿名 `namespace`？ | 为什么 |
|---|---|---|
| **不会**（只调 header inline / `constexpr` 成员，或压根不碰类型） | **落** | 压成内部链接，弱符号合并的问题彻底消失。代价：那些非 inline 成员在本 TU 里**没有定义**，真调了是**响亮的链接错误**，不是静默错行为 |
| **会**（例如要调 `PkRect::normalized()` —— 它编在 `PkRect.cpp`） | **不能落** | 类跟着变内部链接后，`(anonymous namespace)::PkRect::normalized()` 与 `libpkgeometry.a` 里的 `::PkRect::normalized()` 是两个符号，链接期未定义 |

**这条判据是实测出来的，不是推出来的**（三组实验，每组都跑完整
`run_tests.sh`，跑完 `git checkout --` 还原）：

| 实验 | 做法 | 结果 |
|---|---|---|
| **A** | 把 `coexist_compat_rect_first.cpp` 的两个 compat include 落进匿名 `namespace`（系统头照纪律提到外面），探针**只调 `right()`/`bottom()` 这些 header inline 成员** | **`exit=0`，六个类全绿** —— 说明「带进类型定义就必须不落」是**错的** |
| **B** | 在 A 的基础上把探针改成 `r.normalized().right()`（`normalized` 编在 `PkRect.cpp`） | **`exit=2`**：`undefined reference to '(anonymous namespace)::PkRect::normalized() const'` —— 真正的分界在这里 |
| **C** | 对照：**不落**匿名 `namespace` + 同样调 `normalized()` | **`exit=0`** —— 确认 B 的红只由「落 + odr-use 非 inline 成员」这对组合造成 |

> ⚠ 实验 A 还顺带复现了那条老纪律的必要性：第一次做 A 时忘了把系统头提到匿名
> `namespace` 之外，直接炸出一屏
> `'nullptr_t' is not a member of '{anonymous}::std'`。**系统头留在 `namespace`
> 之外**这条对"落"的那一侧永远成立。

**现状：六个受污染 TU，五落一不落。**
落的五个：`point_macro_proof.cpp`、`size_macro_proof.cpp`、`rectf_macro_proof.cpp`、
`coexist_test_first.cpp`、`coexist_geometry_first.cpp`。
其中 `size_macro_proof.cpp` 与 `rectf_macro_proof.cpp` **也把 `PkSize.h` / `PkRect.h`
这类带类型定义的头落了进去**，靠的正是「本 TU 不调那些非 inline 成员」——
两个文件自己的头注释里写着这条自律（`rectf_macro_proof.cpp:20-24`、
`size_macro_proof.cpp:20-23`：「所以本探针只测 `operator==` / `operator!=`，
一个都不调」）。

**不落的一个：`coexist_compat_rect_first.cpp`。**
⚠ **按上表它其实落在「不会 odr-use」那一行，也就是说它「必须不落」并不成立
——实验 A 证明它落进去照样全绿。** 它现在不落是历史选择，不是被判据逼的。
保持现状是因为改它属于交付面变更、且现状能工作；**但正因为不落，它必须付另一半代价**：
它的 `qAbs` / `qRound` / `qFuzzy*` 留在全局作用域、且来自 `pk/test` 那份垫片
（与别的 TU 来自 `PkGlobal.h` 的同名弱符号函数体不同）——**所以那个 TU 里这些名字
一个都不许 odr-use，连间接的也不行**：

> ⚠ **「不许用 `qAbs`/`qRound`/`qFuzzy*`」这句话比它看起来的范围大。**
> `PkPointF` / `PkSizeF` / `PkRectF` 的三个 `operator==` 函数体里走的是
> `pkQtFuzzyCompare`，而那个函数体里含 `qAbs` / `qMin` —— **所以那三个 `==`
> 同样不许用**。当前探针合规（取值只用整数/浮点四则运算），
> 但下一个人在那个 TU 里随手加一条 `rf == rf` 就会踩。

**落的那五个，纪律只有一条：系统头留在 `namespace` 之外**，否则会造出
`(anonymous)::std`。

## 与 `pk/test/compat/QtGlobal` 的共存

两份同名垫片会在试接时同时出现在编译行上（`-I pk/test/compat -I pk/geometry/compat`）。
**"两份共用同一个 include guard 宏名让后来者空转"这条路走不通**：`pk/test` 那份用的是
`#pragma once`，认的是文件身份，两个不同文件各自都会落地一次；而那份文件不在 R-03 的
`locks` 里，不能改。

代之以两个机制，完整说明在 `PkGlobal.h` 顶部，**三种** include 顺序各有一个 TU
（`tests/coexist_test_first.cpp` / `coexist_geometry_first.cpp` /
`coexist_compat_rect_first.cpp`）编译并核对取值。
**这三个 TU 能编过本身就是断言的一半**——去掉任一机制它们立刻编译失败。

**第三种顺序是真实调用点的顺序**：先 `#include <QRect>`（经 `compat/QRect`）、
之后才 `#include <QtGlobal>`（经 `compat/QtGlobal` → `pk/test` 那份）。例如
`libs/global/KisRectsGrid.h:10` 加 `libs/global/kis_assert.h:10` 就是这个形态，
任何 Krita 测试 TU 都走得到。它靠的是**「`compat/` 的每一个垫片都在包各自的 Pk 头
之前先包 `compat/QtGlobal`」这条纪律**（同时也是对真 Qt「每个公开头先包
`qglobal.h`」的复刻）——少了那行 include，`PkGlobal.h` 先自己定义 `qAbs`、
`pk/test` 那份随后再定义一次，硬错。

**这条纪律有两层守卫，合起来 7/7 覆盖**：
① `coexist_compat_rect_first.cpp` 守 `compat/QRect` 一个（拿掉它的
`#include "QtGlobal"` 或 `#include "QSize"` 当场变红），顺带守住第三种 include 顺序；
② `tests/run_tests.sh` 里的**逐垫片守卫**：对 `pk/geometry/compat/` 下**每一个**
类型垫片单独起一个 TU，先包它、再包 `pk/test` 那份 `<QtGlobal>`，`-fsyntax-only` 编。
**一个垫片一个 TU 是必须的** —— include guard 只认第一次，一旦任何一个垫片把
`compat/QtGlobal` 带进了 TU，后面再包的垫片漏没漏那行都看不出来。
**垫片清单是 glob 出来的，新增 compat 垫片自动纳入**，不靠人记得加一行。

**另一半断言必须包含零侧语义。** 这两条路径上 `qAbs`/`qFuzzyCompare`/`qFuzzyIsNull`
都让位给了 `pk/test` 的实现，而 `pk/test` 不在 R-03 的 `locks` 里、R-11 随时可能动它。
探针只取非零点时，给 `pkFuzzyCompare` 注入一个「任一侧为 0 就走 `fuzzyIsNull`」的
分支（真实的对 Qt 偏离）两个 TU 会全绿——所以 `PkCoexistProbe` 里有
`fuzzyZeroA`/`fuzzyZeroB` 两个字段，钉住 `qFuzzyCompare(0.0, 1e-300)` 与反向都是
`false`（真 Qt 5.15.7 实测）。

## API 范围（判据①：一项不多一项不少）

下面五节是逐族的用量表，先把**共用的口径与规则**说清楚，五节不再重复。

**文件集口径**：**保留范围 3 325 个文件** = `git ls-files` ∩ 保留范围前缀 ∩ 扩展名
`.cpp`/`.h`/`.cc` − 路径含 `tests/`|`benchmarks/` − `pk/`。保留范围前缀取
`Qt替代品选型.md` §1 那条：`libs/{image,brush,pigment,global,store,resources,flake,`
`command,psd,psdutils,metadata,impex,color,surfacecolormanagementapi,koplugin,`
`version,multiarch}` + `plugins/{paintops,filters,generators,impex,color,tools,`
`flake,assistants,metadata}`。**`.cc` 算在内**——Krita 有大量 `.cc` 文件，只数 `.cpp`
会系统性低估（`kis_tool_measure.cc` 那个 `dotProduct` 就是这么漏的）。

**调用点口径**：三种形态 `.name(` / `->name(` / `::name(`，
`xargs -a fileset.txt grep -ohE "(\.|->|::)[[:space:]]*<名字>[[:space:]]*\(" | wc -l`。
**`::` 那一形态不能漏**——`QTransform::fromTranslate` 这类静态调用只落在它上面，
计划里那份只数前两种形态的原始导出因此把 `fromTranslate`(87) / `fromScale`(56) /
`quadToSquare`(3) / `squareToQuad`(2) 全报成了 0。

**规则：API 面 = 族内并集，但并集只决定「要不要查」，不决定「要不要实现」。**
`QPoint`/`QPointF`、`QSize`/`QSizeF`、`QRect`/`QRectF` 是同一概念的整数/浮点孪生，
调用点在两者间自由转换（`toPoint`/`toRect`/`toAlignedRect`），按类型归属的实测数字
只是**下界**——链式调用（`foo().x()`）与成员表达式（`d->rect.x()`）都归不到具体类型。
所以族里任一类型用量 > 0 的名字，两个类型都实现。

**但原始测量按名字数、不区分接收者类型，两个方向都会错**，实测各抓到过：
- **会多**：`unite`/`intersect` 计划说 Rect 族有用量，实测全是 `QSet` 的；
  `transposed`/`transpose` 计划说 Point/Size 族有用量，实测全是 `QTransform` 与
  Eigen 的；`isAffine` 实测全是 Krita 自己的接口（这一条**没守住**，见 Transform 节）。
- **会漏**：`dotProduct` 计划说 0，实测 `kis_tool_measure.cc:139` 有一处
  `QPointF::dotProduct`（`::` 形态 + `.cc` 文件，两个口径缺陷叠在一起）。

**所以每个名字都要再归属一次接收者类型**，判定依据是「本文件里该变量的声明类型 /
`QXxx(...)` 临时量 / `xxx.size()` 这类链式调用的返回类型」。

## Size 族成员名的**接收者归属**（判据①两个方向都要守）

实施计划的「按族取并集」规则有个副作用：**原始测量按名字数，不区分接收者类型**，
于是别的类上的同名调用被吸进了 Size 族的必做清单。Task 2 已经实测证伪过两例
（`transposed` 全是 `QTransform` 的、`scaled` 绝大多数是 `QImage`/`QPixmap` 的）。
本 Task 动手前把 Size 族的每个候选名重新归属了一遍。

口径：保留范围 3 325 个文件，三种调用形态 `.name(` / `->name(` / `::name(`，
接收者按「本文件里该变量的声明类型 / `QSize(...)` 临时量 / `xxx.size()` 链」判定。

| 成员名 | 三形态总命中 | 本族（QSize/QSizeF）真实调用点 | 判定 | 依据 |
|---|---:|---:|---|---|
| `width` | 1 292 | ≥ 276 | **实现** | `QSizeVar` 241 + `size-call` 35；其余大头是 `QRect`(509)/`QRectF`(97)/`QImage`(40) |
| `height` | 1 026 | ≥ 245 | **实现** | 同上（`QSizeVar` 220 + `size-call` 25） |
| `setWidth` | 85 | ≥ 18 | **实现** | 例 `libs/flake/KoPatternBackground.cpp:36`；其余是 `QPen`(13)/`QPainterPathStroker`(11)/`QRectF`(5) |
| `setHeight` | 46 | ≥ 17 | **实现** | 同上 |
| `rwidth` | 5 | **5** | **实现** | 5 处全是 QSize：`KisBezierMesh.h:868,888`、`KisBezierPatch.cpp:45,136`、`kis_motion_blur_filter.cpp:70` |
| `rheight` | 5 | **5** | **实现** | 同上（`:820,904`、`:46,137`、`:71`） |
| `isEmpty` | 1 616 | **8** | **实现** | `KoSvgTextShape.cpp:2148`、`kis_image.cc:1765`、`kis_paint_device.cc:1737/1773(×2)`、`kra_converter.cpp:137/294`、`kis_tool_smart_patch.cpp:259`；其余大头是 `QString`(335)/`QRect`(105) |
| `isNull` | 475 | **1** | **实现** | `libs/flake/KoShapeGroup.cpp:152` 的 `oldSize.isNull()`；其余是 `QDomElement`(71)/`QDomNode`(52)/`QString`(35) 等 |
| `isValid` | 292 | **2**（+1 在注释里） | **实现** | `SvgParser.cpp:1437`、`KisResourceThumbnailCache.cpp:196`；`KoSnapStrategy.cpp:633` 那条被注释掉了。其余是 `QModelIndex`(49)/`QRect`(17) 等 |
| `scale` | 139 | **4** | **实现** | `kis_paint_device.cc:1731/1767`、`ora_converter.cpp:88`、`kis_auto_brush.cpp:386`；其余大头是 `QTransform::scale`(43+46 未定名接收者) |
| `scaled` | 20 | **2** | **实现** | `kra_converter.cpp:297`（`newSize.scaled(QSize(256,256),Keep)`）与 **`kis_paint_device.cc:1822`**（`deviceExtent.size().scaled(maxw,maxh,mode)`，`deviceExtent` 是 `QRect` → `.size()` 是 `QSize`）。⚠ **简报说只有 1 处，实测是 2 处**。其余 18 处是 `QImage::scaled`/`QPixmap::scaled` |
| `expandedTo` | 2 | **2** | **实现** | `kra_converter.cpp:297`（接收者是 `QSize::scaled` 的返回值，写法是 `.expandedTo({1,1})` —— 花括号初始化，要求构造函数非 `explicit`）、`kis_brush_selection_widget.cpp:156`（实参是 `widget->sizeHint()`，返回 `QSize`） |
| `toSize` | 4 | **4** | **实现** | 全部是 `<QRectF>.size().toSize()`：`KoMeshPatchesRenderer.h:43`、`KoPatternBackground.cpp:250/256`、`SvgWriter.cpp:283` |
| `transposed` | 5 | **0** | **不实现** | 5 处全是 `QTransform::transposed()`（`kis_algebra_2d.cpp:935` + `kis_free_transform_strategy_gsl_helpers.cpp:356-359`）。**那个名字归 `PkTransform`** |
| `transpose` | 5 | **0** | **不实现** | 5 处全是 **Eigen 矩阵**的 `.transpose()`（`KisBezierUtils.cpp:1201`×2、`kis_algebra_2d.cpp:949` 与 :957 的注释、`kis_selection_filters.cpp:525`），与 Qt 无关 |
| `boundedTo` | 0 | 0 | **不实现** | 三形态实测 0 |
| `grownBy` / `shrunkBy` | 0 | 0 | **不实现** | 三形态实测 0（且需要 `QMargins`，不在 R-03 范围） |
| `fromCGSize` / `toCGSize` | 0 | 0 | **不实现** | 三形态实测 0（macOS 专用桥接） |

零用量清单的复查命令与原始输出（`xargs -a fileset.txt grep -ohE
"(\.|->|::)[[:space:]]*<名字>[[:space:]]*\(" | wc -l`）：
`boundedTo 0`、`fromCGSize 0`、`grownBy 0`、`shrunkBy 0`、`toCGSize 0` —— 与计划一致。

**`transpose`/`transposed` 不实现这条要特别说明**：`QSize::transpose()` 在 Qt 里是
out-of-line 的（`qsize.cpp`），`PkRect` 也不需要它。真到了 S 线替换
调用点时若冒出新的 `QSize::transposed()` 调用点，按判据①补上即可（照抄
`return PkSize(ht, wd);` 与 `qSwap(wd, ht)`，两行）。

## Rect 族成员名的**接收者归属**（判据①两个方向都要守）

与 Size 族同一套口径：保留范围 **3 325 个文件**（`git ls-files` ∩ 保留前缀 ∩
`.cpp`/`.h`/`.cc` − `tests/`|`benchmarks/` − `pk/`，**`.cc` 算在内**），三种调用形态
`.name(` / `->name(` / `::name(`，接收者按变量声明类型 / `QRect(...)` 临时量 /
`xxx.rect()` 链判定。命中 ≤ 20 处的逐条看，命中多的取"同一行里出现 `QRect`/`QRectF`
字面量"的那些做直接文本证据。

**有用量、已实现的 51 个名字**（`x` `y` `setX` `setY` `left` `top` `right` `bottom`
`setLeft` `setTop` `setRight` `setBottom` `width` `height` `setWidth` `setHeight`
`size` `setSize` `topLeft` `topRight` `bottomLeft` `bottomRight` `setTopLeft`
`setBottomRight` `center` `moveLeft` `moveTop` `moveTo` `moveCenter` `moveTopLeft`
`translate` `translated` `adjust` `adjusted` `normalized` `contains` `intersects`
`intersected` `united` `isEmpty` `isNull` `isValid` `setRect` `getRect` `setCoords`
`getCoords` `operator|` `operator&` `operator|=` `operator&=` `operator==`）逐个都
找到了 Rect 族接收者。抽几条最容易被别的类抢走的做证据：

| 成员名 | 三形态总命中 | 本族真实调用点 | 判定 | 依据（原始行） |
|---|---:|---:|---|---|
| `normalized` | 58 | ≥ 30 | **实现** | `kis_painter.cc:411` `QRect r = rc.normalized();`（**整数 QRect**）、`kis_pixel_selection.cpp:121`、`psd_layer_section.cpp:925/1103`；被别的类抢走的是 `QVector2D::normalized()`（`kis_tool_measure.cc:221/259`、`DefaultTool.cpp:887-928`、`SvgTextTool.cpp:903`） |
| `united` | 21 | ≥ 18 | **实现** | `kis_pixel_selection.cpp:251` `const QRect r = selection->selectedRect().united(selectedRect());`、`kis_scanline_fill.cpp:501/588`、`kis_image.cc:1146/1259`；其余是 `QRectF` |
| `intersected` | 24 | ≥ 16 | **实现** | `kis_convolution_worker_fft.h:62` `QRect r = ...selectedRect().intersected(QRect(...))`、`kis_grid_interpolation_tools.h:392/393/401`；被抢走的是 `QPolygonF::intersected`（`kis_safe_transform.cpp:169-184`、`multigridpatterngenerator.cpp:257/296`、`krita_utils.cpp:100`） |
| `intersects` | 71 | ≥ 1 | **实现** | `kis_grid_interpolation_tools.h:580` `QRectF(originalBoundsForGrid).intersects(currentBounds)` |
| `contains` | 772 | ≥ 2 | **实现** | `KisBezierUtils.cpp:789` `QRectF(0,0,1.0,1.0).contains(approxStart)`、`kis_assistant_tool.cc:478`；其余绝大多数是容器的 `contains` |
| `moveTo` | 369（点/箭头两形态） | **1** | **实现** | `kis_constrained_rect.cpp:277-279` `QRect newRect = m_rect; newRect.moveTo(offset);`。其余全是 `QPainterPath::moveTo` 与各种 `RandomAccessor::moveTo` |
| `moveTopLeft` | 1 | **1** | **实现** | `KoShapeRubberSelectStrategy.cpp:62` `d->selectRect.moveTopLeft(...)` |
| `setRect` | 21 | **3** | **实现** | `SvgParser.cpp:881/887`（`QRectF maskRect;`）、`kis_experiment_paintop_settings.cpp:44`（`QRectF ellipse;`）。其余 18 处是 `KisFixedPaintDevice::setRect` |
| `getCoords` | 6 | **6** | **实现** | 全是 Rect：`KisRectsGrid.cpp:93`、`kis_lod_transform_base.h:72/93/113`、`kis_paint_device.cc:1343/1349` |
| `setCoords` | 7 | **7** | **实现** | 全是 Rect：`KisRectsGrid.cpp:112`、`kis_algebra_2d.cpp:535/538/571`、`kis_lod_transform_base.h:86/106/127` |
| `transposed` | 5 | **0** | **不实现** | 5 处全不是 Rect：`kis_algebra_2d.cpp:935` 是 `QTransform::transposed()`，`kis_free_transform_strategy_gsl_helpers.cpp:356-359` 是 Eigen 矩阵。**该名字归 `PkTransform`** |
| **`unite`** | **2** | **0** | **不实现** | ⚠ **实施计划说"实测 1–2 处调用点，用量 >0 就实现"，实测证伪**：`kis_layer_utils.cpp:1384` 是 `QSet<int>::unite`（`frames.unite(rasterChan->allKeyframeTimes())`），`kis_transform_worker.cc:214` 的 `dstBounds` 声明在 `:207` 是 **`KisFilterWeightsApplicator::LinePos`**，不是 `QRect`。Rect 族真实调用点 **0** |
| **`intersect`** | **1** | **0** | **不实现** | ⚠ 同上：唯一命中 `kis_layer_utils.cpp:2567` 是 **`QSet<int>::intersect`**（`allKeyframeTimes().intersect(times)`）。Rect 族真实调用点 **0** |
| `marginsAdded` / `marginsRemoved` | 0 | 0 | **不实现** | 三形态实测 0（且需要 `QMargins`，实测 2 次/1 文件，不在 R-03 范围） |
| `moveBottom` / `moveRight` / `moveTopRight` / `moveBottomLeft` / `moveBottomRight` | 0 | 0 | **不实现** | 三形态实测 0。⚠ `moveTopLeft` 是 1（实现），别把这一族一起砍掉 |
| `setTopRight` / `setBottomLeft` | 0 | 0 | **不实现** | 三形态实测 0。⚠ `setTopLeft`(4) / `setBottomRight`(4) 有用量，实现了 |
| `fromCGRect` / `toCGRect` | 0 | 0 | **不实现** | 三形态实测 0（macOS 专用桥接） |

零用量清单的复查命令与原始输出（`xargs -a fileset.txt grep -ohE
"(\.|->|::)[[:space:]]*<名字>[[:space:]]*\(" | wc -l`）：
`marginsAdded 0`、`marginsRemoved 0`、`moveBottom 0`、`moveBottomLeft 0`、
`moveBottomRight 0`、`moveRight 0`、`moveTopRight 0`、`setBottomLeft 0`、
`setTopRight 0`、`fromCGRect 0`、`toCGRect 0` —— 与计划一致。

> **`unite`/`intersect` 这两条是本 Task 对实施计划的实测纠正**，方向与
> Task 2 的 `dotProduct`（计划说 0、实际非 0）相反：计划说非 0、**实际是 0**。
> 两条合起来正好说明"按名字数、不看接收者"这个原始测量口径在**两个方向**上都会错。
> `toRect` / `toAlignedRect` 有用量（18 / 64）但**接收者全是 `QRectF`**，
> 那两个是 `PkRectF` 的事，`QRect` 本身没有这两个成员。

## RectF 族成员名的**接收者归属**（判据①两个方向都要守）

口径与 Rect 族**完全相同**：保留范围 **3 325 个文件**（`git ls-files` ∩ 保留前缀 ∩
`.cpp`/`.h`/`.cc` − `tests/`|`benchmarks/` − `pk/`，**`.cc` 算在内**），三种调用形态
`.name(` / `->name(` / `::name(`。

**为什么大部分成员不用重查一遍**：实施计划的「API 面 = 族内并集」规则是按**族**算的
（`QRect` 与 `QRectF` 是同一概念的整数/浮点孪生，调用点在两者间自由转换）。
Task 4 已经把 Rect 族 51 个有用量的名字逐个归属到 Rect 族接收者，那份结论对
`PkRectF` 一样有效 —— 所以 Task 5 只需要重查**两类**：① Task 4 判为 0 的那批是不是
仍然是 0（防"多实现"）；② `QRectF` 独有、`QRect` 没有的成员（防"漏实现"）。
两类的原始命中数都在下表，命令是
`xargs -a fileset.txt grep -nE "(\.|->|::)[[:space:]]*<名字>[[:space:]]*\("`。

### ② `QRectF` 独有的成员（`QRect` 上没有，单独查过）

| 成员名 | 三形态总命中 | 本族真实调用点 | 判定 | 依据（原始行） |
|---|---:|---:|---|---|
| `toAlignedRect` | **64** | **64** | **实现** | 接收者全是 `QRectF`：`kis_qimage_pyramid.cpp:128` `rect.toAlignedRect()`、`KoClipMaskPainter.cpp:45` `globalClipRect.toAlignedRect()`、`KoSvgTextShape_p_output.cpp:93/180/613` `painter.transform().mapRect(...).toAlignedRect()`、`kis_global.h:325/326/342/343`、`KoBakedShapeRenderer.h:45/46`。`QRect` 本身没有这个成员，所以命中数与归属数相等 |
| `toRect` | **18** | **18** | **实现** | 同上，接收者全是 `QRectF`：`SvgWriter.cpp:271` `SvgUtil::toUserSpace(bbox).toRect()`、`kis_liquify_transform_worker.cpp:446/453/570`（`.exactBounds()` / `.boundingRect()` 都返回 `QRectF`）、`kis_tool_select_rectangular.cc:49` `rect.normalized().toRect()`、`multigridpatterngenerator.cpp:215`、`CutThroughShapeStrategy.cpp:169` |
| `QRectF(const QRect&)`（隐式提升） | 数不出来 | —— | **实现** | 构造无法按调用点 grep 归属（与偏离 6/14/19 同一个性质）。**照 Qt 头文件全集实现**，登记为偏离 19 |

### ① Rect 族判为 0 的那批，RectF 侧重查仍为 0

复查命令与原始输出（`… | wc -l`，口径同上）：
`setTopRight 0`、`setBottomLeft 0`、`moveRight 0`、`moveBottom 0`、`moveTopRight 0`、
`moveBottomLeft 0`、`moveBottomRight 0`、`marginsAdded 0`、`marginsRemoved 0`、
`fromCGRect 0`、`toCGRect 0` —— **与 Task 4 逐字一致，全部不实现**。

另有三个命中非 0 但**接收者不在 Rect 族**的，Task 5 逐条复核后结论与 Task 4 相同：

| 成员名 | 三形态总命中 | 本族真实调用点 | 判定 | 依据（原始行） |
|---|---:|---:|---|---|
| `transposed` | 5 | **0** | **不实现** | `kis_algebra_2d.cpp:935` 是 `QTransform::transposed()`；`kis_free_transform_strategy_gsl_helpers.cpp:356/357/358/359` 四处是 Eigen 矩阵的 `.transposed()`。**该名字归 `PkTransform`** |
| `unite` | 2 | **0** | **不实现** | `kis_layer_utils.cpp:1384` 是 `QSet<int>::unite`；`kis_transform_worker.cc:214` 的 `dstBounds` 声明在 `:207` 是 `KisFilterWeightsApplicator::LinePos` |
| `intersect` | 1 | **0** | **不实现** | `kis_layer_utils.cpp:2567` 是 `QSet<int>::intersect`（`allKeyframeTimes().intersect(times)`） |

### 与 `PkRect` 的重载集差别（不是笔误，是 Qt 的形状）

- **`contains` 一族在 `QRectF` 上没有 `proper` 参数**：`QRect` 有
  `contains(QRect,bool)` / `contains(QPoint,bool)` / `contains(int,int)` /
  `contains(int,int,bool)` 四个；`QRectF` 只有 `contains(QRectF)` /
  `contains(QPointF)` / `contains(qreal,qreal)` 三个。`rectf_api.map` 比
  `rect_api.map` 少一行、且没有 `*Proper` 标签，是照抄的结果。
- **`toRect` / `toAlignedRect` 只在 `QRectF` 上有**；`QRect` 没有反向的转换成员
  （提升是构造函数的事）。
- 合计：`PkRect` 类体 **62** 条声明、`PkRectF` **64** 条（口径：`run_oracle.sh` 的
  规则三解析器从类体的纯声明里机械数出来的，不是人手数的；两份 `*_api.map` 的行数
  与它逐条对账，不一致即 FAIL）。

## Transform 族成员名的**接收者归属**（判据①两个方向都要守）

口径与前面各族相同：保留范围 **3 325 个文件**，三种调用形态 `.name(` / `->name(` /
`::name(`。**⚠ 这一族的口径比前面几族弱一档，必须先说清**：下表的「三形态总命中」
是**未按接收者归属的原始数**，只有加粗的那几行逐条核过接收者。原因是 Transform
族有 28 个成员名、其中 `map`(781)、`reset`(375)、`type`(306) 这类名字在 Krita 里
被几十个类共用，逐条归属的成本远超其判别价值 —— 而**判据①真正会被违反的方向是
「实现了没人用的东西」**，那个方向由「总命中为 0 就不实现」这条守着（下面「明确
不实现」一节，26 个名字逐个重测过），不需要归属。

| 成员名 | 三形态总命中 | 判定 | 依据 |
|---|---:|---|---|
| `map` | 781 | **实现**（四个重载） | 大头，含 `QPainterPath::moveTo` 之外的各种 `map`；Transform 族接收者例：`kis_qimage_pyramid.cpp` 一片 |
| `reset` | 375 | **实现** | 名字被大量 Krita 类共用，Transform 族接收者存在 |
| `type` | 306 | **实现** | 同上 |
| `translate` | 162 | **实现** | 同上 |
| `inverted` | 153 | **实现** | 同上 |
| `scale` | 139 | **实现** | Size 族那张表里已归属过一部分（`QTransform::scale` 43 + 46 未定名接收者） |
| `fromTranslate` | **87** | **实现** | ⚠ 全部是 `::` 静态形态 —— 计划里那份原始导出只数 `.name(`/`->name(`，把它误报成 0（详见「与决策文档 / 实施计划的差异」第 2 条同型） |
| `rotate` | 78 | **实现** | |
| **`mapRect`** | **68** | **实现**（两个重载） | 逐条看过，接收者全是 `QTransform`：`kis_qimage_pyramid.cpp:140/177/190/248` … |
| `fromScale` | **56** | **实现** | 同 `fromTranslate`，`::` 静态形态 |
| **`rotateRadians`** | **36** | **实现** | Transform 接收者确证：`kis_qimage_pyramid.cpp:139` `QTransform().rotateRadians(...)`、`KoSvgTextShape_p.h:275`、`KisBezierMesh.h:1499` |
| `m11` `m33` | 28 / 28 | **实现** | |
| **`shear`** | **26** | **实现** | Transform 接收者确证：`KoShape.cpp:241` `shearMatrix.shear(sx, sy)`（`shearMatrix` 是 `QTransform`）；`KoShape::shear` 本身是 Krita 自己的同名成员 |
| **`isIdentity`** | **25** | **实现** | Transform 接收者确证：`kis_qimage_pyramid.cpp:290`、`KoMarker.cpp:262/278`、`SvgUtil.cpp:106`；其余是 `KisCubicCurve`/`ToolTransformArgs` 等自备的同名成员 |
| `dx` `dy` | 24 / 23 | **实现** | |
| `m12` `m13` `m21` `m22` `m23` | 20 / 22 / 20 / **23** / 21 | **实现** | 逐条列，不写区间 —— 区间写法既藏掉五个真实值，也曾把 `m22`=23 写成上界 22（全分支评审实测抓到） |
| `m31` `m32` | 12 / 12 | **实现** | Transform 接收者确证：`kis_algebra_2d.cpp:752/782/811` `t.m31()`（`t` 是 `QTransform`） |
| **`determinant`** | **6** | **实现** | Transform 接收者确证 **3 处**：`KoUnit.cpp:389`（形参 `const QTransform &t`）、`kis_algebra_2d.cpp:801/897`（形参 `const QTransform &t0`）。另 3 处是 **Eigen** 矩阵（`kis_algebra_2d.cpp:991`、`kis_free_transform_strategy_gsl_helpers.cpp:389`）与 `DecomposedMatrix::SC`（`kis_free_transform_strategy.cpp:538`） |
| **`transposed`** | **5** | **实现** | Point/Size 两族查过三遍都判 0，5 处**全部**归这里：`kis_algebra_2d.cpp:935` 的 `globalToLocal.transposed()`（`globalToLocal` 声明在同文件 :930 是 `QTransform`）。⚠ 另有 5 处形近的 `transpose` 全是 Eigen，与 Qt 无关 |
| **`Qt::Axis`**（`rotate` / `rotateRadians` 的形参类型） | **3** | **实现** | ⚠ **`PkGlobal.h` 原先把这一条记成「实测 0」，是错的**（它引的那条命令现场重跑给 29，因为把 `pk/` 自己数进去了）。三个口径：**保留集 3 325 = 3**（`patterngenerator.cpp:176/177/178`，`transform` 声明在 `:102` 是 `QTransform`）；**全仓去 `pk/` = 5**（多的两处 `kis_color_selector.cpp:560/638`，`mirror` 声明在 `:559/:637` 是 `QTransform`，**被保留范围前缀滤掉 —— `plugins/dockers` 不在保留范围，与 `qIsFinite` 那条口径缺口同型**）；**含 `pk/` 自己 = 29，别用这个口径**。用量 > 0 → 判据①直接要求实现，**不是偏离** |
| **`setMatrix`** | **2** | **实现** | 两处**全部**是 `QTransform`：`KoSvgTextShape_p_glyphs.cpp:398`（`bitmapTf` 声明在 :388 是 `QTransform`）、`kis_dom_utils.cpp:261`（`t` 是 `QTransform*`） |
| **`isAffine`** | **6** | **⚠ 实现了，但 Transform 族真实调用点 = 0** | 见下面这条警示 |

> **⚠ 判据①「一项不多」在 `isAffine` 上没守住 —— Task 9 收口时才查出来，未修。**
> 三形态 6 处命中**没有一处**是 `QTransform`：`kis_transform_mask.cpp:459/512/578/634`
> 与 `inplace_transform_stroke_strategy.cpp:1003` 是
> `KisTransformMaskParamsInterface::isAffine()`，`kis_transform_mask_adapter.cpp:52`
> 是 `KisTransformMaskAdapter` **自己的定义行**。形状与 Task 4/5 抓到的
> `unite`/`intersect`（都是 `QSet` 的）**一模一样**：按名字数、不看接收者。
> `PkTransform::isAffine()` 现在是公开成员（`PkTransform.h:147/290`），有
> `transform_api.map` 一行、对拍一条 `rec()`、单测 6 处引用。
> **本 Task 不改它**：删一个已实现的成员要同时动头文件、`api_seen.expected`、
> `transform_api.map`、对拍与单测五处，属于交付面变更而不是收口。
> **已作为偏离清单第 25 条登记**（人做裁决时从头扫的是那张表，不是这里）：
> 要么按判据①删掉，要么改判为一条有意的偏离并在那里补上理由。
> 注意它与 `adjoint` 不同 —— `adjoint` 是 `inverted` 的 TxProject 路径**内部要用**
> 所以留成私有 helper，`isAffine` **没有任何内部调用者**
> （`PkTransform.cpp:1065` 出现的 `isAffine` 只是一条 `static_assert` 的消息字符串）。
> 另外：**实施计划把 `isAffine` 列进了「必须实现」清单**，所以这是**计划的实测错误**，
> 与 `dotProduct`（计划说 0、实际非 0）方向相反。

## Line 族与 Margins 族（R-21 T1）

**这两族是 R-21 交付的，不是 R-03。** 补在这里是因为它们复用同一份对拍+试接装置
（`oracle/geometry_difftest.cpp`、`pk/test` harness、`graft/`），方法论与判据全部
继承自上面各节，不重新定义一套。逐条证据、探针原始输出、公式来源见
`PkLine.h`/`PkLine.cpp`/`PkMargins.h` 文件头注释（比这里详细得多，这里只汇总
结论表）。

### `PkLine` / `PkLineF`

`QLine` 保留范围内**唯一真实调用点**：`plugins/tools/tool_knife/RemoveGutterStrategy.cpp:56`
`QLineF l = QLine(QPoint(), QPoint(50, 50));`——构造后立即隐式转 `QLineF`，没有
其它成员被直接调用。`PkLine` 因此只实现默认构造、`(int,int,int,int)` 构造、
`(PkPoint,PkPoint)` 构造、`p1()`/`p2()`（后两个实测调用点也是 0，但
`PkLineF(const PkLine&)` 的隐式提升构造要读它们，与 Qt 自己 `QLineF(const
QLine&)` 的写法同构）。其余（`isNull`/`x1`/`y1`/`x2`/`y2`/`dx`/`dy`/`translate`/
`translated`/`center`/`setP1`/`setP2`/`setPoints`/`setLine`/`operator==`/`!=`）
实测三种调用形态皆为 0，不实现。

`QLineF` 类型名裸词现场重测（`grep -rln "\bQLineF\b" libs plugins --include=*.cpp
--include=*.h --include=*.cc`，排除 `tests/`/`benchmarks/`/`pk/`）：530 处 /
68 个文件（`Qt替代品选型.md` §1.1 给的 585/67 是更早一次基线，随 D 线删代码漂移
是预期内的）。逐成员按调用形态核实接收者后：

| 成员 | 判定 | 依据 |
|---|---|---|
| `p1()`/`p2()` | 实现 | 高用量，`kis_global.h:232` 的两个非模板 inline 函数（`kisDistanceToLine`/`kisSquareDistanceToLine`）直接依赖 |
| `translate`/`translated`（两个重载） | 实现 | 真实调用点确认 |
| `intersects(const PkLineF&, PkPointF*)` | 实现 | Qt5 新签名（非 Qt4 的 `intersect()`），探针确认；`RemoveGutterStrategy.cpp`、`kis_algebra_2d_test.cpp`、`TestPerspectiveBasedAssistantHelper.cpp` 等多处真实调用 |
| `pointAt(qreal)` | 实现 | 不夹持 `t`，允许外插（探针确认） |
| `angle()`/`setAngle()`/`angleTo()` | 实现 | `angle()` 逆时针为正、范围 `[0,360)`，`atan2(-dy,dx)` 换算（探针确认符号约定） |
| `length()`/`setLength()`/`unitVector()`/`dx()`/`dy()`/`x1()`/`y1()`/`x2()`/`y2()`/`isNull()`/`setP1`/`setP2`/`operator==`/`!=` | 实现 | Qt 头文件全量方法面，逐个核实真实接收者非零后实现 |
| **`center()`** | 实现 | ⚠ 任务与 R-21 plan.md 给的"完整方法面"清单都漏了它，实测保留范围内 QLineF 接收者 ≥9 处（如 `KoSvgTextShapeLayoutFunc_inShape.cpp:154`），T1 现场补 |
| **`fromPolar(qreal,qreal)`** | 实现 | 同上，任务清单漏了，实测 3 处真实调用点（`kis_paintop_settings.cpp:558`、`psd_additional_layer_info_block.h:560`、`TwoPointAssistant.cc:315`） |
| `setPoints`/`setLine`/`toLine()`/`intersect()`（Qt5.14 废弃旧签名）/`angle(const QLineF&)`（Qt5.14 废弃旧签名） | 不实现 | 实测调用点 0；后两个还带 `QT_DEPRECATED_SINCE` 卫兵 |
| `qHash`/`QDataStream`/`QDebug` | 不实现 | 归 R-02/R-12/R-08，同 Rect 族先例 |

**`PkTransform::map(const PkLineF&)` 顺带解开**：`PkTransform.h` 文件头早就把
`map(QLineF)` 列在「依赖当时范围外的类型」——T1 交付 `PkLineF` 后按 T2 解开
`squareToQuad`/`quadToSquare` 同一个模式顺手补上（`PkLineF(map(l.p1()),
map(l.p2()))`，逐字照抄 Qt 源码，不是新算法）。真实调用点：
`RemoveGutterStrategy.cpp` 多处 `<transform>.map(<QLineF>)`。

### `PkMargins` / `PkMarginsF`

**真实调用点现为 0**（`grep -rln "\bQMargins\b" libs plugins --include=*.cpp
--include=*.h --include=*.cc`，排除 `pk/`：全仓唯一命中是 `pk/geometry/PkRect.h`
自己的注释）——原消费方 `libkdcraw/rnuminput.cpp` 已被 D-02-a 删除。**仍然实现**：
任务定义明确要求，因为它挡着 `PkRect`/`PkRectF` 的四个互操作成员
（`marginsAdded`/`marginsRemoved`/`operator+=`/`operator-=`，这四个本身也是 0
用量）。与 `PkRect` 构造函数/运算符按 Qt 头文件全集实现同一类处置（偏离清单
第 6/14/19 条同型）——这条「仍要做」来自任务定义本身，不是本次实施自选，
是判据①的一条已获批准的例外。

`PkMargins` 按 Qt `qmargins.h` 头文件全集实现：构造、四个 accessor/setter、
`isNull`、算术运算符（`+`/`-`/`*`/`/`/`+=`/`-=`/`*=`/`/=`，标量与 Margins 两种
操作数）、一元 `+`/`-`。**对 R-21 plan.md 的一条实测纠正**：plan.md 说要实现
"`operator|`（取分量最大值）"——**真 Qt 5.15.7 的 `QMargins` 根本没有
`operator|`**（逐字核对头文件全文 + 现场探针确认 `m1 | m2` 编译失败：`no
match for 'operator|'`）。`QMargins` 从来只有算术运算符，没有位运算式的
"分量取最大"语义，本实现不造一个 Qt 没有的运算符。

`PkRectF` 一侧的探针实测确认：`QRectF::marginsAdded`/`marginsRemoved` 吃的是
`QMarginsF`（不是 `QMargins`），两侧无相互转换的隐式捷径，所以另建了
`PkMarginsF`（四个 `qreal`，含 `PkMarginsF(const PkMargins&)` 隐式提升构造）。
`PkRect`/`PkRectF` 各自新增的四个互操作成员见 `PkRect.h` 头部「T1 新增：
QMargins 互操作」一节；原来「明确不实现：marginsAdded/marginsRemoved/
operator±=(QMargins)…」那句已删除。

### 试接（判据②）的覆盖边界，如实登记

R-03 现有两个试接目标（`KisRectsGridTest`、`KisFourPointInterpolatorTest`）都不
直接调用 `QLineF`/`QMargins` 的方法——它们原本是靠 `stubs/QLineF`（只给
`p1()`/`p2()` 的最小占位符）过关。T1 交付真实 `PkLineF` 后**删除了这个占位符
stub**，于是这两个目标现在经由 `kis_global.h:232` 的无条件 include 链，
**真的**编译并链接完整的 `PkLineF` 实现（`compat/QLineF` → `PkLineF`）——
这满足了 T1 最初的强制来源（`kis_global.h` 那两个非模板 inline 函数要求
`QLineF` 是完整类型）。

**诚实登记一个覆盖边界**：`intersects()`/`length()`/`angle()`/`pointAt()` 这类
`PkLineF` 更复杂的方法，**目前没有真实调用点通过 graft 试接覆盖**——真实调用点
存在（`RemoveGutterStrategy.cpp`、`kis_algebra_2d_test.cpp`、
`TestPerspectiveBasedAssistantHelper.cpp` 等），但这些文件的依赖闭包很重
（`RemoveGutterStrategy.cpp` 要 `KisTool`/`KisCanvas2`/`KoViewConverter` 一整条
工具链；`TestPerspectiveBasedAssistantHelper.cpp` 要 `kis_painting_assistant.h`
与 `PerspectiveBasedAssistantHelper` 这样的完整类）。给它们搭配套 stub 的成本
远超 T1 的范围，与 README 上面「三条证据链各自的盲区」一节「graft 覆盖的 API
面远小于单测与对拍」是同一类边界，不是新问题——**这些方法的取值正确性由单测
（`test_line.cpp`）与对拍（`geometry_difftest.cpp` 的 Line 族，逐输入比对真
Qt）覆盖，只是没有"零改动编译真实生产代码"这一层证据**。`QMargins` 同理：
新增的四个 `PkRect`/`PkRectF` 互操作成员目前没有真实消费方，试接判据②对它们
豁免——这与"无法按调用点归属"的运算符/构造函数偏离不是同一回事，是**零用量
导致的**豁免，写清楚以免与偏离清单第 6/14/19 条混淆。

## Polygon 族（R-21 T2）

`PkPolygon.h`/`PkPolygon.cpp` 文件头注释（比这里详细得多，这里只汇总判据口径）：

### `PkPolygon` / `PkPolygonF`

**必须继承不包一层**：`class PkPolygon : public PkVector<PkPoint>`、
`class PkPolygonF : public PkVector<PkPointF>`——`kis_convex_hull.cpp:60-74` 用
boost::geometry 的 `range_iterator<QPolygon>`/`range_const_iterator<QPolygon>`
直接写 `QPolygon::iterator`/`QPolygon::const_iterator`，这两个名字唯一的自然来源
是公开继承 `PkVector<PkPoint>`（`PkArrayContainer` 现成的标准迭代器接口）。组合
（包一层）拿不到。`PkPolygon` 因此只做三个构造 + 继承来的容器操作，十个成员
（`containsPoint`/`boundingRect`/`translate`/`translated`/`isClosed`/`toPolygon`/
`united`/`intersected`/`subtracted`/`intersects`）实测调用点全部落在 `QPolygonF`
接收者上（int 侧 0 个），只在 `PkPolygonF` 实现。

`Qt::FillRule`（`OddEvenFill=0`/`WindingFill=1`）放 `PkGlobal.h` 的既有
`namespace Qt` 块里（与 `AspectRatioMode`/`Axis` 同一条理由：`namespace Qt` 只此
一处，几何头不许依赖 `compat/`）。取值经真 Qt 5.15.7 探针实测确认，非凭文档记忆。
真实调用点 ≥15 处，全部经 `PkPolygonF::containsPoint(PkPointF, Qt::FillRule)` 落地。

`PkPolygonF` 逐字抄自 `qpolygon.h` + 上游 `v5.15.7-lts-lgpl` 的 `qpolygon.cpp`，
out-of-line 成员：`PkPolygonF(const PkRectF&)`（矩形四顶点顺时针 + 首尾闭合共 5
点）、`boundingRect`、`containsPoint`（射线穿越/环绕数，`pkPolygonIsectLine` 逐
字照抄 `qt_polygon_isect_line`，含"水平线跳过""半开区间"两条扫描线约定）、
`toPolygon`、`translate`/`translated`、`isClosed`。

**`PkTransform::map(const PkPolygonF&)` 与 `squareToQuad`/`quadToSquare` 顺带
解开**（T2 交付 `PkPolygonF` 之前做不出来，与 T1 解开 `map(const PkLineF&)` 同一
模式）。`map(QPolygonF)` 的 `TxProject` 分支与真 Qt 有一处登记在案的偏离（真 Qt
铺进 `QPainterPath` 做透视裁剪，本类落回逐点无裁剪的 `map(const PkPointF&)`），
`oracle/geometry.deviation` 的 `T::map(PolygonF) txproject-deviation` 一行。

### 明确不实现（登记在案的偏离，不是遗漏）

`united`/`intersected`/`subtracted`/`intersects` 四个成员真实调用点确实存在
（`kis_safe_transform.cpp:169,170,183,184`、`KoSvgTextShapeLayoutFunc_inShape.cpp:83,84`、
`SelectionDecorator.cpp:109,110` 等），但真 Qt 5.15.7 内部把多边形铺进
`QPainterPath` 借它的布尔集合运算实现（`qpolygon.cpp:897` 起）。`QPainterPath`
不在 R-21 范围（`Qt替代品选型.md` §1 几何那一行点名的十个类型里没有它，归 R-22），
处置与 `PkTransform::mapRect` 在 `TxProject` 且需透视裁剪时落回四角包围盒**同一个
模式**。依赖这四个成员的调用点在本 Task 之后仍然编不过——诚实登记的缺口，见
`PkPolygon.h` 文件头与「覆盖度缺口」一节。

### 试接（判据②）的覆盖边界，如实登记

与 T1 的 `QLineF` 同一个边界：现有两个试接目标都不直接调用 `QPolygon`/`QPolygonF`
的方法（它们只经 `kis_global.h` 的无条件 include 链间接依赖 `QLineF`）。`PkPolygon`
的"必须继承"这个**最关键的形态决策**由单测钉住（`test_polygon.cpp` 直接验
`PkPolygon::iterator`/`begin()`/`end()`/`std::distance`，即 boost range 概念要用的
那套标准迭代器接口），`containsPoint`/`squareToQuad`/`quadToSquare`/`map(QPolygonF)`
的取值正确性由对拍（`geometry_difftest.cpp` 的 Polygon 族，逐输入比对真 Qt）覆盖。
"零改动编译真实生产代码"这一层证据（`kis_convex_hull.cpp` 的 boost 适配、
`KoPolygonUtils.cpp` 的 `QVector<QPolygon>` 元素类型）**没有**——这些文件的依赖
闭包（boost::geometry 头 + `kis_global.h` 整条工具链）很重，给它们配套 stub 的
成本超出 T2 范围，与 T1 登记 `QLineF` 复杂方法同一个边界。

**⚠ T2 现场实测过一次删除 `stubs/QPolygon`+`stubs/QPolygonF`（让第二个试接目标
`KisFourPointInterpolatorTest` 改用真实 `PkPolygonF`），结果编不过**：真实测试类
`KisFourPointInterpolatorTest.cpp:195,257` 用 `src.length()`——这是 `QVector::length()`
的 Qt5 deprecated 别名（`size()` 同义），`PkPolygonF` 继承的 `PkVector`（R-02）
**没有实现 `length()`**（R-02 只做了 `size()`/`count()`，跳过了 deprecated 别名）。
这是 **R-02 容器的缺口，不是 R-21 的**——R-21 的 locks 只在 `pk/geometry/`，无权
改 `pk/container/`。所以 `stubs/QPolygon` **暂时不能删**（它给 graft 提供了
`length()`），等 R-02 补上 `QVector::length()` 或 S 线全量替换时处理。这条已写进
最终回报的 NOTE 转给主会话。

## VectorND 族（R-21 T3）

`PkVectorND.h`/`PkVectorND.cpp` 文件头注释（比这里详细得多，这里只汇总判据口径）：

### `PkVector2D` / `PkVector3D` / `PkVector4D`

三个 N 维 float 向量族，逐字抄自 `qvector2d.h`/`qvector3d.h`/`qvector4d.h` +
上游 `v5.15.7-lts-lgpl` 的 `qvectornd.cpp`。**两条不是常识的照抄语义**，都是反汇编
真 `libQt5Gui.so.5` 实测钉死的（本机没有 qvectornd.cpp 源码）：

1. **float/double 精度不对称**：`lengthSquared()` 是 float 累加（`mulss`/`addss`），
   `length()` 是 double 累加（`cvtss2sd`/`mulsd`/`addsd`/`sqrtsd`/`cvtsd2ss`），
   `dotProduct()` 是 float 累加。统一成某一档精度会让至少一族在极端量级与真 Qt
   分家（对拍里 `V2::lengthSquared` 的 huge/zero 档已经钉住这条不对称）。
2. **`qIsNull`（精确零）≠ `qFuzzyIsNull`（模糊 1e-5）**：`isNull()` 与
   `toVector2DAffine`/`toVector3DAffine` 用的是 `qIsNull`（`== 0.0f`），
   `normalized()`/`normalize()` 的"已是单位/零向量"判定用的是 `qFuzzyIsNull`。
   探针实测 `qIsNull(1e-6f)=false`、`qFuzzyIsNull(1e-6f)=true`——T3 为此在
   PkGlobal.h 补上了此前标"不做"的 `qIsNull`（之前实测调用点 0，三个向量的
   isNull/toVector*Affine 解出了真实调用点）。

**`normalize()` 与 `normalized()` 不是同一函数的两面**：`normalized()` 对 len≈0
返回零向量，`normalize()` 对 len≈0 **保持原样 no-op**（探针实测
`normalize(1e-20,1e-20)` 仍是 `(1e-20,1e-20)`，`normalized(1e-20,1e-20)` 是
`(0,0)`）。第一版写成 `*this = normalized()` 被对拍抓出来，已改成照上游的独立
实现。

### 范围（判据①）

- 实现：全部分量存取/set、算术运算符（+ - * / 及复合与逐分量）、
  `dotProduct`（2D/3D/4D）、`crossProduct`/`normal`（3D）、`distanceTo*`、
  `toPoint`/`toPointF`、`toVector2D/3D/4D` 互转、`toVector2DAffine`/
  `toVector3DAffine`（4D 独有，透视除法）。
- **不实现**：`operator QVariant()`（PkVariant 是 R-06，不在 R-21 范围）、
  `QDataStream`/`QDebug` 流式（实测调用点 0）、`QVector3D::project`/`unproject`
  （签名吃 `QMatrix4x4`，R-21 T4 才交付；实测调用点 0——之前 grep 到的 `.project(`
  全是 QEllipse/KoEllipse 的投影方法不是 QVector3D::project，T4 交付后如确有调用
  点再补）、`Qt::Initialization` 构造（无类型调用点）。

### 试接（判据②）的覆盖边界

现有两个 graft 目标都不直接调用 QVector2D/3D/4D（它们只经 `kis_global.h` 间接
依赖 QLineF）。真实调用点存在（`KisColorimetryUtils.cpp` 整个色彩空间转换模块、
`kis_warptransform_worker.cc`、`kis_perspective_transform_strategy.cpp` 的
`QVector4D`），但依赖闭包很重，配套 stub 成本超 T3 范围。**`QVector4D` 与
`QMatrix4x4` 的真实调用点由 T4 一起在 graft 试接里覆盖**（两者在
`kis_perspective_transform_strategy.cpp` 里是同一段代码），T3 先只做单测 +
对拍两层，与 T1/T2 登记的覆盖边界同一类。

## Matrix4x4 族（R-21 T4）

`PkMatrix4x4.h`/`PkMatrix4x4.cpp` 文件头注释（比这里详细得多，这里只汇总判据口径）：

### `PkMatrix4x4`

`float m[4][4]` **列主序**（与 OpenGL 一致）+ `int flagBits` 惰性分类位。逐字抄自
`qmatrix4x4.h` + 上游 `v5.15.7-lts-lgpl` 的 `qmatrix4x4.cpp`（本机只有 .so，源码
取自上游同版本标签）。**惰性 flagBits 是可观测语义**——`inverted()`/`map()`/
`operator*(矩阵,矩阵)`/`rotate()` 都在 flagBits 上走不同的快速路径，改错一位整族
在极端输入上与真 Qt 分家，与 PkTransform 的惰性 m_type 是同一条纪律。

实现（判据①，只做真实调用点）：默认构造（单位阵）、
`PkMatrix4x4(const PkTransform&)`（3×3→4×4 提升，真实调用点
`kis_perspective_transform_strategy.cpp:146,542`、`tool_transform_args.cc:287`、
`kis_transform_utils.cpp:179,195`）、`operator()(row,col)`（`KisColorimetryUtils.cpp`
的 matrixFromColumns 与三个 static 初始化 lambda 逐元素填矩阵）、
`inverted(bool*)`（4×4 伴随矩阵求逆，double 中间量，真实调用点
`m_toXYZ.inverted()`、`s_xyzToDolbyLMS.inverted()` 等 ≥5 处）、
`map(PkVector3D/PkVector4D)`、`operator*(矩阵,矩阵/向量)`（透视变换中间量）、
`rotate(angle, PkVector3D)`（Rodrigues + 整 90/180 度与三根坐标轴快速路径）、
`scale(factor)`、`translate`、`toTransform()`/`toTransform(distance)`（降回 3×3）。

**明确不实现**（真实调用点 0，逐个 grep 核实）：`determinant()`（公开版，inverted
内部有私有 static helper）、`transposed`/`normalMatrix`/`isAffine`/`column`/`row`/
`setColumn`/`setRow`/`fill`/`data`/`constData`/`optimize`/`copyDataTo`/`mapRect`/
`mapVector`/`ortho`/`frustum`/`perspective`/`lookAt`/`viewport`/`flipCoordinates`/
`operator QVariant()`/`QDataStream`/`QDebug`/`QGenericMatrix` 模板族/`QQuaternion`
旋转。这些里 `.data()`/`.fill()` 的 grep 命中数看着大，实测全是
`QSharedPointer::data()`/`QVector::fill()`/`QImage::fill()` 等其它类型，
QMatrix4x4 接收者一个都没有。

### 试接（判据②）的覆盖边界

与 T3 登记的一致：`QMatrix4x4` 与 `QVector4D` 的真实调用点集中在
`kis_perspective_transform_strategy.cpp`（`realMatrix * v`、`toVector2DAffine`）
与 `KisColorimetryUtils.cpp`（整个色彩空间转换模块），但依赖闭包（KoCanvasBase/
KisTool 工具链、libkdcraw 色彩管理）很重，配套 stub 成本超 T4 范围。取值正确性由
单测（`test_matrix4x4.cpp` 钉结构自洽：单位阵/列主序/往返/平移缩放旋转）+ 对拍
（`geometry_difftest.cpp` 的 Matrix4x4 族，逐元素比 float、逐 API 覆盖 inverted/
map/operator*/toTransform）覆盖。

## Region 族（R-21 T5）

`PkRegion.h`/`PkRegion.cpp` 文件头注释（比这里详细得多，这里只汇总判据口径）：

### `PkRegion`

**不逐位对齐 Qt 的内部矩形划分**（R-21 plan.md「问 4」的裁决）：Qt 用扫描线/XRegion
算法把矩形集合合并成实现定义的最小非重叠划分，那个划分不是规范承诺的公开语义。
移植成本极高（与 `Qt替代品选型.md` §2 判"COW 容器"不值得移植同一理由）。本类内部
用 `std::vector<PkRect>` + 两趟相邻合并（同 KisRegion 的 mergeSparseRects 精神），
维持非重叠不变式，对拍**只比较覆盖谓词**（isEmpty/boundingRect/contains/面积/
intersects），不比较 rects() 的逐条内容。这是一条登记在案的偏离。

实现（判据①，按真实调用点）：默认构造、`PkRegion(const PkRect&)`（隐式，
`QRegion dirtyRegion = realNodeRect;`）、isEmpty/isNull、begin/end 迭代
（`const PkRect*`）、rects/rectCount、boundingRect、contains(PkPoint/PkRect)、
translate/translated、并/交/差/异或（具名 + 运算符 + 复合赋值）、intersects、
operator==/!=。

不实现：`QRegion(const QPolygon&)` 多边形填充构造（0 用量且依赖 QPainterPath 填充）、
`QRegion(const QBitmap&)`（QBitmap 不在范围）、`operator QVariant()`、`QDataStream`、
`QDebug`。

**实测对拍修过一处**：空区域与任何区域取交应得空区域（`QRegion(rect).intersected(
QRegion())` 是空），第一版漏掉这个短路，对拍在退化矩形（`0,0,0,0`/`0,0,-1,-1`）
的输入上抓到面积不一致。

## 明确不实现的清单

三组，理由与归属各不相同。**三组的数字都在 Task 9 收口时重跑过**（口径：保留范围
3 325 个文件，`xargs -a fileset.txt grep -ohE "(\.|->|::)[[:space:]]*<名字>[[:space:]]*\(" | wc -l`）。

### ① 实测调用点 0 的 26 个成员名（判据①「一项不多」）

| 族 | 名字 | 实测 |
|---|---|---:|
| Point | `fromCGPoint` `toCGPoint` | 各 0 |
| Size | `boundedTo` `fromCGSize` `grownBy` `shrunkBy` `toCGSize` | 各 0 |
| Rect | `fromCGRect` `toCGRect` `marginsAdded` `marginsRemoved` `moveBottom` `moveBottomLeft` `moveBottomRight` `moveRight` `moveTopRight` `setBottomLeft` `setTopRight` | 各 0 |
| Transform | `adjoint` `isInvertible` `isRotating` `isScaling` `isTranslating` `mapToPolygon` `quadToQuad` `toAffine` | 各 0 |

合计 2 + 5 + 11 + 8 = **26**。几点必须写清：

- **`adjoint` 不进公开面，但代码里有**：`inverted` 的 TxProject 路径要用它，所以它
  以**私有 helper** 的形式存在（Qt 那边是公开的）。「不实现」指的是不进公开 API 面。
- **`fromCG*` / `toCG*` 那 5 个是 macOS 专用桥接**，Qt 头里就带 `#if defined(Q_OS_DARWIN)`
  卫兵，本身也编不出来。
- **`grownBy` / `shrunkBy` / `marginsAdded` / `marginsRemoved` 还额外依赖 `QMargins`**
  （实测 2 次 / 1 文件，归属未定，见下一节）——就算用量不是 0 也做不了。
- **计划把 `dotProduct` 也列进了这 26 个里面（当时是 27 项），实测证伪**：它有 13 处，
  其中 1 处（`kis_tool_measure.cc:139`）是 `QPointF::dotProduct`，已实现。详见文末
  「与决策文档 / 实施计划的差异」第 2 条。**去掉 `dotProduct` 之后恰好是 26。**

### ② `qRound64`：标量工具里唯一不做的

实测 **0** 次（三形态）。`PkGlobal.h` 只做实测有用量的十项
（`qreal` `qAbs` `qMin` `qMax` `qBound` `qRound` `qFuzzyCompare` `qFuzzyIsNull`
`qIsNaN` `qInf`）。判据①「一项不多」。

### ③ `affine` 与 `det`：**名字碰撞，实为 0 用量**

计划里那份原始导出给的是 `affine` **7**、`det` **6** —— 那是**裸词**口径
（`grep -ohE "\baffine\b"`）。换成调用形态口径（`.affine(` / `->affine(` / `::affine(`）
两个都是 **0**：7 处 `affine` 是注释里的英文单词与 `QMatrix affine` 这个成员名，
6 处 `det` 是局部变量 `qreal det`。**两个口径都在这里给出，是因为只给一个必然被
读成另一个** —— 这正是「报任何数字必须带口径」要防的那种错。
`QTransform::toAffine()` 返回 `QMatrix`（Qt5 已废弃类型），`det` 在 Qt 里根本
不是公开成员名。两个都不实现。

## 归属未定：**需要人来分派**，不在 R-03 交付范围

`Qt替代品选型.md` §1 几何那一行只点名了**四个**类型
（`QRect` / `QPointF` / `QSize` / `QTransform`）。R-03 按「族并集」把它们的孪生
一起做了，实际交付**七个**。而 Krita 里还有**九个几何相关的 Qt 类型**，
**任何一条线都没有认领它们**。下表数字是 **Task 9 收口时现场重测**的
（口径：保留范围 3 325 个文件，`grep -ohE "\bQXxx\b"` 数**出现次数**与**文件数**，
不是调用点数——这批是**类型名**，按调用形态数不出来）：

| 类型 | 出现次数 | 文件数 | 备注 |
|---|---:|---:|---|
| `QPainterPath` | 766 | 168 | **规模最大，是一整个子系统而不是一个类**。R-03 的唯一一条真实偏离就卡在它身上（`mapRect` 的透视裁剪支，偏离 21） |
| `QLineF` | 585 | 67 | `kis_global.h:232` 无条件 `#include <QLineF>`，紧跟两个**非模板** inline 函数用了 `p1()`/`p2()` —— 于是**任何**包含 `kis_global.h` 的 TU 都要求它是完整类型。试接靠 `graft/stubs/QLineF` 顶住 |
| `QPolygonF` | 274 | 65 | 真 Qt 里 `class QPolygonF : public QVector<QPointF>` —— 它同时压着 R-02（容器）与几何两条线 |
| `QPolygon` | 108 | 22 | 整数版；两个试接目标一次都没用到 |
| `QVector3D` | 73 | 16 | |
| `QVector2D` | 65 | 20 | Task 2 归属 `dotProduct` / `normalized` 时反复撞到它 |
| `QRegion` | 36 | 17 | |
| `QMatrix4x4` | 48 | 9 | |
| `QVector4D` | 13 | 2 | |
| `QMargins` | 2 | 1 | 数量最小，但它挡着 `QRect` 的四个互操作成员 |
| `QLine` | 1 | 1 | 整数版，仅 1 处 |

> 口径说明：`\bQPolygon\b` **不会**命中 `QPolygonF`（`F` 是词字符，词边界不成立），
> `\bQLine\b` 同理不命中 `QLineF`。两对孪生的数字是**互斥**的，不能相加去和
> 「`QPolygon` 相关的总量」对账。

### `squareToQuad` / `quadToSquare`：**实施计划自相矛盾，按范围外处理**

计划**一处**写着这两个「实测 3 次 / 2 次，用量 > 0，**必须实现**」；**另一处**又把
`QPolygonF` 列为归属未定、明确在范围之外。**而这两个函数的签名吃的就是
`QPolygonF`**（`static bool squareToQuad(const QPolygonF &square, QTransform &result)`），
两条要求不可能同时满足。

**Task 9 收口时重数了一遍**（口径：保留范围 3 325 个文件，三形态）：

```
squareToQuad   2 处：plugins/assistants/Assistants/PerspectiveAssistant.cc:302
                     plugins/assistants/Assistants/PerspectiveEllipseAssistant.cc:138
quadToSquare   3 处：plugins/generators/pattern/patterngenerator.cpp:169
                     plugins/generators/screentone/KisScreentoneGeneratorFunctionSampler.h:111
                     plugins/generators/screentone/KisScreentoneGeneratorTemplate.cpp:246
```

**两个数都对，但计划把它们写反了**：计划的行文是「`squareToQuad`/`quadToSquare`
（实测 3 次 / 2 次）」，实际是 `squareToQuad` **2**、`quadToSquare` **3**。
全部 5 处都是 `QTransform::` 静态调用形态。**另有 1 处 `squareToQuad` 在
`libs/image/tests/kis_algebra_2d_test.cpp:175`**，被「排除 `tests/`」的口径滤掉了 ——
S 线做全量替换、连 `tests/` 一起编时它会再冒出来。

**处置：按范围外处理，两个都不实现，等人裁决。** 不是"忘了做"，是
「**唯一站得住的偏离理由是决策文档已明确划在范围外的东西**」这条规则在这里的
直接后果 —— `QPolygonF` 没人认领，吃它的函数就做不出来。
`PkTransform.h:103-105` 的注释里也点名了这一条。

## 要转给别条线的两个缺口（**试接压出来的**）

这两条都是 Task 7/8 拿真实调用点去编才暴露的，**单测与对拍都发现不了**
（下面「三条证据链各自的盲区」一节解释为什么）。

### ① R-01 的 `PkString` 缺 `arg(int a, int fieldWidth)` —— 缺口精确到一个签名

真实调用点 **`libs/global/KisRectsGrid.cpp:23`**：

```cpp
KisUsageLogger::log(QString("… Grid size: %1, log grid size: %2 …")
                    .arg(gridSize, m_logGridSize));
```

走的是 Qt 的 `QString::arg(int a, int fieldWidth = 0, int base = 10, QChar fillChar = ' ')`
—— **第二个 `int` 是字段宽度，不是第二个占位符的值**（Krita 这行本身大概率是笔误，
但笔误也得编过）。`PkString` 现有四个 `arg`：`arg(const PkString&)` /
`arg(const PkString&, const PkString&)` / `arg(int)` / `arg(double)`，没有 `(int,int)`
形态，重载决议去试两参数那个，报
`invalid user-defined conversion from 'int' to 'const PkString&'`。

**处置（历史）**：`graft/stubs/QString` 用**继承** `PkString` 只补这一个重载，
其余 `arg` 靠 `using PkString::arg;` 原样透传。**刻意不整份替换掉 `QString`** ——
整份 `std::string` 垫片也能让试接编过，但那样 R-01 就完全没被压到，试接也就证明
不了"我们的字符串替代品接得住真实调用点"。**缺口因此精确到一个函数签名**，
转给 R-01 时不需要再查一遍。

**已关闭（R-21 T1）**：R-13 已把 `arg(int v, int fieldWidth)` 补进 `pk/string/PkString.h`
的正式实现（`PkString.h:47`）。垫片因此冗余，`graft/stubs/QString` 这个文件已删除——
`pk/geometry/graft/stubs/KisUsageLogger.h:19` 的 `#include <QString>` 现在经
`graft_run.sh` 的 `-I` 顺序（`$STUBS` 里没有同名文件了）自然落到
`pk/string/compat/QString`（`#define QString PkString`），验证过 `graft_run.sh`
仍然全绿。

### ② `qIsFinite` 的**口径冲突** —— 这是要人判的，不是要人补的

`PkGlobal.h` 头部写着「`qIsNull` / `qIsInf` / `qIsFinite` / `qQNaN` / `qSNaN` /
`qFpClassify` / `qFloatDistance` 不在本头」，依据是**实测 0 调用点**。
**Task 9 收口时重测，那个 0 在它自己的口径下是对的**：

| 口径 | `qIsFinite` 命中 |
|---|---:|
| 保留范围 3 325 个文件（排除 `tests/` `benchmarks/`） | **0** |
| 全仓 `.cpp`/`.h`/`.cc`（不排除任何东西） | **4** |

那 4 处拆开是：
- `libs/image/tests/KisFourPointInterpolatorTest.cpp:347` 与 `:348` ——
  **被「排除 `tests/`」滤掉**。而它们正是试接目标②的源文件，所以试接一编就撞上。
- `plugins/dockers/animation/KisAnimCurvesView.cpp:454` 与 `:455` ——
  **被「保留范围前缀」滤掉**（`plugins/dockers` 不在保留范围里）。

**所以缺的不是一次测量，是一层口径**：项目口径统计的是「**保留范围内的生产代码**」，
而「**测试代码也要能编**」是另一层要求，没有被任何一条口径覆盖。S-00 做全量替换时
一定会连 `tests/` 一起编，这个缺口不会自己消失。

**当前处置**：垫在 `graft/stubs/QtGlobal` 末尾并显式标注「这一项与其它垫片都不同，
它是**试接压出来的一个 R-03 范围缺口**」。实现照真 Qt 5.15.7 `qnumeric.h:49`
（`bool qIsFinite(double)`，实现就是 `std::isfinite`）。
**要不要收进 `PkGlobal.h` 由人判** —— 收的话应照 `qIsNaN`/`qInf` 的先例放
`PkGlobal.cpp`，而不是留成 inline。

## `graft/` 的 stub 清单：每个是什么、真正的归属在哪条线

判据②要求「真实调用点试接、零改动」。零改动意味着**上游依赖一个都不能改**，
只能在编译行外面垫。下面这些垫片**没有一个是 R-03 的交付物**，每一个的头注释里
都写着自己的归属。**口径：`git ls-files pk/geometry/graft/stubs` 现在得 12 个文件，
下表 11 行**（`QPolygon` 与 `QPolygonF` 合成了一行，真 Qt 里两个名字也指向
同一个 `qpolygon.h`）——**R-21 T1 删了两个**：`stubs/QString`（`PkGraftQString::arg(int,int)`
已被 R-13 补进 `pk/string/PkString.h` 正式实现，垫片冗余，见上「已关闭（R-21 T1）」）、
`stubs/QLineF`（T1 交付了真实 `PkLineF` 与 `compat/QLineF`，继续留着旧垫片会让
`-I` 顺序把真实实现挡住——试接会一直在测那个只有 `p1()`/`p2()` 的占位符，不是
T1 真正交付的东西）。

| stub | 是什么 | 真正的归属 |
|---|---|---|
| `stubs/QtGlobal` | 第一行就 `#include "../../compat/QtGlobal"`（**标量工具的真身是 R-03 的交付物，不重复实现**），另补三类：定长整数 typedef、版本宏 + `qt_noop`/`Q_FOREACH`/`Q_UNUSED`、`qIsFinite` | typedef → **R-02**；版本宏与 `Q_*` → **S-00**；`qIsFinite` → **R-03 的口径缺口**（见上②） |
| `stubs/QVector` | `std::vector` 薄包装，成员刻意压到试接真用到的 14 个 | **R-02（容器）** |
| `stubs/QPolygon` + `stubs/QPolygonF` | `class QPolygonF : public QVector<QPointF>`，照真 Qt 的继承关系；只给浮点版 | **归属未定**（见上一节；R-21 T2 交付时应比照 `QLineF` 的处置一并清理） |
| `stubs/QSharedPointer` | **刻意只前置声明、不写类体**：`kis_pointer_utils.h` 里全是模板，一个都没被实例化 | **R-02 之后的智能指针面** |
| `stubs/QtCore/qmath.h` | 只补 `qFloor` / `qCeil` 两个（调用点写的是 `#include <QtCore/qmath.h>`，带前缀，垫片也得放同名子目录） | `qmath.h` 整套归属未定；`qFloor`/`qCeil` 在 R-03 的 Rect 族**无调用点**，导出去才是违反判据①（偏离 18） |
| `stubs/kis_algebra_2d.h` | 真品 1 254 行、是一整个二维代数库；只做试接用到的三个模板 | **它自己是一个独立迁移单元**，既不属 R-03 也不属 R-02 |
| `stubs/kis_debug.h` | 只做 `dbgKrita` / `ppVar` / `ENTER_FUNCTION`（**四次全带 `<< …` 续接**）/ `KIS_*` | **R-08（日志与调试设施）** |
| `stubs/KisUsageLogger.h` | 只给 `log` 的真实签名 + 一份定义 | **R-08** |
| `stubs/KoConfig.h` | **留空**。⚠ 刻意不往里塞猜出来的 `HAVE_*` 开关 —— 那会让试接悄悄走进一条与真实构建不同的分支 | 构建系统生成物 |
| `stubs/kritaglobal_export.h` | 导出宏置空。真品由 CMake `generate_export_header()` 生成，源树里根本没有 | 构建系统生成物；**S 线把 target 搬成静态库之后整类消失** |
| `stubs/graft_stubs.cpp` | 实现侧：`kis_assert.h` 那四个函数的定义（**头文件用的是真品**）、`KisUsageLogger::log` | **R-08 与 S 线** |

**`kis_assert.h` 没有同名垫片这一点很重要**：`KIS_ASSERT` 家族的宏体、参数顺序、
"recoverable 返回后继续执行"这条语义全部来自**真品头文件**，没有被垫片改写；
缺的只是 `.cpp` 里的定义（真品那份依赖 `QMessageBox`/`QThread` 一整套 UI 与异常设施）。

## 三条证据链各自的盲区（**方法论账**）

「说不出覆盖不到什么的，说明还没想清楚」——这一节回答的是**每条链各自看不见什么**，
而不是「还有哪些功能没做」（那在下面「覆盖度缺口」）。

| 证据链 | 证明了什么 | **看不见什么** |
|---|---|---|
| `tests/`（PK_* 单测） | 期望值来自真 Qt 探针，逐条钉住反直觉语义；**唯一**能钉住"预处理期宏改写"与"共存 include 顺序"的地方 | ① 期望值是**我们挑的**输入，不是输入空间；② include 顺序是**我们写的**，不是真实调用点的；③ `PK_COMPARE` 对 `double` 走模糊比较（相对 1e-12），主张"逐位一致"必须改用 `PK_VERIFY(sameBits(...))` |
| `oracle/`（逐输入对拍） | 1.54 亿次逐输入与真 Qt 比取值；**唯一**能抓住"单测全绿但取值分家"的地方（实测：`PkSizeF` 隐式提升丢精度，单测 33 用例全绿、对拍抓到 962 323 处） | ① 只覆盖**写了 `rec()` 的重载**，漏一条就是整个重载零覆盖（规则三的机器闸门补这一条）；② 编译行里**没有 `compat/`、没有 `pk/test` 的垫片**（硬闸门禁止），所以预处理期语义偷换与 include 顺序问题它一概看不见；③ 输入是**全组合不是穷举**，覆盖靠输入集选得对 |
| `graft/`（真实调用点试接） | 真实 Krita 测试类**零改动**编译并跑绿；**唯一**能抓住"接口形状对但接不上"的地方 | ① 只有 **2 个**目标、**14 个**测试函数，覆盖的 API 面远小于前两条；② 它证明的是"能编能跑"，不证明取值对（取值对是前两条的事）；③ stub 顶住的那些依赖等于**没被验证** |

**这一节的由来是一个真实的 Critical：`compat/` 漏复刻 Qt 的传递 include。**
每个 Qt 公开头都先包 `qglobal.h`；我们的 `compat/QRect` 一开始没有对应的
`#include "QtGlobal"`。后果是：先 `#include <QRect>`、之后才 `#include <QtGlobal>`
的 TU 里，`PkGlobal.h` 先定义 `qAbs`、`pk/test` 那份随后再定义一次，**硬编译错**。
而 `libs/global/KisRectsGrid.h:10` 加 `libs/global/kis_assert.h:10` 就是这个形态，
**任何 Krita 测试 TU 都走得到**。

- **单测发现不了**：三个 `coexist_*.cpp` 的 include 顺序是**我们自己写的**，
  当时写的两种顺序都不长这样。
- **对拍发现不了**：对拍的 `-I` 里**被硬闸门禁止带 `compat/`**（几何头不许依赖
  `compat/`，否则对拍就不是在比替代品本体了）。
- **只有拿真实调用点去编才暴露得出来。**

修法是把「**每个 `compat/` 垫片都要先包 `compat/QtGlobal`，再包各自的 Pk 头**」
立成纪律，并配回归守卫。

⚠ **纪律立了、守卫只覆盖 1/7，等于没立** —— 这是全分支评审注入验证抓到的：
把另外六个垫片的那行 include 全删掉，`run_tests.sh` 与 `graft_run.sh` **双双 exit=0、
零红**，而手写一个真实形态的 TU（`#include <QTransform>` 后 `#include <QtGlobal>`）
立刻 `redefinition of 'template<class T> constexpr T qAbs'`。**纪律是承重的，守卫没跟上。**
现在 `run_tests.sh` 的逐垫片守卫把覆盖面补到 **7/7**，且按目录 glob，新增垫片自动纳入
（见上「与 `pk/test/compat/QtGlobal` 的共存」）。**「修好了但没有回归守卫，改回去不会
有任何东西变红」是本项目自己命名的缺陷类，它在这里成立过 6/7。**

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

Size 族又添了六条（全部实测真 Qt 5.15.7，`tests/test_size.cpp` 逐条钉住）：

- **`QSize()` 是 `(-1,-1)`，`QSizeF()` 是 `(-1.,-1.)`** —— 不是 `(0,0)`。
  默认构造出来的尺寸 `isValid()==false`，Krita 里 `QSize s;` 之后判 `isValid()`
  的写法靠这个哨兵值。实施计划与简报都没提到这一条。
- **`isNull` / `isEmpty` / `isValid` 三条公式互不相同**，且
  **`QSize(0,0).isValid()` 是 `true`** —— 而 `QRect(0,0,0,0).isValid()` 是 `false`。
  照搬矩形的语义过来会翻车（注入自证第 2 组就是这个，抓到 46 处差异）。
- **整数版 `isEmpty` 是 `wd<1||ht<1`，浮点版是 `wd<=0.||ht<=0.`**。整数上两者
  等价（注入自证第 1a 组把整数版改成 `<=0` 跑出**零差异**，等价性是实测的），
  **浮点上不等价**：`(0.5,0.5)` 在整数公式下会被误判成空（第 1b 组抓到 624 处）。
- **源尺寸任一分量为 0 时 `scaled` 直接返回目标尺寸**，不做比例运算 ——
  `QSize(0,0).scaled(30,30,Keep) == 30x30`，反直觉但实测如此。浮点版的判据是
  `qIsNull`（`== 0.0`，**`-0.0` 也走这条**），而次正规数 `5e-324` **不走**。
- **`QSize::scaled` 的中间量是 `qint64`，整数除法截断**（`(3,7)→10x10 Keep` 得
  `(4,10)` 而不是 `(4.28…)` 的四舍五入 `(4,10)`… 具体见单测里那 3 条），
  回窄到 `int` 时按二补数回绕（`(INT_MAX,2).scaled(2,INT_MAX,Expand)` 实测得
  `(INT_MIN, INT_MAX)`）。改成四舍五入的注入抓到 **570 248** 处差异。
- **`QSizeF::operator==` 是逐分量各一次 `qFuzzyCompare`，没有零分支** ——
  与 `QPointF::operator==`（任一侧为 0 就改走 `fuzzyIsNull`）**不是同一个公式**：
  `QPointF(0,0)==QPointF(1e-300,0)` 是 `true`，`QSizeF` 上同样的值是 **`false`**。
  两边恰好都是 0 时仍相等。抄错族会静默改掉整片相等语义。

## 偏离清单

| # | 偏离 | 理由 |
|---|---|---|
| 1 | `qFuzzyCompare`/`qFuzzyIsNull` 去掉 Qt 原文的 `static` / `Q_REQUIRED_RESULT` / `Q_DECL_UNUSED` | `static` 只影响链接性（每 TU 一份内部副本），另两个是编译器诊断属性。**无行为差异。** |
| 2 | 不做 `qIsNaN(float)` / `qIsFinite` / `qIsInf` 等 `qnumeric.h` 的其余成员 | 用量表只点名 `qIsNaN`/`qInf`，且实参都是 `double`。**`qIsNaN` 的 19 是裸词口径**（`grep -ohE "\bqIsNaN\b"`，3 325 文件），**调用形态口径是 17** —— 两个数都 > 0，判定不受口径选择影响，但**报的时候必须说清是哪一个**；`qInf` 同口径 1；`float` 实参隐式提升到 `double`，取值一致。**少做一项，不是行为差异。** |
| 3 | 不实现 `qRound64` | 实测 0 调用点（R-03 用量表）。判据①「一项不多」。 |
| 4 | `pk/test/compat/QtGlobal` 先进 TU 时，`qAbs`/`qFuzzyCompare`/`qFuzzyIsNull` 用的是它那份实现 | 写法不同、语义等价，两处差异已逐条核对：`t >= T(0)` vs `t >= 0` 对全部算术类型等价（含 `-0.0`，两边都原样返回 `-0.0`）；`std::fabs`/`std::fmin` vs `qAbs`/`qMin` 只在实参含 NaN 时取值不同，而那种情况下两边的 `<=` 都被 NaN 拉成 false。**无行为差异。** 这条不是口头断言：`tests/coexist.h` 的探针把让位路径上的取值（含零侧语义 `fuzzyZeroA`/`fuzzyZeroB`）逐条钉住，`pk/test` 漂离 Qt 时两个 coexist TU 会变红。 |
| 5 | Qt 的宏映射到 C++17：`Q_DECL_CONSTEXPR`→`constexpr`、`Q_DECL_RELAXED_CONSTEXPR`→`constexpr`、去掉 `Q_CORE_EXPORT` / `Q_DECLARE_TYPEINFO` / `QT_WARNING_*` | 分别是可见性属性、容器移动优化提示、`-Wfloat-equal` 的诊断压制。**都不进入可观察行为**，且 35 569 662 次逐输入对拍零真实差异。`Q_DECL_RELAXED_CONSTEXPR` 在 C++14 起就是 `constexpr`，`PkPoint.cpp` 用 `static_assert` 钉住"放宽 constexpr 真的能在编译期改状态"。 |
| 6 | **运算符按 Qt 头文件全集实现**，不按实测调用点裁剪 | 运算符无法按调用点 grep 归属（`a + b` 认不出类型），因此**没有实测用量数字**可依。这违反判据①「一项不多」的字面要求，**登记在案请 reviewer 判**。理由：运算符是值类型的基本语法面，漏一个就让调用点编不过，而"哪些漏了"在替换之前无法测量。范围 = `qpoint.h` 里为 `QPoint`/`QPointF` 声明的全部运算符，一个不多一个不少。 |
| 7 | `PkPointF::isNull()` 直接写 `xp == 0.0 && yp == 0.0`，不引入 `qIsNull` 这个名字（`PkSizeF::isNull()` 同样处理） | `qglobal.h:925-928` 的 `qIsNull(double d)` 就是 `d == 0.0`，逐字等价（实测 `QPointF(-0.0,-0.0).isNull() == true`、`5e-324` 为 `false`，两侧一致）。不把 `qIsNull` 提进 `compat/` 是因为它在保留范围内实测 **0 调用点**，导出去才是违反判据①。 |
| 8 | **不实现 `QSize`/`QSizeF` 两个除法里的 `Q_ASSERT(!qFuzzyIsNull(c))`**，且**对拍以 `-DQT_NO_DEBUG` 编 Qt 头** | 断言设施归 R-08，`pk/geometry` 里没有。Krita 的**发布构建里这条断言整条编译掉**（Qt5 的 cmake 模块给任何非 Debug 构建追加 `-DQT_NO_DEBUG`，见 krita 根 `CMakeLists.txt:968` 那条 option 的说明文字），所以对齐的是发布形态：除以 0 得 `qRound(±inf)`，实测两侧都是 `INT_MIN`。**未对齐的部分诚实登记**：Debug 构建下 Qt 会 `qt_assert` 中止而 `PkSize` 不会 —— 这是行为差异，只是它发生在 Krita 不发布的那种构建里。不加 `-DQT_NO_DEBUG` 时对拍**根本跑不完**（一喂"除以 0"就 `abort()`）。实测：加它前后 Point 族 `total=35569662 mismatch=3` 逐字不变（`qpoint.h` 里一个 `Q_ASSERT` 都没有）。 |
| 9 | `PkSize`/`PkSizeF` **照抄 `noexcept`**，`PkPoint`/`PkPointF` 没有 | 不是不一致：`qsize.h` 几乎每个成员都标了 `noexcept`，`qpoint.h` 只有 `transposed` 标了 —— 两边都是**逐字照抄各自的头**。`noexcept` 是可观察的（`noexcept` 运算符、容器移动选择），所以连"带 `Q_ASSERT` 的两个除法 Qt 恰好没标 `noexcept`"这个不对称也抄了，`PkSize.cpp` 用 7 条 `static_assert` 钉住。 |
| 10 | `PkSize::scaled` 里用 `long long` 而不是 `qint64` | `qint8`..`quint64` 那批 typedef 按用量表归 R-02，R-03 不预先实现（判据①）。本平台 `qint64` 就是 `long long`，逐字等价。**换平台要重看**（LP64/LLP64 上 `long long` 恒 64 位，与 `qint64` 一致）。 |
| 11 | 对拍 TU 里 `#include "PkSize.cpp"`（在 `namespace pkoracle` 内） | 两个 `scaled(const Pk*&, mode)` 是 out-of-line 的（照 Qt 的形态）。`libpkgeometry.a` 里那份是 `::PkSize::scaled`，而对拍需要的是 `pkoracle::PkSize::scaled` —— 两个不同符号，链不上。把 `.cpp` 一起包进 namespace 是**唯一**能让对拍压到那两个函数的办法（改成 header-only 就不是"照抄 Qt 的结构"了）。纪律：`PkSize.cpp` 的系统头（只有 `<type_traits>`）必须在对拍的系统头区里已经出现过。 |
| 12 | **`PkGlobal.h` 里有一个真 C++ `namespace Qt`**（装 `AspectRatioMode` 与 `Axis` **两个**枚举）—— 这是「全局 `Pk` 前缀、不引 namespace」那条架构约束在本目录里的**唯一例外** | `QSize::scaled`/`scale` 的签名里就写着 `Qt::AspectRatioMode`，调用点写的是 `Qt::KeepAspectRatio` 这个**限定名**（实测用量：类型名 22 次、`IgnoreAspectRatio` 12 次、`KeepAspectRatio` 18 次、`KeepAspectRatioByExpanding` 3 次，口径 3 325 个文件），不套 `namespace Qt` 对不上。与那条约束不冲突：约束针对的是**我们自己的类型** —— compat 垫片靠 `#define QRect PkRect`，而 Krita 里有 `class QRect;` 前置声明，套 namespace 这个技巧就废；枚举没有前置声明这个问题。完整论证见上「`Qt::AspectRatioMode`：全项目唯一一个真 `namespace`」一节。**⚠ 共存副作用**：同一个 TU 里若同时出现 `PkGlobal.h` 与**真 Qt 的** `qnamespace.h`，`Qt::AspectRatioMode` 是**重定义硬错**；对拍是唯一有这个形态的编译行，靠把替代品整包塞进 `namespace pkoracle` 绕开。表现是响亮的编译错误、不是静默错行为，所以只登记、不改代码。|
| 13 | **对拍侧不带 `-fwrapv`，库与单测侧带** —— 同一个目录里两套编译旗标，刻意不一致 | 这一条**取代了曾经登记过的"9 行 `span-overflow-ub` 偏离"**（那 9 行连同 704 589 次差异已删除，完整来龙去脉见下面「对拍侧为什么不带 `-fwrapv`」）。为什么不统一：**对拍要与 `libQt5Core.so` 对等**，而那份 `.so` 是编好的、旗标改不动——`operator\|`/`operator&`/`contains(QRect)`/`intersects` 的实现就住在里面。对拍 TU 带 `-fwrapv` 时，同一条 `x2 - x1 + 1 < 0` 判据两侧取值不同，凭空造出 704 589 条与 `PkRect` 行为无关的差异；去掉之后 `mismatch=3`（只剩 canary）。**库/单测侧则必须带**：那边没有 `.so` 对等物的问题，而 `-Os` 无 `-fwrapv` 时 `pointManhattanLength()` 变红（Task 2 裁决，本轮复核仍复现）。两个旗标各自服务于一个目的，统一反而两边都不对。代价另见覆盖度缺口。 |
| 14 | **`PkRect` 的四个构造函数按 Qt 头文件全集实现**，不按实测调用点裁剪 | 与偏离 6（运算符）同一个理由、同一个性质：构造函数无法按调用点 grep 归属（`QRect(a,b,c,d)` 这种写法数不出接收者），因此**没有实测用量数字**可依。范围 = `qrect.h` 里为 `QRect` 声明的全部构造，一个不多一个不少（Darwin 专有的 `fromCGRect` 除外，那个有 `#if defined(Q_OS_DARWIN)` 卫兵且实测 0）。 |
| 15 | **`PkRect` 不实现 `unite` / `intersect`**（Qt5 已废弃的两个别名），与实施计划的字面要求相反 | 计划写的是「实测各 1–2 处调用点，**用量 > 0 就实现**」。本 Task 按三形态重新归属，**实测证伪**：`intersect` 唯一命中是 `QSet<int>::intersect`（`kis_layer_utils.cpp:2567`），`unite` 两处分别是 `QSet<int>::unite`（`kis_layer_utils.cpp:1384`）与 `KisFilterWeightsApplicator::LinePos::unite`（`kis_transform_worker.cc:214`，`dstBounds` 的声明在 `:207`）。**Rect 族真实调用点 0**，按判据①「一项不多」不实现。这是执行判据①，不是缩范围。 |
| 16 | ~~**`compat/QRect` 目前只 `#define QRect`，没有 `QRectF`**~~ —— **Task 5 已补齐，本条不再是偏离** | 原文登记的是一条**跨 Task 的半成品**：`PkRectF` 当时还不存在，`compat/QRect` 只给了 `QRect` 一个名字，于是 `#include <QRect>` 之后直接用 `QRectF` 的调用点会编不过。Task 5 交付 `PkRectF` 的同时补上了 `#define QRectF PkRectF`，并新建 `compat/QRectF`（内容与 `compat/QRect` 等同 —— 垫片是按**文件名**被找到的，宏改写不了 `#include` 的路径）。六个几何垫片现在形态一致：每个都把孪生的两个名字一起给。保留本行而不是删掉，是为了让「这条曾经存在、被指派给谁、什么时候消的」在 README 里留痕。 |
| 17 | `PkRect` 的默认构造函数**函数体挪到类体外**（Qt 写在类体里） | 取值一字不差（`x1(0), y1(0), x2(-1), y2(-1)`），改的只是位置。理由是工程性的：`run_oracle.sh` 的规则三闸门要从 `PkRect.h` 的**类体**机械解析出重载清单，类体里混着初始化列表会让那个解析多一条只为它存在的特例。**无行为差异**，`tests/test_rect.cpp` 的 `rectDefaultIsNullSentinel` 与 `PkRect.cpp` 的 `static_assert(PkRect().isNull())` 各钉一遍。 |
| 18 | **`PkRectF::toAlignedRect` 用 `std::floor` / `std::ceil`，Qt 用 `qFloor` / `qCeil`（`qmath.h`）** | `qmath.h` 的 `qFloor(qreal v)` 就是 `int(std::floor(v))`、`qCeil` 同（只是把 `int(...)` 收进函数里），逐字等价。不把 `qFloor`/`qCeil` 提进 `PkGlobal.h` 是因为它们在保留范围内于 **Rect 族没有调用点** —— 导出去才是违反判据①（与偏离 7 的 `qIsNull` 同一个处理）。`qmath.h` 整体归谁未定，本条同时是给后续线的提醒。取值一致由 149 255 069 次对拍里 `RF::toAlignedRect` 那条 `rec()` 逐输入证明（零真实差异），并被注入实验 C 组反证（抄成 `toRect()` 的实现 → 8 548 次差异、5 个未声明 tag）。 |
| 19 | **`PkRectF` 的五个构造函数按 Qt 头文件全集实现**，不按实测调用点裁剪 | 与偏离 6（运算符）、偏离 14（`PkRect` 的构造）同一个理由、同一个性质：构造函数无法按调用点 grep 归属。范围 = `qrect.h` 里为 `QRectF` 声明的全部构造，一个不多一个不少（Darwin 专有的 `fromCGRect` 除外，有 `#if defined(Q_OS_DARWIN)` 卫兵且实测 0）。 |
| 20 | `PkRectF` 的默认构造函数**函数体挪到类体外**（Qt 写在类体里） | 与偏离 17（`PkRect`）逐字同理：取值一字不差（`xp(0.), yp(0.), w(0.), h(0.)`），改的只是位置，为的是让 `run_oracle.sh` 的规则三闸门能从**类体的纯声明**机械解析出重载清单。**无行为差异**，`tests/test_rectf.cpp` 的 `rectfDefaultIsAllZero` 与 `PkRect.cpp` 的 `static_assert(PkRectF().isNull())` 各钉一遍。 |
| 21 | **`PkTransform::mapRect` 的两个重载在「`type() == TxProject` 且需要透视裁剪」那一支落回四角包围盒，Qt 走 `QPainterPath`** —— R-03 唯一一条**真实**的行为偏离，`geometry.deviation` 里那 23 行全是它 | **唯一站得住的偏离理由：决策文档已明确划在范围外。** `QPainterPath` 不在 `Qt替代品选型.md` §1 几何那一行点名的四个类型里，**归属未定**（实测 766 次 / 168 文件，是一个独立子系统，不是一个函数）。它不是"我们写错了"，是"这一跳的被调方还没有人认领"。**没有别的处置**：Qt 在这一支里把矩形当路径、在近裁剪面 `w = 1e-6` 上真的裁一刀再取包围盒；落回四角包围盒时被夹持到 `1e-6` 的那个角会把包围盒撑到 `1e7` 量级，而 Qt 裁出来的是 `1e6` 量级（实测 `t(1,0,-1, 0,1,0, 0,0,1)` 对 `(0,0,10,10)`：Qt 给 `(0,0,999999.0000000007,1e7)`，四角包围盒给 `(0,0,1e7,1e7)`）。**这一支在代码里是显形的**：`PkTransform.cpp` 的两个 `mapRect` 保住了 Qt 原本的四分支结构，合并分支会省几行、也会让这个洞消失在视野里。逐条对齐见下面「偏离清单里那 23 行怎么读」。 |
| 22 | **`PkTransform` 不留 Qt5 那个永远是 `nullptr` 的 `Private *d`**，代价是 `sizeof(PkTransform) != sizeof(QTransform)` | 那个字段不经任何 API 露出来（Qt6 已删）。**代价诚实登记**：对拍里 Transform 族**没有** `sizeof` 相等的 `static_assert`，而 Point/Size/Rect 三族都有。**无行为差异**，但「布局一致」这条在这一族上确实弱一档。 |
| 23 | **`PkTransform` 不复刻 `#ifndef QT_NO_DEBUG` 的七个 NaN 早退分支** | 与偏离 8（`Q_ASSERT`）同一条口径：实测本机 `libQt5Gui.so` 是带 `QT_NO_DEBUG` 编的（探针：`translate(NaN,1)` 之后 `dx == nan`，说明早退分支不在），Krita 的发布构建同样带 `QT_NO_DEBUG`。对齐的是**发布形态**。**未对齐的部分**：Debug 构建下 Qt 会 `nanWarning()` 并早退而 `PkTransform` 不会 —— 行为差异，只是它发生在 Krita 不发布的那种构建里。 |
| 24 | **`graft/stubs/` 里 14 个垫片不是 R-03 的交付物**，其中 `stubs/QtGlobal` 末尾的 `qIsFinite` 是一条**试接压出来的 R-03 范围缺口** | 垫片本身不是偏离（它们顶的是别条线的东西，清单与归属见上面「`graft/` 的 stub 清单」）。**真正要判的是 `qIsFinite` 那一条**：它不是"别的线的东西暂时垫一下"，而是 R-03 自己的口径缺口 —— 完整论证见上面「要转给别条线的两个缺口」②。放在垫片里而不是直接收进 `PkGlobal.h`，是为了**不擅自改 R-03 的交付面**，请人裁决。 |
| 25 | **`PkTransform::isAffine()` 实现了，但 Transform 族实测调用点 = 0** —— **这条违反判据①「一项不多」，没有站得住的理由，登记在案等人裁决** | **这不是一条有理由的偏离，是一个未闭合的口子。** 三形态 6 处命中没有一处是 `QTransform`：`kis_transform_mask.cpp:459/512/578/634` 与 `inplace_transform_stroke_strategy.cpp:1003` 是 `KisTransformMaskParamsInterface::isAffine()`，`kis_transform_mask_adapter.cpp:52` 是 `KisTransformMaskAdapter` 自己的定义行。形状与 `unite`/`intersect`（都是 `QSet` 的）一模一样：**实施计划把它列进了「必须实现」清单，那是计划的实测错误** —— 与 `dotProduct`（计划说 0、实际非 0）方向相反。**它没有任何内部调用者**（与 `adjoint` 不同 —— 那个是 `inverted` 的 TxProject 路径要用才留成私有 helper，`PkTransform.cpp:1065` 出现的 `isAffine` 只是一条 `static_assert` 的消息字符串）。**收口时才查出来，本 Task 没删**：删一个已实现成员要同时动 `PkTransform.h`、`api_seen.expected`、`transform_api.map`、对拍 `rec()` 与单测五处，属于交付面变更而非收口。**两条出路二选一，由人定**：按判据①删掉，或改判为一条有意的偏离并在这里补上理由。 |

### 偏离清单里那 23 行怎么读（`oracle/geometry.deviation`）

23 行**只有一个根因**，就是偏离 21。它们不是 23 条独立的偏离，是**同一条偏离被
tag 按输入形态切成了 23 格**——切细是为了让额度可推导，不是为了扩大豁免面。

- **谓词与理由逐个限定词对齐（方法论规则二：谓词不许比理由宽）**：tag 只在
  `tfFreshIsProject(m)`（与 Qt 的 `type()` 第一档**逐字相同**的模糊门槛）**且**
  `tfNeedsClip(m, l, r, t, b)`（`qtransform.cpp:1934-1940` 的就地重算，连
  `qMin` 用 `(a<b)?a:b` 而不是 `std::fmin` 都照抄——NaN 上两者取值不同，
  而这里真会吃到 NaN）同时成立时才构造。**两个限定词都在，一个不多。**
  两侧都是拿**输入的九个 double** 重算的，**不问被测对象的 `type()`** ——
  问它的话被测对象坏掉时 tag 会跟着坏。
- **额度（第三列）说得清"为什么恰好是这么多"**：分母合计 **27 492**、
  分子合计 **25 495**（`run_oracle.sh` 结论块打的 27 495 / 25 498 是**含三条 canary**
  的合计，差的正好是那 3 条 —— **两个数字都对，口径不同，别互相对账**）。
  23 行里 **12 行分子 == 分母**（命中即分家，合计 1 547 次）；剩下 11 行的差额
  1 997 次是**裁剪前后包围盒重合**的输入（被夹持的角落在已有包围盒之内）。
- **这 23 行不豁免任何别的东西**：Transform 一节跑了 29 020 413 次比对，
  除这 25 495 次外**一次都没分家**——包括惰性缓存那一整类多步序列、直角特判、
  `inverted` 的三条路径、`map` 四个重载的夹持与不夹持。
- **R-03 至今只有这一条真实偏离，其余全是 canary。** 这是正确结果不是漏测：
  另外六个类型都是**逐字照抄 Qt 头文件与实现**，逐输入对拍下来本来就该是零差异。
  判别力靠**注入自证**（每族至少三组），不靠 `total` 这个数字本身。

## 覆盖度缺口

「说不出覆盖不到什么的，说明还没想清楚」：

### 对拍侧为什么不带 `-fwrapv`（**给 S 线看**）

**Task 4 初版走过一条错路，这一节把它连同数字一起留在这里，因为下一族会原样撞上。**

初版的处置是：把「跨距溢出」那片输入整片豁免 —— `geometry.deviation` 里 9 行
`span-overflow-ub`，声称那 704 589 次差异是旗标造成的、源码层面无解。
**根因诊断是对的，处置是错的**：既然根因是旗标，就该把旗标改掉，而不是把那片
输入白名单化。修复轮去掉对拍侧的 `-fwrapv`，同一份源码同一批输入
`mismatch` 从 704 589 直接回到 **3（只剩 canary）**。

**盲区规模必须量对量 —— 初版这里错了一个数量级。**

| 口径 | 数字 | 占当时的 `total=125 338 365` |
|---|---:|---:|
| 初版报告写的「盲区」（其实是**差异**次数） | 704 589 | 0.56% |
| 被 `span-overflow-ub` 实际覆盖的**比对**次数（实测计数器） | **3 286 575** | **2.62%** |

**差 4.7 倍。** 白名单豁免的是"比对"不是"差异"——一条已声明的 tag 底下，今天
零差异的那些比对明天变红也照样被吞掉。**报豁免规模要报被覆盖的比对数**，
这是「说不出覆盖不到什么就是没想清楚」在这一族的具体形态。
（那 328 万次现在全是真比对。）

去掉之后连带修好的三件事：

1. `geometry.deviation` 回到**全 R-03 canary-only**，「任何非 canary tag = FAIL」
   这条 R 线 spec 的地基保住 —— 它原本要在 Rect 这一族第一次被破例。
2. 与 **Krita 发布构建的旗标一致**（那边同样不带 `-fwrapv`），对拍证明的东西
   离出货形态更近。

**换来的新代价，诚实登记：**

- 溢出输入上"两侧一致"从「被 `-fwrapv` 钉死」变成了
  **「同编译器同旗标下的巧合」**。换一个 GCC 版本编出来的 Qt，或换一档优化，
  可能冒出新 tag。**但那是 FAIL（响的）不是静默放行 —— 失败方向是对的。**
- `x2 - x1` 与 `x2 - x1 + 1` 溢出那片输入上，**两侧仍然都是 UB**，"正确"在那里
  没有定义。我们现在证明的是"同旗标下逐输入同取值"，不是"行为有定义"。
  对拍源里那一档 tag 因此叫 `shape/…` 而不再叫 `defined/…`（标签不许比事实宽）。
- 是否存在真依赖回绕的调用点归 S 线查。实务上这片输入要求矩形跨距超过
  2³¹ 像素，Krita 的图像坐标到不了 —— 但**不拿这条当理由**。

**Task 5/6 直接照做**：`QRectF` 同样有 `.cpp`、`QTransform` 的 `.cpp` 更大，
`.so` 侧够不到对拍 TU 旗标这件事会原样重演。**对拍侧保持不带 `-fwrapv`**，
遇到"两侧分家"先查旗标对不对等，**旗标不对等是装置缺陷，不是偏离**。

### Rect 族其余的结构性缺口

- **对拍 TU 的旗标管不住 `libQt5Core.so`。** Point/Size 两族全部成员都是头文件
  inline 的，两侧天然同旗标；而 `QRect` 有六个成员编在 `.so` 里（`normalized`、
  `operator|`、`operator&`、`contains` ×2、`intersects`）——**对这六个，我们能做的
  只有"让对拍 TU 的旗标与 `.so` 尽量一致"**，做不到"钉死"。这是 Rect 族独有的
  一块结构性缺口，Task 5/6 原样撞上，**别当成新发现重新查一遍**。
- **已声明 tag 的额度由 `geometry.deviation` 第三列守着**（Task 4 修复轮新增）。
  在那之前"键在清单里"就等于无限额度：实测把 4 个 `.so` 侧 API 的判据加宽成
  `(long long)x2 - x1 + 1 < 0`（16 处源码），差异从 704 589 涨到 1 485 313（2.1 倍），
  **两个脚本双双 exit=0 放行**。现在同一注入产生 22 984 个未声明 tag 并 FAIL。
- **`toRect` / `toAlignedRect` / `QRectF` 相关的一切不在本 Task**。它们有实测
  用量（18 / 64）但接收者全是 `QRectF`，归 Task 5。
- ~~**`QRect` 与 `QMargins` 的四个互操作（`marginsAdded`/`marginsRemoved`/
  `operator+=`/`operator-=`）不实现**：`QMargins` 实测 2 次 / 1 文件，
  归属未定，与 `QLineF`/`QPolygonF` 一起是要报回主会话的归属问题。~~
  **R-21 T1 已补齐**：`PkMargins`/`PkMarginsF` 与 `PkRect`/`PkRectF` 的四个
  互操作成员（`PkRect` 吃 `PkMargins`，`PkRectF` 吃 `PkMarginsF`——探针实测
  两侧签名不同）都已实现，`QMargins` 保留范围内真实调用点仍为 0（原消费方
  `libkdcraw/rnuminput.cpp` 已被 D-02-a 删除），仍要做是任务定义本身的要求，
  与 `PkRect` 构造函数/运算符按 Qt 头文件全集实现同一类处置。保留本行不删，
  为了让"这条曾经是缺口、什么时候补上"在 README 里留痕（与偏离清单第 16 条
  同一种写法）。
- **`qHash(PkRect)` 不实现，而且它是 R-02 的一个真实待办**（下面「`qHash` /
  `QDataStream` / `QDebug` 不实现」那条已经点过名，这里按 Task 4 的要求再点一次）：
  本 Task 现场复测，口径为保留范围 3 325 个文件、`grep -ohE` ——
  `QHash<QRect` **1**（`libs/image/kis_suspend_projection_updates_stroke_strategy.cpp:121`
  的 `QHash<QRect, QVector<QRect>> fullRefreshRequests;`）、`QSet<QRect` 0、
  `QMap<QRect` 0、`QMultiHash<QRect` 0、`QHash<QPoint` 0、`QSet<QPoint` 0。
  `QHash` 的键类型要求有 `qHash` 重载而 `PkRect` 没有，**R-02 落地哈希容器时
  必须连 `qHash(PkRect)` 一起补**，否则那一处编不过。
- **对拍的输入是坐标四元组的组合爆破，不是"所有矩形"**：密集域 `6⁴ = 1 296`、
  极值域 `7⁴ = 2 401`、双目极值域 `5⁴ = 625`、手挑 16 个。双目做满
  `1 296² + 625²` 再加手挑与两个域的**双向**交叉。**没覆盖到的是"中等大小的
  随机坐标"** —— 那一片没有分支边界，风险最低，但确实没测。
- **`contains(point)` 的点坐标由矩形自己的边界导出**（`l-1/l/l+1/r-1/r/r+1`），
  导出值越出 `int` 时那一格被跳过（`INT_MIN` 的 `l-1`）。那一格由极值 token
  自己作为矩形坐标覆盖，但**没有"点恰好在 INT_MIN-1"这种输入**（它不存在）。

- **`PkGlobal.h` 那 10 项标量工具本身没有进自动对拍**，它们的期望值来自人工跑真
  Qt 探针（`tests/test_global.cpp`）。`oracle/` 是经由 `PkPoint`/`PkPointF` **间接**
  压到 `qRound`/`qAbs`/`qFuzzy*` 的（`toPoint`、`operator*`、`operator==` 都调它们），
  但 `qMin`/`qMax`/`qBound`/`qIsNaN`/`qInf` 一次都没被对拍碰到。后续 Task 可以在
  `geometry_difftest.cpp` 里加一节直接对拍标量工具，成本很低。
- **⚠ 整数溢出这一整类，两侧（Qt 与替代品）都是 UB，编译器有权给出任何结果。**
  `manhattanLength`/`operator+`/`dotProduct` 在 `INT_MIN`/`INT_MAX` 上就是这类。
  **两侧的旗标口径从 Task 4 修复轮起是分开的**（偏离 13）：
  - **库与单测**：`CMakeLists.txt` 的 `target_compile_options(pkgeometry PUBLIC
    -fwrapv)` 把取值钉成二补数回绕。依据是 `-Os` 无 `-fwrapv` 时
    `pointManhattanLength()` 变红（本轮复核仍复现），加上之后各 `-O` 档取值与
    `-O0` 逐字相同（矩阵见上）—— 它只挡优化器、不改取值。
  - **对拍**：`run_oracle.sh` 的 `CXXFLAGS_ORACLE` **不带** `-fwrapv`，为的是与
    `libQt5Core.so` 旗标对等（理由全文见下面「对拍侧为什么不带 `-fwrapv`」）。
    于是对拍保证的是「**同一组旗标下两侧逐输入同取值**」，**不是**"行为有定义"，
    也不是语言保证。
  - **两者都不等于 Krita 发布构建里的行为**（那边同样不带 `-fwrapv`，取值由优化器
    自由裁量）—— 只是对拍这一侧现在与发布形态更近了一步。
  > 曾经写在这里的「对拍那边 `-O2` 无 `-fwrapv` 时 `std::to_string` 打印溢出后的
  > 负数直接段错误」**已不复现**：Task 4 修复轮不带 `-fwrapv` 跑满 125 338 365 次
  > 比对、退出码 0。这句现在时陈述已订正，别再拿它当依据。
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
- **共存覆盖三种 include 顺序**（见上「与 `pk/test/compat/QtGlobal` 的共存」），
  三个 `tests/coexist_*.cpp` 各测一条。**没有第四种**：`compat/` 是 Krita 源码
  够得到 `PkGlobal.h` 的唯一入口，而三个 TU 已经把「`pk/test` 先」「`compat/QtGlobal`
  先」「`compat/QRect` 一类类型垫片先」三条入口路径占全。真撞上没覆盖的形态时，
  表现是响亮的编译错误（`qAbs` 重定义），不是静默错行为。
  **另一条同类的、三个 coexist TU 都没有覆盖的形态**：`PkGlobal.h` 的
  `namespace Qt { enum AspectRatioMode }` 与**真 Qt 的 `qnamespace.h`** 落进同一个
  TU 时是**重定义硬错**（偏离清单第 12 条）。目前唯一有这个形态的编译行是
  `oracle/`，它靠把替代品整包塞进 `namespace pkoracle` 绕开 —— 于是这条从来没被
  真正撞过。剥离完成后的 Krita 里不存在"真 Qt 与替代品共存"的编译行，所以
  **只登记不解决**；表现是编译期硬错、不是静默错行为，撞上的人不会漏看。
- **让位路径上的取值校验只有探针里那 10 项**（`PkCoexistProbe` 的字段）。
  `pk/test` 若在这 10 项之外的地方漂离 Qt（例如 `pkFuzzyIsNull` 的阈值），
  coexist TU 不会发现。`oracle/` 已经落地，但**它跑的是不带 `pk/test` 垫片的编译行**，
  帮不到这一条；根治办法是把标量工具做成一节直接对拍（见上）并让 coexist 探针
  覆盖更多取值。
- **`oracle/` 的输入是全组合，不是穷举。** 两组输入集，两组都做**满**：
  手挑对抗集 44 个 double / 25 个 int，token 集 21 个 double / 20 个 int；
  一元 API 走 44²+21² = 2 377 个 double 点、25²+20² = 1 025 个 int 点，
  二元 API 走 44⁴+21⁴ = 3 942 577 组 double 两点、25⁴+20⁴ = 550 625 组 int 两点，
  带标量参数的 API 做三层，Point 族合计 35 569 662 次比对；Size 族又加了
  63 189 837 次（一元/二元/带标量参数的部分与 Point 同形，`scaled`/`scale`
  那四个 API 走「手挑 × token 双向交叉」，见下），总计 **98 759 499**。
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
- **⚠ 对拍只覆盖「写了 `rec()` 的那些重载」——漏写一条就是一整个重载零覆盖。**
  Task 3 交付时 `cmp_sizef_scaled` 漏了 `SF::scale(w,h)`（整数版四个重载齐全、
  浮点版只有三个），复评把 `PkSizeF::scale(qreal,qreal,mode)` 整个改坏之后
  **对拍一条都没红、退出码 0 放行**。现已补上，并在 `geometry_difftest.cpp` 顶部
  立为 **tag 规则三**：*每个已实现的重载都要有自己的 `rec()`*，Size 族的
  `setWidth`/`setHeight`/`rwidth`/`rheight` 也因此从合并的 rec 拆开。
  **Task 4–6 必须逐条对着头文件声明列「重载 vs `rec()`」对照表**（Rect 8 分量、
  Transform 18 分量，重载数是 Size 族的几倍，靠记忆必漏）。
  已知残留：Point 族（Task 2）的 `setX/setY`、`rx/ry`、`F::setX/setY`、`F::rx/ry`
  仍是两个重载挤一条 rec —— 不是覆盖漏洞（两个 mutator 在同一次往返里都被调到），
  只是归因粗，有意不动以保住「Point 族 total=35 569 662」这条基线。
- **实测唯一一条「单测全绿、只有对拍抓得到」的形态：`PkSize` → `PkSizeF`
  隐式提升丢精度。** 把 `PkSizeF(const PkSize &)` 的两个初值改成 `float(sz.width())`
  （`int` → `float` 只有 24 位有效位，`INT_MAX` 被舍成 `2147483648.f`；`int` → `double`
  则是精确的），实测：**`run_tests.sh` 33 个用例全绿、退出码 0**，`run_oracle.sh`
  抓到 **962 323** 处、56 条未声明 tag（`SF::fromSize` 358 555 + `SF::roundTrip`
  358 555 + `SF::mixedExpandedTo` 245 123 + `SF::mixedEquality` 90）。
  修复轮 1 已给 `sizefPromotionFromPkSize()` 补三条断言把最粗的一层堵上
  （`PkSizeF(PkSize(INT_MAX, INT_MAX-1))` 的两个分量 + 往返），**再注入同一个缺陷
  单测当场变红**；但断言只钉住 2 个取值，整条提升路径的取值面仍然只有对拍在压。
  ⚠ **不要把 `scaled` 的取整方向、`PkSizeF::expandedTo` 用 `qMin` 当成同类例子** ——
  把它们改坏之后**单测也会红**（`sizeScaledThreeModes`/
  `sizeScaledUsesInt64Intermediate`/`sizefExpandedTo`），所以它们不是
  「只有对拍抓得到」的形态。
- **`graft/` 试接只有 2 个目标 / 14 个测试函数**（`KisRectsGridTest` 3 +
  `KisFourPointInterpolatorTest` 11）。它们压到的 API 面远小于单测与对拍，
  而 stub 顶住的那些依赖等于**没被验证**。判据②要的是"不同 target、零改动"，
  这一条满足了；但**不要把它读成"真实调用点覆盖率"**。详见上面
  「三条证据链各自的盲区」。
- **Size 族的 `scaled`/`scale` 对拍走的是「手挑 × token 双向交叉」，不是全组合。**
  这四个 API 有 4 个分量 + 1 个 mode，`25⁴×25⁴` 做不了。做法：源取手挑对 × 目标取
  token 对，再反过来一遍，×3 个 mode —— **每个分量位上都取得到手挑值**（README
  上面那条纪律的要求），但"某个手挑源尺寸 × 某个手挑目标尺寸"这一格没覆盖。
  规模：int 25²×20²×2×3 = 1 500 000 组，double 44²×21²×2×3 = 5 122 656 组。
  注入自证第 3 组（`scaled` 改成四舍五入）在这个输入集上抓到 570 248 处差异、
  分布在 8 个根因 tag 上，说明这个交叉对"取整/分支"类缺陷是够用的；
  **对"只在两个手挑值同时出现时才触发"的缺陷则可能漏**。
- **`Q_ASSERT` 那条差异只在 Debug 构建里存在**（偏离清单第 8 条）：对拍以
  `-DQT_NO_DEBUG` 编 Qt 头，比的是 Krita 发布构建的形态。Debug 构建下真 Qt 的
  `QSize(1,1)/0.0` 会 `qt_assert` 中止，`PkSize` 不会 —— **这一片没有对拍覆盖**，
  也不打算覆盖（断言设施归 R-08）。
- **`qint64 → int` 的窄化是实现定义行为，不是 `-fwrapv` 管的那一类。**
  `PkSize::scaled` 的返回值里有一次窄化（实测 `(INT_MAX,2).scaled(2,INT_MAX,Expand)`
  两侧都得 `(INT_MIN,INT_MAX)`）。C++17 里它由实现定义（C++20 起才规定为二补数），
  GCC 在所有档位上都取二补数。**换编译器/平台要重测**，与下面那条 Android/ARM 一起。
- **`qHash` / `QDataStream` / `QDebug` 配 `QSize*` 的实测用量全是 0**（口径同上
  3 325 个文件）：`QHash<QSize`、`QSet<QSize`、`QMap<QSize`、`qHash(const QSize`、
  `QDataStream` 与 `QSize` 同行、`QVariant` 与 `QSize` 同行 —— **各 0 次**。
  所以 Size 族这三组不实现不欠 R-02/R-12 任何东西（`QRect` 那边欠 1 处，见上）。
  `qDebug() << <某个 size>` 形态的行有 16 处，归 R-08 日志线。

### RectF 族的结构性缺口

1. **`toRect` / `toAlignedRect` 在越界输入上两侧都是 UB，`-fwrapv` 管不着。**
   浮点→`int` 的越界转换（`int(std::floor(1e10))`、`qRound(1e10)`）是未定义行为的
   另一类，`-fwrapv` 只管整数溢出。我们靠的是"两侧在本机都编成同一条 `cvttsd2si`"，
   实测取值一致（对拍里这批输入贴 `out-of-int-range` 这个 tag，零真实差异）。
   **这不等于换一个编译器/架构也一致** —— 与 `-fwrapv` 那条同类，标签写实、不叫
   "defined"。更麻烦的一点：`toAlignedRect` 是 **out-of-line**（Qt 那份编在
   `libQt5Core.so` 里），它内部的 `xmax - xmin` 是有符号减法，两侧旗标不对等的
   风险与 Task 4 的 `operator|`/`operator&` 完全同型 —— 本轮 `run_oracle.sh` 不带
   `-fwrapv`（与 `.so` 同旗标），实测 `RF::toAlignedRect` 零真实差异。
2. **`PK_COMPARE` 对 `double` 走的是 `pk/test` 的模糊比较（相对 1e-12），不是位相等。**
   RectF 族几乎所有量都是 `double`，所以 `test_rectf.cpp` 里凡是要主张"位一致"
   （±0.0、NaN、次正规）的断言一律用 `PK_VERIFY` + 本地的 `sameD()`（`memcpy` 到
   `uint64_t` 比位）。这是 R-11 harness 的能力边界，跨线，R-03 内不修。
3. **注入实验 E 是一次"假注入"，记在这里当反例。**
   把 `operator&` 的第一处判空 `if (l1 == r1)` 改成 `if (l1 >= r1)`，单测与对拍
   **双双全绿（`exit=0`、`mismatch=3` 只剩 canary）**。复核后确认这**不是覆盖漏洞，
   而是一次语义等价改写**：Qt 的摊平步骤保证了 `l1 > r1` 不可达 —— `w >= 0`（含
   `+0.0`/`-0.0`/`NaN`）走 `r1 = xp + w`，IEEE 加法单调，`xp + w >= xp`；`w < 0` 走
   `l1 = xp + w <= xp = r1`；任一侧出 `NaN` 时两个比较都为假、分支一致。
   于是 `l1 >= r1` 与 `l1 == r1` 在全部可达状态上同真同假。**留这条是因为"改了源码
   却没变红"第一反应容易写成覆盖缺口** —— 先证明这个改写是不是等价，再下结论。
   真正的 NaN 短路注入（E2：把 `l1 >= r2 || l2 >= r1` 改写成德摩根形式
   `!(l1 < r2 && l2 < r1)`，只在 NaN 上分家）被抓到 **215 970 次差异 / 1 554 个未声明
   tag，全部落在 `nan-axis` 那一档**，单测也红了一条。
4. **`operator==` 的模糊比较在对拍里只比"两侧结论一致"，不比阈值本身。**
   `RF::operator==` 的 `rec()` 比的是 `(qa == qb) == (pa == pb)`，所以"两侧用了同一
   个错阈值"这种情况对拍看不见。堵这个洞的是 `tests/rectf_macro_proof.cpp`（预处理期
   宏改写探针）与 `test_rectf.cpp::rectfEqualityIsFuzzy` 的实测期望值，不是对拍。
5. **`qHash(QRectF)` / `QDataStream` 的 `<<`/`>>` / `QDebug` 的 `<<` 不实现**，
   与 Rect 族同一个理由（归 R-02 / R-12 / R-08）。本 Task 实测（口径同上：3 325 个
   文件，裸 `grep -ohE` 计行）：`QHash<QRectF` **0**、`QSet<QRectF` **0**、
   `qHash(...QRectF` **0**、同一行同时出现 `qDebug` 与 `QRectF` 的 **0**；
   对照 `QHash<QRect` **1**（那一处归 R-02 接哈希容器时处理）。
   **所以 RectF 这一侧目前没有任何调用点在等这三组**，比 Rect 那侧还干净。

### Transform 族的结构性缺口

1. **`type()` 的惰性缓存让"状态"进了对拍的输入空间，而输入空间只能抽样。**
   同一个矩阵、同一串操作，**只因为中间问过一次 `type()`**，答案就从 `TxNone`
   变成 `TxProject`、`isIdentity()` 从 true 变成 false、`map()` 走的分支也跟着变。
   对拍为此喂了**多步序列**（四个标量运算符各自的过期路径 × 问过/没问过两条分支、
   旋转往返、连续 mutator 链），但**序列空间是无穷的，喂的是手挑的那几条**。
   一元/二元 API 的全组合在这一族上不再是充分覆盖。
2. **`mapRect` 的透视裁剪支只证明了"我们与 Qt 在这里分家"，没有证明分家的量有界。**
   偏离 21 的 23 行额度是**当前输入集**下的数（分母合计 27 492）。换一组矩阵输入，
   分家次数会变 —— 那时闸门会 FAIL（额度漂移），**失败方向是对的**，但不要把
   25 495 这个数读成"偏离的规模上限"。
3. **`isAffine` 是判据①在这一族上的一个未闭合口子**，见上面 Transform 归属表下方
   那条警示。它不是覆盖缺口（对拍与单测都压到了它），是**范围缺口**。
4. **`squareToQuad` / `quadToSquare` 有用量但做不出来**（签名吃 `QPolygonF`），
   见上面「归属未定」一节。这两个的调用点在 S 线替换时会**编不过**，
   不是"跑起来行为不对"——失败方向是响的。
5. **注入实验前必须先论证它非空操作。** 本族真踩过：一次注入被设计成
   「把 `+=`/`-=` 的档位钉死改成条件抬升到 `TxProject`」，而 **`TxProject = 0x10`
   是最高档、抬无可抬**，那是**编译期可证的空操作** —— 照做必然"抓不到"，
   会让人从一个不存在的 bug 推出"对拍失效"的错误结论。
   配套的一条：**「mismatch 一点没动」的第一解释永远是「注入没生效」**，
   不是「代码没问题」。

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
   Task 2 实现的就是它）、Krita 自备的 `KisAlgebra2D::dotProduct` **6 处**、
   `QVector2D::dotProduct` **3 处**、`QVector3D::dotProduct` **3 处**
   （**Task 3 复核修正**：这两个数原来写成 5 与 4，方向相反，总数 13 与文件数 7 侥幸还对）
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

4. **⚠ 实施计划把 `transpose` / `transposed` 列进 Size 族「有用量、必须实现」——
   实测证伪，本 Task 不实现它们。** 归属表见上：`transposed` 的 5 处全是
   `QTransform`，`transpose` 的 5 处全是 Eigen 矩阵，Size 族各 **0 处**。
   与 Point 族那条是同一个根因（按名字数、不区分接收者类型）。

5. **⚠ `QSize::scaled` 的真实调用点是 2 处，不是 1 处。** Task 2 复评给出的清单说
   `QSize::scaled` 只有 `plugins/impex/libkra/kra_converter.cpp:297` 一处；实测还有
   **`libs/image/kis_paint_device.cc:1822`**：
   `const QSize thumbnailSize = deviceExtent.size().scaled(maxw, maxh, aspectRatioMode);`
   —— `deviceExtent` 是 `QRect`，`.size()` 返回 `QSize`，走的是 `QSize::scaled(int,int,mode)`
   这个**另一个重载**。按接收者归属时"链式调用的返回值类型"这一类最容易漏，
   Task 4–6 注意（`.size()` / `.rect()` / `.boundingRect()` 这类链）。

6. **⚠ `QSize()` / `QSizeF()` 的默认值是 `(-1,-1)`，实施计划与简报都没提。**
   计划的 Task 3 清单里只点了 `isNull`/`isEmpty`/`isValid` 三条公式，没提默认构造
   不是零值。这不是文档错误（它没写反），但它是本 Task 最容易照直觉写错的一条，
   记在这里给 Task 4/5 提个醒：**`QRect()` 是 `(0,0,-1,-1)`，也不是全零**，
   到时候一样要先问探针。

7. **⚠ 实施计划把 `intersect` / `unite` 列进「Rect 族有用量、必须实现」，还专门
   写了「`intersect`/`unite` 是 Qt5 已 deprecated 的原地版，实测各 1–2 处调用点。
   **用量 > 0 就实现**（判据①是实测用量，不是「Qt 推荐用法」）」——实测证伪，
   本 Task 不实现这两个。** 口径同上（3 325 个文件，三形态）：
   - `intersect` 合计 **1 处**：`libs/image/kis_layer_utils.cpp:2567`
     `paintDevice->keyframeChannel()->allKeyframeTimes().intersect(times)` ——
     `allKeyframeTimes()` 返回的是 `QSet<int>`，这是 **`QSet::intersect`**。
   - `unite` 合计 **2 处**：`libs/image/kis_layer_utils.cpp:1384`
     `frames.unite(rasterChan->allKeyframeTimes())` 是 **`QSet::unite`**；
     `libs/image/kis_transform_worker.cc:214` 的 `dstBounds.unite(dstPos)`，
     `dstBounds` 的声明在同文件 `:207` 是
     **`KisFilterWeightsApplicator::LinePos`**（Krita 自备的类型，它自己有个
     `unite` 成员），也不是 `QRect`。
   **Rect 族真实调用点 0**，按判据①「一项不多」不实现。
   与第 2 条（`dotProduct`：计划说 0、实际非 0）**方向相反**，两条合起来说明
   原始测量那条「按名字数、不看接收者」的口径在**两个方向上都会错**：
   会漏（静态调用、`.cc` 文件），也会多（同名成员属于别的类）。
   > 因此「按族取并集」这条规则的正确用法是：**并集只用来决定「要不要查」，
   > 不能直接当成「要不要实现」** —— 每个名字都得再归属一次接收者类型。

8. **实施计划 Rect 族那一步要求「注入三组假 bug」，实际做了五组。**
   多出来的两组不是加戏：一组（D，`contains` 的差一边界）是计划正文点名过的
   「已知会咬人的五个地方」之一而那三组没覆盖到；另一组（E）是
   **规则三机器闸门的自证** —— "闸门装了但没验证它会响"本身就是这次要根治的
   那类问题。

9. **⚠ `Qt替代品选型.md` §1 几何那一行只点名四个类型，实际交付七个，
   另有九个几何相关类型无归属。** 文档点名的是 `QRect` / `QPointF` / `QSize` /
   `QTransform`；按「族并集」把孪生一起做之后是七个（多出 `QPoint` / `QSizeF` /
   `QRectF`）—— 这一步**不是扩范围**，是同一概念的整数/浮点两半，调用点在两者间
   自由转换，只做一半会让另一半的调用点编不过。**真正的缺口是另外九个类型
   （`QPainterPath` `QLineF` `QPolygonF` `QPolygon` `QVector2D/3D/4D` `QRegion`
   `QMatrix4x4` `QMargins` `QLine`）—— 它们不在任何一条线的交付面里。**
   逐个实测数字见上面「归属未定」一节。**这一条需要人来分派，R-03 无权自己认领。**

10. **⚠ `pk/test/README.md` §3 说「`KisGlobalTest.cpp` 依赖 `kis_global.h` →
    `QRect`/`QPoint`，落在 R-03 范围」——与代码事实不符。** 本 Task 现场核实
    （口径：`libs/global/tests/KisGlobalTest.{h,cpp}` 两个文件全文，
    `grep -ohE "\bQ(Rect|RectF|Point|PointF|Size|SizeF|Transform)\b"`）：
    - **几何类型匹配数 = 0**；
    - **不 `#include` `kis_global.h`**（它包的是 `KisGlobalTest.h` / `simpletest.h` /
      `KisFileUtils.h` / `<QTest>`）；
    - 它测的是 `KritaUtils::deduplicateFileName`，阻塞项是 `QFileInfo` / `QDir` /
      `QRegularExpression` 加一个 `operator<<(const char*)` 重载 —— **与 R-03 无关**。

    **因此 R 线 spec 里「R-03 落地后补 `pk/test` 数据驱动族的真实调用点试接」
    这条前提落空**：R-03 的两个试接目标（`KisRectsGridTest`、
    `KisFourPointInterpolatorTest`）**都不用 `addColumn`**。
    （`pk/test/README.md` 不在 R-03 的 `locks` 内，**只报告，没有去改它**。）

11. **⚠ 实施计划在 `squareToQuad` / `quadToSquare` 上自相矛盾**（一处「必须实现」、
    一处把 `QPolygonF` 划在范围外，而这两个函数的签名吃 `QPolygonF`），
    且把「2 次 / 3 次」写反了。完整实测与处置见上面「归属未定」一节。
