# pk/port —— 零 Qt 端口层

独立 `project(pkport CXX)` 薄壳工程，不接入 Krita 主构建。目前只有 R-12 Task 1
交付的 `PkStream`（对应 Qt 的 `QIODevice`）。跑法见 `tests/run_tests.sh`。

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

## 后续消费者

`PkStream` 的消费者是 S-01（`kritastore`，`KoStoreDevice` 等）与其他需要文件/
内存/zip 适配器的批次——本任务只出接口 + 基类模板逻辑，不出任何具体适配器。
