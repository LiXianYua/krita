# pk/concurrent —— 零 Qt 依赖的并发原语库（Q-8 的自写替代）

`PkTimer` 的可 join 等待线程只负责判断正间隔的到期时机，回调一律 post 到指定线程并等显式 pump；`stop()`/析构会 join 并使已投递回调失效，不使用 detached 线程。负间隔按零间隔处理；重复零间隔定时器始终至多保留一个排队回调，只有目标线程 pump 并交付当前回调后才重新投递，因此每次 pump 快照至多触发一次且不会形成无界生产循环。

零 Qt 依赖的 C++17 并发原语：`PkMutex`/`PkMutexLocker`（互斥锁）、
`PkReadWriteLock`/`PkReadLocker`/`PkWriteLocker`（读写锁）、`PkAtomicInt`/`PkAtomicPointer<T>`（原子数）、
`PkThread::idealThreadCount()`（线程数查询）、`PkThreadPool`（线程池）、`PkRunnable`（可运行任务）、
`PkWaitCondition`（条件变量）、`PkSemaphore`（信号量）。
`PkThreadCallQueue::warmUpCurrentThread()` 把队列预热与线程 id 获取合成一个
有顺序的操作：目标线程只发布其返回值，不得先发布 `currentThreadId()`。
`PkEventLoop::processEvents()` 只处理入口时的队列快照；
`PkEventLoop::execUntil()` 是调用者线程上的显式 pump + 谓词循环，没有后台
线程或隐式事件循环。
独立 `project(pkconcurrent)` 薄壳工程，不接入 Krita 主构建。

范围上界 = `docs/Qt替代品选型.md` §6.8 的 Q-8 用量表 + 现场修正（下表逐项对照）。
唯一机器锁：`pk/concurrent`。

## 1. API 清单表（来源：`docs/Qt替代品选型.md` §6.8 + 现场实测口径）

| # | Qt 用法 | §6.8 用量 | R-10 交付 | 备注 |
|---|---|---|---|---|
| 1 | `QMutex` 互斥锁 | 60 处 | `PkMutex::lock()` / `unlock()` / `try_lock()` / `tryLock()` | STL Lockable 概念兼容（`try_lock()`）+ Qt 驼峰式转发（`tryLock()`，final review C1c 补回，`libs/image/kis_image_animation_interface.cpp:499` 真实调用点）；Recursive 模式零调用点 |
| 2 | `QMutexLocker` 互斥锁 RAII | 39 处 | `PkMutexLocker`（构造/析构/`unlock()`/`relock()`/`mutex()`） | RAII；`mutex()` 供 `PkWaitCondition::wait()` 场景 |
| 3 | `QReadWriteLock` 读写锁 | 13 处 | `PkReadWriteLock::lockForRead()`/`lockForWrite()`/`tryLockForRead()`/`tryLockForWrite()`/`unlock()`；构造接受 `RecursionMode`（`NonRecursive`/`Recursive`，仅前者有真实调用点） | 二者互斥；final review 更正：`tryLockFor*()` 此前误判为零调用点被删，已补回（`libs/image/tiles3` 三处） |
| 4 | `QReadLocker` 读锁 RAII | 8 处 | `PkReadLocker`（构造/析构/`unlock()`/`relock()`/`readWriteLock()`） | RAII；`readWriteLock()` 供 relock-upgrade 模式 |
| 5 | `QWriteLocker` 写锁 RAII | 6 处 | `PkWriteLocker`（构造/析构/`unlock()`/`relock()`/`readWriteLock()`） | RAII；同上 |
| 6 | `QAtomicInt` 原子整数 | 30 处 | `PkAtomicInt::operator int()` / `operator=()` / `ref()` / `deref()` / `fetchAndAddOrdered()` / `fetchAndStoreOrdered()` / `testAndSetOrdered()` / `loadAcquire()` / `storeRelease()` / `loadRelaxed()` / `storeRelaxed()` / `fetchAndAddRelaxed()` / `fetchAndAddAcquire()` | 内存序判读见下表；ref/deref 返回"新值非零"语义。后 6 个显式内存序方法系 final review I1 补齐 |
| 7 | `QAtomicPointer<T>` 原子指针 | 15 处 | `PkAtomicPointer<T>::operator T*()` / `operator->()` / `operator=()` / `fetchAndStoreOrdered()` / `testAndSetOrdered()` / `loadAcquire()` / `storeRelease()` / `loadRelaxed()` / `storeRelaxed()` / `fetchAndAddRelaxed()` / `fetchAndAddAcquire()` | 内存序同上；后 6 个方法同 final review I1 补齐 |
| 8 | `QThread::idealThreadCount()` | 2 处 | `PkThread::idealThreadCount()` | 静态方法 |
| 9 | `QThreadPool` 线程池 | 5 处 | `PkThreadPool::start()` / `setMaxThreadCount()` / `maxThreadCount()` / `waitForDone()` | 通用固定线程数池；排队式 |
| 10 | `QRunnable` 可运行任务 | 3 处 | `PkRunnable::run()` / `setAutoDelete()` / `autoDelete()` | 虚方法；`QThreadPool::start()` 接收其指针 |
| 11 | `QWaitCondition` 条件变量 | 3 处 | `PkWaitCondition::wait(PkMutex*)` / `wakeOne()` / `wakeAll()` | Qt 语义：`wait()` 内解锁-等待-重新加锁 |
| 12 | `QSemaphore` 信号量 | 3 处 | `PkSemaphore::acquire()` / `tryAcquire(int,int)` / `release()` | C++17 自写（std::counting_semaphore 是 C++20） |
| 13 | `QThread::moveToThread()` + 事件循环残余 | R-30 现场重数见任务 SOT | `PkThreadCallQueue` + `PkEventLoop` + `PkTimer`；对象亲和与 `deleteLater()` 在 `pk/signal` | 显式 pump，不提供隐式事件循环；真实调用点迁移归 S 批次 |

### 交付的公开类型与入口

| 名字 | 文件 | 替代什么 |
|---|---|---|
| `PkMutex` | `PkMutex.h` | `QMutex` 互斥锁 |
| `PkMutexLocker` | `PkMutex.h` | `QMutexLocker` 互斥锁 RAII |
| `PkReadWriteLock` | `PkReadWriteLock.h` | `QReadWriteLock` 读写锁 |
| `PkReadLocker` | `PkReadWriteLock.h` | `QReadLocker` 读锁 RAII |
| `PkWriteLocker` | `PkReadWriteLock.h` | `QWriteLocker` 写锁 RAII |
| `PkAtomicInt` | `PkAtomic.h` | `QAtomicInt` 原子整数 |
| `PkAtomicPointer<T>` | `PkAtomic.h` | `QAtomicPointer<T>` 原子指针 |
| `PkThread` | `PkThread.h` | `QThread` 的静态方法部分 |
| `PkThreadPool` | `PkThreadPool.h` | `QThreadPool` 线程池 |
| `PkRunnable` | `PkRunnable.h` | `QRunnable` 可运行任务 |
| `PkWaitCondition` | `PkWaitCondition.h` | `QWaitCondition` 条件变量 |
| `PkSemaphore` | `PkSemaphore.h` | `QSemaphore` 信号量 |
| `PkThreadCallQueue` | `PkThreadCallQueue.h` | 跨线程排队 + 目标线程显式 pump |
| `PkEventLoop` | `PkEventLoop.h` | `processEvents()` / 条件式 `execUntil()` 的薄封装 |
| `PkTimer` | `PkTimer.h` | 到期后向指定线程 post 的显式-pump 定时器 |
| `compat/Q*` 垫片 | `compat/` 目录 | `<QMutex>` / `<QReadWriteLock>` / `<QAtomicInt>` 等头文件 |

## 2. 内存序判读表

**来源：`docs/Qt替代品选型.md` §6.8 的 Q-8 分析 + Task 2 实测判读**

| 方法 | 调用点示例 | 语义需求 | 实现内存序 | 判据 |
|---|---|---|---|---|
| `operator int()` (implicit load) | `if (m_frameSize > ...)`（kis_lockless_stack.h） | Qt 5.15 implicit load 默认 acquire | `memory_order_acquire` | Qt 头文件 `loadAcquire()` 映射 |
| `operator=()` (implicit store) | `m_bufferIndex = 0`（kis_lockless_stack.h） | Qt 5.15 implicit store 默认 release | `memory_order_release` | Qt 头文件 `storeRelease()` 映射 |
| `ref()` / `deref()` | `if (m_refCount.deref())`（KisGlobalResourcesInterface）| Qt 5.15 默认调用 ordered ref/deref | `memory_order_seq_cst` | Qt 头文件 `ref()`/`deref()` 映射到 ordered |
| `fetchAndAddOrdered()` | `m_refCount.fetchAndAddOrdered(-1)`（kis_stress_job.cpp） | 明确名字"Ordered" → 全同步化（seq_cst）| `memory_order_seq_cst` | 调用点显式要求 |
| `fetchAndStoreOrdered()` | `old = m_p.fetchAndStoreOrdered(nullptr)`（kis_lockless_stack.h） | 明确名字"Ordered" → 全同步化（seq_cst） | `memory_order_seq_cst` | 调用点显式要求 |
| `testAndSetOrdered()` | `while (!m_p.testAndSetOrdered(nullptr, new))`（kis_lockless_stack.h） | CAS 循环同步需求强，用 seq_cst | `memory_order_seq_cst` | 调用点显式要求；CAS 强序 |
| `loadAcquire()` | `.loadAcquire()`（qsbr.h、kis_tile_data_store.h 等，final review I1） | 方法名直接点名 acquire | `memory_order_acquire` | 方法名即语义要求，机械映射 |
| `storeRelease()` | `.storeRelease()`（KisTiledExtentManager.cpp 等，final review I1） | 方法名直接点名 release | `memory_order_release` | 同上 |
| `loadRelaxed()` | `.loadRelaxed()`（KisTiledExtentManager.cpp 等，final review I1） | 方法名直接点名 relaxed | `memory_order_relaxed` | 同上 |
| `storeRelaxed()` | `.storeRelaxed()`（KisTiledExtentManager.cpp、KisLazySharedCacheStorage.h:170 等，final review I1） | 方法名直接点名 relaxed | `memory_order_relaxed` | 同上 |
| `fetchAndAddRelaxed()` | `.fetchAndAddRelaxed()`（KisTiledExtentManager.cpp，final review I1） | 方法名直接点名 relaxed；返回旧值同 fetchAndAddOrdered 约定 | `memory_order_relaxed` | 同上 |
| `fetchAndAddAcquire()` | `.fetchAndAddAcquire()`（KisTiledExtentManager.cpp，final review I1） | 方法名直接点名 acquire；返回旧值同 fetchAndAddOrdered 约定 | `memory_order_acquire` | 同上 |
| `PkWaitCondition::wait()` | 互斥锁保护的获取点 | 条件变量语义要求 acquire（入）release（出） | std::condition_variable_any（隐含） | std::库保证 |
| `PkThreadPool` 队列 push/pop | 任务投递与获取 | 队列同步需 acquire-release | std::deque<> + std::mutex（隐含） | std::库保证 |
| `PkSemaphore::tryAcquire()` | 计数等待 | 信号量同步需全序 | std::condition_variable_any（隐含）| std::库保证 |

**口径说明**：
- Qt 默认隐式读/写与引用计数不能按调用点猜测放宽：分别固定为 acquire、release 与 seq_cst。
- `*Ordered()` 系列（显式方法名）：调用点自己明确要求全同步化，不因作用域降序
- `PkWaitCondition`/`PkThreadPool`/`PkSemaphore` 内部的内存序由 std 库保证，不需额外判读

## 3. 偏离清单（对齐口径下逐条登记）

1. **不提供 Qt 式隐式事件循环或应用对象**：R-30 只交付目标线程显式 pump、
   条件式循环、timer-to-post 与 deferred deletion；消费方必须安装 pump。

> `QReadWriteLock::tryLockForRead()`/`tryLockForWrite()` 此前在这里被记成
> "零调用点、不实现"——final review 核实这是假前提（`libs/image/tiles3` 三处
> 真实调用点），已补回实现，不再是偏离项，见 §1 表格第 3 行。

## 4. 事件投递与生命周期交付记录

### 4.1 `moveToThread()` 及相关（24 处）—— R-24 已交付

**R-24（`pk/signal`+`pk/concurrent` 双锁任务）已交付**：`PkObject::thread()`/
`moveToThread(PkThreadId)`（`pk/signal/PkObject.h`，方法体见 R-24 Task 2）+
跨线程投递原语 `PkThreadCallQueue`（`pk/concurrent/PkThreadCallQueue.h`，
R-24 Task 1）+ `activateSignal` 按连接类型真实分派（R-24 Task 3，Queued/
BlockingQueued 不再退化为 Direct）。

24 处真实调用点的编译试接结论（零改动，`-fsyntax-only`，见
`pk/concurrent/graft/graft_check.sh` 的 `check_movetothread`）：

| 文件 | 结论 |
|---|---|
| `libs/canvas/kis_gui_context_command.cpp` | 卡住：`kundo2command.h` 缺失（本任务未交付） |
| `libs/canvas/opengl/kis_texture_tile_info_pool.h` | 卡住：`boost/pool/pool.hpp` 缺失（boost 依赖未端口化） |
| `libs/flake/flake/kis_shape_layer.cc` | 卡住：`kritaflake_export.h` 缺失（导出宏头未生成） |
| `libs/flake/flake/kis_shape_selection.cpp` | 卡住：`QPainterPath` 未声明（Qt 几何类型未端口化） |
| `libs/flake/KoShapeManager.cpp` | 卡住：`QList` 未声明（Qt 容器类型未端口化） |
| `libs/global/KisDeleteLaterWrapper.cpp` | 卡住：`kritaglobal_export.h` 缺失（导出宏头未生成） |
| `libs/global/kis_thread_safe_signal_compressor.cpp` | 卡住：`kritaglobal_export.h` 缺失（导出宏头未生成） |
| `libs/image/kis_image.cc` | 卡住：`QString` 未声明（Qt 基础类型未端口化） |
| `libs/image/kis_memory_statistics_server.cpp` | 卡住：`QtGlobal` 头缺失 |
| `libs/image/kis_node.cpp` | 卡住：`QVector` 未声明（Qt 容器类型未端口化） |
| `libs/image/kis_processing_visitor.cpp` | 卡住：`kritaimage_export.h` 缺失（导出宏头未生成） |
| `libs/image/KisSafeBlockingQueueConnectionProxy.cpp` | 卡住：`QQueue` 未声明（Qt 容器类型未端口化） |
| `libs/image/KisSafeNodeProjectionStore.cpp` | 卡住：`QScopedPointer` 未声明（Qt 智能指针未端口化） |
| `libs/image/KisSelectionUpdateCompressor.cpp` | 卡住：`kritaimage_export.h` 缺失（导出宏头未生成） |
| `libs/image/lazybrush/kis_colorize_mask.cpp` | 卡住：`QScopedPointer` 未声明（Qt 智能指针未端口化） |
| `libs/impex/KisCloneDocumentStroke.cpp` | 卡住：`kritaimage_export.h` 缺失（导出宏头未生成） |

**汇总**：16/16 卡住，0/16 编到底，全部卡在 `moveToThread` 那一行**之前**——
一次都没有触发过 `qApp`/`QApplication::instance()`/`QCoreApplication::instance()`
这几个符号本身的解析问题（详见 R-24 Task 4 报告 Step 2 的桩验证）。卡点归类
（每个文件按第一条 `fatal error` 只归一类）：导出宏头缺失 6 个、Qt 容器/基础
类型未端口化 8 个、其他未交付依赖（`kundo2command.h`、boost）2 个。

**R-30 已补齐 `PkObject::deleteLater()`**：删除操作 post 到对象亲和线程，
重复请求合并；父对象先析构子对象时，排队删除安全失效。执行仍依赖目标线程 pump。

### 4.2 事件循环残余（R-30 已交付 pk 侧落点）

| 缺口 | 数字 | 示例位置 | 理由 | 建议归口 |
|---|---|---|---|---|
| `QTimer` 计时器投递 | 见 R-30 SOT | kis_signal_compressor.h（信号节流）等 | `PkTimer` 到期后 post，回调只在目标线程 pump 时执行 | S 批次替换调用点 |
| `QCoreApplication::instance()` 应用对象 | 见 R-30 SOT | 应用全局状态查询 | 线程身份用 `PkThread::mainThreadId()`；不建 Pk 应用对象 | S 批次按真实用途拆解 |
| `deleteLater()` 延迟删除 | 见 R-30 SOT | 信号投递与生命周期管理 | `PkObject::deleteLater()` | S 批次替换调用点 |
| `processEvents()` 事件处理 | 见 R-30 SOT | 阻塞等待 | `PkEventLoop::processEvents()` / `execUntil()` | S 批次替换调用点 |
| `postEvent()` / `QEventLoop` | 见 R-30 SOT | 信号投递与任务调度 | `PkThreadCallQueue` + `PkEventLoop` | S 批次替换调用点 |

⚠ **投递不等于执行**：`PkThreadCallQueue::post()`/`postBlocking()` 投递到某个
线程的调用不会自动执行，该线程必须自己调用
`PkThreadCallQueue::processPendingCalls()`，或调用 R-30 已交付的
`PkEventLoop::processEvents()` / `PkEventLoop::execUntil()` 来抽干队列——
不这么做，投递的调用会永远停在队列里，不
报错、不崩溃、不打日志，是一个纯静默的行为缺失（final whole-branch review
I-3）。全仓 `pk/` 之外零 pump 调用点，第一个跨线程投递的消费方必须自己在
目标线程装 pump。

⚠ **目标线程发布自己的线程 id 之前要先"预热"**：目标线程第一次调用
`processPendingCalls()` 时，会把它自己 id 名下、此刻已经排队的全部条目
原样丢弃（不执行）——这是识别"线程 id 被 OS 复用、队列里可能是上一个用过
这个 id 的线程留下的陈旧调用"的唯一手段，但这个判定天生分不清"陈旧调用"
和"合法投给我、只是我还没来得及第一次 pump"。结果：任何人在目标线程第一次
`processPendingCalls()` 之前投给它的调用，都有被当成陈旧条目一并丢弃的
风险——**不是必然发生，是一个启动期竞态**（final whole-branch review
NEW-I2；本仓 `pk/concurrent`/`pk/signal` 两边的既有跨线程测试基本靠这条预热
规避——其中一个是靠同一可执行文件里更早跑过的另一个测试顺带完成预热，不是
自己独立预热，final whole-branch review round 2 re-review 指出过这条不够
严谨的地方）。R-30 已把通用写法收敛为
`PkThreadCallQueue::warmUpCurrentThread()`：目标线程只发布这个 API 的返回值，
不得先发布 `PkThread::currentThreadId()` 再补 pump。不预热的后果
因入口不同而不同：`post()` 投的调用被丢弃时是静默降级，不报错、不崩溃；
`postBlocking()` 投的调用**在目标线程第一次触达队列系统时被当成陈旧条目
丢弃**，或者**目标线程至少 pump 过一次之后正常退出**，这两种情况下发射
线程都会被正常唤醒（不会永久挂起）、收到 `PkCallAbandonedException`
（final whole-branch review NEW-C1）。**但目标线程如果从头到尾一次都没
调用过 `processPendingCalls()`/`post()`/`pendingCount()` 任何入口就退出，
`postBlocking()` 仍然会永久挂起发射线程**——这与 Qt 同线程
`BlockingQueuedConnection` 死锁是同一类已接受风险，本原语同样不做防护，
"预热"因此不是可选的性能优化，是避免这条永久挂起路径的唯一手段。详见
`PkThreadCallQueue.h` 类头注释。

**统计口径（已裁决，2026-08-18，主会话）**：**95 处是权威数字**——
`docs/Qt替代品选型.md` §6.8 正文（R37 复核后）的结论，早先"172"是复核前的
旧数字，出自本表下面按类目分列的粗估（`QTimer` ~40 + `QCoreApplication`
~25 + `deleteLater` ~50 + `processEvents` ~20 + `postEvent`/`QEventLoop`
~37），跟 95 的分类口径不是一一对应（95 的分类是 `QTimer` 25 /
`QCoreApplication` 42 / `deleteLater` 15 / `processEvents` 8 / `postEvent`
4 / `QEventLoop` 1）。**下面按类目的分列数字尚未按 95 这个基线重新核实**，
谁先碰这批残余谁现场重数，不要直接拿本表任何一行当权威值用
（final whole-branch review M-6 的后续裁决）。

**R-10 当时未实现的原因（历史背景）**：
- 核心方法（尤其 `deleteLater()`）与 `PkObject` 生命周期强耦合，超出 R-10 前置与锁范围。
- "要不要有事件循环"当时是跨 R-05/R-10 的架构决策，`S-00` 因此把它
  交接给后续任务，而非由 R-10 越界拍板。
- 后续 R-24 确认显式 pump 架构方向，R-30 再交付下文列出的薄封装；
  这个历史阻塞已解除。

**架构方向（R-24 历史交接与 R-30 交付）**：R-24 摸清 24 处
`moveToThread` + 8 处真实 `Queued`/`BlockingQueued` connect 调用点后确认：
**这批残余不需要 `QThread::exec()`/`QCoreApplication` 式的隐式事件循环**。
`PkThreadCallQueue`（R-24 交付）已经是"投递到指定线程执行"的完整最小原语。
R-30 按这个方向交付了具体封装：`PkTimer::start()` 在到期后向目标线程
post 回调，`PkTimer::stop()`/析构会取消并 join；
`PkObject::deleteLater()` 把删除操作 post 进对象亲和线程的队列；
`PkEventLoop::processEvents()` 处理入口时队列快照，
`PkEventLoop::execUntil(const std::function<bool()>&)` 在调用者线程上持续 pump
直到谓词成立。这些 API 均不启动隐式事件循环，消费方仍必须显式 pump。
`QCoreApplication::instance()`/`qApp` 的落点是 `PkThread::mainThreadId()`
（R-24 交付，调用方需要先在真正的程序入口调一次
`PkThread::registerMainThread()`，这一步本身仍然是"要不要有一个 Pk 应用
对象"这个更大问题的一部分，R-24 不越权回答，只确认底层原语已经够用）。
R-30 因此已解决 pk 侧核心原语与薄封装的交付；具体每一处 Krita
消费点怎么改，仍然是各 S 批次替换调用点时的事。

### 4.3 Task 5 编译试接补充：`kis_updater_context.h` 预期失败细节

`pk/concurrent/graft/graft_check.sh` 新增编译试接 `libs/image/kis_updater_context.h`（Task 1/2 
遗留的次要目标），预期失败。卡在第一个依赖未剥离的类型：

```
In file included from libs/image/kis_updater_context.h:15:
libs/image/kis_base_rects_walker.h:10:10: fatal error: QStack: 没有那个文件或目录
```

**原因**：`kis_base_rects_walker.h` 依赖 `<QStack>`（来自 Qt），而该类型的替代品（`PkStack` 或等价物）
是 S 批次的事。这个 header chain 卡在第一个 Qt 未剥离类型即终止，符合"预期内的编译不通"的特征。

具体解析链路：
- `kis_updater_context.h` → `kis_base_rects_walker.h` → `<QStack>` ✗

后续任务需要实现或提供 `kis_base_rects_walker.h`、`kis_async_merger.h`、`kis_update_scheduler.h` 
等该 header 的传递依赖，才能完成 `kis_updater_context` 的剥离试接。

## 5. 用法示例

```cpp
#include <QMutex>              // → compat/QMutex（PkMutex）
#include <QReadWriteLock>      // → compat/QReadWriteLock（PkReadWriteLock）
#include <QAtomicInt>          // → compat/QAtomicInt（PkAtomicInt）
#include <QThreadPool>         // → compat/QThreadPool（PkThreadPool）

// 互斥锁与 RAII
PkMutex m;
{
    PkMutexLocker lock(&m);    // 自动加锁
    // 临界区
}                              // 自动解锁

// 读写锁
PkReadWriteLock rwLock;
{
    PkReadLocker reader(&rwLock);  // 读锁 RAII
    // 只读操作
}

{
    PkWriteLocker writer(&rwLock); // 写锁 RAII
    // 修改操作
}

// 原子操作
PkAtomicInt refCount(1);
if (refCount.deref()) {        // 返回"新值非零"
    // 还有其他引用者
}
int old = refCount.fetchAndAddOrdered(-1);  // seq_cst 语义

// 线程池 + 可运行任务
class MyTask : public PkRunnable {
public:
    void run() override {
        // 任务实现
    }
};

PkThreadPool pool(4);
pool.start(new MyTask());
pool.waitForDone();

// 条件变量
PkMutex mutex;
PkWaitCondition cond;
{
    PkMutexLocker lock(&mutex);
    cond.wait(&mutex);         // 内部解锁-等待-重新加锁
    cond.wakeAll();            // 唤醒所有等待者
}

// 信号量
PkSemaphore sem(0);
sem.acquire();                 // 阻塞直到计数>0，计数-1
sem.release();                 // 计数+1，唤醒等待者
```

## 6. 怎么跑

```bash
# 全量单测（CMake + Ninja + ccache，46 个测试函数——PkTest 报 56 条通过，
# 含 mutex/rwlock/atomic/threadpool/semaphore 五套各 1 对 initTestCase/
# cleanupTestCase 共 10 条）
cd pk/concurrent && rm -rf build && cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache && \
  cmake --build build && ./build/test_pkconcurrent

# 真实 Krita 测试类试接（判据②，中心判据）
# kis_lockless_stack_test.cpp：测 QMutex/QAtomicInt/QAtomicPointer/QThreadPool/QRunnable 五类
./pk/concurrent/graft/graft_check.sh

# 判据③：替代品本体不得有 Qt 未定义符号
# R-10 final review I3 修正：裸 `nm -u | grep -i qt` 对 Itanium 名字修饰后的
# 符号基本失效（QString::number(int) 修饰后是 _ZN7QString6numberEi，小写化
# 不含 "qt" 子串）。改用 -C 反修饰 + 匹配 Qt 类名/命名空间的实际形状。
cd pk/concurrent
QTSYM='(^|[^[:alnum:]_])(Q[A-Z][A-Za-z0-9_]*|qt_|Qt::)'
nm -uC build/libpkconcurrent.a 2>/dev/null | grep -E "$QTSYM" || echo "libpkconcurrent.a 无 Qt 未定义符号"
nm -uC build/test_pkconcurrent 2>/dev/null | grep -E "$QTSYM" || echo "test_pkconcurrent 无 Qt 未定义符号"
```

## 7. 试接中修掉的缺陷（Task 系列实录）

各 Task 试接或实现时发现并修掉的库缺陷：

1. **Task 1 (PkReadWriteLock)**：`tryLockForRead()`/`tryLockForWrite()` 删除又补回
   - Task 1 review 阶段以"保留范围内零调用点"为由删除
   - **final review 更正**：该前提是假的——`libs/image/tiles3` 有 3 处真实调用点
     （`kis_tile_data_store.cc:269`、`kis_tile_data.cc:210`、
     `kis_tile_hash_table_p.h:423`），已补回实现，见 `PkReadWriteLock.h`

2. **Task 2 (PkAtomicInt/PkAtomicPointer)**：`ref()`/`deref()` 语义修正
   - 真实调用点 `if (refCount.deref())` 需要"新值是否非零"，不是"是否成功"
   - `pk/concurrent/PkAtomic.h` 注释已明文

3. **Task 3 (PkThreadPool/PkRunnable)**：线程池排队模型确认
   - 实测调用方 `KisUpdaterContext` 自己管理"最多 N 个并发"，线程池不绑定 1:1 槽位
   - 因此实现为通用排队式，不是"槽位恒等线程数"的模型

4. **Task 4 (PkSemaphore 试接)**：`tryAcquire()` 超时语义
   - `kis_updater_context.cpp:222` 唯一调用点 `tryAcquire(1, -1)` 中 `-1` = "无限等待"
   - 其余 `-1`/`0`/正整数 三种取值境界均有覆盖
