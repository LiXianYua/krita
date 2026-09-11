# pk/font —— PkFont / PkFontRasterizer（零 Qt 文字内核）

独立薄壳工程（R 线）。按 `pk/port/PkFontProvider.h` 的成文裁决**四平台统一走
FreeType**：fontconfig 解析字形族 → Raqm/HarfBuzz 整形 → FreeType 栅格 → 移植
自 Qt 的字形混合/gamma。本目录**零 Qt 依赖**；唯一碰 Qt 的地方是同目录的
`oracle/`（一个独立测试可执行文件，Qt 只活在 `PK_FONT_QT_ORACLE` 分支里）。

## `oracle/`：Qt 侧 vs native 侧逐像素对拍

`oracle/CMakeLists.txt` 编出两个可执行：native 侧链 `pkfont`，Qt 侧链
`Qt5::Gui`。`oracle/compare.cmake` 让两侧对同一批文本各渲一遍，逐字节比
`--pixels` 输出、逐行比摘要，任何一处不同就红。

跑法（薄壳是独立 CMake 工程；macOS 上**必须**显式给
`-DCMAKE_OSX_DEPLOYMENT_TARGET=13.3`——`pk/` 层用到 macOS ≥ 13.3 才有的
`std::to_chars` 浮点重载，而 CI env 把 `MACOSX_DEPLOYMENT_TARGET` 设成 10.15。
构建目录一律放 `/tmp/`，别落进 worktree）：

```sh
cmake -S pk/font/oracle -B /tmp/pkfont-oracle -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=13.3
ninja -C /tmp/pkfont-oracle
ctest --test-dir /tmp/pkfont-oracle -V -R font_pixels_qt_comparison
```

### 判据④（逐输入对拍）在本机不可运行 —— 实测原因

**结论**：本机（macOS arm64）**判据④不可运行**。`font_pixels_qt_comparison`
在本机打印 `SKIP:` 而不是真跑对拍，且**不改写任何产出**、退出 0。

这不是判据本身有问题，是**参照系本身依赖平台**：Qt 在 Linux 走 FreeType、在
macOS 走 CoreText（本机 Qt 编译时带 `QT_NO_FONTCONFIG`），而 Pk 按
`pk/port/PkFontProvider.h` 的成文裁决四平台统一走 FreeType。两侧的栅格器不是
同一个，逐像素相等在本机不可能成立——它红着从来不是「谁改坏了什么」。

人 2026-09-12 已裁决：保持 FreeType 裁决不动，「对齐 Qt」对文字的含义定为
「对齐 Qt-**with-FreeType**」；判据④按引擎条件化，本机登记「不可运行 + 实测
原因」，绿灯只在 Qt 带 FreeType 引擎的宿主上认。本机完成判据可达上界 =
① ② ③ + Pk 侧反向证伪（变异注入），一条都不降。

**P6 — 本机 Qt 无 FreeType 文字引擎（实测数字，R-55 plan §2 P6）：**

```
qtgui-config.h:17                    #define QT_NO_FONTCONFIG
nm -a libQtGui | QFontEngineMulti     40
                   QFontEngineQPF     29
                   QFontEngineGlyphCache 7
                   QFontEngineFT       0        ← 真阴性（兄弟类都在，判别力在）
FT_*                                  0
CoreText/CGContext                   20
QFontDatabase().families() = 251，含 .AppleSystemUIFont / Helvetica Neue
```

本机 2026-09-12 用 `compare.cmake` 里那段探测子复测，逐字一致；`nm -a` 原始
行数 9594 是**分母**（证明 nm 真读到了那个二进制）：

```
-- font oracle probe: QtGui=/Users/liyang/Developer/projects/krita-ci-env/_install/lib/QtGui.framework/Versions/5/QtGui nm_lines=9594 QFontEngineFT=0 QFontEngineMulti=40
SKIP: Qt has no FreeType text engine (0)
```

### P8 — 判别力对照：同一条判据在几何类上于本机是绿的

「本机做不了对拍」不能退化成万能借口。**同一条〈逐输入对拍〉判据**作用在**几何
类**（`pk/render/oracle/run_shape_primitive.sh`，Qt 侧真实 `QPainter` vs Pk 侧
`libpkrender.a`）上，在本机**是绿的**——所以「本机不可达」是**文字类特有**的平台
事实，不是这条判据在本机整体失效：

```
$ PK_RENDER_BUILD_DIR=/tmp/r55-pkrender bash pk/render/oracle/run_shape_primitive.sh
shape primitive Qt oracle: identical (36 cases)
```

（2026-09-12 本机复现，退出 0。原文见 R-55 plan §2 P8。）

### `compare.cmake` 的探测与 SKIP 语义

`compare.cmake` 在跑比较**之前**先做一次**实测的能力探测**：定位 `ORACLE`
真正链接的那份 QtGui，数它里面 `QFontEngineFT` 符号的条数。

| 探测结果 | 行为 |
|---|---|
| `QFontEngineFT > 0` | Qt 走 FreeType，参照系成立 → **照旧真跑两侧比较**（与条件化之前逐字节一致；受 FT 宿主上行为不变） |
| `== 0` 且 `nm_lines > 0` | 正面证据说明 Qt 无 FreeType 引擎 → 打印 `SKIP: Qt has no FreeType text engine (0)`、**退出 0**、**不写任何产出** |
| 探测不到（找不到 QtGui，或 nm 没读到东西） | 探测**无效** → **不跳过**，照旧真跑比较。宁可红着，也不把「探测失败」当成「无引擎」而在带 FT 的宿主上假装通过 |

QtGui 路径两条路都实现，注释里写明了为什么这么选：**主路**是对 `ORACLE` 跑
`otool -L`（macOS）/ `ldd`（Linux）反查它链接的 QtGui（最贴近「ORACLE 用的那份
QtGui」这个语义，且不依赖 `CMAKE_PREFIX_PATH` 在测试运行时还在不在），macOS 上
拿到的是 `@rpath/...` 形式，再拼回候选根；**兜底路**是在 `CMAKE_PREFIX_PATH`
下 glob 出 QtGui 库文件（otool/ldd 不是每个宿主都有）。两条路产出的候选取第一个
存在的。

探测打印的**全是实测数字**（QtGui 路径、nm 行数、`QFontEngineFT` 与
`QFontEngineMulti` 计数），没有一处硬编码「这是 macOS 所以跳过」——跳过与否只由
`QFontEngineFT` 的实测计数决定。`QFontEngineMulti` 计数一并打印是**判别力对照的
一半**：它非零说明 nm 确实看得见字体引擎类，`0` 不是「nm 读了个空文件」的假阴性。

## 登记（R-55 Task 2；本批的完整偏离表随 R-55 Task 5 落定）

1. **判据④在本机不可运行**（人 2026-09-12 已裁决，见上）。含判别力对照
   （P8：几何类判据本机为绿）。**这不是「未实现」，是「不可运行」**——两者的
   区别就是 P8：判据本身在本机是好的。
2. **`font_pixels_qt_comparison` 不在 `.exec/baseline/tests.txt` 里**。它是
   `pk/font/oracle` 这个**独立薄壳工程**里的测试，不属于 Krita 主构建的
   CTest 清单（基线只收主构建的测试）；本机它以 `SKIP` 绿着，判读时按本条
   而不是按「测试通过」理解。
3. **观测 native 侧的环境事实（不是本目录要修的东西）**：本机默认
   `FONTCONFIG_PATH` 未指向 CI 前缀的 `_install/etc/fonts`，不设会在 native 侧
   打印 `Fontconfig error: Cannot load default config file`，且 CJK 回退会变。
   这是**环境**的既有事实，不是 `pk/font` 的缺陷；任何读 native 侧输出的场合
   （含后续对拍工装）都该显式设上 `FONTCONFIG_PATH=<CI 前缀>/_install/etc/fonts`，
   否则结果随环境漂。

## 相关

- 判据④的通则与裁决：`docs/superpowers/specs/R线-spec.md`〈文字类对拍〉
- 本批的范围（判据①③、`drawText` 两处真实调用点、`PkDrawTextInRectCommand`
  零用量登记）：`docs/superpowers/plans/R-55.md`
- 几何类对拍的兄弟目录：`pk/render/oracle/`（`pk/render/README.md` 有它的登记）
