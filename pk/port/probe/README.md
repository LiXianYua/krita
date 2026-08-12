# pk/port/probe —— `PkStream` 对齐用的真 Qt 探针

R 线的硬规则是「Qt 的行为一律去问真 Qt，不许推断」（`R线-spec.md`「实测优先」）。
这两个探针是 `PkStream` 端口接口设计的一手依据，**不是测试，不参与任何构建**。
它们链真 Qt，产物只往 `$PROBE_TMPDIR` 写，不留在源树里。

## 怎么跑

```bash
QT=/mnt/ssd-disk/liyang/projects/kde-deps/usr
OUT=/tmp/pkport-probe && mkdir -p "$OUT"

g++ -fPIC -std=c++17 pk/port/probe/probe_qiodevice.cpp -o "$OUT/probe_qiodevice" \
  -I$QT/include/x86_64-linux-gnu/qt5 -I$QT/include/x86_64-linux-gnu/qt5/QtCore \
  -L$QT/lib/x86_64-linux-gnu -lQt5Core

PROBE_TMPDIR=$OUT LD_LIBRARY_PATH=$QT/lib/x86_64-linux-gnu "$OUT/probe_qiodevice"
LD_LIBRARY_PATH=$QT/lib/x86_64-linux-gnu ldd "$OUT/probe_qiodevice" | grep -i qt
```

最后那条 `ldd` **必须看到 `libQt5Core`** —— 看不到就说明比的不是真 Qt，结论作废。
`probe_qdatastream_openmode.cpp` 用同样的命令行。`probe_qdir.cpp`（评审 C-1
补的）也用同样的命令行，只是换成链接目标：

```bash
g++ -fPIC -std=c++17 pk/port/probe/probe_qdir.cpp -o "$OUT/probe_qdir" \
  -I$QT/include/x86_64-linux-gnu/qt5 -I$QT/include/x86_64-linux-gnu/qt5/QtCore \
  -L$QT/lib/x86_64-linux-gnu -lQt5Core
LD_LIBRARY_PATH=$QT/lib/x86_64-linux-gnu "$OUT/probe_qdir"
```

实测环境：Qt 5.15.13（运行时与编译期一致），g++ 13.3.0。

## 它们回答了什么

完整的原始输出与逐条结论，见
`$PK/docs/superpowers/plans/R-12.md` §3（**那里是结论的落点，这里只放可复现的源**）。

`probe_qiodevice.cpp` 覆盖 `read`/`readAll`/`peek`/`seek`/`atEnd`/`bytesAvailable`/
`getChar`/`ungetChar`/`skip`/`readLine`/`write`/未 open 设备，
对 `QFile`（随机访问）、`QBuffer`（内存）、`pipe(2)` 上的 `QFile`（顺序）三种设备各测一遍。
`probe_qdatastream_openmode.cpp` 覆盖 `QDataStream` 的默认值与读越界行为、
以及 `QIODevice::OpenMode` 的位值。

**十一条与直觉相反的发现**在 plan §3 里逐条列了。最该记住的三条：

1. **EOF 不是错误**：`read` 到末尾返回 `0`，`-1` 只在设备没 open 时出现；
   `errorString()` 在 EOF 后、甚至在只读设备上 `write` 被拒之后，都还是常量 `"Unknown error"`，
   `error()` 还是 `NoError`。**返回值才是唯一的错误信号。**
2. **`QFile` 与 `QBuffer` 在两处语义分叉**：`seek(size()+100)` 前者 `true`（pos 真到 110）、
   后者 `false`；EOF 时 `peek(10)` 前者 `isNull=true`、后者 `isNull=false`。
3. **顺序设备上 `pos()` 恒为 0**，`QIODevice` 根本不为它推进内部 pos。
   探针踩过一次真事故：把 `QBuffer` 子类的 `isSequential()` 改成 `true` 之后
   `readAll()` 死循环、吐了 64MB —— 因为 `QBuffer::readData` 用 `pos()` 索引，pos 不动就永远重发第 0 字节。
   → **`isSequential()==true` 时 `readData()` 必须自己维护游标，这条要写进 `PkStream` 的接口契约。**

## `probe_qdir.cpp` —— `PkResourceStorage` 路径工具对齐用的真 Qt 探针

评审 R-12 Task 4 时补的（C-1）：`PkResourceStorage::cleanPath()` 此前按 Qt
**文档 + 常识**实现，从未拿真 Qt 核对过越过根的 `".."` 该怎么处理——这正是 R
线「Qt 的行为一律去问真 Qt，不许推断」要拦的那类漏洞。跑法见上一节，产物
`/tmp/pkport-probe/probe_qdir` 同样不进版控。

**三条结论**（Qt 5.15.13 实测，完整输出见 commit 里贴的原始日志）：

1. **`QDir::cleanPath` 不折叠越过根的 `".."`，原样保留**：`"/.."` → `"/.."`、
   `"/../.."` → `"/../.."`、`"/../a"` → `"/../a"`、`"/a/../.."` → `"/.."`。
   （其余 37 种既有形态——`"//"`、结尾 `"/"`、`"."`、空串、`"../.."`、
   `"a/./b"`、`"a/b/../.."`——都和 `PkResourceStorage::cleanPath()` 改之前的
   实现一致，只有越根 `".."` 这一类不一致。）
2. **`QDir::filePath`/`absoluteFilePath` 在 `name` 是绝对路径时原样返回
   `name`，完全不管 `dir` 是什么（哪怕 `dir` 带不带结尾 `/`）**：
   `QDir("/root").filePath("/a")` = `QDir("/root/").filePath("/a")` = `"/a"`。
3. **`QDir::relativeFilePath` 在 `target` 是 `base` 的祖先目录时，结果带一个
   多余的尾部 `"/"`**：`QDir("/a/b/c").relativeFilePath("/a")` = `"../../"`
   （不是 `"../.."`）、`QDir("/a/b").relativeFilePath("/a")` = `"../"`（不是
   `".."`）。这个尾部斜杠只在「爬完 `".."` 之后，`target` 侧没有剩余段可拼」
   这一种形态出现——`target` 有剩余段（`"../d/e"`）或 base==target（`"."`）
   都没有这条尾部斜杠。
