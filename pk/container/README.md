# pk/container —— Qt 容器族的零 Qt 替代品

`PkVector` / `PkList` / `PkMap` / `PkHash` / `PkSet` / `PkStack` / `PkQueue` /
`PkStringList` / `PkPair`，外加 COW 地基 `PkArrayData<C>`、Java 风格迭代器、
`PK_FOREACH`、`qDeleteAll`，以及 `compat/` 下 9 个让真实调用点 `#include <QVector>`
一个字不改就能解析过来的垫片。

`libpkcontainer.a` **不链接、不引用任何 Qt 符号**（`nm -u` 自证，见 §10）。

---

## 0. 全表的第一条口径：**所有用量数字都是「保留范围的上界」**

**这一条比表里任何一个数字都重要，误读它会让人凭空造出不需要的 API。**

本文件的用量统计是 **v6.0.3 全仓**（只扣 `tests/` 与 `benchmarks/`），**没有扣掉
D 线要删的部分**。按 `docs/实施边界-构建目标视图.md` §6 逐档收窄，实测：

| 口径 | 文件数（含 `QVector`\|`QList`\|`QMap`\|`QHash` 任一 token 的文件） |
|---|---:|
| 全仓（含 `tests/`、`benchmarks/`） | 1 921 |
| 排除 `tests/`、`benchmarks/` ← **本文件用量表的口径** | 1 760 |
| 再排除 D1（`krita/`、`plugins/dockers|extensions|platforms|qt/`、`libs/libkis/`、`qmlmodules/`） | 1 555 |
| 再排除 `libs/ui`（D3） | **1 225** ← 最接近 |
| `docs/Qt替代品选型.md` §5 记的 | 1 123 |

**结论：选型文档统计的是保留范围，本表统计的是全仓。** 逐档扣到 D1+D3 之后残差
约 8%，可能来自版本时点差异。**这不是文档错了**——两边测的本来就是不同的集合。

> 口径细则：SRC = `git ls-files '*.cpp' '*.h' '*.cc'` 排除路径含 `tests/` 或
> `benchmarks/` 的文件，**并排除 `pk/`**（我们自己新增的树）= **5 350 个文件**。
> 排除 `pk/` 是必须的：那是替代品自己的代码，把它算进"Krita 的调用点"会自我污染，
> 而且随每个任务增删漂移（同一口径把 `pk/` 算进去，今天是 1 959 / 1 782，
> Task 1–6 落表时是 1 946 / 1 773——差的全是我们自己新写的文件）。

### 这条口径已经三次消解掉「伪缺口」

判断「某个 API 有没有真实调用点」时**必须先看命中的文件在不在保留范围内**。
三个已经发生过的例子：

1. **`QStringList::filter(QRegularExpression)`**：唯一调用点在
   `plugins/dockers/textproperties/`（D1 删）→ **不需要为它造 `PkRegularExpression`**。
2. **`QSetIterator`(全仓 1)、`QStringListIterator`(2)、`QMutableStringListIterator`(0)**：
   命中全落在 D1/D3 → 保留范围 0 处 → `compat/QSet`、`compat/QStringList` 里都不给
   （两个文件的头注释各自记了具体行号）。
3. **对拍报告列的 6 条「API 面缺口」，逐个核实后一条都不是缺口**（核实方法：先收集
   声明为容器类型的变量名，再按**接收者**匹配方法调用，而不是裸 grep 方法名）：

| 报的缺口 | 保留范围内的容器接收者命中 | 结论 |
|---|---|---|
| `QVector::startsWith` / `endsWith` | 0（146 / 84 处全是 `QString` 的） | 不补 |
| `QSet::intersects` | 0（84 处全是 `QRect::intersects`） | 不补 |
| `QList::swapItemsAt` | 0（全仓 2 处，都在 `libs/ui/kis_painting_assistant.cc`，D3 删） | 不补 |
| `QMap::insertMulti` | **全仓 0**（4 个 grep 命中全是 `insertMultipleKeyframes` 的子串误报） | 不补 |
| `QMap::unite` | 0（`unite` 全仓 2 处：`kis_layer_utils.cpp:1384` 是 `QSet<int>::unite`——**已交付**；`kis_transform_worker.cc:214` 的接收者是 Krita 自己的 `LinePos`，不是 Qt 容器） | 不补 |
| `QList::mid` | **1 处**：`libs/widgetutils/config/krecentfilesaction.cpp:286` `items.mid(...)`。而 `libs/widgetutils` 在边界文档的删减链里（§6：「最后才轮到 `libs/widgets`/`widgetutils`」），`KRecentFilesAction` 继承 `QAction`，是 UI 类 → 同样在删除范围内 | 不补 |

**这 6 条不是漏交付，是判据①（一项不多一项不少）正确执行的结果。**

---

## 1. API 范围表

**数字口径**：SRC（§0 定义，5 350 个文件）下正则 `(\.|->)名字\s*\(` 的**出现次数**，
不是文件数。**注释、字符串字面量、以及同名的非容器 API 全部计入** ——
凡与 `QString`/`QRect`/`QSharedPointer` 等共享名字的行，标注的都是**上界**，
真实的容器调用点远少于此。用量表的用途是「排出优先级、证明这一项有人用」，
**不是**精确的调用点清单。

### 1.1 序列容器公共面（`PkArrayContainer`：`PkVector`/`PkList`/`PkStack`/`PkQueue`/`PkStringList` 共用）

| API | 用量 | 备注 |
|---|---:|---|
| `data` | 3 523 | **重度污染**：绝大多数是 `QSharedPointer::data()` / `KisXxxSP::data()` |
| `size` | 3 010 | 与 `QString`/`QImage`/`QRect` 共享 |
| `append` | 2 868 | 与 `QString::append` 共享 |
| `isEmpty` | 2 697 | 与 `QString`/`QRect` 共享 |
| `value` | 2 392 | 与 `QVariant`/`QDomNode` 共享（关联容器也用这个名字） |
| `at` | 1 414 | 与 `QString::at` 共享 |
| `contains` | 1 341 | 与 `QString`/`QRect` 共享 |
| `insert` | 1 274 | 与 `QString::insert` 共享 |
| `end` / `begin` | 1 061 / 805 | |
| `clear` | 907 | |
| `count` | 778 | |
| `first` / `last` | 550 / 284 | |
| `push_back` | 353 | STL 风格别名，Qt 有，调用点在用 |
| `constEnd` / `constBegin` | 190 / 167 | **不 detach**，是 COW 语义的关键一半 |
| `indexOf` | 211 | |
| `constData` | 156 | |
| `prepend` | 65 | 见 §6「`prepend` O(n) 化」 |
| `erase` | 104 | |
| `empty` | 97 | |
| `reserve` | 45 | |
| `cend` / `cbegin` | 35 / 28 | |
| `back` / `front` | 21 / 18 | STL 风格别名 |
| `swap` | 17 | |
| `rend` / `rbegin` | 16 / 14 | |
| `lastIndexOf` | 15 | |
| `push_front` | 13 | |

### 1.2 `PkVector` 专属

| API | 用量 | 备注 |
|---|---:|---|
| `resize` | 182 | |
| `fill` | 176 | 与 `QImage::fill` 共享 |
| `toList` | 51 | |
| `remove` | 294 | **重度污染**：与 `QString::remove`、关联容器 `remove`、`QLayout::remove*` 共享 |
| `capacity` | 1 | 交付理由是它是 `reserve` 的对偶，单测要压 |

### 1.3 `PkList` 专属

| API | 用量 | 备注 |
|---|---:|---|
| `removeAt` | 57 | |
| `removeAll` | 53 | |
| `move` | 50 | 与 `std::move`/`QRect::move*` 无关（正则要求 `.`/`->` 前缀） |
| `takeFirst` | 42 | |
| `takeAt` | 40 | |
| `takeLast` | 30 | |
| `removeOne` | 21 | |
| `pop_back` | 17 | |
| `removeLast` / `removeFirst` | 16 / 16 | |
| `toVector` | 14 | |
| `pop_front` | 10 | |

### 1.4 关联容器（`PkAssocContainer`：`PkMap` / `PkHash`）

| API | 用量 | 备注 |
|---|---:|---|
| `value` / `insert` / `contains` / `clear` / `size` / `count` / `isEmpty` / `erase` | 见 §1.1 | 名字与序列容器共享，未单独拆分 |
| `key` | 398 | 与 `QKeyEvent`/`QSettings` 共享 |
| `keys` | 287 | |
| `values` | 124 | |
| `find` | 93 | 与 `QString::find`(无) / `std::find` 无关（要求 `.`/`->`） |
| `constFind` | 26 | **不 detach** |
| `take` | 14 | |
| `upperBound` / `lowerBound`（**仅 `PkMap`**） | 8 / 2 | `PkHash` 无序，不给 |

### 1.5 `PkSet`

| API | 用量 | 备注 |
|---|---:|---|
| `insert` / `contains` / `remove` / `clear` / `size` / `count` / `isEmpty` / `values` / `toList` | 见上 | |
| `unite` | 2 | 其中 1 处是真 `QSet::unite`（`libs/image/kis_layer_utils.cpp:1384`） |
| `subtract` | 2 | |
| `intersect` | 1 | `libs/image/kis_layer_utils.cpp:2567` |

### 1.6 `PkStack` / `PkQueue`（都是薄派生类，照 Qt）

| API | 用量 | 备注 |
|---|---:|---|
| `top` | 284 | **重度污染**：绝大多数是 `QRect::top()` |
| `push` | 58 | |
| `pop` | 40 | |
| `enqueue` | 17 | |
| `dequeue` | 15 | |
| `head` | 13 | |

### 1.7 `PkStringList`

| API | 用量 | 备注 |
|---|---:|---|
| `join` | 155 | |
| `filter` | 66 | 与 `QSortFilterProxyModel` 一族共享；容器接收者实测 8 处（列在 `PkStringList.h` 里） |
| `sort` | 47 | |
| `removeDuplicates` | 4 | |
| `replaceInStrings` | 1 | `KisDlgImportVideoAnimation.cpp:246`，传字面量 `"output_"` |

### 1.8 自由设施

| 设施 | 用量 | 说明 |
|---|---:|---|
| `Q_FOREACH` / `foreach` | 2 161 / 133 | `PK_FOREACH` + 两个别名。**CoW 是它们的前提**，见 §6 |
| `qDeleteAll` | 132 | 保留小写 `q` 前缀原名（与 `qHash`/`qMakePair` 同一条口径） |
| `QMapIterator` | 27 | Java 风格迭代器，归 `<QMap>` 提供（照 Qt 的分法） |
| `QListIterator` / `QMutableListIterator` | 15 / 11 | 归 `<QList>` |
| `QVectorIterator` | 7 | 归 `<QVector>` |
| `QHashIterator` | 4 | 归 `<QHash>` |
| `QMutableMapIterator` | 1 | `libs/flake/svg/SvgStyleParser.cpp:527`（全文抄在 `PkMapIterator.h` 里） |
| `qMakePair` | — | 由 `PkPair.h` 直接给，垫片不需要 `#define` |

**不给的 Java 迭代器**：`QMutableVectorIterator` / `QMutableHashIterator` /
`QMutableStringListIterator` **全仓 0 处**；`QSetIterator`(1) / `QStringListIterator`(2)
保留范围 0 处（见 §0）。每个"不给"的判据都写在对应的 `compat/` 文件头注释里。

---

## 2. 与 `docs/Qt替代品选型.md` 的差异：**文档没有容器的逐方法用量表**

选型文档 §5 只有**类型级的文件数**（`QVector`/`QList`/`QMap`/`QHash` 合计 1 123 个
文件）与一句 `QString` 的方法枚举，**没有任何容器的逐方法统计**。

上面 §1 那张表是照 `pk/test/README.md` 的先例**自行实测**得出的。

**这是作为文档差异回报的，未修改任何决策文档**（`docs/` 下四篇只有人能改）。
如果将来要把它并进选型文档，注意 §0 那条口径限定必须一起搬过去——脱离口径的
用量数字会被当成"保留范围的真实调用点数"读，那是错的。

---

## 3. 明确排除的项，逐条理由

### 3.1 归 S 线按点改写的容器类型（**不做替代品**）

| Qt 类型 | 声明处数（`Type<`） | 理由 |
|---|---:|---|
| `QMultiHash` | 14 | 多值语义与 `PkHash` 的"insert 同 key 覆盖"冲突，是另一个类不是另一个方法 |
| `QMultiMap` | 9 | 同上 |
| `QVarLengthArray` | 7 | 栈上小数组优化，语义是**性能承诺**不是容器接口；换 `std::vector` 或 `std::array` 按点判 |
| `QLinkedList` | 6 | 双向链表，Qt6 自己都删了；`std::list` 直接换 |
| `QByteArrayList` | 0 | `Type<` 0 处、token 仅 1 处（一个 `#include`）。没有调用点 |

数量都在个位到十几，**逐点改写比造替代品便宜**——造一个 `PkMultiMap` 要连带
一整套多值迭代器语义与对拍。

### 3.2 依赖 `PkVariant`（R-06 未交付）

| Qt 类型 | token 数 | 说明 |
|---|---:|---|
| `QVariantList` | 521 | Qt 里就是 `QList<QVariant>` 的 typedef |
| `QVariantHash` | 366 | `QHash<QString, QVariant>` |
| `QVariantMap` | 64 | `QMap<QString, QVariant>` |

**容器这一层已经就绪**，缺的只是元素类型。R-06 交付 `PkVariant` 之后，
这三项各加一行 typedef 即可，不需要改本目录任何实现。

### 3.3 实测全 0 处，一个都不做

`squeeze` · `equal_range` · `toSet` · `fromSet` · `toStdVector` · `isDetached` ·
`isSharedWith` · `uniqueKeys` · `insertMulti` · `qSort` · `qStableSort` · `qCount` ·
`qFind` · `qLowerBound` · `qUpperBound` —— SRC 下 `(\.|->)名字\s*\(`（自由函数按
token）**全部 0 次命中**。

> `isDetached` / `isSharedWith` 有内部对应物 `PkUseCount()` / `PkIsSharedWith()`，
> **只给单测与对拍用，不进 `compat/` 垫片**——没有调用点的东西进了垫片，就是给
> S 线多留一个"其实没人用但你得维护"的形状。

---

## 4. 对拍（`oracle/`）：跑法与**覆盖度限制**

对拍拿真 Qt 5.15.7 当 oracle，逐输入比对。基线 **2 672 959 次比对，mismatch 36**，
全部落在一条已声明的偏离上（`replaceInStrings` 空 `before`，见 `oracle/R-02.deviation`）。
五组注入实验全部产生未声明 tag（判据有效性自证，明细同文件）。

**说不出覆盖不到什么的对拍，说明还没想清楚。** 以下 8 条是已知的覆盖度限制
（原文与更详细的论证在 `oracle/R-02.deviation`，此处是索引，不要两边各写一份）：

1. **越界 `at()` / `operator[]()` / 空容器 `first()`/`last()`/`takeFirst()` 不可对拍**
   ——不是"会崩"，是**Qt 在这里没有定义行为**：默认构建（`Q_ASSERT` 生效）abort，
   `-DQT_NO_DEBUG` 下返回垃圾值继续跑。release 下不崩比崩更麻烦：两侧各自返回不同
   的垃圾会产生**虚假 mismatch**。输入集里根本不生成这些调用。可对拍的越界形态由
   `value(i)` / `value(i,def)` / `indexOf(t,from)` / `lastIndexOf(t,from)` 承担
   （这四个对任意下标都有定义）。
2. **`QHash` / `QSet` 的迭代顺序不可对拍**：Qt5 的 `QHash` 带随机化种子，同一段代码
   在 5.15.7 下打出 `6,7,4,5,2,3,0,1`，5.15.13 下打出 `4,5,6,7,0,1,2,3`——**跨补丁
   版本就变**。hash/set 通道只比**排序后的集合**。连带退化两处：`erase(find(k))` 的
   **返回值**一格都没比；`key(v)` 只在"至多一个 key 映射到 v"时才比。
   （`QMap` 通道相反：`QMap` 与 `std::map` 都按 `operator<` 排序，迭代顺序**可观察
   且该一致**，所以按顺序直接比。）
3. **迭代器失效时机不可对拍**：失效后使用是 UB，两侧各自的垃圾不构成差异。
4. **自定义元素类型不可对拍**：只能用两侧都有的类型。本轮元素与 key 类型是 `int`
   与 `QString`/`PkString` 两种。
5. **`QList<T>` 在 Qt5 对大对象退化成指针数组**是内存布局，不可观察——**对拍证明
   不了这一层的等价**。这正是 §6 第一条风险为什么必须靠人工审计而不是靠对拍。
6. **负数 `reserve(n)` / `resize(n)` 没有生成**：替代品会把负数转成 `size_t` 去要
   2^64 字节（不是行为差异，是资源炸弹）。
7. **API 面本身没有的那几格没有对拍对象**（不是行为差异，是缺方法，模板实例化时就
   编不过）：`QVector/QList::mid()`、`QVector::startsWith()/endsWith()`、
   `QList::swapItemsAt()`、`QSet::intersects()`、`QMap::unite()/insertMulti()`。
   **§0 已核实这 6 条在保留范围内都没有调用点**，所以不补。
8. **`QStringList` 只跑了 5 个专属方法**（`join`/`filter`/`sort`/`removeDuplicates`/
   `replaceInStrings`），正则相关的重载（`QRegExp`/`QRegularExpression`）整片不在范围内。

---

## 5. `PkArrayData<C>` 的复用契约（**R-13 会照着迁，按这一节接**）

线级 spec 的「已裁决的岔路」定了：**COW 地基共用一份，字符串与容器同源**。
Qt 就是这样（`QArrayData` → `QTypedArrayData<T>` → `QString::Data` /
`QByteArray::Data` / `QVector<T>::Data`）。

**理由不是省代码，是 detach 的时机可观测**——迭代器何时失效、写共享实例何时真拷贝。
两套 COW 实现就是两次把 detach 时机写歪的机会。

### 对模板参数 `C` 的要求（只有两条，头里有 `static_assert` 钉住）

1. **可默认构造** —— 共享空哨兵是 `make_shared<C>()`
2. **可拷贝构造** —— `PkDetach()` 的深拷贝是 `make_shared<C>(*d)`

> `static_assert` 覆盖不到的一格：标准库容器的拷贝构造是**无条件声明**的，
> `std::is_copy_constructible<std::vector<MoveOnly>>` 实测为 `true`，断言放行，
> 真错误要等实例化到 `stl_uninitialized.h` 深处才报。被挡住的是 `C` 本身不可拷贝／
> 不可默认构造（例如误传 `std::unique_ptr`）。

**按内层容器 `C` 参数化，而不是按元素类型 `T`** —— 比 Qt 更通用：`QMap` 在 Qt 里用
的是另一套 `QMapData`，不共用 `QArrayData`；这里 `PkMap`/`PkHash`/`PkSet`
（内层 `std::map`/`unordered_map`/`unordered_set`）与序列容器共用同一份。

### `PkString` 该怎么用它

```cpp
PkArrayData<std::vector<char16_t>>      // PkString 的 buffer
```

`PkArrayData.cpp` 里**已经有这一条显式实例化**，作为「字符串与容器共用一份地基」
从声明变成**编译期验证过的事实**的证据。R-13 照此形态接。

### detach 语义与 Qt 的对应

| 本地基 | Qt 5.15 的对应物 |
|---|---|
| `PkSharedEmpty()` 返回的进程内共享空 `C`——**默认构造与 moved-from 的源都指向它**，零堆分配 | `QArrayData::shared_null`（实测 Qt：默认构造的空容器 `isDetached()==0`） |
| 引用计数由 `std::shared_ptr` 保证是**原子**的 | `QtPrivate::RefCount`（`QAtomicInt`） |
| 写时分裂判据 **`use_count() > 1`** ——只看引用计数，不看容量、不看是否 const | `if (d->ref.isShared()) reallocData(...)` |
| `PkMut()` 是**唯一**写入口；`PkConst()` **绝不** detach | 非 const `begin()`/`operator[]`/`find()` detach；`constBegin()`/`cbegin()`/全部 const 重载不 detach |

**同一个 `PkArrayData` 实例本身不是线程安全的**（多线程各自持有拷贝、各自 detach
才是安全的）——与 Qt 的隐式共享容器同口径。

### 迁移归属

**归 R-13**（它本来就要动 `pk/string`，零额外越界）。**R-02 不碰 `pk/string/`。**

### `PkList` **没有**用 spec 给的「布局无法统一则各自一套」那个出口

Qt5 的 `QList` 因为 `void* array[]` + O(1) 前插而自带 `QListData::Data`（与
`QArrayData` 不同源）。但选型文档 §5 定死内层用 `std::vector`，**所以 `PkList` 与
`PkVector` 共用同一套地基**。代价见 §6 前两条。

---

## 6. 移交 S 线的已知风险（**这一节最重要**）

### 6.1 `QList<大对象>` 的元素地址稳定性 —— 换成 `std::vector` 之后**不再成立**

Qt5 的 `QList<T>` 在 `sizeof(T) > sizeof(void*)` 时内部存的是**指针数组**，元素各自
单独分配 —— **元素地址跨插入/删除保持稳定**。实测（真 Qt 5.15.7，`sizeof(Big)=32 >
sizeof(void*)=8`）：

```
QList<Big>   prepend 后元素地址稳定 = 1
QVector<Big> prepend 后元素地址稳定 = 0
```

选型文档 §5 已定死内层用 `std::vector`（连续数组），**替换后元素地址不再稳定**——
任何跨插入/删除持有 `T*` / `T&` 的调用点会变成悬垂指针。

**这是对拍证明不了的**（§4 限制 5：内存布局不可观察），只能人工审计。

实测面：`QList<T>` 有 **367 种不同 `T`、3 643 处参数化使用**。剔掉指针（`sizeof==8`，
Qt5 直接内联存）、小标量、智能指针 typedef 之后剩 **1 469 处 / 154 种 `T`**——
这是**上界**，其中最大的一宗 `QList<QString>`(144) 实际上 `sizeof(QString)==8`，
仍走内联存储，应剔除。**精确的审计面要等 S 线拿到能编译的树后按真实 `sizeof` 判。**

头部大对象（这才是 S-02 要逐点看的东西）：

| `QList<T>` 的 `T` | 处数 |
|---|---:|
| `QPointF` | 130 |
| `KoResourceLoadResult` | 112 |
| `KoID` | 68 |
| `KoPathPointData` | 64 |
| `QPair<KoID, KoID>` | 58（两种写法 31 + 27） |
| `QKeySequence` | 53 |
| `KoColor` | 50 |
| `KoSvgTextProperties` | 37 |
| `KoGradientStop` | 36 |
| `QPainterPath` | 31 |

**S-02 替换调用点时需逐点审计「有没有跨修改持引用」。**

### 6.2 `prepend` O(n) 化

`std::vector` 没有 O(1) 前插。`prepend` 全 SRC 65 处、保留范围 **31 处**，绝大多数
是初始化/配置期的一次性调用。**唯二在热路径上的**：

- `libs/image/kis_stroke.cpp:315` —— `m_jobsQueue.prepend(new KisStrokeJob(...))`
- `libs/image/tiles3/kis_memento_manager.cc:315` —— `m_cancelledRevisions.prepend(changeList)`

按 `R2'`（不预先优化）**这一轮什么都不做**，留给 M0 的 benchmark 基线实测。真要动，
优先考虑给这两处换 `std::deque` 内层，而不是给整个 `PkList` 改布局。

### 6.3 CoW 是硬需求，不是优化

`Q_FOREACH`(2 161) + `foreach`(133) = **2 294 处按值拷贝整个容器**。Qt 下靠隐式共享
是 O(1)。**地基不做 CoW，这 2 294 处全部变深拷贝**——这不是"慢一点"，是把一个
O(1) 操作变成 O(n) 并且放在 2 294 个地方。

---

## 7. `qHash` 链路

`PkHash` / `PkSet` 通过**非限定查找 + ADL** 命中 Krita 自己的 `qHash()` 重载：SRC 下
`qHash(` 的重载声明/定义 **19 行、分布在 18 个文件**。**S 线替换容器时这些定义要
原样保留**——它们是调用点侧的代码，不是 Qt 的。

`PkStringHash.h` 额外给 `PkString` 补了一条（`PkHash<PkString,V>` / `PkSet<PkString>`
是高频形态，Qt 那边由 `<QHash>` 传递 include `qstring.h` 提供）。

### 为什么哈希值**不必**与 Qt 逐位相同（附核实依据）

这条不能只写结论，下面三段是实测依据：

1. **Qt 的 `qHash` 返回值在 Qt 自己那里也不可观察。** `QHash` 的迭代顺序未定义并且
   带随机化种子——同一份代码在 Qt 5.15.7 下 keys 打出 `6,7,4,5,2,3,0,1`，5.15.13 下
   打出 `4,5,6,7,0,1,2,3`。**跨补丁版本就变**，不可能有代码正确地依赖它。
2. **已实测核实：全仓没有把 `qHash` 返回值持久化或跨进程比较的地方。** 口径是 SRC 下
   `= qHash(` 与 `return qHash(` 共 **13 处**，其中 12 处是自定义 `qHash` 重载内部的
   转调（`return qHash(x)`）。**唯一存进成员变量的是**
   `libs/pigment/KoColorSpace.cpp:48` `d->idNumber = qHash(d->id);`，而 `idNumber`
   全仓只有 3 处使用（`KoColorSpace_p.h:42` 声明、`KoColorSpace.cpp:48` 赋值、
   `KoColorSpace.cpp:99` 比较），比较处是：

   ```cpp
   return d->idNumber == rhs.d->idNumber && ((p1 == p2) || (*p1 == *p2));
   ```

   **哈希只是快速排除的前置项，后面还有真正的相等比较，碰撞不会导致错误**；
   `idNumber` 也不写进 `.kra`。
3. 所以要求相同的是**签名形状**（`unsigned int qHash(const T &, unsigned int seed = 0)`），
   **不是数值**。`PkStringHash.h` 用 FNV-1a——这是有意的选择，不是将就。

> **给 S 线的提示**：若将来有人新增「把 `qHash` 值写进文件或跨进程传」的代码，
> **上面这条结论立刻失效**，届时必须改成与 Qt 逐位一致的算法。加这类代码的人
> 有义务回来改这一节。

---

## 8. 判据②：真实 Krita 测试类试接

**源树零改动**——只允许 D-23 定义的机械宏改名（规则表就是 `pk/test/graft/rename.sed`，
两条试接链路共用一份）。**不允许任何手工改动测试源或被测实现源**：手工改了源就说明
API 形状不对，而那正是判据②要抓的东西。改名是**编译期抓遗漏的手段**
（`-DPK_TEST_NO_QT_MACRO_ALIASES` 关掉 `QCOMPARE` 别名，漏改的地方直接编不过），
**不是绕过**。

### 交付的试接（1 个，合规）

| 测试源 | 被测实现 | target |
|---|---|---|
| `libs/image/tests/kis_fill_interval_map_test.cpp` + `.h` | `libs/image/floodfill/kis_fill_interval_map.cpp`/`.h`/`_p.h` + `kis_fill_interval.h` | `kritaimage` |

改名的**全部改动 = 7 处 `QCOMPARE` → `PK_COMPARE`**（`.h` 零改动），脚本每次跑都把
逐行 diff 打出来存证。

**它压到什么**（这是它比别的候选都值的原因）：

- **`QHash<int, QMap<int, KisFillInterval>>` —— 嵌套关联容器**（hash of map）
- `QMap<int, KisFillInterval>` —— 有序映射，值是自定义 POD（3 个 `int` +
  `boost::equality_comparable1`）
- `QStack<KisFillInterval>` —— `fetchAllIntervals()` 的返回类型
- `QScopedPointer<Private>` pimpl（`const` 成员，压 `operator->` 的 const 正确性）
- 方法：`find` `insert(k,v)` `begin` `end` `constBegin` `constEnd` `erase(it)` `clear` `append`
- **Qt 关联迭代器语义**：`it->field`（解引用得 **value** 而非 pair）、`*it`、`++it`、
  `it++`、`it != end`、`rowMap->insert(...)`（对 `QHash::iterator` 用 `operator->`
  拿到里层 `QMap` 再调它的方法）、`PK_COMPARE(range.beginIt, range.endIt)`（迭代器互比）
  —— **调研时第一次用 `std::map` 裸包做垫片就是在这里编译失败的**，迭代器包装层是
  为这一格写的
- 测试用 `friend class KisFillIntervalMapTest` 直接摸 `m_d->map` 与
  `Private::findFirstIntersectingInterval` —— 白盒测试，所以 `_p.h` 也编进来了

### 与 `pk/test/graft/graft_run.sh` 的**一处结构差异**

R-11 那份只编**一个 TU**（测试 `.cpp` + binder + driver），因为它选的被测类是
header-only。**容器族没有 header-only 的被测类**，所以本目录的 graft 必须把被测实现
`.cpp` 一起编进同一条命令行。**这是 harness 的形状差异，不是对测试源/实现源的改动**
——那个 `.cpp` 是直接从源树按原路径编的，连副本都没做。

`pk/test/graft/` 是**只读依赖**：本脚本调用它的 `rename.sed` 与 `pk_test_moc.py`，
不修改它们。

### stub（`tests/graft/stubs/`）：**试接脚手架，不是交付的替代品本体**

`kritaimage_export.h`（空壳）· `boost/operators.hpp`（只有 `equality_comparable1`）·
`kis_global.h`（只复刻"把 `kis_assert.h` 带进来"这条传递性）· `kis_assert.h`
（`KIS_SAFE_ASSERT_RECOVER*`，宏体逐字抄自真品——它是带悬空 `if` 的语句宏，形状不能变）·
`kis_debug.h`（哑 `QDebug`；**并复刻 Qt 的 `<QDebug>` → `qhash.h` 传递 include**，
被测实现正是靠它拿到 `QHash`）· `QScopedPointer`（`unique_ptr` 薄壳，**不用别名**，
因为 Qt 的 `QScopedPointer` 不可移动而 `unique_ptr` 可以）。

**不碰**：几何、信号槽、`QVariant`、XML、`QSettings`、线程、`QImage`/`QColor`、
`KisPaintDevice`/`KoColorSpace`、文件 IO。

### 为什么只有一个 —— 其余候选全灭

线级 spec 原文是「挑 **1–2 个**真实 Krita 测试类」，**1 个合规**。判据②在 R-02 阶段
受 R-01 的范围限制：`PkString` 只交付 **14 项**方法，而容器密集的真实 Krita 代码
**几乎必然同时用到不在 14 项内的字符串方法**。逐个实测的结果：

| 候选 | 灭因 |
|---|---|
| `libs/resources/tests/TestResourceSearchBoxFilter.cpp`（容器 34 处，最密） | **双重违规**：impl 用 `.toLower()`×2 `.remove()`×2 `.endsWith()`×1 `.length()`×1 `.split()`×1；测试源自己还用 `toStdString` |
| `libs/global/tests/KisGlobalTest.cpp` | 15 行数据行全是**字符串字面量喂 `addColumn<QString>`**（撞 R-14 缺口，见 §11）；impl 还用 `QFileInfo`/`QDir`/`QRegularExpression` |
| `KisRectsGridTest`（元素就是 `QRect`）、`TestPathSegment`（`QPointF`/`QTransform`） | → R-03（几何） |
| `kis_lockless_stack_test` | `QThreadPool` → R-10 |
| `kis_categories_mapper_test` | 真信号槽 `connect(this, SIGNAL(...), SLOT(...))` → R-05 |
| `kis_simple_math_parser_test` | impl 用 `QRegularExpression`×4 + `QLocale` + `.split()` |

**「真实源文件」这条路（R-01 的试接形态）实测也无解**：按「`.cpp` 与同名 `.h` 都不
include 禁区头 + 容器计数 ≥6」筛出 91 个，取最密的 40 个逐个 `-fsyntax-only`，
**0 个通过**。卡点集中在 `kundo2command.h`(15，命令系统未剥离)、几何(9，R-03)、
`QObject`(4，R-05)、`QVariant`/`QMetaType`(3，R-06)、`Qt` 命名空间头(3)、GUI 类(6)。
最接近的 `libs/pigment/KoCompositeColorTransformation.cpp` 只卡在 QtGlobal 标量垫片
没写全，但它容器用量只有 7，**压不到容器**。

**结论：字符串型容器测试基本全灭，能活下来的必然是非字符串元素类型的**——这正是
选中 `kis_fill_interval_map_test` 的原因。**第二个试接待 R-14 落地后补**，因为容器族
里数据驱动的候选普遍撞 §11 那条缺口。

---

## 9. 交接 R-13 的两条

1. **`replaceInStrings` 的空 `before`** —— 对拍咬出来的**真实不一致**（不是设计上
   接受的等价行为）。Qt 5.15.7 实测把 `after` 插到**每一个 UTF-16 码元之间以及首尾**
   （`"a".replace("","b") == "bab"`），替代品原样返回输入。不复刻的理由：Qt 那套
   会把 🎨 这种代理对**从中间劈开**；Krita 唯一调用点传字面量 `"output_"`，走不到。
   **R-13 决定是补上还是显式不支持。** 全文与谓词细节在 `oracle/R-02.deviation`。
2. **`filter(..., PkCaseInsensitive)` 只做 ASCII 折叠** —— 需要
   `QString::toCaseFolded()` 的对应物，而 `pk/string` 不归 R-02 改。
   **退化方向是安全的**（非 ASCII 退回逐码元精确比较 → 可能漏掉本该匹配的项，
   不会匹配到不该匹配的项），但 **8 个 `filter` 调用点里有 5 个传 `CaseInsensitive`**，
   是资源/标签搜索框，而**Krita 的标签能输中日韩**。测试已经把现状钉住；
   R-13 补上 `toCaseFolded()` 之后，那几条测试应当改成**命中**。

**`PkCaseSensitivity` 枚举的归宿是 `pk/string`**（它是 `Qt::CaseSensitivity` 的替代品，
不是容器的东西）。现在住在 `PkStringList.h` 里只是因为 R-02 不碰 `pk/string`。
枚举值刻意与 Qt 对齐（`CaseInsensitive == 0`、`CaseSensitive == 1`），搬家时不会变。

---

## 10. 怎么跑

```bash
# 从 fork 仓库根执行。

# ① 构建（库 + 13 个单测可执行文件）
cmake -S pk/container -B pk/container/build -G Ninja
cmake --build pk/container/build

# ② 单测：一个测试类一个可执行文件（PK_TEST_MAIN 展开出 main）
for t in pk/container/build/test_*; do "$t"; done
#   13 个套件 / 266 条

# ③ 判据③：库里不得有 Qt 未定义符号。**必须无输出。**
nm -u pk/container/build/libpkcontainer.a | grep -i qt

# ④ 对拍（需要真 Qt 5.15，默认找 PK_QT_PREFIX，见脚本头注释里的三段链接旗标）
pk/container/oracle/run_oracle.sh

# ⑤ 判据②：真实 Krita 测试类试接（要先跑过 ①）
pk/container/tests/graft/graft_run.sh
```

> `pk/container/build/` 与 `oracle/build/` 都被顶层 `.gitignore` 排除。

---

## 11. R-14 的指路

`operator<<(PkTestDataRow &, const char *)` 那条**非模板重载**不在 R-02 范围内，
已拆成 **R-14**（锁 `pk/test`）。

缺了它，`QTest::addColumn<QString>` 配字面量数据行（`<< "foo.tar.gz" << "_"`）会走
模板推导，存进 `std::any` 的是 `const char*`，而 `PK_FETCH` 按 `typeid(PkString)`
取值必然失败。Qt 靠的就是那条非模板重载把字面量在入表前转成 `QString`。

**选下一个试接目标时必须逐行核对数据行**：喂 `QString` **变量**可以（走模板推导存
`PkString`，类型对得上），喂**字面量**不行。完全不用数据驱动的测试类最安全。
