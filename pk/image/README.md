# pk/image —— `QImage` 内存值类型的零 Qt 替代品

`PkImage`（COW 像素 buffer + 格式描述 + 逐像素读写 + 格式转换/派生操作），
外加 `compat/QImage` 垫片让真实调用点 `#include <QImage>` 一个字不改就解析过来。
范围只做 QImage 的 **(a) 内存像素 buffer 能力**，**不做 (b) 文件编解码**（PNG/GIF/
JPEG 等经 `QImageIOHandler` 插件体系的能力，是「换库」不是「值类型」，岔路 A 排除）。

`libpkimage.a` **不链接、不引用任何 Qt 符号**（`nm -u -C` 自证，见 §5）。

---

## 0. 口径：所有用量数字都是「保留范围上界」，且**方法级用量表才是实现范围的依据**

**这条比表里任何一个数字都重要。** 本目录实现范围由 `docs/superpowers/plans/R-15.md`
的**方法级用量表**决定（文件数只用来定位目标文件）。那张表的口径是：

- SRC = `git ls-files '*.cpp' '*.h' '*.cc'`，排除路径含 `tests/` 或 `benchmarks/`，
  再排除 `pk/`（自己的树，算进去会自我污染）。
- 只数**保留范围内**文件（两步收窄：① 前缀白名单 ② 接收者类型核实——用变量名启发式
  排掉 `QSize`/`QRect`/`std::vector` 等同名方法的误伤）。
- 次数是 `(\.|->)名字\s*\(` 的**出现次数**，不是精确调用点清单——注释、字符串字面量、
  同名非 QImage API 全部计入，标的是**上界**。

文件数口径（R-15 plan 现场重数）：**178 文件 / 629 处**（词边界），去掉不在保留范围
白名单的 `libs/canvas` 后 **168 / 629**。任务行记的「159/616」是更早时点口径，差值
见 plan「与任务行口径的差异」一节——**这不是决策文档错误**（决策文档只给了「值类型
173 文件」这个更早的量级），如实记录供主会话一并确认。

---

## 1. API 范围表

**格式枚举**：`PkImage::Format` **29 个值 1:1 对齐 Qt 序数**（含零引用的
`Format_BGR888`）。枚举常量声明成本趋近于零，声明不全会让 `switch(format)` 覆盖
全部 Qt 枚举值的调用点在替换时编不过——签名/常量表完整性与「方法该不该做」是两件事。

**像素级读写的高/低频边界**（最重要的实现边界）：

| 边界 | 格式 | 依据 |
|---|---|---|
| **9 个高频格式，`pixel()`/`setPixel()`/`scanLine()` 等像素级读写完整实现** | `ARGB32`(52) `Indexed8`(12) `RGB32`(11) `Grayscale8`(8) `ARGB32_Premultiplied`(5) `Mono`(4) `RGBA8888`(3) `RGBA64`(2) `MonoLSB` | plan 用量表：有真实「构造 + 像素访问」组合 |
| **其余 20 个低频格式，只保证 `depth()`/`bytesPerLine()`/`sizeInBytes()` 正确**（构造 + 尺寸查询），像素级读写给占位值 0 + debug `assert` | `RGB16` `ARGB8565_Premultiplied` `RGB666` `ARGB6666_Premultiplied` `RGB555` `ARGB8555_Premultiplied` `RGB888` `RGB444` `ARGB4444_Premultiplied` `RGBX8888` `RGBA8888_Premultiplied` `BGR30` `A2BGR30_Premultiplied` `RGB30` `A2RGB30_Premultiplied` `Alpha8` `RGBX64` `RGBA64_Premultiplied` `Grayscale16` `BGR888` | 判据①：像素级访问在这些格式上零真实调用点（偏离②） |

### 1.1 Task 1 —— 构造与查询 API

| API | 用途 |
|---|---|
| `PkImage()` / `PkImage(int w, int h, Format)` / `PkImage(const PkSize&, Format)` | 默认构造走 `PkArrayData` 共享空哨兵（`isNull()==true`）；负宽高/`Format_Invalid` 保持 null |
| 拷贝 / 移动构造与赋值 | COW 共享（`PkArrayData` 天然给） |
| `format()` `width()` `height()` `size()` `rect()` | 查询；`rect()` = `PkRect(0,0,w,h)` |
| `isNull()` | `w<=0 \|\| h<=0 \|\| format==Format_Invalid` |
| `depth()` | 29 项 depth 表，逐条来自探针实测（非文档推断） |
| `bytesPerLine()` | `((width*depth+31)/32)*4`，标准 BMP 式 32bit 对齐填充 |
| `sizeInBytes()` | `bytesPerLine()*height()` |
| `colorCount()` | Mono/MonoLSB 恒 2；Indexed8 是颜色表 size；其余 0 |

### 1.2 Task 2 —— 像素访问与写入 API

| API | 语义要点 |
|---|---|
| `scanLine(y)`（非 const，**detach**）/ `scanLine(y) const`（转发 `constScanLine`，**绝不 detach**）/ `constScanLine(y)`（**绝不 detach**） | 行指针；越界行返回 `nullptr`（确定性，不构造越界指针） |
| `bits()` / `bits() const`（转发 `constBits`，**绝不 detach**）/ `constBits()` | 同 detach 时机 |
| `pixel(x,y)` / `setPixel(x,y,v)` | 直接色格式 v 是 0xAARRGGBB 打包；Indexed8/Mono/MonoLSB 的 v 是颜色表**索引** |
| `pixelColor(x,y)` / `setPixelColor(x,y,v)` | 复用 pixel/setPixel 的 uint32 打包（无 PkColor，简化决策）；setPixelColor 在索引格式 no-op |
| `pixelIndex(x,y)` | indexed 原始索引 |
| `fill(uint32_t)` | 按 pixel/setPixel 同一套打包/索引约定逐像素写 |
| `fill(Qt::GlobalColor)` | 只保证 white/black/red/gray/transparent 5 个真实调用点值精确（`gray` 实测是 `(160,160,164)` 不是 `(128,128,128)`） |
| `colorTable()` / `setColorTable()` / `setColorCount()` / `color(i)` / `setColor(i,v)` | 仅 indexed 格式有意义；非 indexed `colorTable()` 恒空 |
| `allGray()` | 逐像素 R==G==B（alpha 无关）；Grayscale8/16 恒 true、Alpha8 返 false |

### 1.3 Task 3 —— 格式转换与派生操作

| API | 语义要点 |
|---|---|
| `copy()` | 无条件深拷贝（探针：即使无共享者也强制新分配） |
| `convertToFormat(Format)` | 同格式**共享**（探针 same-ptr=1）；不同格式逐像素转换 |
| `convertToFormat(Format, const std::vector<uint32_t>& colorTable)` | 调用方指定调色板重载（`kis_svg_brush.cpp:53` 真实调用点需要）；最近色匹配，非 Qt dithering 位对齐 |
| `convertTo(Format)` | 原地版本，语义 `*this = convertToFormat(f)` |
| `scaled(size, aspectMode, mode)` / `transformed(matrix, mode)` | Fast 模式良定义最近邻（与 Qt 结构偏离，见 §2①）；Smooth 双线性自定义 |
| `devicePixelRatio()` / `setDevicePixelRatio()` | 默认 1.0，变换后原样透传 |
| `operator==` / `operator!=` | 深度像素内容比较（不比较 devicePixelRatio，对齐 Qt） |

### 1.4 只给单测用、不进 compat 垫片

`PkUseCount()` / `PkIsSharedWith()` —— 真 Qt 的 `isDetached()/isSharedWith()` 在
Krita 调用点实测 0 处，这两个 Pk 前缀观测器只服务 detach 时机回归单测
（先例：`pk/container/PkArrayContainer.h`）。

---

## 2. 偏离清单（**最重要的一节**）

对齐口径：替代品与 Qt 的任何行为差异默认都是缺陷（R线-spec「对齐口径」）。以下 6 条
是**逐条写明理由、经 reviewer 放行**的已声明偏离。每条标三类之一：
**结构偏离**（Qt 走 QPainter/定点，移植不在 R 线范围）· **判据①范围裁剪**（该路径
零真实调用点，一项不多）· **Qt 自身 bug 的规避**（Qt 自身行为不自洽/是 UB，PkImage
选更安全自洽的一侧）。逐条的 DIFFTAG 与期望计数在 `oracle/image.deviation`（SOT）。

| # | 偏离 | 类别 | 理由 |
|---|---|---|---|
| ① | **Fast/Smooth 变换模式逐位不对齐** | **结构偏离** | plan 岔路 B 原本只裁 Smooth；Task 4 探针 + Qt 5.15 源码（qimage.cpp:4788-4916）证实 **Fast 模式同样**：`format>=RGB32` 时走 QPainter drawImage、`format<RGB32` 时走定点 12.4 的 `qt_xForm_helper`（`*4096`/`>>12`）——都不是 plan 声称的「纯确定性整数坐标映射」。PkImage 的 `floor(inv.map(pixel-center))` 是良定义正确最近邻，只在部分比例上与 Qt 定点碰巧重合。移植 QPainter/定点引擎不在 R 线范围，与 Smooth 完全同构 |
| ② | **低频 packed-bit 格式像素级读写未实现** | **判据①范围裁剪** | `RGB666/RGB555/RGB444/ARGB6666_Premultiplied` 等 20 个低频格式零真实「构造 + 像素访问」组合；`depth()/bytesPerLine()/sizeInBytes()` 已正确，像素级读写给占位 0 + debug assert |
| ③ | **`convertToFormat(Format)` 到索引目标格式零初始化** | **判据①范围裁剪** | 真 Qt 会做真正的调色板量化；用量表没有覆盖「转换到索引格式」这个方向。PkImage 保持构造时零初始化（确定性、不产出错误数据），不是 bug——`rawPixelArgb` 读回 ARGB32 而 `writeRawPixelArgb` 对索引格式把 value 当索引，两者直拼会真写错 |
| ④ | **`fill(uint32_t)` 在 RGBA8888/Grayscale8 与 Qt 自身对不齐** | **Qt 自身 bug 的规避** | Qt 的 `fill(uint)` 在这两个格式走裸 memfill、绕过格式的字节序/灰度换算，与 Qt 自己的 `setPixel()` **不自洽**。PkImage 选与 `setPixel()` 同一套语义（更自洽），对 Qt 对不齐。其余 6 个高频格式 Qt 自身 fill==setPixel，实测 SAME |
| ⑤ | **`setColor` grow-beyond-256：PkImage 无条件 resize** | **判据①范围裁剪** | Qt 拒绝把 Indexed8 颜色表长过 256（8bit 索引上限）并 qWarning；PkImage 无条件 `resize(idx+1)`。grow-beyond-256 这条路径零真实调用点，且超出的表项经 8bit 索引本就不可达，PkImage 选简单确定性行为 |
| ⑥ | **越界坐标读 PkImage 定死返 0 vs Qt 未初始化内存 UB** | **Qt 自身 bug 的规避** | 真 Qt 的 `pixel(-1,-1)` 读未初始化内存（探针实测返回 `0x00003039`、`pixelIndex` 返 `-12345`，纯垃圾值连重跑都不确定）；PkImage 定死返 0（透明黑）——没有可对齐语义，PkImage 更安全。oracle 只比越界**写**的 no-op 语义，不比越界读 |

---

## 3. 依赖说明

`pk/image/CMakeLists.txt`（独立薄壳工程，同 `pk/geometry`/`pk/container` 模式）：

| 依赖 | 提供 | 状态 |
|---|---|---|
| `pk/container`（`PkArrayData<C>` 地基） | COW 共享/detach | R-02 已 VERIFIED（只用 include 路径，不 `add_subdirectory`，避免 pktest 目标名重复） |
| `pk/geometry`（`PkSize`/`PkRect`/`PkPoint`/`PkPointF`/`PkTransform` + `namespace Qt { AspectRatioMode; Axis; GlobalColor; TransformationMode; }`） | 几何类型与签名枚举 | R-03 已 VERIFIED（`add_subdirectory` 引入） |

> `docs/TASKS.md` R-15 行的 Deps 列只记 `P-04 spec:R R-11`，漏了 `R-01/R-02/R-03`
> 这两个真实依赖（`pk/container`/`pk/geometry`）——两者都已 VERIFIED、在本 worktree
> 可见可用，不构成调度阻塞，但**最终报告已提醒主会话把 R-02/R-03 补进 R-15 的 Deps 列**。

`compat/QImage` 垫片：`#define QImage PkImage`（宏不是 `using`——Krita 里有
`class QImage;` 前置声明，宏把那一行改写成 `class PkImage;`，前置声明照样成立）。
垫片还 `#include "QRect"`（经 `../PkRect.h` 一并带进 PkSize/PkPoint/PkPointF/
PkSizeF/PkRect/PkRectF），对应真 Qt 的 `<QImage>` 传递 include 关系。

---

## 3.1 R-75：文件 I/O 的落地位置（`pkimage` / `pkimageio` 的分工）

R-75 给 `PkImage` 加了按路径构造 / `load` / `save`，以及内存操作 `invertPixels`。
**它们刻意落在两个库里**，这是本节要记的已知事实：

| 符号 | 声明在 | **定义在** | 编进 |
|---|---|---|---|
| `explicit PkImage(const PkString &, const char *)`（`explicit` 逐字对齐真 Qt `qimage.h:152`） | `PkImage.h` | `PkImageFileIo.cpp` | **`pkimageio`** |
| `bool load(const PkString &, const char *)` | `PkImage.h` | `PkImageFileIo.cpp` | **`pkimageio`** |
| `bool save(const PkString &, const char *, int) const` | `PkImage.h` | `PkImageFileIo.cpp` | **`pkimageio`** |
| `void invertPixels(InvertMode)` | `PkImage.h` | `PkImage.cpp` | `pkimage` |
| `PkPngWriter`（PNG 编码，libpng 写侧） | `PkPngWriter.h` | `PkPngWriter.cpp` | `pkimageio` |

**为什么不在 `pkimage` 里**：`pkimage` 是**零 Qt、零编解码**的静态库（§0 的口径：
范围只做内存像素 buffer），PNG/JPEG/TIFF/GIF/WebP 的编解码住在 `pkimageio`。
R-15 把编解码从 `pkimage` 剥出去是已 VERIFIED 的边界，R-75 不回退。

**由此产生的一个已知事实（不是缺陷，是设计）**：

> `libpkimage.a` 里**根本没有** `PkImage(PkString,…)` / `load` / `save` 这三个
> 符号——它们既不是定义、也不是「被引用但没定义」，而是**整个不在那个归档里**
> （R-75 实测：`nm -g -C pk/image/build/libpkimage.a | grep -E 'PkImage::(load|save)\('`
> → 0 命中）。定义只存在于 `libpkimageio`，且是**导出**符号（`nm -gU -C … | grep`
> 三条都是 `T`）。**任何调用它们的消费方必须在链接行上带 `pkimageio`**，只链
> `pkimage.a` 会拿到 `Undefined symbols: PkImage::load(...)`。
> 不碰文件 I/O 的消费方完全不受影响。

单测 `test_pkimage` 正是这么链的（`pkimageio` + `pktest`，`pkimage` 由 `pkimageio`
的 `PUBLIC` 链接传递而来，见 `pk/image/CMakeLists.txt`）——它同时也是「这一层
链接真的成立」的回归闸门：符号在归档里不存在，链不到就是链不到。

**导出标记落在定义处**：`PkImage` 类本身住在 `pkimage`（静态库，编译期用默认
visibility），类上带不了导出宏——它不知道上层有没有 dylib。所以
`PkImageFileIo.cpp` 里这三个定义各自带 `PKIMAGEIO_EXPORT`（`pkimageio` 设了
`CXX_VISIBILITY_PRESET hidden`，不带就链不上）。**加/删这三个定义时要一起改**。

### 3.1.1 `pkimage` 现在也依赖 `pkstring`

`PkImage.h` 起用 `PkString` 做文件路径（`PkImage(PkString)`/`load`/`save`），
`PkString` 因此进入 `pkimage` 的**公共** API 面，所以：

* `pk/image/CMakeLists.txt`：`target_link_libraries(pkimage PUBLIC pkgeometry pkstring)`
* `pk/CMakeLists.txt`（主树聚合）：同样加 `pkstring`（R-65 教训：主树与薄壳两份
  依赖声明必须一致，否则薄壳绿、主树红或反过来）。
* **不需要**在 `pk/image/CMakeLists.txt` 里 `add_subdirectory(../string …)`：
  `pk/geometry` → `pk/test` 已经用它注册过 `pkstring`（`pk/test/CMakeLists.txt:24`
  用 `CMAKE_BINARY_DIR/pkstring`），重复 add 会撞
  「binary directory already used to build a source directory」。

### 3.1.2 `save` 的格式支持面（不假装成功）

`PkImage::save` **只**支持 PNG（`PkPngWriter` 是本次唯一落地的编码器）。
其余格式令牌、路径后缀不认识、没有后缀——一律返回 `false`，**不做「假装成功」
的桩**。语义对齐真 Qt 的探针实测：未知格式令牌 / 无后缀 / 坏目录都返回 false
（`oracle/image.deviation` 第 9-11 条与 `probe_*.cpp` 证据在 task-1-report.md）。

`PkPngWriter` 支持的**源格式**是 `rawPixelArgb` 能读的那一组：`ARGB32` / `RGB32`
/ `ARGB32_Premultiplied` / `RGBA8888` / `Grayscale8` / `Indexed8` / `Mono` /
`MonoLSB`。其余（`RGBX8888` / `RGBA8888_Premultiplied` / `RGBA64`）会返回空
字节 → `save` 返回 `false`，**写明而不静默写错**。输出恒为 8 位直通 alpha 的
PNG：全不透明时写 color type 2（RGB），否则 6（RGBA）。

---

## 4. 试接说明

**两个候选都走 driver 降级**（真实文件物理编不过，spec「试接怎么做」的降级路径，
R线-spec.md:173-196 四条逐条满足）：

| 候选 | 复刻的调用点 | 挡住的真实原因 | 依赖墙归谁 |
|---|---|---|---|
| A `compareQImagesImpl`（`sdk/tests/qimage_test_util.h:85-165`） | 逐行复刻，只替换 QImage→PkImage、QPoint→PkPoint、QRgb→shim | 整个文件体在 `#ifdef FILES_OUTPUT_DIR` 里：不定义是空 TU（假通过），定义则撞 `QString/QFile/QDir/QFileInfo/QApplication` | QString→R-13，文件 IO→R-14/S 线 |
| B `QImagePolygonOp::fastCopyArea`（`libs/image/kis_grid_interpolation_tools.h:355-418`） | 逐行复刻构造 + `fastCopyArea` 两个重载（一个 token 没动） | include 闭包含 kis_algebra_2d.h/kis_painter.h/KisRegion.h 等 kritaimage/kis-global 头；测试文件还用了 `QImage(fileName)`（PNG 解码） | kritaimage 整库→S-06，PNG 解码→impex 各自 S 批次 |

四条降级判据的满足方式（逐条）：

1. **逐行复刻代码形状**——两个 driver 的源都是照抄真实函数（`driver_compare_qimages.cpp`
   与 `driver_fast_copy_area.cpp` 头注释逐条列出「替换/省略」，其余一个 token 不动）。
2. **校验值来自真 Qt 探针**——`/tmp/graft_probe_bin`（真 Qt 5.15，`ldd` 确认链上
   libQt5Core/libQt5Gui）现场跑出全部期望值，探针命令与原始输出见 `task-5-report.md`。
3. **显式标注是替代品**——两个 driver 头注释第一句就是「这不是 `<原始文件路径>`，是
   复刻调用点形状的 driver，因为 `<挡住的真实原因>`」。
4. **指名依赖墙、为什么在 locks 之外**——见上表「依赖墙归谁」列，墙拆掉后理论上可补
   一次真编译，但不强制回补（driver 这层证据已接受为完成判据）。

试接中**新发现一个真实 API 形状缺口**（登记为待认领缺口⑦，见 §6）：真 Qt 的 QImage 有
`const uchar *scanLine(int) const` 这个 const 重载（qimage.h:226），`compareQImagesImpl`
拿 `const QImage&` 调 `.scanLine(y)` 解析到的正是它；PkImage 只有非 const `scanLine`
与 `constScanLine`，**没有** const `scanLine` 重载。driver 用 `constScanLine` 拼写
（语义零差，Qt 里两者逐字节是同一个函数），但 20 个共用 compareQImagesImpl 的测试
文件将来走「真实文件零改动」试接会卡在这一格。

**跑法**：`pk/image/tests/graft/run_graft.sh`（真编译 + 真运行 + 每个二进制
`nm -u | grep -i qt` 零 Qt 自证）。两个 driver 现均跑绿。

---

## 5. 怎么跑

```bash
# 从 fork 仓库根执行。

# ① 构建（库 + 单测可执行文件）
# macOS 要带 -DCMAKE_OSX_DEPLOYMENT_TARGET=13.3（与主树构建一致）。
cmake -S pk/image -B pk/image/build -G Ninja -DCMAKE_OSX_DEPLOYMENT_TARGET=13.3
cmake --build pk/image/build

# ② 单测：test_pkimage（1 个套件；测试方法清单见 tests/image_case.h，数字会漂移不写死）
#    注意：R-75 起 test_pkimage 同时链 pkimageio —— 文件 I/O 用例测的是定义在
#    pkimageio 里的 PkImage(PkString)/load/save（见 §3.1），只链 pkimage 会
#    "Undefined symbols: PkImage::load(...)"。
ctest --test-dir pk/image/build --output-on-failure

# ③ 判据③：库里不得有 Qt 未定义符号。**除已知假阳性外必须无输出。**
#    已知假阳性：PkSize::scaled(..., Qt::AspectRatioMode) 里的 `Qt::` 是本仓库
#    pk/geometry 自己的 namespace，不是真 Qt 符号（真 Qt 类符号 0 处，见 report）。
#    R-75 实测两个库都是 0 命中，假阳性那条没有触发。
nm -u -C pk/image/build/libpkimage.a | grep -i qt
nm -u -C pk/image/build/libpkimageio.dylib | grep -i qt

# ④ 对拍（需要真 Qt 5.15；依赖前缀 PK_QT_PREFIX，没设就按脚本头注释那套解析）
#    R-75 补了 macOS 支（framework / otool -L / 无 timeout），判据一字未动；
#    缺薄壳产物时脚本会先自己跑一次 cmake 建起来。
pk/image/oracle/run_oracle.sh

# ⑤ 试接（两个 driver，跑绿 + 零 Qt 自证）
pk/image/tests/graft/run_graft.sh
```

> `pk/image/build/` 被顶层 `.gitignore` 排除（`oracle/build/` 同理）。

---

## 6. 待认领的缺口（供主会话写回 TASKS.md / spec）

完整清单与理由见 `task-5-report.md`，这里只列条目：

1. **图像文件编解码**——`QImage(fileName)` / `.load()` / `.save()` 已由 **R-75 落地**
   （定义在 `pkimageio`，见 §3.1；`save` 目前只有 PNG 编码器）。**仍未落地的**：
   `fromData`/`loadFromData`/`QImageReader`/`QImageWriter` 这几个数据流形态——
   归 impex 插件 / libs/resources 各自 S 批次用外部编解码库。
2. **`QAbstractItemModel::data()` 返回 QVariant 持有 QImage**（Model/View，3 处）——
   需 Model/View 端口先落地。
3. **R-06 `PkVariant::toImage()` 闭包**（S-06 完成时 QImage 替代品落地）。
4. **`.valid(int,int)`** —— Task 1 开工核实无真实调用点，故未实现（记录供追溯）。
5. **`QRgb`/`qRed`/`qGreen`/`qBlue`/`qAlpha`/`qRgb` 颜色辅助**（实施新增）——不是
   QImage 方法，driver 用 `tests/graft/qrgb_shim.h` 脚手架提供，归未来 PkColor 任务
   或 S 线。
6. **Fast 模式变换逐位不对齐**（实施新增的**订正**，不是缺口是偏离）——plan 岔路 B
   对 Fast 的「必须逐位对齐」裁决被 Task 4 事实性推翻，见 §2①，醒目提醒主会话。
7. **PkImage 缺 const `scanLine(int) const` 重载**（实施新增）——真 Qt 有、20 个测试
   文件经 compareQImagesImpl 用到；一行转发即可补，归后续 R-15 修复轮或 S 线。
