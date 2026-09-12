# R-64 probes — 每支回答什么问题、期望读数、怎么跑

本目录是 R-64（`addEllipse` 放大尺度 4 px 差分的真根因）的**可复现证据**。  
R-58 的纪律条（`pk/geometry/README.md` 记的）要求「任何『登记』必须附探针原始输出」——
原始输出要能被下一个人重跑出来，否则它跟「凭印象」没有区别。这六支探针与
`run_probes.sh` 就是为此落盘的：**一条命令重跑出计划里引用的全部读数**。

结论的落点（真根因、判据、可接受偏离的登记）在 `pk/render/README.md` 的
**R-64** 一节；这里只负责「怎么把证据再跑一遍」。

## 怎么跑

`run_probes.sh` 只依赖**一个已经配置好的 `pk/render` 构建目录**（它现场从这个目录里
已经在的 `svg_primitive_oracle_pk` 目标取编译/链接参数，不自己 configure、
不写死任何平台路径）：

```bash
source <工作空间根>/krita-ci-env/env
PK_RENDER_BUILD_DIR=<已配置的 pk/render 构建目录> \
    bash pk/render/oracle/probes/run_probes.sh
```

- 不设 `PK_RENDER_BUILD_DIR` 时默认 `${render_root}/build`（与
  `pk/render/oracle/run_svg_primitive.sh` 同值）；该目录没配置过会直接报错退出。
- 退出码：**任一步骤失败即非 0**；五条读数都打印出来、最后一行是
  `probes: all five readings printed above (exit 0)`。
- 脚本按 macOS / Linux 两条分支写（`find_env_script()` 向上找 `krita-ci-env/env`；Qt 侧用
  `pkg-config`；Qt 链接自检 macOS 用 `otool -L`、Linux 用 `ldd`），**但本任务只在
  macOS arm64 上实跑过（2026-09-12）；Linux 分支未被执行过**——「能在两平台跑」未观测。

## 输入集合与口径（措辞不超出探针喂过的输入）

所有对拍都跑在 `pk/render/oracle/svg_primitive_cases.h` 的 `table()` 上——**共 288 例**
（`total=288`）。渲染尺寸统一 **32×32 ARGB32**。文档的两种根属性形态：

- **bounds** — `PkSvgPainterBackend(canvasRect())`（= oracle 现用形态，有
  width/height/viewBox）；
- **default** — `PkSvgPainterBackend()`（= `libs/flake/svg/SvgWriter.cpp` 真实调用点形态，
  **无** width/height/viewBox）。

Qt 侧参照由 `QSvgGenerator` 产出、`QSvgRenderer` 渲染。

## 六支探针

| 文件 | 侧 | 回答什么问题 |
|---|---|---|
| `pkdocs.cpp` | Pk（Qt-free） | 逐用例产出 bounds / default 两份 Pk 文档，喂给 Qt 侧探针当比较对象 |
| `probe.cpp` | Qt | 三种比较面各差几例：bounds-vs-QtViewBox、default-vs-QtNoViewBox、default-vs-QtViewBox |
| `isolate.cpp` | Qt | 把「6 位有效数字舍入」与「`<ellipse>` vs `<path>` 渲染路径」两个因素分开 |
| `control.cpp` | Qt | Qt **打** Qt：`drawEllipse` vs `drawPath` 的文档在同一隐式拉伸下是否也差 4 px |
| `mirror.cpp` | Qt | 让 Qt 走**与 Pk 同形的路径式命令流**，与 Pk default 文档逐像素比（全表） |
| `focus.cpp` | Qt | 单例聚焦：把某一例两侧的隐式 viewBox 以 17 位有效数字打印 + 逐像素差 |

## 期望读数（逐字）

> **来源标注**：下面每条读数的**首次产出**来自 `.superpowers/sdd/R-64/probe-raw/`
> （上一轮摸排的六支探针，原始 stdout 逐字落在那一目录的 `probe.out` / `isolate.out` /
> `control.out` / `mirror.out` / `focus19.out`）。本目录的 `run_probes.sh` **于
> 2026-09-12 在 macOS arm64（本机）上实跑一次**（`exit=0`），**逐字复现了**下面全部五条
> 读数。每条的「来自哪支探针」在标题里点名。

### ① `probe.cpp` — 三种比较面（SUMMARY 行）

```
SUMMARY total=288 mism[bounds-vs-qtVB]=0 mism[default-vs-qtNoVB]=2 mism[default-vs-qtVB]=161
```

- `bounds-vs-qtVB=0`：oracle 现用形态两侧**全表相同**。
- `default-vs-qtNoVB=2`：**调用点形态**下只差 **2 例**——正是
  `svg-ellipse#19:pen2.5:nofill` 与 `svg-ellipse#20:pen2.5:fill`（极扁 rect
  `{2,2,1,28}`），每例 4 px。
- `default-vs-qtVB=161`：交叉比较面（一边无 viewBox、一边有），差异来自比较面本身
  不一致，不是缺陷；列在这儿只为说明**比较面必须成对**。

### ② `isolate.cpp` — 精度隔离

```
diff ellipse-vs-path6  = 4
diff ellipse-vs-path17 = 0
diff path6-vs-path17   = 4
```

同一份 `<path>`，只改数字精度（6 位 vs 17 位），比较面固定为 `QSvgRenderer` 32×32：
全精度路径与 `<ellipse>` **diff=0** ⇒ 4 px **只**来自 6 位数字，不是 `<ellipse>` 那条
近似路径。

### ③ `control.cpp` — Qt 打 Qt（判别力对照）

```
diff Qt(drawEllipse) vs Qt(drawPath) under implicit stretch = 4
```

**Qt 自己的** `QSvgGenerator` 对同一条椭圆分别走 `drawEllipse`（→ `<ellipse>`，全精度）
与 `drawPath`（→ 6 位 `<path>`），默认 generator 设置，两份文档都由 Qt 的
`QSvgRenderer` 渲染。**同样差 4 px** ⇒ 4 px 不是 Pk 的偏离，而是「6 位数字的 SVG 路径
序列化 + 无 viewBox 时的隐式内容 bbox 拉伸」的固有限制。

### ④ `mirror.cpp` — 全表镜像对照

```
MIRROR total=288 mismatch=0
```

Qt 走**与 Pk 后端同形的路径式命令流**（椭圆 `addEllipse`+`drawPath`、多边形
`addPolygon`+`closeSubpath`+`drawPath`、弧 `arcMoveTo`+`arcTo`+`drawPath`），默认
generator 设置，与 **Pk default 文档**逐像素比：**全表 288 例逐像素相同** ⇒
Pk 只有一个统一的 path 发射器，且与 Qt 的 `drawPath` 逐位对齐。

### ⑤ `focus.cpp` — svg-ellipse#19:pen2.5:nofill — 单例聚焦

```
qt viewBox: x=0.75 y=0.75 w=3.5 h=30.5  aspectRatio=0
pk viewBox: x=0.75 y=0.75 w=3.5 h=30.5  aspectRatio=0
delta viewBox: dx=0 dy=0 dw=0 dh=0
scale: qt=9.1428571428571423x1.0491803278688525  pk=9.1428571428571423x1.0491803278688525
  px(3,1) qt=51000000 pk=4f000000 dA=-2
  px(28,1) qt=51000000 pk=4f000000 dA=-2
  px(3,30) qt=51000000 pk=4f000000 dA=-2
  px(28,30) qt=51000000 pk=4f000000 dA=-2
diff pixels = 4
```

两侧的隐式 viewBox **逐位相同**（`delta viewBox` 全 0），拉伸倍数**逐位相同**
（x `9.142857…`、y `1.049180…`）⇒ 排除「两侧内容 bbox 不同导致拉伸倍数不同」。
差异只有 4 个边缘像素，每处 alpha 差 2/255（`dA=-2`）。这正是那 4 px。

## 与 `run_svg_primitive.sh` 的关系

本目录**独立**于 `pk/render/oracle/run_svg_primitive.sh`：

- `run_svg_primitive.sh` 是 **R-54 的判据④证据链**（比较面 = bounds ctor Pk 文档 vs
  Qt viewBox 文档，契约是 `DIFF total=` 记的 mismatch）。那条**不是** 4 px 复现面
  （那里 `bounds-vs-qtVB=0` 恒绿）。
- 本目录复现的是**调用点形态**（default ctor，无 viewBox）下的 4 px。R-64 **不改**
  `run_svg_primitive.sh` 的比较面/golden/契约。

## 落盘来源

这六支探针的原始源码即本次摸排用过的探针，落在
`.superpowers/sdd/R-64/probe-raw/`（连同各自的原始 stdout：`probe.out` / `isolate.out` /
`control.out` / `mirror.out` / `focus19.out`），这里保留其**实质逻辑**，只把路径改为相对
本目录、命令收进 `run_probes.sh`、文件头补了落点说明。
