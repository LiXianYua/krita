# Krita 剥 Qt 内核化 · 仓库约定

本仓库是 Krita 的 fork（基点 `v6.0.3`），目标：剥掉全部 Qt 依赖做成 C++ 绘画内核，再接 Flutter。
**完整背景在 `$PK/docs/README.md`，不要通读，按需查节。**

## 路径：先记这一条

```
PK = /mnt/ssd-disk/liyang/projects/paint_tips     ← 工作空间根
```

**下面所有 `$PK/...` 都是绝对路径，不要换成 `../`。** 原因：你现在大概率在
`$PK/krita-worktrees/<任务ID>/` 这样一个独立 worktree 里（`scheduler.py auto claim`
建的），相对层级跟在仓库根时不一样，写 `../docs` 会指向不存在的地方。

**这个 worktree 是给你的，不是主仓库。** `docs/`、`.exec/`、`.claude/` 不在这
个 worktree 里（它们是 `paint_tips` 那个仓库的东西，`krita/` 是完全独立的另
一个 git 仓库）——那些是主会话的东西，你不碰，也碰不到 git 层面的它们（能碰到
的只是文件系统上仍然可达的绝对路径，别去碰）。

| 你要找 | 路径 |
|---|---|
| 任务状态 SOT（只读，别手改） | `$PK/docs/TASKS.md` |
| 决策文档 / 工作流 | `$PK/docs/` |
| 摘出算法的归属登记（D 线硬规则三） | `$PK/.exec/salvage/<ID>.tsv` |

## 你正在做什么

任务 ID、线级 spec 路径、这个 worktree 的绝对路径、批次分支名、你那一行任务
定义——都在启动 prompt 里给出（主会话的 `/drive` 派发时给的）。**你不挑任务、
不合并分支、不改任务状态**——那些都是主会话的事，见
`.claude/agents/task-agent.md`「你不做」。

## 你怎么跑

1. 读线级 spec 与你那一行任务定义
2. `superpowers:writing-plans` 写实施计划
3. `superpowers:subagent-driven-development` 逐 Task 实施
4. `superpowers:requesting-code-review` 全分支评审
5. **任务 ID 以 `D-` 开头时，第 4 步再并行派一个 `salvage-auditor`**——只答
   「有没有该摘而没摘的算法」，这是 D 线硬规则三
6. 按「回报格式」回报

**完整流程、停止条件、回报格式，以 `.claude/agents/task-agent.md` 为准**——
本文件只补它没覆盖的「这个仓库有什么特殊之处」，不重复它已经写清楚的东西。

## 硬约束（任何会话、任何子 agent 都适用）

**判定层（验收脚本、陷阱、禁区闸门、对抗面板）已经整体删掉**——下面这些约束
现在只靠角色定义与流程结构撑着，**不再有脚本拦你**。发现被绕过了，说明角色
定义或流程本身要补，不是"再加一个检查"。

**禁止**：
- 改 `$PK/docs/` 下的四篇决策文档：`迁移执行计划.md`、`实施边界-构建目标视图.md`、
  `Qt替代品选型.md`、`复刻方案与技术选型.md`——只有人能改。发现它们与代码/
  实测不符，**报差异，不要自行改**
- 手改 `$PK/docs/TASKS.md`——状态与依赖只能通过主会话的
  `python3 $PK/.exec/scheduler.py status` 改；改任务状态本来就不是你的事
  （见上「你正在做什么」），你不该直接调用这条命令，也不该手改这个文件
- 改 `$PK/.exec/`、`$PK/.claude/` 下任何文件，或 `$PK/CLAUDE.md`——流水线
  本身、角色定义、工作空间约定
- 改本文件
- 修改不属于你当前任务的 target
- 任何与当前任务无关的改动——包括"顺手"重构、改格式、修无关 bug
- 合并到 `strip-qt`（主干）；**不要 `git push`**——合并由主会话跑
  `scheduler.py auto finish` 执行：批次分支 → rebase → ff-merge 回 `strip-qt`

**必须**：
- 遵守 `superpowers:verification-before-completion`：声称完成前真的跑命令并确认输出
- 测试失败先用 `superpowers:systematic-debugging`，不要猜
- 判断不了的事按 `task-agent.md` 的停止条件回报（`RESULT=need-human` 或
  `RESULT=stuck`），不要自己拍板继续做

## 完成判定

以 `.claude/agents/task-agent.md`「完成判定」一节为准，这里不重复，避免两份
判据各自漂移。**这里只强调一条本仓库特有的**：**S 线（`S-*`）任务额外要跑
`nm -u <产物> | grep -i qt`，把输出贴进报告**——这是 S 线「Qt 符号真的没了」
这条判据在本仓库的具体命令，别的地方不会替你补。

**没有验收脚本。** 不要去找 `$PK/.exec/verify/`，它不存在。

## 跑测试：排掉基线本来就红的，别手抄名单

```bash
cd <你的 worktree>
source /mnt/ssd-disk/liyang/projects/krita-ci-env/env          # ← 必须与 ctest 同一次调用
LD_LIBRARY_PATH="$(pwd)/<build 目录>/bin:$LD_LIBRARY_PATH" QT_QPA_PLATFORM=offscreen \
  ctest --test-dir <build 目录> --output-on-failure \
    -E "$(python3 /mnt/ssd-disk/liyang/projects/paint_tips/.exec/baseline/known_failures.py)"
```

> **`source env` 必须和 `ctest` 在同一次 Bash 调用里。** Bash 工具每次调用是独立
> 进程，**shell 环境变量不跨调用保留**。上一次调用里 `source` 过不算数。
>
> **症状极具误导性**：D-01b 实测过一次——`source` 掉了之后**全部 298 个测试瞬间
> 失败**，报 `libicui18n.so.70` 找不到。看起来像自己把什么改崩了，实际一行代码
> 都没问题。**全部瞬间失败 = 环境没配好，不是代码回归**；真回归不会这么整齐。

四段各自防一件事，**一段都不能省**：

| 那一段 | 防什么 |
|---|---|
| `source .../krita-ci-env/env` | 前缀里的 Qt/KF5/ICU 等运行时库。**Bash 工具不跨调用保留 shell 变量**，上次 `source` 过不算数——掉了它会 298 个测试**瞬间全红**报 `libicui18n.so.70` 找不到 |
| `-E "$(known_failures.py)"` | 基线里**本来就红**的测试不在任何判据的判定范围内（判据只要求「基线里绿的仍然绿」），却是耗时大户——`libs-ui-kis_shape_layer_test` 会死锁、挂满 ctest 默认超时 **1500 秒**，`libs-flake-TestSvgParser` 三兄弟再耗约 12 分钟 |
| `LD_LIBRARY_PATH=<自己的 bin>:...` | 共享前缀 `krita-ci-env/_install` 里有旧的基线库（`libkritaglobal.so` 等），而 `LD_LIBRARY_PATH` 优先级**高于**可执行文件自己的 `RUNPATH`。不排前面会加载到旧库、报一片假的 `undefined symbol`——改了导出符号的任务必然撞 |
| `QT_QPA_PLATFORM=offscreen` | 无头环境下 QtTest 默认连 X11 会直接崩 |

**名单是算出来的，不要手抄。** 它跟着 `.exec/baseline/tests.txt` 走，基线一重建
（换机器、改构建配置、删掉带测试的 target）名单就变，抄下来的那份**漂了也不会
有人报错**。脚本读不到基线时打印的是「谁都不排除」的安全值并以非 0 退出——
失败方向一律偏向多跑测试。

**报告里要点名**：被排掉的那些是「基线即红、不在判定范围」，**不能算成
「测试丢了」**。还有 **「未跑到」不是「跑绿了」**——有测试没结果就单独重跑确认，
别混着报。

> 测试 I/O 已经改道 SSD（`~/.qttest` 是指向 `/mnt/ssd-disk/liyang/.qttest` 的
> 符号链接），**你什么都不用做**。改道前 `libs/flake` 那批资源库 I/O 密集的测试
> 单个要 250–600 秒，改道后 3–4 秒。

## 构建：configure 时必须挂上 ccache

**每次 `cmake` 配置新 build 目录都要带这两个 `-D`**，一次都不能漏：

```bash
source /mnt/ssd-disk/liyang/projects/krita-ci-env/env   # PATH/LD_LIBRARY_PATH/CMAKE_PREFIX_PATH
export CCACHE_DIR="$KDECI_CC_CACHE"                     # ccache 认的是这个，不是 KDECI_CC_CACHE

cmake -B <build 目录> -G Ninja \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
  ...其余按 $PK/docs/构建环境配置.md
```

**为什么这条写在这里**：每个任务一份 worktree，各自全量构建约 9 分钟（7 212 个对象
文件）；而各 worktree 之间只有你 `locks` 内的几十个文件不同，其余 3 000 多个源文件逐
字节相同。缓存已按多 worktree 场景配好（`base_dir` + `hash_dir=false`，实测跨 worktree
命中），**漏了这两个 `-D` 就等于把别人已经编好的东西再编一遍**。

**验证你确实用上了**（声称完成前跑一次，属于 `verification-before-completion`）：

```bash
ccache -s | grep -E 'Hits:|Misses:'    # 全量构建后 Hits 应远大于 0
grep COMPILER_LAUNCHER <build 目录>/CMakeCache.txt   # 两行都要在
```

已经建好的 build 目录补这两个 `-D` 会触发一次全量重建（ninja 认为命令行变了）——
**不要在任务中途补**，那一次只会更慢；建新 build 目录时带上即可。

调试信息有一处代价：`DW_AT_comp_dir` 指向第一个填缓存的 worktree，gdb 单步要
`set substitute-path` 才找得到源文件。`ctest`、benchmark、backtrace 的函数名与行号
都不受影响。

## 这个仓库的两个特殊之处

**① 是 shallow clone（`depth=1` @ `v6.0.3`）。**
`git log` 只有一个根提交，**`git blame` 查不到 Krita 的历史**——想知道某段代码为什么这么写：

```bash
cd /home/liyang/projects-ssd/krita && codegraph explore "<符号名或问题>"
```

官方只读 clone 有 codegraph 索引（基点 `v6.0.3`，与本仓同源），一次调用给出相关符号
原文加调用路径，**含 grep 追不到的动态分派**。**本仓库与各 worktree 都没有
`.codegraph/`**，所以全局 `CLAUDE.md` 那条「没索引就跳过」说的是这里，不是说没索引可用。
索引反映**未删减的原始 Krita**：问「原来怎么设计的」它权威，问「现在还剩什么」要在本仓库数。
那个 clone 只读，**不许改它的源码**。

再往上是 `$PK/docs/krita/` 的架构知识库，或 GitHub 上的 `KDE/krita`。
**不要 `git fetch --unshallow`**（9.5 GB，且 D-18 本来就要截断历史）。

**② 剥离期整树编译不过是正常的（D-19）。**
每个 target 用 `$PK/.exec/shell/<target>/` 的独立薄壳 CMake 工程构建（I-01 产出，未完成前不存在），
**不要试图让全树 `ninja` 通过**——那要等 M5 合拢。删减期则相反，全树应始终可构建。

## 工作方法

- 一个 target 的完整生命周期：`$PK/docs/任务做法.md` **W0**（动手前先读）
- 各类工作的做法：同文档 W1–W7
- Qt → Pk 类型对应：`$PK/docs/Qt替代品选型.md` §1（**以它为准，不许自创**）
- 该 target 的文件清单与规模：`$PK/docs/实施边界-构建目标视图.md` §6
- 该子系统的机制说明：`$PK/docs/krita/` → 边界实测：`$PK/docs/boundary-analysis/`

## 报数字必须带口径（`任务做法.md` W6）

三个已知的坑，踩过不止一次：

- **全仓有 207 个 `.cc` 文件**（`git ls-files '*.cc' | wc -l`），其中 `libs/image` 一家占 42 个
  ——**报之前先说清是全仓还是某目录**，只数 `.cpp` 会系统性低估
- **统计要排除 `tests/` 和 `benchmarks/`**（`libs/pigment/benchmarks/` 4 文件含 2 个 `Q_OBJECT`）
- 按 target 解析 `target_link_libraries` 求**传递闭包**，不要按文件 grep——一个 `CMakeLists.txt`
  常定义多个 target，且「MODULE 只链自己的 `_static` 中间层、Qt 依赖藏在中间层的 `PUBLIC`」
  这种形态有 4 例，只看 MODULE 那一行永远看不见 `kritaui`

**对不上时先怀疑自己的口径**——文档核实阶段约 80 项断言里文档只错 9 项，而核实方错了 6 次。
