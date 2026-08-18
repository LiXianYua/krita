# pk/signal —— 自写信号槽库（Q-1 的自写替代）

零 Qt 依赖的 C++17 信号槽：`PkObject`（父子树 + 生命周期 + 编译期 connect/emit/disconnect）、
`PkConnection`（连接句柄）、`PkPointer`（弱引用防悬垂）、`compat/QObject`（一字不改的
compat 垫片）+ `pk_signal_moc.py`（替代 moc 的信号定义生成器）。独立
`project(pksignal)` 薄壳工程，不接入 Krita 主构建。

范围上界 = `docs/Qt替代品选型.md` §6.1 的 Q-1 用量表 + 现场修正（下表逐项对照）。
唯一机器锁：`pk/signal`。

## 1. API 清单表（来源：`docs/Qt替代品选型.md` §6.1 + 现场实测口径）

| # | Qt 用法 | §6.1 用量 | R-05 交付 | 备注 |
|---|---|---|---|---|
| 1 | 老式 `SIGNAL()/SLOT()` 宏 | 1 530 处（90%） | **不交付，归 S 批次** | 覆盖 473 文件里绝大多数调用点；正式全仓转换归 S 批次，试接只用脚手架 `graft/rename_extra.sed`（见 §5）。任何替代方案都要求先把它们改成编译期形式，这一步是 S 批次的地毯式 sed，不在 `pk/signal` 锁内 |
| 2 | 新式 `&Class::method` 函数指针 | 166 处 | `PkObject::connect(sender, &S::sig, receiver, &R::slot, type)` | 成员函数指针→成员函数指针重载 |
| 3 | lambda 作为槽 | 13 处 | `PkObject::connect(sender, &S::sig, receiver, lambda, type)` | 成员函数指针→lambda 重载；receiver 仅用于生命周期绑定 |
| 4 | `sender()` | 12 处 | `PkObject::sender()`（static） | thread_local 发射栈，嵌套 emit 返回最内层 sender |
| 5 | `QMetaObject::invokeMethod` | 0 处 | 不实现 | 零调用点不预先实现（线级 spec 判据①） |
| 6 | `disconnect()` | 100 处 / 29 文件 | `PkObject::disconnect(PkConnection&)`（句柄式）<br>`PkObject::disconnect(sender, &S::sig, receiver, &R::slot)`（4 参函数指针式）<br>`PkObject::disconnect(sender, 0, receiver, 0)`（断开全部式） | 保留范围实测 `disconnect(` 90 处 = 句柄式 19 + 4 参函数指针式 4 + 断开全部式 41 + 老式字符串 26 + 注释 2；三种新式形态全部交付 |
| 7 | `new X(this)`（父子树托管） | 182 处 | `PkObject(PkObject* parent)` + 析构托管 | FIFO 析构顺序（实测探针 1：c1→c2→c3，非 LIFO） |
| 8 | `QPointer<>`（弱引用防悬垂） | 70 文件 | `PkPointer<T>` | 对象析构后 `isNull()==true`、`data()==nullptr` |
| 9 | `Qt::ConnectionType` 族 | (§6.1 连接类型成句提及) | `PkConnectionType` + `namespace Qt` 别名 | `AutoConnection`/`DirectConnection`/`QueuedConnection`/`BlockingQueuedConnection`/`UniqueConnection`；R-24 起 `Queued`/`BlockingQueued` 真实走 `pk/concurrent` 的 `PkThreadCallQueue` 投递（详见下方偏离清单第 1 条） |
| 10 | `Q_OBJECT` 元对象声明 | 473 文件隐含 | friend 声明（喂 R-11 binder）+ 生成器产物 | 无元对象、无字符串表、无属性系统 |

### 交付的公开类型与入口

| 名字 | 文件 | 替代什么 |
|---|---|---|
| `PkObject` | `PkObject.h` / `PkObject.cpp` | `QObject`（父子树 + 生命周期 + 信号连接） |
| `PkConnection` | `PkConnection.h` | `QMetaObject::Connection` |
| `PkConnectionType` | `PkConnect.h` | `Qt::ConnectionType` |
| `QOverload<Args...>::of` | `PkConnect.h` | `QOverload<Args...>::of`（重载消歧） |
| `PkPointer<T>` | `PkPointer.h` | `QPointer<T>` |
| `PkSignalTraits` / `PkMemberFnKey` | `PkSignalTraits.h` | 内部：从成员函数指针提类与参数包、打包成可比 key |
| `compat/QObject` | `compat/QObject` | `<QObject>` 垫片（宏：`QObject`/`Q_OBJECT`/`Q_SIGNALS`/`signals`/`slots`/`Q_SLOTS`/`emit`/`Q_EMIT`/`QPointer` + `QMetaObject::Connection` 替身类 + `namespace Qt` 连接类型族） |
| `pk_signal_moc.py` | `pk_signal_moc.py` | moc 的信号定义生成（`Q_SIGNALS` 段 → `activateSignal` 调用） |

`PkObject::connect` 两个重载的**实际签名**：

```cpp
// 成员函数指针 → 成员函数指针
static PkConnection connect(const typename PkSignalTraits<F1>::Object* sender, F1 signal,
                            const typename PkSignalTraits<F2>::Object* receiver, F2 slot,
                            PkConnectionType type = PkConnectionType::Auto);
// 成员函数指针 → lambda（receiver 仅用于生命周期绑定）
static PkConnection connect(const typename PkSignalTraits<F1>::Object* sender, F1 signal,
                            const PkObject* receiver, Lambda&& lambda,
                            PkConnectionType type = PkConnectionType::Auto);
static bool disconnect(PkConnection& connection);   // 句柄式断开
// 4 参函数指针式：断「同信号同槽」一条连接
static bool disconnect(const typename PkSignalTraits<F1>::Object* sender, F1 signal,
                       const typename PkSignalTraits<F2>::Object* receiver, F2 slot);
// 断开全部式：断 sender→receiver 所有连接（裸 0 选此重载）
static bool disconnect(const PkObject* sender, std::nullptr_t,
                       const PkObject* receiver, std::nullptr_t);
```

**信号参数可多于槽参数**（Qt 语义）：槽只取信号参数的前缀。这是试接
`KisSignalAutoConnectionTest::testOverloadConnection` 里
`sigTest2(const QString&,const QString&) → slotTest2(const QString&)` 真实压出来、
随后在 `PkConnect.h`（`PkMakeSlotFnFromTupleHelper` / `PkCallSlotPrefix`）落实的，
不是预先铺好而是试接驱动的修正。

### 偏离清单（对齐口径下逐条登记）

1. ~~`Queued`/`BlockingQueued` 在 R-05 阶段退化为 `Direct`~~：**R-24 已交付真实投递**——`PkObject` 现在有线程亲和性（`thread()`/`moveToThread()`），`Auto` 按 sender/receiver 是否同线程决定退化 `Direct` 还是走 `Queued`；显式 `Queued`/`BlockingQueued` 一律走 `pk/concurrent` 的 `PkThreadCallQueue`（按线程 id 分桶的待执行调用队列 + 显式 `processPendingCalls()` pump，不是隐式事件循环）。语义细节（同线程显式 Queued 不折叠为立即执行、BlockingQueued 阻塞发射线程直到目标线程 pump 执行完）逐条对齐真 Qt 实测（探针见 `docs/superpowers/plans/R-24.md`）。`pk/signal` 现在依赖 `pk/concurrent`（此前不依赖），CMake 已接线。
   ⚠ **投递不等于执行**：投递到某个线程的调用不会自动执行，该线程必须自己
   调用 `PkThreadCallQueue::processPendingCalls()`（或未来某个封装它的机制）
   来抽干队列——不这么做，投递的调用会永远停在队列里，不报错、不崩溃、
   不打日志，是一个纯静默的行为缺失。final whole-branch review 核实：全仓
   `pk/` 之外零 pump 调用点，第一个把跨线程 `Auto`/`Queued` 连接搬进来的
   S 批次消费方必须自己在目标线程装 pump（I-3）。
   ⚠ **receiver 发布自己的线程 id（即 `moveToThread()` 之后被别的线程看见）
   之前要先"预热"**：目标线程第一次 pump 会把它自己名下已经排队的条目
   当陈旧条目丢弃，这个判定分不清"陈旧"与"合法但我还没来得及第一次
   pump"，所以任何人在目标线程第一次 `processPendingCalls()` 之前发给它
   的 `Queued`/`BlockingQueued` 调用都有被丢弃的风险（final whole-branch
   review NEW-I2，详见 `pk/concurrent/PkThreadCallQueue.h` 类头注释与
   `pk/concurrent/README.md`）。**决策 4 提到的 8 处真实
   `BlockingQueuedConnection` 调用点**（工作线程 → GUI 线程单向送调用）
   接入时，GUI 线程侧要保证在被工作线程知道自己的 id 之前先调用过一次
   `processPendingCalls()`——遵守这条前提下，"预热之前投递"的后果不是
   永久挂起：`postBlocking()` 丢弃这次调用时会正确唤醒发射线程、抛
   `PkCallAbandonedException`（见 NEW-C1），Queued 的等价后果是槽函数被
   悄悄丢弃、不执行、不报错。**但如果 GUI 线程从头到尾一次都没调用过
   `processPendingCalls()`/`post()`/`pendingCount()` 就退出（不是"预热
   之后不再调用"，是"从来没碰过这套队列系统的任何入口"），`postBlocking()`
   仍然会永久挂起发射线程**——这与 Qt 同线程 `BlockingQueuedConnection`
   死锁是同一类已接受风险（详见 `pk/concurrent/PkThreadCallQueue.h` 类头
   注释），本原语同样不做防护。"预热"不是可选的优化，是避免这条永久挂起
   路径的唯一手段。
2. **receiver 析构只 `disconnectAllIncoming` 清自己的 `m_incoming`，不从 sender 的 `m_outgoing` 摘除** → dead 条目与悬垂 receiver 裸指针滞留（无 UB，emit 靠共享 state 的 alive 跳过、不触碰 receiver 内存；类内未解引用 receiver 指针）。性能不预先优化，M0 benchmark 再判是否加 sender 反向指针做 eager 摘除。
3. **`BlockingQueued` 分支存在窄场景的跨线程迭代失效风险，已知限制、暂不处理**：`activateSignal` 遍历 `sender->m_outgoing` 时，若某条目是 `BlockingQueued`，发射线程会阻塞在 `PkThreadCallQueue::postBlocking` 里等目标线程执行完槽函数；如果那个槽函数反过来对同一个 sender 做 `connect()`（可能触发 `m_outgoing` 的 vector 扩容重分配）或者再次 emit，会与发射线程正在用的 for 循环迭代器产生跨线程的迭代失效。场景很窄（需要槽函数反向操作自己正阻塞等待的 sender），保留范围内的真实 `BlockingQueuedConnection` 调用点（决策 4 提到的 8 处）均是单向"工作线程→GUI 线程"送调用，不构成这种反向操作。真要修需要"遍历前先拍一份 `m_outgoing` 快照"这类更大的结构改动，超出 R-24 Task 3 这轮修复的合理范围，评审判定登记为已知限制、不在本轮处理。

4. **两条更普遍的线程安全约束，final whole-branch review I-4 补登记**（不是 3 的窄场景，是 `m_outgoing`/`m_incoming` 本身隐含依赖、此前只字未写的不变式；本轮不加锁修复，只登记，供后续 S 批次消费者知道边界在哪）：
   - **任何线程在 `activateSignal` 遍历 `sender->m_outgoing` 期间对同一 sender 做 `connect()`/`disconnect()`/析构，都是 UB**（vector 重分配/`clear()` 使正在遍历的迭代器失效），与是不是 `BlockingQueued` 无关——3 只是这条更宽约束下的一个具体触发路径（"反向操作自己正阻塞等待的 sender"），不是唯一路径。
   - **receiver 必须在它自己的（即 pump 的）线程上析构**。否则 `PkObject.h` 里排队路径的 `if (!state->alive) return;` 是一个 TOCTOU：pump 线程读到 `alive==true` 之后、`impl2->fn(args...)` 执行之前，receiver 在第三个线程析构 ⇒ 槽闭包里捕获的 receiver 裸指针悬垂 ⇒ use-after-free。

## 2. 三条缺口登记（逐条 + 建议归口）

试接与全量扫描口径下，本任务**明确不交付**、已定位的三条：

| # | 缺口 | 数字 | 建议归口 |
|---|---|---|---|
| 1 | `qobject_cast<T*>(obj)` | 122 处 | 需要「类型标识 + 运行时安全向下转型」。R-05 交付 `PkObject` 后，可给 `PkObject` 加一个轻量 type-id（或 RTTI 包装）再提供 `pkObjectCast`。**它不是信号槽**，是对象系统里独立的一块，建议单列到后续对象系统任务、或随 S 批次里第一个真实 `qobject_cast` 调用点的 consumer 一起做 |
| 2 | `objectName()` / `setObjectName()` | 72 处 | 对象命名，与信号槽无关。归到「对象系统补全」批次 |
| 3 | `KisSignalCompressor`（21 文件，信号节流）、`KisThreadSafeSignalCompressor`（9 文件）族 | 30 文件 | §6.1 明写「跨线程投递…归 Q-8，S6 前完成」。`KisSignalCompressor` 依赖事件循环/计时器投递，随 Q-8（线程/事件）批做，不复现为独立节流器 |

口径：1/2 两行的数字来自本任务 brief 的缺口清单；3 的文件数来自 §6.1 原文。
这三条都**不在** `pk/signal` 锁内，登记出来是让后续认领方一眼找到，不是在本任务实现。

## 3. 用法示例

```cpp
#include <QObject>          // → compat/QObject（PkObject）
#include <QPointer>         // → PkPointer

struct Sender : QObject {
    Q_OBJECT
public:
    void fire(int v) { emit valueChanged(v); }
Q_SIGNALS:
    void valueChanged(int v);
};

struct Receiver : QObject {
    Q_OBJECT
public Q_SLOTS:
    void onValue(int v) { last = v; }
    int last = 0;
};

// connect / emit / disconnect（句柄式）
Sender s;
Receiver r;
PkConnection c = QObject::connect(&s, &Sender::valueChanged, &r, &Receiver::onValue);
s.fire(42);                        // r.last == 42
QObject::disconnect(c);            // 断开；对象析构也会自动断开

// 重载消歧
QObject::connect(&s, QOverload<int>::of(&Sender::valueChanged), &r,
                 QOverload<int>::of(&Receiver::onValue));

// lambda 槽 + 生命周期绑定 + Unique 去重
QObject::connect(&s, &Sender::valueChanged, &r,
                 [](int v){ /* ... */ }, Qt::UniqueConnection);

// 弱引用：对象析构后 isNull()==true
QPointer<Sender> p(new Sender);
delete p.data();                   // p.isNull() == true

// 跨线程投递（R-24）：Queued/BlockingQueued 真的推迟到目标线程 pump 才执行
Receiver r2;
r2.moveToThread(workerThreadId);   // PkThreadId，例如另一个 std::thread 的 id
QObject::connect(&s, &Sender::valueChanged, &r2, &Receiver::onValue, Qt::QueuedConnection);
s.fire(42);                        // 立即返回，r2.last 还没变
// ……在 workerThreadId 对应的线程里：
PkThreadCallQueue::processPendingCalls();  // 这时候 r2.last 才变成 42

// 信号连信号（signal→signal）：接收端是信号地址，发射时转发
```

信号/槽的**定义**由 `pk_signal_moc.py` 生成（替代 moc）：对头里 `Q_SIGNALS:` 段的
`void sig(...);` 生成定义体，调用 `PkObject::activateSignal`。重载信号的产物用
`static_cast<void (C::*)(...)>(&C::name)` 消歧。生成的 `.inc` 必须与使用它的 TU
同源 include（ODR 硬规则，见 `graft_run.sh` 的 `driver.cpp` 做法）。

## 4. 怎么跑

```bash
# 全量单测（g++ 直编，覆盖 tree/connect/pointer/generator 五个测试文件）
g++ -std=c++17 -Wall -Wextra -I pk/signal -I pk/signal/tests pk/signal/PkObject.cpp \
  pk/signal/tests/test_main.cpp pk/signal/tests/test_tree.cpp \
  pk/signal/tests/test_connect.cpp pk/signal/tests/test_pointer.cpp \
  pk/signal/tests/test_generator.cpp -o pk/signal/build/all_test && \
  pk/signal/build/all_test

# 真实 Krita 测试类试接（判据②，中心判据；自举构建全部依赖库）
./pk/signal/graft/graft_run.sh

# 判据③：替代品本体不得有 Qt 未定义符号
nm -u pk/signal/build/libpksignal.a | grep -i qt || echo "libpksignal.a 无 Qt 未定义符号"
```

`graft_run.sh` 把 `libs/global/tests/KisSignalAutoConnectionTest.{h,cpp}`（target
`kritaglobal`，它测的 `libs/global/kis_signal_auto_connection.h`）复制到
`pk/signal/build/graft/`，在副本上跑两个 sed（`pk/test/graft/rename.sed` 的 D-23
机械改名 + `graft/rename_extra.sed` 的老式宏脚手架）、跑双生成器（`pk_signal_moc.py`
+ `pk_test_moc.py`）、`g++` 编译链接、跑、`nm -u` 查 Qt、`git diff` 自证源树零改动。
源树 `libs/global/` 一个字节不动。

## 5. 老式宏转换归 S 批次的交接说明

`SIGNAL()/SLOT()` 的 1 530 处正式全仓转换**不在本任务**（`pk/signal` 锁装不下
`libs/`/`plugins/`/`krita/` 里的调用点），归 S 批次各 consumer 认领。

- **S 批次认领时查自己 consumer 目录的 `SIGNAL(`/`SLOT(`**，把每个
  `SIGNAL(xxx(...))`/`SLOT(xxx(...))` 转成对应的 `&Sender::xxx` 或
  `QOverload<...>::of(&Sender::xxx)`（同名多参数的重载需要 QOverload）。
- `graft/rename_extra.sed` 是**试接脚手架，不是通用规则**：它写死
  sender/receiver 都是 `TestClass`（`KisSignalAutoConnectionTest` 的事实），全仓
  别处 sender 类型千变万化，S 批次不能当通用规则复用。写全仓通用转换时按
  「信号签名 → 取地址形式」的原则重新写，别照抄。
- 转换的机械性已被试接证明：试接副本上的 6 条转换（含两处重载 QOverload）让
  真实测试类零手工改动跑绿 5/5。

## 6. 试接中修掉的库缺陷（Task 5 实录）

试接 `KisSignalAutoConnectionTest` 是真实 API 形状的自证，它压出并修掉了两个
`pk/signal` 自身的缺陷（**不是手工改测试源绕过**，改的是 `pk/signal` 库，测试源
副本零改动）：

1. **信号参数可多于槽参数（前缀语义）**——`testOverloadConnection` 里
   `sigTest2(const QString&,const QString&)` 连 `slotTest2(const QString&)`。修在
   `PkConnect.h`：槽盒统一按信号参数签名收参，槽经 `std::get` 取前缀调用。
2. **`PkTestObject` override 的 include 顺序**——`pk/test/PkTest.h` 顶部
   引号 `#include "PkTestObject.h"` 同目录优先，`-I` 优先级抢不赢（GCC 引号
   include 规则「先查当前文件所在目录」）。改法在 `graft/stubs/PkTestObject.h`
   头注释里完整推导，靠 `-include` 预包含 + `#pragma once` 跨 include 去重实现
   `PkTestObject = PkObject`，并配 `graft/stubs/simpletest.h` 覆盖（QObject 引到
   pk/signal 版而非 pk/test 版）。这些 override 都是脚手架，`pk/test` 真品
   改指 PkObject 是 R-11 锁内的事（R-11 README §3 已预见）。
