# pk/concurrent —— 零 Qt 依赖的并发原语库（Q-8 的自写替代）

零 Qt 依赖的 C++17 并发原语：`PkMutex`/`PkMutexLocker`（互斥锁）、
`PkReadWriteLock`/`PkReadLocker`/`PkWriteLocker`（读写锁）、`PkAtomicInt`/`PkAtomicPointer<T>`（原子数）、
`PkThread::idealThreadCount()`（线程数查询）、`PkThreadPool`（线程池）、`PkRunnable`（可运行任务）、
`PkWaitCondition`（条件变量）、`PkSemaphore`（信号量）。
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
| 13 | `QThread::moveToThread()` + 事件循环残余 | ~196 处（24+172） | **不交付，归后续任务** | 见下表缺口登记 |

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
| `compat/Q*` 垫片 | `compat/` 目录 | `<QMutex>` / `<QReadWriteLock>` / `<QAtomicInt>` 等头文件 |

## 2. 内存序判读表

**来源：`docs/Qt替代品选型.md` §6.8 的 Q-8 分析 + Task 2 实测判读**

| 方法 | 调用点示例 | 语义需求 | 实现内存序 | 判据 |
|---|---|---|---|---|
| `operator int()` (implicit load) | `if (m_frameSize > ...)`（kis_lockless_stack.h） | 非同步化场景，放宽 | `memory_order_relaxed` | 隐式读，无明确同步 |
| `operator=()` (implicit store) | `m_bufferIndex = 0`（kis_lockless_stack.h） | 非同步化场景，放宽 | `memory_order_relaxed` | 隐式写，无明确同步 |
| `ref()` / `deref()` | `if (m_refCount.deref())`（KisGlobalResourcesInterface）| Qt 语义：是否还有其他引用者，读后判断需相对强序 | `memory_order_relaxed` | Qt 默认行为；无跨线程 sync-before 需求 |
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
- `memory_order_relaxed` 的调用点（`ref()`/`deref()`/隐式 load/store）：保留范围内没有跨线程 happens-before 需求，只是原子性（防 data race）
- `*Ordered()` 系列（显式方法名）：调用点自己明确要求全同步化，不因作用域降序
- `PkWaitCondition`/`PkThreadPool`/`PkSemaphore` 内部的内存序由 std 库保证，不需额外判读

## 3. 偏离清单（对齐口径下逐条登记）

1. **`QThread::moveToThread()` / `deleteLater()` / `QObject::thread()`**：
   见下表缺口登记的理由。

2. **事件循环残余（`QTimer`/`QCoreApplication`/`processEvents` 等）**：
   见下表缺口登记的理由。

> `QReadWriteLock::tryLockForRead()`/`tryLockForWrite()` 此前在这里被记成
> "零调用点、不实现"——final review 核实这是假前提（`libs/image/tiles3` 三处
> 真实调用点），已补回实现，不再是偏离项，见 §1 表格第 3 行。

## 4. 缺口登记表（两个大项，建议归口）

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

**`deleteLater()` 不在本次交付范围**——它需要"延迟删除队列"的 flush 时机，
与"172 处事件循环残余"里的架构方向是同一个问题（见 §4.2），R-24 没有实现，
`PkObject` 目前没有 `deleteLater()` 方法；这 24 处 `moveToThread` 调用点本身
不依赖 `deleteLater()`（`moveToThread` 只是打线程标记，`deleteLater()` 是
另一个独立方法）。

### 4.2 事件循环残余（~172 处，架构待拍板）

| 缺口 | 数字 | 示例位置 | 理由 | 建议归口 |
|---|---|---|---|---|
| `QTimer` 计时器投递 | ~40 处 / ~20 文件 | kis_signal_compressor.h（信号节流）等 | 需要事件循环的周期投递；与 `PkObject` 生命周期耦合 | 与 moveToThread 同批处理；需先拍板"Pk 世界的事件循环模型" |
| `QCoreApplication::instance()` 应用对象 | ~25 处 | kis_base_node.cpp 等 | 应用全局状态查询；没有 Pk 替代品 | 同上 |
| `deleteLater()` 延迟删除 | ~50 处 | 信号投递与生命周期管理 | 依赖事件循环的后续投递；与 `moveToThread` 强耦合 | 同上 |
| `processEvents()` 事件处理 | ~20 处 | kis_stroke.cpp 等阻塞等待 | 手工事件循环；无事件循环世界等价物 | 同上 |
| `postEvent()` / `QEventLoop` | ~37 处 | 信号投递与任务调度 | 跨线程投递归 Q-8（`pk/signal` §1 Row 9 已决议退化为 Direct） | 同上 |

⚠ **投递不等于执行**：`PkThreadCallQueue::post()`/`postBlocking()` 投递到某个
线程的调用不会自动执行，该线程必须自己调用 `processPendingCalls()`（或未来
某个封装它的机制）来抽干队列——不这么做，投递的调用会永远停在队列里，不
报错、不崩溃、不打日志，是一个纯静默的行为缺失（final whole-branch review
I-3）。全仓 `pk/` 之外零 pump 调用点，第一个跨线程投递的消费方必须自己在
目标线程装 pump。

**统计口径**：按 `docs/Qt替代品选型.md` §6.8 的原始扫描结果；合计约 172 处（按 Q-8 原始表）。
`Qt替代品选型.md` §6.8 正文（R37 复核后）实际写的是 95 处，两个数字口径不同
（172 是复核前的旧数字），这条差异已经在 R-24 plan 的「待报告给主会话的事实
差异」一节登记，这里不裁决哪个对，只提醒读者别直接拿 172 当权威数字用
（final whole-branch review M-6）。

**为何不在 R-10 实现**：
- 核心方法（尤其 `deleteLater()`）与 `PkObject` 生命周期强耦合，前置同上
- "要不要有事件循环"是一个**跨 R-05/R-10 的架构决策**（不只是"实现细节"）
- `S-00` 已交接这个问题、明确注记"跨 R-05/R-10 才能回答"
- 本任务**不拍板**这个架构问题，只如实登记；**建议主会话在收到 Task 5 报告后判断是否单独发起一次决策讨论**
- 这不是"技术停工"，而是"超范围的架构问题"

**架构方向（R-24 交接，供后续任务参考，不代表已实现）**：R-24 摸清 24 处
`moveToThread` + 8 处真实 `Queued`/`BlockingQueued` connect 调用点后确认：
**这批残余不需要 `QThread::exec()`/`QCoreApplication` 式的隐式事件循环**。
`PkThreadCallQueue`（R-24 交付）已经是"投递到指定线程执行"的完整最小原语
——`QTimer` 的落点可以是"定时调用 `PkThreadCallQueue::post()`"而不需要
新造一层事件系统；`deleteLater()` 的落点是"把 delete 操作 post 进对象所在
线程的队列"；`processEvents()`/`QEventLoop` 的落点是直接调
`PkThreadCallQueue::processPendingCalls()`（如果调用方需要"阻塞直到某个
条件达成才返回"的 `QEventLoop::exec()` 语义，需要在 `processPendingCalls()`
基础上加一层循环+条件判断，不需要动 `PkThreadCallQueue` 本身）；
`QCoreApplication::instance()`/`qApp` 的落点是 `PkThread::mainThreadId()`
（R-24 交付，调用方需要先在真正的程序入口调一次
`PkThread::registerMainThread()`，这一步本身仍然是"要不要有一个 Pk 应用
对象"这个更大问题的一部分，R-24 不越权回答，只确认底层原语已经够用）。
**这不是"架构问题已解决"，是"实现这批残余不再需要新的核心原语，可以直接
消费 R-24 的交付物"**——具体每一处怎么改仍然是各 S 批次替换调用点时的事。

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
