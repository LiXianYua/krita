# `libs/image/tiles3/swap` —— 磁盘交换层

这一层把 `KisTiledDataManager` 的 tile 落进一个 mmap 出来的临时交换文件
（`KisSwappedDataStore` → `KisMemoryWindow` / `KisChunkAllocator` / `KisTileCompressor2`）。

---

## 1. 契约：映射窗口必须页对齐（**别再退回裸 `::mmap`**）

`KisMemoryWindow::adjustWindow()` 按**请求的字节区间**决定映射窗口，而
`mapFile()` 把窗口基址原样当文件偏移交给 `::mmap()`。**POSIX 要求这个偏移是页大小的
整数倍**，否则 `mmap` 返回 `MAP_FAILED` / `errno=EINVAL`。

上游 Krita 走的是 Qt 的 `QFile::map()`，**它内部替调用方做了这个对齐**并把 delta 折进
返回指针，所以 Krita 自己的代码里从来没有过对齐这一步。剥离期把 `QFile::map()`
换成裸 `::mmap()`（为了去掉 Qt file engine 的 map-handle 缓存），**这层兜底就没了**，
代码里也没补上。

两条实测（2026-09-12，macOS arm64 / Apple M4，`pagesize=16384`）：一条打裸 `::mmap`，
一条打真 Qt 的 `QFile::map`（链 `<依赖前缀>/lib/QtCore.framework`）：

```c
/* 裸 ::mmap 探针：mkstemp -> ftruncate(1048608) -> mmap(..., fd, off) */
ftruncate(fd, 1048608);
mmap(0, 1048576, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);      /* off=0     */
mmap(0, 262144,  PROT_READ|PROT_WRITE, MAP_SHARED, fd, 584);    /* off=584   */
mmap(0, 262144,  PROT_READ|PROT_WRITE, MAP_SHARED, fd, 16384);  /* off=16384 */
```

```
# 裸 ::mmap
mmap off=0      -> ok
mmap off=584    -> MAP_FAILED, errno=22 (Invalid argument)
mmap off=16384  -> ok

# 真 Qt 的 QFile::map —— 非对齐偏移照样成功，且返回指针 = 映射基址 + delta
QFile::map(0, 1048576)    -> 0x105a40000 OK
QFile::map(584, 262144)   -> 0x105b40248 OK      # 0x105b40248 - 0x105b40000 = 584
QFile::map(16384, 262144) -> 0x105b84000 OK
```

**所以**：任何非页对齐的 chunk 偏移都会让 `mapFile()` 返 `nullptr` →
`getReadChunkPtr()` 返 `nullptr` → `KisSwappedDataStore::swapInTileData()`
（`kis_swapped_data_store.cpp:96-98`）把这个空指针交给
`KisTileCompressor2::decompressTileData()`，后者在 `buffer[0]` 上段错误：

```
frame #0: KisTileCompressor2::decompressTileData(buffer=0x0, bufferSize=292)
          at kis_tile_compressor_2.cpp:160
```

中间的 `PK_TILES_ASSERT(ptr)` **拦不住**——它在 `NDEBUG` 下展开成
`((void)sizeof(condition))`（定义见 `kis_chunk_allocator.h:16-18`），RelWithDebInfo
把它编掉了。

**维护这条不变量**：窗口的 `m_begin` 与 `size()` 都必须落在页边界上——
`alignDown(请求基址)` 定基址、`alignUp(基址 + 窗口大小)` 定末端。这样 `chunk` 就是
被映射的那一段，`MappingWindow::calculatePointer()` 的
`window + other.m_begin - chunk.m_begin` 仍然成立，`unmapFile(window, chunk.size())`
也拿到对齐的基址与整页的长度。页大小走 `::sysconf(_SC_PAGESIZE)`，**不要硬编 16384**
（Linux 常见 4096）。

> 本仓库里其余裸 `::mmap` 只有 `plugins/metadata/common/KisExiv2IODevice.cpp:223`
> 一处，**offset 恒为 0**，不受影响。

---

## 2. 「`paint_smoke` 会随执行者的 HOME 翻转」是怎么来的

`paint_smoke` 是 M5 的验收载体。它在**干净 `HOME`** 下 `exit=0`，在**真 `HOME`** 下
曾 `exit=139`（SIGSEGV）。逐项二分（把真 `HOME` 的顶层条目逐个复制进一个临时 HOME）
定位到**单个文件**：

| `HOME` 里的条目 | `paint_smoke` 退出码 |
|---|---|
| `Library/Preferences/kritarc` | **139**（触发） |
| `Library/Application Support/krita`（app data，含 `swap/`） | 0 |
| `Library/Caches` | 0 |
| `.qttest` | 0 |
| `.cache/fontconfig` | 0 |

在小写平台相应位置：

| 平台 | 配置文件 |
|---|---|
| macOS | `$HOME/Library/Preferences/kritarc` |
| Linux | `$HOME/.config/kritarc`（或 `$XDG_CONFIG_HOME/kritarc`） |

空文件 ⇒ `exit=0`，**所以是内容**。按段落删除 + 二分，落到 `kritarc` 里一个
`[PkConfig-v1:]` 段（pk 配置栈自己的格式，真源 `pk/config/PkConfigStore.cpp:32`，
键值是十六进制编码的）。对该段 7 个键做**穷举最小子集搜索**，结论：

```
CRASH combo: ['memoryHardLimitPercent', 'memoryPoolLimitPercent']
             [0.004069010416666667,        10]
```

留一法旁证：去掉这两个键里**任何一个**就 `exit=0`；其余五个键单键测试全 0。
⇒ 触发条件是这两个键的**组合**，与 `swapWindowSize` / `swapSlabSize` / `maxSwapSize`
无关。

**机制**：`KisImageConfig::tilesHardLimit() = totalRAM * (hard%/100) * (1 - pool%/100)`
是**内存阈值**（`KisStoreLimits` 拿它当 emergency threshold）。默认内存上限下
256×256 的图**根本不会走 swap-in**，所以干净 `HOME` 恒 0；把上限压到几 MiB 以下才会
真的换出/换入 tile，于是踩到上面那条对齐缺陷。

**它是谁写进来的**：同一段里还有
`ResourceDirectories=<HOME>/.qttest/Library/Caches/TestTagResourceModel/testdest/`，
而那三个键的取值分别对得上 `libs/image/tiles3/tests/` 里的测试——
`kis_tile_data_store_test.cpp:143`（`100.0/totalRAM`）、
`kis_low_memory_tests.cpp:24`（`1.1*100.0/totalRAM`）、
`kis_store_limits_test.cpp:20`（pool 10）、
`kis_swapped_data_store_test.cpp:31-33`（swap 尺寸）。
⇒ **这些测试把内存/交换设置写进了使用者的真实配置文件**，留下的是几个测试设定的
叠加。**不在本任务范围**，但下一节的操作会碰到它。

---

## 3. 第二个成因：`tilesHardLimit()` 把正数上限截断成 `0 MiB`

修好对齐之后，真 `HOME` 下不再崩了，但**改成了长时间不收敛**（实测 300 s 仍未结束，
单线程 100% CPU，交换文件不增长——是空转不是进展）。`sample` 全落在这条链上：

```
KisPainter::fillPainterPath -> bitBltImpl -> KisRandomAccessor2::moveTo
  -> KisTile::lockForRead -> KisTileDataStore::ensureTileDataLoaded  (kis_tile_data_store.cc:231)
  -> KisTileDataSwapper::doJob                                       (kis_tile_data_swapper.cpp:162)
  -> pass<AggressiveSwapStrategy> -> trySwapTileData
```

`lldb` 读到 `m_d->limits.m_emergencyThreshold = 0`。原因是
`tilesHardLimit()` 声明成 `int`，而 0.9 MiB 截断成 **0**：

```
memoryHardLimitPercent=0.005     tilesHardLimit≈1.1059 MiB  -> exit=0, 1s
memoryHardLimitPercent=0.004069  tilesHardLimit≈0.9 MiB     -> 300s 仍未结束
```

阈值 0 的语义是「一个 tile 都不许留在内存里」，而 tile store 永远持有正在被使用的那个
tile ⇒ **条件恒不可满足** ⇒ `KisTileDataSwapper::checkFreeMemory()` 在每一次 tile 载入
时都跑一整轮换出，永远达不到目标。

**改法**：`tilesHardLimit()` 里正数上限最小取 1 MiB（保留用户意图），`0` 仍是 `0`，
≥1 MiB 一律不变。**这是 R-67 唯一一处偏离上游 Krita 行为的改动**——上游同一份代码在
同样的配置下会同样空转（`checkFreeMemory` / `doJob` / `pass` 都没被剥离改动过）。

---

## 4. 复现与验证（可复制粘贴）

```bash
source <依赖前缀>/env          # 机器相关；本机是 krita-ci-env/env
export CCACHE_DIR="$KDECI_CC_CACHE"
B=<build 目录>/bin/paint_smoke
```

**三 HOME 矩阵**（`HOME` 是唯一变量）：

```bash
# 1) 干净 HOME —— 对照组
D=$(mktemp -d); HOME="$D" "$B" /tmp/a.kra;  echo "clean exit=$?"    # 期望 0

# 2) 真 HOME —— 任务行点名的那一格
"$B" /tmp/b.kra;                            echo "real  exit=$?"    # 期望 0

# 3) 触发 HOME —— 把触发条件固化成 fixture（不依赖跑的人 HOME 里恰好有什么）
TH=$(mktemp -d); mkdir -p "$TH/Library/Preferences"
python3 - "$TH" <<'PY'
import sys, pathlib
def hx(s): return s.encode().hex()
root = pathlib.Path(sys.argv[1]) / "Library/Preferences/kritarc"
root.write_text("[PkConfig-v1:]\n" + "\n".join(
    f"{hx(k)}={hx(str(v))}" for k, v in [
        ("memoryHardLimitPercent", 0.004069010416666667),
        ("memoryPoolLimitPercent", 10)]) + "\n")
PY
HOME="$TH" "$B" /tmp/c.kra;                 echo "trig  exit=$?"    # 期望 0
```

三条都期望 `exit=0` 且 stdout 上 10 条 `[PASS]` / `RESULT=done failures=0`。
**修前**：第 1 条 0，第 2、3 条 139（第 2、3 条走的是同一条 swap 路径）。

**常驻回归载体**（比 `paint_smoke` 快得多，秒级）：

```bash
ninja -C <build 目录> kis_memory_window_alignment_test
./<build 目录>/bin/kis_memory_window_alignment_test     # 期望 exit=0
```

它直接压 `KisMemoryWindow` 的页对齐契约，`ctest` 里注册名是
`libs-image-tiles3-kis_memory_window_alignment_test`。

---

## 5. 已知缺口

- **`libs/image/tiles3/tests/kis_memory_window_test.cpp` 编不过**：它用 `QString` 构造
  `KisMemoryWindow`（形参已变成 `PkString`）。归 **R-65**（`docs/TASKS.md`，把 178 个
  测试 target 从 `kritatestsdk` 迁到 pk 测试栈，`NOT_STARTED`）。R-67 交付的
  `kis_memory_window_alignment_test.cpp` 是**复刻调用点形状的 driver**，不是它。
- **测试写真实配置文件**（§2 末）：`libs/image/tiles3/tests/` 那几个测试用
  `KisImageConfig config(false)`，`sync()` 会落到使用者的真实 `kritarc`；pk 配置栈有
  隔离入口（`PkConfigStore::setConfigFilePathForTesting`，见
  `pk/config/tests/test_main.cpp:32`）但它们没用。**未修**（那些 target 本身编不过，
  归 R-65）。
- **页大小只在 macOS arm64（16384）实测过**；Linux（4096）上的行为由代码逻辑推出。
- 交换层的其余分支（`maxSwapSize` 上限、`swap out of tile failed`、
  `KisChunkAllocator` 的碎片整理）不在 R-67 的验证范围内。
