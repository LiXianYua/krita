# Krita 剥 Qt 内核化 · 仓库约定

本仓库是 Krita 的 fork（基点 `v6.0.3`），目标：剥掉全部 Qt 依赖做成 C++ 绘画内核，再接 Flutter。
**完整背景在 `$PK/docs/README.md`，不要通读，按需查节。**

## 路径：先记这一条

```
PK = /mnt/ssd-disk/liyang/projects/paint_tips     ← 工作空间根
```

**下面所有 `$PK/...` 都是绝对路径，不要换成 `../`。** 原因：你很可能在 worktree 里
（`.claude/worktrees/<名>/`），相对层级跟在仓库根时不一样，写 `../docs` 会指向不存在的地方。

| 你要找 | 路径 |
|---|---|
| 任务状态 SOT | `$PK/docs/TASKS.md` |
| 你这个任务的 plan | `$PK/.exec/plans/<ID>.md` |
| 状态更新脚本 | `$PK/.exec/status.sh` |
| 决策文档 / 工作流手册 | `$PK/docs/` |
| 验收脚本（**你无权改**） | `$PK/.exec/verify/` |

## 你正在做什么

任务 ID 在启动 prompt 里给出，对应 plan 在 `$PK/.exec/plans/<ID>.md`。
**不确定该做哪个就跑 `$PK/.exec/status.sh next`**，它按依赖算，不要凭印象挑。

## 硬约束（任何会话、任何子 agent 都适用）

**禁止**：
- 修改 `$PK/docs/` 下任何文件（决策文档，只有人能改）
- 修改 `$PK/.exec/verify/` 下任何文件（判卷标准，你无权改）
- 修改 `$PK/CLAUDE.md`（工作空间级约定）与本文件
- 手改 `$PK/docs/TASKS.md`（用下面的脚本，并发手改会互相覆盖）
- 修改不属于你当前任务的 target
- 任何与当前任务无关的改动——包括"顺手"重构、改格式、修无关 bug
- 合并到 `strip-qt`（主干）或 `integration`；**不要 `git push`**
  —— 凭据虽已配好，但合并路径是 `worktree 分支 → integration → strip-qt`，最后一段只由人做

**必须**：
- 遵守 `superpowers:verification-before-completion`：声称完成前真的跑命令并确认输出
- 测试失败先用 `superpowers:systematic-debugging`，不要猜
- 判断不了的事**写 BLOCKED 停下**，不要自己拍板（见下）

## 状态怎么更新

    $PK/.exec/status.sh set   <ID> IN_PROGRESS|PENDING_VERIFY
    $PK/.exec/status.sh block <ID> "<原因>"        # 同时写进待决队列
    $PK/.exec/status.sh note  <ID> "<发现>"        # 写进 .exec/progress/<ID>.md

**`VERIFIED` 你写不进去**——只有 `$PK/.exec/verify/` 的验收脚本能置。
你自认完成时最多到 `PENDING_VERIFY`。

进度细节由 SDD 的 ledger 管（`.superpowers/sdd/<plan>/progress.md`），你不用手写。

## 什么时候必须停下（写 BLOCKED，不要自己决定）

- 需要改本任务之外的文件
- 需要改任何公开 API 的签名
- 替代品（`PkString`/`PkVector`/…）缺 API —— **不要自己往替代品里加方法**
- 判断不了某个文件是算法还是 UI
- 测试失败且 systematic-debugging 之后仍原因不明
- 分片文件清单与实际不符（说明 plan 过期）

## 这个仓库的两个特殊之处

**① 是 shallow clone（`depth=1` @ `v6.0.3`）。**
`git log` 只有一个根提交，**`git blame` 查不到 Krita 的历史**——想知道某段代码为什么这么写，
去 `$PK/docs/krita/` 的架构知识库，或 GitHub 上的 `KDE/krita`。**不要 `git fetch --unshallow`**
（9.5 GB，且 D-18 本来就要截断历史）。

**② 剥离期整树编译不过是正常的（D-19）。**
每个 target 用 `$PK/.exec/shell/<target>/` 的独立薄壳 CMake 工程构建，
**不要试图让全树 `ninja` 通过**——那要等 M5 合拢。删减期则相反，全树应始终可构建。

## 工作方法

- 一个 target 的完整生命周期：`$PK/docs/工作流手册.md` **W0**（动手前先读）
- 六类工作的做法：同文档 W1–W6
- Qt → Pk 类型对应：`$PK/docs/Qt替代品选型.md` §1（**以它为准，不许自创**）
- 该 target 的文件清单与规模：`$PK/docs/实施边界-构建目标视图.md` §6
- 该子系统的机制说明：`$PK/docs/krita/` → 边界实测：`$PK/docs/boundary-analysis/`

## 报数字必须带口径（`工作流手册.md` W6）

三个已知的坑，踩过不止一次：

- **Krita 有 42 个 `.cc` 文件**，只数 `.cpp` 会系统性低估
- **统计要排除 `tests/` 和 `benchmarks/`**（`libs/pigment/benchmarks/` 4 文件含 2 个 `Q_OBJECT`）
- 按 target 解析 `target_link_libraries` 求**传递闭包**，不要按文件 grep——一个 `CMakeLists.txt`
  常定义多个 target，且「MODULE 只链自己的 `_static` 中间层、Qt 依赖藏在中间层的 `PUBLIC`」
  这种形态有 4 例，只看 MODULE 那一行永远看不见 `kritaui`

**对不上时先怀疑自己的口径**——文档核实阶段约 80 项断言里文档只错 9 项，而核实方错了 6 次。
