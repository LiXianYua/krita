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

## 登记（R-55 Task 2 起，Task 5 收口 —— 本批的完整偏离表）

**范围**：本节是 **R-55 全批的登记表** —— plan §6 的六条 + `task-4fix-report.md` §9
的三条新增未修分歧 + Task 5 写 driver 时实测到的两条，再加 Task 2 自身两条。
**命令面与判据①③ 的登记在 `pk/render/README.md`**（那儿同时是本批文字 oracle 的主
文档），本文件不重复。每条一律给 **实测数字 · 出处 · 为什么这样处理**。

**读法**：凡标「零用量」的，都指**保留范围**（`libs/ plugins/ pk/ sdk/`，排除任意层级
`tests|benchmarks|oracle|build|thirdparty|3rdparty`）内 **`PkPainter` 那条路**的调用点
计数 —— 本批两处真实 `drawText` 调用点的文本分别是 `#<n>~<m>\n(<idx>)` 形式的 ASCII
标签和 `handleName()` 的 `i18nc` 手柄名，两者都不含下列码位。

### A. plan §6 的六条

1. **判据④在本机不可运行**（plan §6-1；人 2026-09-12 已裁决）。

   - **实测**：本机 Qt **无 FreeType 文字引擎** —— `QFontEngineFT=0`，**分母
     `nm_lines=9594`**（证明 nm 真读到了那个二进制），判别力对照 `QFontEngineMulti=40`
     非零（⇒ `0` 不是「nm 读了个空文件」的假阴性）。`font_pixels_qt_comparison` 打印
     `SKIP: Qt has no FreeType text engine (0)`、**退出 0**、**不改写任何产出**。
   - **判别力对照（P8）**：同一条〈逐输入对拍〉判据作用在**几何类**上本机**是绿的**：
     `PK_RENDER_BUILD_DIR=/tmp/r55-pkrender bash pk/render/oracle/run_shape_primitive.sh`
     → `shape primitive Qt oracle: identical (36 cases)`，退出 0。所以「本机不可达」是
     **文字类特有**的平台事实，不是这条判据在本机整体失效。
   - **为什么是「不可运行」而不是「未实现」**：两者的区别就是上面这条对照 —— 判据本身
     在本机是好的，缺的是**参照系**（Qt 在 macOS 走 CoreText，Pk 按
     `pk/port/PkFontProvider.h` 的成文裁决四平台统一走 FreeType）。对齐语义因此定为
     「对齐 Qt-**with-FreeType**」，绿灯只在带 FreeType 引擎的宿主上认；本机完成判据
     可达上界 = ①②③ + Pk 侧反向证伪（变异注入），**一条都不降**。
   - 源：plan §2 P6 / P8；`pk/font/oracle/compare.cmake` 的探测输出（见上「P6」「P8」两节）。

2. **`font_pixels_qt_comparison` 不在 `.exec/baseline/tests.txt` 里**（Task 2 登记）。
   它是 `pk/font/oracle` 这个**独立薄壳工程**里的测试，不属于 Krita 主构建的 CTest
   清单（基线只收主构建的测试）；本机它以 `SKIP` 绿着，判读时按本条而不是按
   「测试通过」理解。

3. **空 `PkFont`（`setFont` 从不调用）的字体解析随平台**（plan §6-3）。

   - **实测**（plan §2 P3）：`default painter font: family=.AppleSystemUIFont
     pixelSize=-1 pointSize=12`。Qt 用平台/应用默认字体；Pk 走 fontconfig `sans-serif`
     \+ `12pt@96dpi`（`pk/font/PkFontRasterizer.cpp:170`
     `FT_Set_Char_Size(face, 0, (pointSize>0?pointSize:12)*64, 96, 96)`）。
   - **为什么保留并登记**：**这是范围划定，不是实现疏漏**。Pk 内核没有 application
     对象；字体**发现**按 `pk/port/PkFontProvider.h` 的成文裁决属平台端口，**度量/栅格**
     统一 FreeType。零用量：`PkSetFontCommand` 的活调用点 = **0**，两处真实调用点都不
     调 `setFont`，所以这条分歧在本批根本不可见。（`PkPaintCommand.h:32` 注释里那个
     「setFont 1」计的是 `KisTextBrush::setFont`，**接收者不同**，不是本条的反例。）
   - 源：plan §1.1 / §2 P3 / §6-3。

4. **gamma（CFF）字形的彩色合成未验证**（plan §6-4）。

   - **实测口径**：字形混合在 `pk/font/PkFontRasterizer.cpp:110-130`
     `blendCoverage(previous, coverage, gammaCorrect)`；`gammaCorrect` 只在
     `format == "CFF"` 时为真（`:267`、`:376`）。非 gamma 路径下
     `coverage.mask[i] = 255 - raster.stored[i]`（`:539`）**即覆盖率本身**。
   - **为什么保留并登记**：gamma 路径下该式是否等于 Qt 的 alpha map，**本机无 FreeType
     宿主可证** —— 与第 1 条同源，是判据④不可达的直接后果。
   - **这条不是「死代码」**（2026-09-12 全分支评审订正）：gamma 分支**被用例集踩到**——
     变异组 E（`gammaCorrect` 恒 false）在 `test_text` 上命中 3 例
     （`callsite2/handlename/devpt30/nonascii`、`adv/nonascii`、`vis/bidi/lri`；tag 原名
     `.../scale1.5/...`，2026-09-12 修复轮随单映射建模改名）。
     缺的只是 **Qt 侧的彩色合成参照系**，不是这条分支没有用量。原文「零用量：本批用例
     覆盖到的字体里没有一个 CFF-format 彩色字形」字面不假（命中的是 CFF **单色回退**字形，
     非 COLR/CBDT 彩色字形），但会被读成「死代码」，故订正。
   - 源：plan §3.1 / §6-4。

5. **fontconfig 配置必须显式设**（plan §6-5；与 Task 2 登记的「环境事实」同条）。

   - **实测**：不设 `FONTCONFIG_PATH` 指向 CI 前缀的 `_install/etc/fonts` 时，native 侧
     打印 `Fontconfig error: Cannot load default config file`，且 CJK 回退与字形栅格随之
     改变。**实测到过一次 ctest 因环境假红**（未设时红、导出后绿），修复见 commit
     `4dee3da`。
   - **处理**：`pk/render/CMakeLists.txt:83-109` 把值**烘进** `test_text` 的 ctest 环境
     ——优先取现成的 `FONTCONFIG_PATH`，否则从 `CMAKE_PREFIX_PATH` 推
     `<prefix>/etc/fonts`，两条都拿不到才 `message(WARNING)` 并退回宿主默认配置
     （**它不静默**，假红会以一条 CMake 警告的形式留痕）。
   - **为什么登记**：这是**环境**的既有事实，不是 `pk/font` 的缺陷；任何读 native 侧
     输出的场合（含后续对拍工装）都必须显式设上，否则结果随环境漂。零用量不适用
     ——它影响的是**所有**用例，所以修在工装里而不是登记为「看不见」。

6. **`text_golden.txt` 没有 Qt 出生证明**（plan §6-6）。

   - **实测**：`pk/render/oracle/text_golden.txt` 前三行是
     `# generated by pk/render/oracle/run_text.sh` / `# born=Pk` /
     `# backend=PkImageRasterBackend`，正文 **44** 条 `DIFFTAG`。`run_text.sh` 在本机走
     SKIP 分支，golden 由 **Pk 侧**写出。
   - **为什么登记**：本机 `test_text` 守的是「**Pk 侧自注入以来没漂**」，**不是**
     「与 Qt 逐像素相等」—— 任何拿它当 Qt 金标读的结论都是错的。有 FreeType 的宿主跑
     一次 `run_text.sh` 会换成 Qt 出生证明，同一个测试即变成真正的跨侧断言。
   - 源：plan §6-6；第 1 条同源。

### B. `task-4fix-report.md` §9 的三条新增未修分歧

7. **`\t`（U+0009）：Qt 走文本引擎的 tab stop，Pk 交给 Raqm**（§9-2 / §6.1）。

   - **实测**（DejaVu Sans px24，`qtextengine.cpp:1341 calculateTabWidth`，
     **与 `applyVisibilityRules` 同层 = 文本引擎层**）：`U+0009` 单字符 Qt 推进量
     **80.000** / Pk **7.000**；`"X\tY"` Qt **94.656** / Pk **38.000**。Qt 的 80.000 是
     `QTextOption::tabStop` 的默认值，不是字体度量。
   - **为什么保留并登记**：**零用量**（两处真实调用点的文本里没有制表符）。要修得先把
     `coverage()` 的「一整串 → 一张 mask」出口**拆成逐段摆放**，属设计改动，超出本批；
     而两处活调用点都不需要它。
   - 源：task-4fix-report.md §6.1（完整扫描表在 §3）。

8. **bidi 控制符 U+2066/U+2067/U+2068/U+2069/U+061C：Qt 视作无操作，Pk 的 Raqm 会挪动
   邻字**（§9-2 / §6.2）。

   - **实测**：Pk 摘要（`coverage` 的 mask 上算 FNV）`"AB"` = **2598421920**；
     `A LRI B` / `A RLI B` / `A LRI B PDI` / `A FSI B` 全是 **2038677757**；
     `A RLO B` / `A PDF B` **等于** `"AB"`。Qt 侧 `A LRI B` 的 advance 与墨迹与 `"AB"`
     **逐像素相同**（260 组非白像素、bbox `[20..50]` 完全一致）。
   - **为什么保留并登记**：这不是「丢弃」缺陷 —— **两侧推进量都是 0、字形都不出墨**；
     它是 Raqm 的 bidi 解析与 Qt 自带 bidi（`QTextEngine::itemize`）的**实现差异**，
     属另一条线。零用量：五个码位在两处真实调用点的文本里都不出现。
   - 源：task-4fix-report.md §6.2。

9. **U+007F / U+180E / U+FFF9..FFFB / U+0008 / U+001D：字体引擎差异**（§9-2 / §6.3）。

   - **实测**：这五个在 Qt 侧是**字体引擎**（本机 CoreText）隐藏的，不是文本引擎规则
     ——「五字体一致性」列全读 `no`（U+007F 3/5 字体丢、U+0008 与 U+001D 4/5、
     U+FFF9/U+FFFA/U+FFFB 各 1/5 且只有 DejaVu），而**文本引擎规则**的判别列是 5/5
     一致（见 §2.1：五字体一致的 35 个码位里 Pk 只错 3 个，恰好是
     `applyVisibilityRules` 里非 DICP 的 LF/FF/CR）。Pk 走 FreeType，本来就可能不同。
   - **为什么保留并登记**：这正是已登记的〈文字类对拍〉**平台差异**（plan §6-1），
     **按构造就在范围外**，不是遗漏。**把它们也滤掉的诱惑必须顶住** —— 那会与
     「对齐 Qt-**with-FreeType**」的既有裁决冲突（有 FreeType 的宿主上 Qt 走 FreeType，
     未必隐藏它们）。`pk/render` 的 `vis/kept/del` / `vis/kept/mvs` 就是守住这条的反向
     守卫。
   - 源：task-4fix-report.md §2.1 / §6.3。

### C. Task 5 实测到的两条（driver 期）

10. **实现建模与 Qt 有细微差异：Qt「保留字形、推进量归零」vs 本实现「从串里删掉」**
    （`task-4fix-report.md` §9-3）。

    - **实测**：已知样本上**等价** —— `vis/hidden/lf` 与 `vis/hidden/lf-twin` 摘要相同，
      `adv/newline-mid` 与 `adv/newline-twin` 摘要相同。
    - **未测**：「隐藏字符参与邻字整形」的场景（本实现删掉它们之后，邻字拿不到那段
      整形上下文）。**为什么登记而不改**：两种建模在**全部实测样本**上等价，而改成
      「保留字形 + 归零推进量」要动 `coverage()` 的出口形状，是设计改动；零用量
      （两处真实调用点的文本里没有隐藏字符）。
    - 源：`task-4fix-report.md` §5.1 末段 / §9-3。

11. **真实测试类的编译级证据仍欠**（`task-4fix-report.md` §9-4）。

    - **Task 5 已补**：按 `R线-spec`〈依赖墙挡住真实测试类时〉的降级路径交付了
      `pk/render/tests/graft/text_draw_driver.cpp`（逐行复刻两处真实 `drawText` 调用点的
      代码形状；详见 `pk/render/README.md` 的「R-55 Task 5」节，原始 `-fsyntax-only`
      报错见 `task-5-report.md` §2）。**但真实测试类本身的编译级证据仍欠**：那两处文件
      编在 `kritaflake` / `krita_tool_svgtext_static` 里，其 CMake 生成产物与
      `PUBLIC` 传递闭包**不在 R-55 的 locks 内**，物理编不过。
    - 源：`R线-spec`〈依赖墙挡住真实测试类时〉；plan §4 Task 5。

12. **扫描覆盖不到的区**（`task-4fix-report.md` §9-5）。

    - **已覆盖**：U+0000..001F · U+007F..009F · U+00A0 · U+00AD · U+061C · U+180E ·
      U+2000..200F · U+2028..202F · U+205F..2064 · U+2066..206F · U+FEFF ·
      U+FFF9..FFFB · U+110BD。
    - **未覆盖**：非 BMP DICP 区、U+FFF9..FFFB 之外的标点替换符、非拉丁脚本字体。
    - **为什么登记**：判据本身不依赖这些（它是**文本引擎层**的，与字体无关），但
      **例外若存在，本表看不到** —— 如实登记覆盖边界，避免把「扫描里没有」读成
      「不存在」。
    - 源：`task-4fix-report.md` §2 末段 / §9-5。

## 相关

- 判据④的通则与裁决：`docs/superpowers/specs/R线-spec.md`〈文字类对拍〉
- 本批的范围（判据①③、`drawText` 两处真实调用点、`PkDrawTextInRectCommand`
  零用量登记）：`docs/superpowers/plans/R-55.md`
- **命令面 / 判据①③ / 判据② 的降级路径 driver（`pk/render/tests/graft/text_draw_driver.cpp`）**：
  `pk/render/README.md` 的「R-55 Task 5」节
- 本批登记的**原始证据**（扫描表、变异的逐条退出码）：`.superpowers/sdd/R-55/task-4fix-report.md`
  （本轮收口报告：`.superpowers/sdd/R-55/task-5-report.md`）
- 几何类对拍的兄弟目录：`pk/render/oracle/`（`pk/render/README.md` 有它的登记）
