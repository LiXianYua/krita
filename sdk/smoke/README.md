# `sdk/smoke/` —— `paint_smoke` M5 验收载体

本目录是 `paint_smoke`（纯 C++ CLI，非 GUI）的落点。它链主树内核三件套
（`kritaimage` / `kritaimpex` / `kritalibkra`）+ kra 编解码 + `kritalcmsengine`，
跑通「建文档 → 画一笔 → 存 `.kra`」，做的事见 `paint_smoke.cpp` 与
`CMakeLists.txt` 的头注释。

## ⚠ 环境说明：`paint_smoke` 的**退出码不是与 `HOME` 无关的判据**

**`paint_smoke` 会读使用者的真实 Krita 配置**（`PkConfigStore`，macOS 上是
`$HOME/Library/Preferences/kritarc`；Linux 上是 `~/.config/kritarc`）。因此：

- **同一个二进制，换一个 `HOME`，退出码可以从 `0` 变成 `139`（SIGSEGV），反之亦然。**
- 所以**不能**把「`paint_smoke` 退出码 = 0」当作一条与运行者环境无关的通过判据。
  要判 `paint_smoke` 本身对不对，**必须固定 `HOME`（用隔离 / fixture HOME）**，
  或者把「HOME 里恰好有什么」当成判据的一部分显式写下来。

**R-67 定位结论**：触发条件不是「`HOME` 里随便什么」，而是**使用者真实 Krita 配置
文件里 `[PkConfig-v1:]` 段的两个内存上限键**
`memoryHardLimitPercent` + `memoryPoolLimitPercent` 的**组合**（穷举最小子集搜索 +
留一法旁证得到，见 R-67 计划 F4）。这两个键把内存上限压得极小 ⇒
`KisTiledDataManager` 真的把 tile 换出到 swap 文件、再换回来 ⇒ 走到
`KisMemoryWindow` 的映射路径。**根因不在 `HOME`，在 `libs/image` 的映射对齐**；
`HOME` 只决定「会不会走到 swap」。默认内存上限下，`paint_smoke` 的 256×256 图
**根本不会走 swap-in**，所以干净 `HOME` 恒 `0`。

> **口径与原始数据以 `PK/docs/superpowers/plans/R-67.md` 为准**（§1 F1 的三 HOME 矩阵、
> §1 F4 的触发键、§8.2 的时序相关性说明）。本 README 只把「怎么复现」与「这条
> 判据的边界」写清；计划文档里那些值是**当时那台机器、那个时刻**的实测，**不是恒定条件**。

## 三 HOME 矩阵（R-67 实测原始数据）

同一份 `build-r67/bin/paint_smoke`（1430 步 `ninja` 现场编出）：

| `HOME` | 命令 | 退出码 |
|---|---|---|
| 真 `HOME`（`/Users/liyang`） | `paint_smoke /tmp/r67_real.kra` | **139** |
| 干净 `HOME`（`mktemp -d`） | `HOME=$D paint_smoke /tmp/r67_clean.kra` | **0** |
| 触发 `HOME`（R-67 构造的 fixture，见下） | `HOME=/tmp/trigger_home paint_smoke …` | **139** |

- 真 `HOME` 连跑 5 次全 `139`、干净 `HOME` 连跑 3 次全 `0` ⇒ **当时**是确定性的，不是间歇。
- 干净 `HOME` 下 `paint_smoke` 的 10 条 `check()` 全 PASS，`.kra` 正常写出。
- 逐项复制 `HOME` 顶层定位到**单文件**触发点：`Preferences/kritarc` → `139`；
  `.qttest` / `.cache/fontconfig` / `Library/Application Support/krita` / `Library/Caches`
  → 全 `0`。空 `kritarc` → `0` ⇒ **是内容，不是文件存在与否**。

> ⚠ **修前的 SIGSEGV 是时序相关的**（R-67 §8.2）：同一份修前二进制、同一份 fixture，
> 17:36–17:53 窗口内 5/5 `139`，18:24 起 0/21 复现（全部 `0`）。原因与
> `KisTileDataSwapper` 的时钟迭代器 + 后台 swapper 线程的交错有关，**不是稳定可复现**。
> **所以这张表是「某个时刻的观察」，不是「随时都能复现的断言」。**
> R-67 后来改用两条**确定性**证据做判据（页对齐 driver 直接构造非页对齐偏移 +
> 「只留对齐修复 ⇒ 真/触发 HOME 各 3/3 挂死」），见计划文档 §8.2。

## 复现命令（可复制粘贴）

前置（每次 shell 都要，`shell` 工具不跨调用留环境变量）：

```bash
source /Users/liyang/Developer/projects/krita-ci-env/env
export CCACHE_DIR="$KDECI_CC_CACHE"
cd /Users/liyang/Developer/projects/paint_workspace/krita-worktrees/R-65
```

构建 `paint_smoke`（退出码判据用 `${PIPESTATUS[0]}`，管道后的 `$?` 是 `tail` 的）：

```bash
ninja -C build-r65 paint_smoke 2>&1 | tail -3 ; echo "ninja exit=${PIPESTATUS[0]}"
```

**① 真 `HOME` 那一格**（会读使用者的真实 `kritarc`；**跑之前先备份你自己的配置**）：

```bash
cp "$HOME/Library/Preferences/kritarc" /tmp/kritarc.backup  # 若存在，先备份
build-r65/bin/paint_smoke /tmp/r67_real.kra ; echo "exit=$?"
```

**② 干净 `HOME` 那一格**（隔离，判 `paint_smoke` 本身是否通过时用这一格）：

```bash
D="$(mktemp -d)"
HOME="$D" build-r65/bin/paint_smoke /tmp/r67_clean.kra ; echo "exit=$?"
rm -rf "$D"
```

**③ 触发 `HOME` 那一格**（复刻 R-67 的 fixture：在隔离 `HOME` 里写入那两个内存上限键）：

```bash
D="$(mktemp -d)"
mkdir -p "$D/Library/Preferences"
cat > "$D/Library/Preferences/kritarc" <<'INI'
[PkConfig-v1:]
INI
# 该段的键名与值都是十六进制编码，真实格式见 pk/config/PkConfigStore.cpp:32；
# R-67 的 fixture 由 PK/docs/superpowers/plans/R-67.md 的 F4 给出（穷举命中的组合）。
HOME="$D" build-r65/bin/paint_smoke /tmp/r67_trigger.kra ; echo "exit=$?"
rm -rf "$D"
```

> 触发 fixture 的 `[PkConfig-v1:]` 段内容以 `PK/docs/superpowers/plans/R-67.md` 为准
> （那里有穷举搜索的原始输出与十六进制编码的键值）；上例只是段标题的骨架，
> **不是**能触发的那份——**照计划文档构造**。

## 边界与残留

- **判据不建立在「能崩」之上**：修前的 SIGSEGV 时序相关（§8.2），所以 `paint_smoke`
  的 `139` **不能**当成稳定的失败信号用。判 `paint_smoke` 正确性请固定 `HOME`，
  并优先依赖 `paint_smoke` 自己的 `check()`（笔触内部像素为红）而非退出码。
- **本目录的 `paint_smoke.cpp` 不改**：本 README 是**文档条目**，不是代码判据。
- **真实 `kritarc` 里的 `[PkConfig-v1:]` 段是测试进程写进用户 `HOME` 的**
  （同段还有 `ResourceDirectories=…/.qttest/…/TestTagResourceModel/…`）——这是一条
  独立的可疑项（测试未隔离 `HOME`），归 R-67 的交接项，不在本目录范围。
