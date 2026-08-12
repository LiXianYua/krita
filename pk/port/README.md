# pk/port —— 零 Qt 端口层

独立 `project(pkport CXX)` 薄壳工程，不接入 Krita 主构建。目前有 R-12 Task 1
交付的 `PkStream`（对应 Qt 的 `QIODevice`）与 Task 3 交付的 `PkEventSink`（状态
通知端口，对应 `libs/image/kis_node_graph_listener.h` 等）。跑法见
`tests/run_tests.sh`。

## 等 R-02 交付的符号

以下三个 `PkStream` 成员**只声明，不定义**，链接期未使用则不报错、被调用则
响亮报错（`undefined reference`）——这是刻意的设计，不是遗漏，理由见
`PkStream.h` 里「一个刻意的设计」那段头注释。`PkByteArray`（R-02 交付）落地后
需要给它们补实现：

| 符号 | 签名 |
|---|---|
| `PkStream::readAll()` | `PkByteArray readAll()` |
| `PkStream::peek(pk_int64)` | `PkByteArray peek(pk_int64 maxSize)` |
| `PkStream::readLine()` | `PkByteArray readLine()` |

## 等 R-03 交付的类型：`PkEventSink::imageUpdated()`

`PkEventSink::imageUpdated(const PkRect &rect)`（来源：`kis_image.h:818`
`sigImageUpdated(const QRect &)`）**已经给了真实的空函数体实现**（在
`PkEventSink.cpp` 里），不是像 `PkStream::readAll()` 那三个一样只声明不定义
——原因见 `PkEventSink.h` 里这个方法的注释：它是 `virtual` 的，若像
`PkByteArray` 那三个方法一样留空不定义，任何没有 override 它的子类在被实例
化时都会在**虚函数表**上出现未决议符号，链接期必炸（不是「被调用才炸」，
是「实例化就炸」），这条路对虚函数不安全。空函数体不触碰 `rect`，不需要
`PkRect` 的完整定义，两者在「只前置声明、不 include」这一点上是一致的，只
是「声明是否配一个定义」这一步因为虚函数表的缘故必须不同。

代价：`PkRect` 完整定义交付前，没法在测试里构造一个实参去调用这个方法，
`pk/port/tests/test_eventsink.cpp` 因此没有覆盖它的调用链路，只覆盖了另外
8 个事件。`PkRect`（R-03）落地后要给这个方法补一条真正传值调用的测试用例。

## 后续消费者

`PkStream` 的消费者是 S-01（`kritastore`，`KoStoreDevice` 等）与其他需要文件/
内存/zip 适配器的批次；`PkEventSink` 的消费者是防腐层（转发到 Flutter）与
测试记录器——两者本任务都只出接口，不出任何具体适配器/生产实现。
