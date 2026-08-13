# pk/pointer

`QSharedPointer<T>` / `QWeakPointer<T>` / `QScopedPointer<T>` / `QScopedArrayPointer<T>`
的 Qt-free 替代品：`PkSharedPointer<T>` / `PkWeakPointer<T>` / `PkScopedPointer<T>` /
`PkScopedArrayPointer<T>`。组合 `std::shared_ptr`/`std::weak_ptr`（不是继承）与裸
`T*`，不依赖任何 Qt 头。设计依据、探针原始输出、判据推导过程见
`docs/superpowers/plans/R-04.md` §0/§1（本文件引用其结论，不重复推导）。

验证入口：

```
bash tests/run_tests.sh          # 23 个单测（PK_* harness）
bash oracle/run_oracle.sh        # 逐输入对拍：Qt 侧 vs Pk 侧
bash graft/graft_check.sh        # 两个真实 Krita 测试类零改动试接
bash oracle/mutate/inject.sh list   # 变异测试：9 条注入清单
```

## 1. API 面清单

出处：`.superpowers/sdd/R-04/usage-table.md` §2/§6。用量数字口径两层：**保留范围**
= 选型文档 §1 的目录前缀清单、排除 `tests/`+`benchmarks/`+`autotests/`；**全仓**
= `git ls-files` 全部 `.cpp/.cc/.h/.hpp` 不过滤。别名闭包已展开（`QSharedPointer`
有 104 个 `typedef`）。数字是任务实施时的实测快照，会随 D 线删代码漂移——要精确
数字现场用 `usage-table.md` 记录的方法重数，不要抄这里。

### 1.1 `PkSharedPointer<T>`

| API | 保留范围用量 |
|---|---|
| `T *data() const` / `T *get() const` | 13 / 1 |
| `bool isNull() const` | 7 |
| `operator RestrictedBool() const` | ≥117 处布尔语境 |
| `bool operator!() const` | ≥27 |
| `T &operator*() const` / `T *operator->() const` | ≥11 / ≥711 |
| `void reset()` / `void reset(T*)` / `template<class D> void reset(T*, D)` | 12 / — / 1 |
| `void clear()` | 19 |
| `template<class X> PkSharedPointer<X> staticCast() const` | 3 |
| `template<class X> PkSharedPointer<X> dynamicCast() const` | 86 |
| `static PkSharedPointer create(Args&&...)` | 38 |
| 默认构造 / 从 `T*` 构造 / 2 参 `(T*, Deleter)` 构造 | ≥455 / ≥115 / 1 |
| 拷贝、移动构造与赋值；跨类型（派生→基类） | ≥1117 / `std::move` 1 |
| 从 `PkWeakPointer<X>` 构造（提升） | 1 处模板，覆盖 13 个调用点 |
| 赋 `nullptr` | ≥18 |
| 自由 `operator==` / `!=`：与另一智能指针、与裸指针、与 `nullptr` | 2 / 6 / 0 |
| 自由 `pkSharedPointerCast` / `pkSharedPointerDynamicCast` | 18 / 30 |
| 隐式转换成 `PkWeakPointer<T>` | 1 处模板 |
| `qHash` 钩子（`PkHash<KoPatternSP, QString>`） | 2 |

### 1.2 `PkWeakPointer<T>`

| API | 保留范围用量 |
|---|---|
| `PkSharedPointer<T> toStrongRef() const` | 12 |
| `bool isNull() const` | 0（语义地基，Qt 也公开，实现） |
| `operator RestrictedBool()` / `bool operator!()` | 1 / 2 |
| 默认构造、拷贝构造/赋值、赋 `nullptr` | 1 / 2 |
| 从 `PkSharedPointer<X>` 隐式构造 | 1 处模板 |
| 自由 `operator==` / `!=`（与 `PkSharedPointer` 比） | 探针 P12 证明 Qt 有；实现 |

存储形态：`std::weak_ptr<T> m_weak; T *m_value = nullptr;` 两个成员都要——Qt 的
`QWeakPointer` 同时存控制块与 value，`isNull()` 要能表达"控制块还在、value 是空"
这一格（判据 A 第二面），只存 `std::weak_ptr` 表达不出这一格。

`operator->` **不提供**——见 §1.5。

### 1.3 `PkScopedPointer<T>`

| API | 保留范围用量 |
|---|---|
| `T *data() const` / `T *get() const` | ≥61 / 5 |
| `bool isNull() const` | 2 |
| `void reset(T *other = nullptr)` | ≥65 |
| `T *take()` | 1 |
| `T &operator*() const` / `T *operator->() const` | ≥66 / ≥4398 |
| `operator RestrictedBool()` / `bool operator!()` | ≥30 / ≥162 |
| 默认构造 / 从 `T*` 构造 | 375 / 35 |
| 自由 `operator==` / `!=`（与另一个 `PkScopedPointer`、与 `nullptr`） | 2 / 1 |
| 拷贝与移动一律 `= delete`（判据 C） | — |

### 1.4 `PkScopedArrayPointer<T>`

独立类型，**不**继承 `PkScopedPointer`——按用量表收窄过 API 面，真 Qt 里
`QScopedArrayPointer` 公开继承 `QScopedPointer` 会连带暴露 `isNull/take/swap/
operator*/operator->`，Krita 全仓对数组指针的调用点用不到它们。

| API | 保留范围用量 |
|---|---|
| 从 `new T[N]` 构造 | 9 |
| `T *data() const` | 26 |
| `T &operator[](int i) const` | 4 |
| `void reset(T *other = nullptr)` | 1 |
| 拷贝与移动 `= delete`（判据 C） | — |

## 2. 有意不实现的清单

判据①「一项不多」：以下全部实测保留范围/全仓用量为 0（或经复核后确认用量表
原记录是受体误判）。**S 线撞上其中任何一项时编译立刻报错，那是设计意图，不是
缺陷**——碰到时先查这张表，不要先怀疑替代品缺了什么。

| 未实现 | 保留范围 / 全仓 用量 | 备注 |
|---|---|---|
| `QSharedPointer::swap()` | 0 / 0 | `.swap(` 命中全是容器与 `std::unique_ptr` |
| `QSharedPointer::constCast()` · `qSharedPointerConstCast` | 0 / 0 | `QSharedPointer<const T>` 全仓 0，自洽 |
| `QSharedPointer::objectCast()` · `qSharedPointerObjectCast` · `qobject_cast` 用于智能指针 | 0 / 0 | 依赖 `QObject`，归 R-05 |
| `QSharedPointer::toWeakRef()` | 0 / 1 | 全仓唯一一处不在保留范围；隐式转换成 `PkWeakPointer` 已覆盖同等能力 |
| `QSharedPointer<T[]>` · `QEnableSharedFromThis` | 0 / 0 | |
| `QSharedPointer::operator<` | 0 / 0 | 没人拿它当 map key |
| `QScopedPointer` 第二模板参数 `Cleanup` · `QScopedPointerDeleter/PodDeleter/ArrayDeleter` | 0 / 0 | grep 命中经核实全是嵌套模板里的逗号误报 |
| `QScopedPointer::swap()` | 0 / 0 | |
| `QScopedArrayPointer` 的 `isNull/take/swap/operator*/operator->` | 0 / 0 | |
| `QWeakPointer::lock()` · `clear()` · `data()` | 0 / 0（`clear` 全仓 1） | `lock()` 在 Qt 里是 `toStrongRef()` 的别名 |
| `PkWeakPointer::operator->` | 保留范围「用量 2」是受体误判，实测 0 | 真 Qt5 `QWeakPointer::operator->` 整个包在 `#if defined(QWEAKPOINTER_ENABLE_ARROW)` 里（`qsharedpointer_impl.h:666`），该宏 Qt5 自身从未定义过——对非 `QObject` 的 `T`，`weakPtr->foo()` 在真 Qt5 上默认编不出来。用量表原记的两处点名调用点（`kis_paintop_settings.cpp:579`）实为 `toStrongRef()` 取到的**强**引用局部变量的 `operator->`，与 `QWeakPointer::operator->` 无关。详见 `oracle/pointer.deviation` 覆盖度限制第 4 条、`task-1-report.md`「Task 2 修复轮 1 补记」 |
| `operator==(const PkWeakPointer<T>&, const PkSharedPointer<X>&)` | 保留范围用量 0 | 探针 P12 证明 Qt 有这个重载，但只证明 Qt 有、不证明 Krita 用；用量表 §2.3 的 `QWeakPointer` 一节没有 `operator==` 这一行。判据①优先于计划 Step 4 的示例代码——那段是计划的示例写多了。详见 `task-1-report.md`「评审后修订②」 |

## 3. 明确不属于本任务

以下四项选型文档 §1/§5 只点了名、没给方向，是要报给主会话的归口缺口，不是本
任务自己吞掉的东西：

| 能力 | 保留范围 / 全仓（文件 / 次数） | 为什么不归 R-04 |
|---|---|---|
| `QPointer` | 75 / 157（全仓 226 / 544） | 地基是 `QObject` 销毁通知，不是引用计数；四分之三用量在保留范围外。跟去-QObject / 信号槽（R-05）走 |
| `QSharedData` | 22 / 50 | 侵入式 COW 三件套，`operator->` 触发 detach，落点 R-13 / R-02，不与本任务共用地基 |
| `QSharedDataPointer` | 19 / 30 | 同上 |
| `QExplicitlySharedDataPointer` | 6 / 17 | 同上 |

## 4. 四条设计判据与探针出处

探针源：`probe/qt_semantics_probe.cpp`；复现命令：`bash probe/run_probe.sh`
（真 Qt 5.15.13，`-fPIC` 不能省，否则每一个编译探针都会假 FAIL）。原始输出与
编译矩阵完整贴在 `docs/superpowers/plans/R-04.md` §0，这里只列结论：

- **判据 A**（空按 value 判，不按控制块判）：`isNull()` 是 `data()==nullptr`，
  不是 `use_count()==0`；`PkWeakPointer::isNull()` 是
  `m_weak.expired() || m_value==nullptr`。出处：探针 P2、`qsharedpointer_impl.h:309/562`。
- **判据 B**（`operator bool` 是隐式 safe-bool，不是 `explicit operator bool`）：
  四个类型一律 `operator RestrictedBool`。出处：探针 P11、D2、编译矩阵
  `QSharedPointer p==1` FAIL。
- **判据 C**（`PkScopedPointer`/`PkScopedArrayPointer` 不可拷贝不可移动）：
  拷贝/移动构造与赋值四项 `= delete`。出处：探针 P10。
- **判据 D**（空指针 + 自定义 deleter 时 Qt 照样调用 deleter）：2 参构造/2 参
  `reset` 直接转发给 `std::shared_ptr` 对应构造，不因判据 A 短路空指针。
  出处：探针 D3。

设计决定"组合，不是继承"的三点理由见 `docs/superpowers/plans/R-04.md` §0 末段，
不在此重复。

## 5. 偏离清单

`oracle/pointer.deviation`：当前 **0 条真实偏离**（`DIFF total=1445276
mismatch=0`）。该文件同时记录：两条评审裁决删除的 API（`PkWeakPointer::
operator->`、`operator==(PkWeakPointer,PkSharedPointer)`，见 §2）、8 条覆盖度
限制、tag 粒度约束，以及 Task 4 的变异测试验证记录（见 §8）。

## 6. 两个局部垫片（试接产物，非本任务交付范围）

`graft/stubs/` 下两个局部垫片是 Task 3 试接两个真实 Krita 测试类时为了绕开
"零改动"判据而做的临时垫片，**不是** `pk/pointer` 的一部分，真品到位后应删除：

| 垫片 | 归口 | 用量（试接闭包内实测） | 退场条件 |
|---|---|---|---|
| `graft/stubs/QAtomicInt` | R-10（Q-8 并发与同步原语） | `ref()`/`deref()`/`operator int()`/`fetchAndAddOrdered(int)`，`load()`/`store()` 零调用 | R-10 交付真正的 `PkAtomicInt` 后删除本文件，两个候选改用真品重新试接确认仍跑绿 |
| `graft/stubs/qConstOverload` | R-18（QtGlobal 标量设施） | 仅空参数包形态 `qConstOverload<>(&Struct::overloaded)`（`KisMplTest.cpp` 三处） | R-18 交付真正的 `pkConstOverload` 后删除本文件，同上重新试接确认 |

## 7. compat 垫片互不透传（Task 3 发现，6 处）

真 Qt 靠头文件之间互相 `#include` 把类型隐式带进来（`<QtTest/qtest.h>` 直接
透传 `QObject`/`QScopedPointer`/`QRect`，`<QDebug>` 直接透传 `QSharedPointer`/
`QVector`），而我们的 compat 垫片彼此不 `#include` 对方，只转发自己名义上负责
的那一个类型。试接两个真实 Krita 测试类时踩到 6 处，**没有一处该由 `pk/pointer`
自己解决**——归 `pk/test`/`pk/log`/`pk/container`/`pk/geometry` 各自的垫片，或
纯标准库缺口：

| 缺口 | 真 Qt 的透传链 | 我们缺的复刻 |
|---|---|---|
| `QAtomicInt` 未显式 `#include` 就被用 | `kis_shared_ptr.h:479` 靠 `kis_shared.h` 的先行 include | — |
| `QScopedPointer`/`QVector` 未显式 `#include` | `QScopedPointer` 经 `qtest.h:58`→`qobject.h:53`→`qscopedpointer.h`；`QVector` 经 `qdebug.h:51` 直接 include | `pk/pointer/compat/QScopedPointer`、`pk/container/compat/QVector` 均不转发 |
| `QObject` 未随 `<QTest>` 透传 | `qtest.h:58` 直接 `#include <QtCore/qobject.h>` | `pk/test/compat/QTest` 只转发 `PkTest.h`/`PkTestData.h` |
| `QSharedPointer` 未随 `<QDebug>` 透传 | `qdebug.h:54` 直接 `#include <QtCore/qsharedpointer.h>` | `pk/log/compat/QDebug` 不做这层转发（预期内，`pk/log` 本不该知道 `pk/pointer`） |
| `QRect` 未随 `<QTest>` 透传 | `qtest.h:65` `#include <QtCore/qrect.h>` | `pk/test/compat/QTest` 不转发几何类型 |
| `std::accumulate`（`<numeric>`）未被测试源自己 `#include` | Qt 重量级头之间互相 include 带进了 `<numeric>` | 标准库头的透传缺口，与 Qt 类型替代无关 |

`graft/graft_check.sh` 用 `-include` 编译参数顶掉了这 6 处，不是对调用点的改动
（"零改动"判据不允许改测试源自己的 include 列表）。**这不会拖慢 S-00**：真实
剥离时调用点会加上自己需要的 `#include`（或构建系统 force-include）。**给 S-00
的提醒**：批量替换调用点时，如果某处原来靠 Qt 头的隐式透传拿到某个类型，替换
后大概率编不过——缺失的不是"这个类型没实现"，是"这处调用点自己没写
include"。两个候选一共踩到 6 处，说明这类隐式透传在 Krita 里*不*罕见，S-00 应
把"编不过就先看是不是隐式透传断了"列进排查清单第一条，不要先怀疑替代品本身。

## 8. 变异测试（Task 4）

9 条注入（`oracle/mutate/inject.sh`，逐条论证与原始输出见 `task-4-report.md`），
**全部被抓到**：7 条被对拍（`mismatch` 从 0 变非 0，或对拍进程直接崩溃）与/或
单测抓到；2 条是判据 B/C 的编译期性质，被 `tests/shape_asserts.cpp` 的
`static_assert` 抓到（编译直接失败）。其中 2 条只被对拍抓到、现有单测没覆盖到
——已记入 `oracle/pointer.deviation` 覆盖度限制第 9 条，是单测覆盖度的真实缺口
（不是对拍缺口，对拍两条都抓到了）：

- **resetSame**：`tests/` 没有"reset 成自己当前持有的指针"这个用例。
- **析构持有真实数组数据的 `PkScopedArrayPointer`**：唯一相关单测在析构前先
  调用了 `reset()`，析构函数本身在"持有真实数据"状态下从未被单测执行过。

## 9. 覆盖度限制

完整清单见 `oracle/pointer.deviation` 末尾，摘要：

1. **多线程不覆盖**：对拍单线程，引用计数原子性继承自 `std::shared_ptr` 自身，
   不是本任务实现的东西。
2. **UB 形态不覆盖**：double free、reset 成已被别的智能指针拥有的裸指针——这
   类输入本身是未定义行为，"两侧字节相同"判据在 UB 下没有意义。
3. **编译期性质不在对拍里**：判据 C 由 `tests/shape_asserts.cpp` 的
   `static_assert` 覆盖，对拍是运行期的，压不到编译期这条。
4. **§2「有意不实现」的 API 不在对拍里**：两侧 API 面不对称，没法比。
5. **`QObject` 相关不覆盖**：`objectCast` 与依赖 `QObject::destroyed` 的弱引用
   失效路径不在范围——`Payload` 不是 `QObject`，探针与用量表都没有这个场景。
6. **单元测试覆盖度**（Task 4 新增，见 §8）：`tests/` 不覆盖 resetSame 与
   "析构持有真实数组数据"两个输入形态，对拍的组合爆破覆盖了它们。
