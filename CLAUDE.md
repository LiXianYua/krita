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

**这个 worktree 是给你的，不是主仓库。** `docs/`、`.exec/`、`.claude/` 不在这个
worktree 里——它们属于 `paint_tips` 仓库，`krita/` 是完全独立的另一个 git 仓库。
**这些路径你要读**（下表就是让你读的），**但一个字都不要改**：它们不在你的批次
分支上，你的改动既不会被合并，也不会被任何人看见。

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

**流程、停止条件、回报格式，以 `$PK/.claude/agents/task-agent.md` 为准**——那是
`task-agent` 这个角色专属的。

> **为什么通用纪律写在这份文件里，而不是写在 `task-agent.md` 里**：本文件在 fork
> 仓库的版控里，**每个 worktree 都带一份**，所以在这棵树里工作的 agent 全都加载得到
> ——包括 `task-agent` 派出去的 `general-purpose` 实现者与评审员。而 `task-agent.md`
> 是角色定义，**只有 `task-agent` 自己读得到**。下面「交结果」「派子 agent」「不要
> `sleep`」三节标着「所有层级都适用」，就是因为它们必须够得着孙子辈。

## 你怎么把结果交出去（**所有层级的 agent 都适用**）

**你的最终输出文本就是返回值，派你的人自动收到。**

- **不要用 `SendMessage` 找派你的人。** `task-agent` 是**类型名不是地址**，解析不到。
  你不需要它——通道本来就是通的。
- **不要在报告里写「请上层转达」。** 没有人需要转达。
- **最终输出要装的是报告本身，不是关于通道的说明。** 发现自己在最终输出里谈论
  `SendMessage`、返回值机制、或本节任何内容，**删掉，换成结果**——**上面这几条是
  给你自己看的纪律，不是给派你的人看的汇报内容**。（踩过：一个子 agent 干完全部活、
  提交都在，最终输出却整段在解释「我的输出就是返回值」，派它的人拿到一份空报告。）
- **写进文件的报告仍然要在最终输出里给出结论与路径。** 文件是详情的去处，不是
  结论的去处——派你的人先读返回值。

## 你派子 agent 时：**别转述，给路径**（所有层级都适用）

**你在派发提示里复述的每一个数字、名字、签名，都是一次可能写错的机会——
而子 agent 会把你写错的当权威，它没有第二个来源可以对照。**

- **要求写在文件里，提示里只给路径。** 「先读这份，它就是你的要求，里面的值逐字照用」。
  精确值（数量、魔法字符串、函数签名、测试用例名）**只出现在那份文件里**。
- **不要给工作流字母/任务类型加注解。** `W1`、`D-02-*` 这类代号照抄原文，
  不要写「W1 就是删」这种解释——**注解错了，子 agent 会照着错的做，而且编得过、测得绿**。
- **不要预告「什么东西预期会消失/失败」**。那等于**提前替判据签字**：真的漏删了，
  子 agent 也会当成预期之内——而且编得过、测得绿，判据唯一要守的东西就这样被
  正当化掉了。

**两个层级都踩过**：主会话给工作流代号加了错注解，三条指令方向全反；`task-agent`
复述两个函数的出现次数时名字与数值错位，实现者数对了却归因错，反过来说「计划有误」
——**计划原文是对的**。

**转述数字时要逐个对配对，不能凭顺序印象重排**——但更稳的是根本不转述。

## ⛔ 不要用 `sleep` 等任何东西

**等待的正确做法是「停下来」，不是「循环检查」。**

每次 `sleep 1; echo ok` 是一个完整工具往返，**你的全部上下文会被重发一遍**——
成本随轮数平方增长。实测有任务因此空转掉几百次工具调用，产出为零。

**你根本不需要等**：

| 情形 | 做法 |
|---|---|
| 要派多个 agent 并行 | **一条消息里发多个 Agent 调用**——它们并发执行，每个都是阻塞的，返回值一起给你。**没有「要等」这回事** |
| 等 **Agent 工具派的子 agent** | **停下来结束当前回合**。它完成时会唤醒你，上下文从会话记录重建，一个字节不丢 |
| 等 **你自己用 Bash 起的进程**（`ctest`、全量 `ninja`） | **必须前台跑（阻塞的 Bash 调用）。** 你一结束回合，你自己的会话就结束了，**后台进程跟着被杀** |
| 想看进度 | 让长命令把进度写进文件，回来一次性读。不要边跑边查 |

**全量 `ctest` 跑不完一次 Bash 超时怎么办**：`-I <起>,<止>` 分段，**每段仍然前台跑**，
段与段之间正常返回。先 `ctest -N | tail -1` 拿到总数再决定切几段。
**不要为了「跑得完」就放后台**——你一结束回合，你的会话就结束了，后台 ctest 跟着
被杀，跑了一半的结果全部作废。

**停下来等是零成本，轮询等是每轮全量重发**——这是 0 与 N² 的差别。
**写了 `sleep`/`while`/`until`/`kill -0` 的等待循环就是错的，删掉，直接结束回合。**

## 硬约束（任何会话、任何子 agent 都适用）

**下面这些约束只靠角色定义与流程结构撑着，没有脚本拦你**——没有验收脚本，也
不会有。发现某条被绕过了，说明角色定义或流程本身要补，**不是「再加一个检查」**
（那条路是无底洞，这套流水线走过一次，最后把整个判定层删掉了）。

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

> **测试全红时先看是不是环境**：`source` 掉了会让**全部测试瞬间失败**，看起来像
> 自己把什么改崩了。**全部瞬间失败 = 环境没配好，不是代码回归**——真回归不会
> 这么整齐。

四段各自防一件事，**一段都不能省**：

| 那一段 | 防什么 |
|---|---|
| `source .../krita-ci-env/env` | 前缀里的 Qt/KF5/ICU 等运行时库。**Bash 工具不跨调用保留 shell 变量**，上次 `source` 过不算数——掉了它会**全部测试瞬间全红**报 `libicui18n.so.70` 找不到 |
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

> 测试 I/O 已经改道 SSD（`~/.qttest` 是指向 `/mnt/ssd-disk/liyang/.qttest` 的符号
> 链接），**你什么都不用做**——但如果发现某批测试慢了两个数量级，先查这个链接还在不在。

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

**为什么这条写在这里**：每个任务一份 worktree，各自全量构建约 9 分钟；而各 worktree
之间**只有你 `locks` 内的几十个文件不同**，其余绝大多数源文件逐字节相同。缓存已按多
worktree 场景配好（`base_dir` + `hash_dir=false`，实测跨 worktree 命中），**漏了这两个
`-D` 就等于把别人已经编好的东西再编一遍**。

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

### ⚠ `ninja … | tail -N` 之后的 `$?` 是 **`tail` 的**退出码，不是 ninja 的

省 token 的规则要你只看结果行（`… | tail -3`），**这条与「拿退出码判构建过没过」
直接冲突**：管道的退出码默认取**最后一个命令**的。实测——

```
$ false | tail -5 ; echo $?
0                      ← 前面明明失败了
```

**于是失败的构建会报成「`ninja` 编过」**——而「编得过」是 D/S 两线用得最多的一条判据。

**两种写法都对，任选**：

```bash
ninja -C <build> 2>&1 | tail -5 ; echo "ninja exit=${PIPESTATUS[0]}"
set -o pipefail; ninja -C <build> 2>&1 | tail -5 ; echo "ninja exit=$?"
```

**同样适用于 `ctest | tail`、`grep | tail` 等任何「拿退出码当证据」的管道。**
贴进报告的那行必须是**真实的**退出码——`echo exit=$?` 紧跟在带管道的命令后面
就已经错了。

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
**不要 `git fetch --unshallow`**（9.5 GB，且决策 D-18 本来就要截断历史）。

**② 剥离期整树编译不过是正常的（决策 D-19）。**
每个 target 用 `$PK/.exec/shell/<target>/` 的独立薄壳 CMake 工程构建（I-01 产出，未完成前不存在），
**不要试图让全树 `ninja` 通过**——那要等 M5 合拢。删减期则相反，全树应始终可构建。

## 大输出别灌进上下文（所有层级都适用）

跑命令、扫全仓、读日志这类**输出量不可预测**的活，用 context-mode MCP
（`ctx_batch_execute` 批量跑 + 就地检索、`ctx_execute*` 在沙箱里算完只打结论），
不要裸 `Bash` 之后整段读回来。**你的上下文每轮全量重发**，一次几百行的 grep 输出
会在你剩下的每一个 turn 里重复计费。

写文件仍然用 `Write`/`Edit`——`ctx_execute*` 是沙箱，写不进磁盘。

## 工作方法

- 一个 target 的完整生命周期：`$PK/docs/任务做法.md` **W0**（动手前先读）
- 各类工作的做法：同文档 W1–W7
- Qt → Pk 类型对应：`$PK/docs/Qt替代品选型.md` §1（**以它为准，不许自创**）
- 该 target 的文件清单与规模：`$PK/docs/实施边界-构建目标视图.md` §6
- 该子系统的机制说明：`$PK/docs/krita/` → 边界实测：`$PK/docs/boundary-analysis/`

## 报数字必须带口径（`任务做法.md` W6）

**已知的坑列在 `$PK/CLAUDE.md`「报任何数字必须带口径」一节，开工前读那一份。**
这里**故意不复述**：删减版的副本会让你以为自己读全了，而你没有——这类副本在本项目
里已经产出过错误结论。

在 fork 里最常撞的两条，先记住：

- **一切数字现场数，别抄任何文档里的值**（含本文件）。总数随 D 线删代码持续下降。
- **对不上时先怀疑自己的口径**，不要先怀疑文档。
