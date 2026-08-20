# pk/time —— QDateTime / QElapsedTimer 的零 Qt 替代品

`pk/time` 是 R-16 交付 + R-29 归位的组合：`PkElapsedTimer`（单调计时，
`std::chrono::steady_clock` 支撑）+ `PkDateTime`（墙钟时间核心值语义、字符串转换与
calendar 运算，内部为 `PkDate` + `PkTime` 模型）+ `PkDate`/`PkTime`（calendar 版，
对齐 Qt QDate/QTime）。独立薄壳工程，C++17，不接主构建，不 `find_package(Qt...)`。

R-16 拆成 5 个 Task：Task 1 `PkElapsedTimer`（commit `9de59a1`）、Task 2
`PkDateTime` 核心值语义（`1778c32`）、Task 3 字符串转换 + `DateParser` driver
（`48f9b70`）、Task 4 compat 垫片 + oracle 对拍（`1e0e4e5`，修复轮 1 `ad3ab2e`）、
Task 5 试接 + 本文档 + 收口验证。**R-29（2026-08-19）**把 `PkDateTime` 从 epoch 版
（内部 `std::chrono::system_clock::time_point`）改为 **calendar 版**（内部 `PkDate` +
`PkTime`，对齐 Qt QDateTime 的 date+time 模型），并新增 `PkDate`/`PkTime` 两个独立类
（原 variant 占位类删除，时间类型统一归位 pk/time）。epoch 换算 API 全部保留（经内部
`toTimePoint()`/`fromTimePoint()` 走 `mktime`/`localtime_r`，LocalTime 语义不变）。

## 1. API 清单

### `PkElapsedTimer`（对应 `QElapsedTimer`）

| 方法 | 对应 Qt API | 真实调用点数 |
|---|---|---:|
| `start()` | `QElapsedTimer::start()` | 12 |
| `elapsed()` | `QElapsedTimer::elapsed()` | 12 |
| `restart()` | `QElapsedTimer::restart()` | 4 |
| `nsecsElapsed()` | `QElapsedTimer::nsecsElapsed()` | 5 |
| `isValid()` | `QElapsedTimer::isValid()` | 1 |
| `invalidate()` | `QElapsedTimer::invalidate()` | 1 |
| 默认构造 | `QElapsedTimer()` | — |

**不做**（保留范围内零调用点）：`hasExpired()`、`msecsTo()`、`secsTo()`、
`msecsSinceReference()`、`operator==`/`operator!=`/`operator<`、`clockType()`、
`isMonotonic()`。

### `PkDateTime`（对应 `QDateTime`）

静态构造/工厂：

| 方法 | 对应 Qt API | 真实调用点数 |
|---|---|---:|
| `currentDateTime()` | `QDateTime::currentDateTime()` | 12 |
| `currentDateTimeUtc()` | `QDateTime::currentDateTimeUtc()` | 2 |
| `fromMSecsSinceEpoch(qint64)` | `QDateTime::fromMSecsSinceEpoch(qint64)` | 3 |
| `fromSecsSinceEpoch(qint64)` | `QDateTime::fromSecsSinceEpoch(qint64)` | 4 |
| `fromString(s)` | `QDateTime::fromString(s)`（默认 `Qt::TextDate`） | 2 |
| `fromString(s, customFormat)` | `QDateTime::fromString(s, QString)` | 5（`"yyyy"`/`"yyyy-MM"`/`"yyyy-MM-dd"`/`"yyyy-MM-ddThh:mm"`/`"yyyy-MM-ddThh:mm:ss"` 各 1，均来自 `kis_meta_data_parser.cc::DateParser::parse`） |
| `fromString(s, DateFormat::ISODate)` | `QDateTime::fromString(s, Qt::ISODate)` | 2 |

实例方法：

| 方法 | 对应 Qt API | 真实调用点数 |
|---|---|---:|
| `toString()` | `QDateTime::toString()`（默认格式） | 3 |
| `toString(DateFormat::ISODate)` | `QDateTime::toString(Qt::ISODate)` | 3 |
| `toString(DateFormat::RFC2822Date)` | `QDateTime::toString(Qt::RFC2822Date)` | 2 |
| `toString(DateFormat::ISODateWithMs)` | `QDateTime::toString(Qt::ISODateWithMs)` | 1 |
| `isValid()` | `QDateTime::isValid()` | 2 |
| `isNull()` | `QDateTime::isNull()` | 2 |
| `secsTo(const PkDateTime&)` | `QDateTime::secsTo(const QDateTime&)` | 3 |
| `toSecsSinceEpoch()` | `QDateTime::toSecsSinceEpoch()` | 3 |
| `operator==`/`operator!=` | 同名 | 1 |
| 默认构造 | `QDateTime dt;` | 多处（`KisResourceStorage`/`KisMemoryStorage` 等结构体成员） |
| `date()` | `QDateTime::date()` | 0（R-29 新增，对齐 Qt calendar） |
| `time()` | `QDateTime::time()` | 0（R-29 新增） |
| `addDays(qint64)` | `QDateTime::addDays(qint64)` | 0（R-29 新增） |
| `addMonths(int)` | `QDateTime::addMonths(int)` | 0（R-29 新增） |
| `addYears(int)` | `QDateTime::addYears(int)` | 0（R-29 新增） |
| `addSecs(qint64)` | `QDateTime::addSecs(qint64)` | 0（R-29 新增） |
| `addMSecs(qint64)` | `QDateTime::addMSecs(qint64)` | 0（R-29 新增） |
| `daysTo(const PkDateTime&)` | `QDateTime::daysTo(const QDateTime&)` | 0（R-29 新增） |
| `msecsTo(const PkDateTime&)` | `QDateTime::msecsTo(const QDateTime&)` | 0（R-29 新增） |
| `operator<` | `QDateTime::operator<` | 0（R-29 新增） |
| `setDate(const PkDate&)` | `QDateTime::setDate(const QDate&)` | 0（R-29 新增） |
| `setTime(const PkTime&)` | `QDateTime::setTime(const QTime&)` | 0（R-29 新增） |

### `PkDate`（对应 `QDate`，R-29 新增）

| 方法 | 对应 Qt API | 真实调用点数 |
|---|---|---:|
| `PkDate()` | `QDate()` | — |
| `PkDate(int y, int m, int d)` | `QDate(int, int, int)` | 0（R-29 新增） |
| `year()`/`month()`/`day()` | `QDate::year()/month()/day()` | 0（无效返回 0，对齐 Qt） |
| `dayOfWeek()` | `QDate::dayOfWeek()` | 0（1=Mon..7=Sun） |
| `dayOfYear()` | `QDate::dayOfYear()` | 0 |
| `daysInMonth()`/`daysInYear()` | `QDate::daysInMonth()/daysInYear()` | 0 |
| `addDays(qint64)`/`addMonths(int)`/`addYears(int)` | `QDate::addDays/addMonths/addYears` | 0 |
| `daysTo(const PkDate&)` | `QDate::daysTo` | 0 |
| `toJulianDay()`/`fromJulianDay(qint64)` | `QDate::toJulianDay/fromJulianDay` | 0（julianDay 模型） |
| `isValid()`/`isNull()` | `QDate::isValid()/isNull()` | 0 |
| `operator==`/`!=`/`<`/`<=`/`>`/`>=` | 同名 | 0 |
| `isLeapYear(int)` | `QDate::isLeapYear(int)` | 0 |
| `currentDate()` | `QDate::currentDate()` | 0 |

### `PkTime`（对应 `QTime`，R-29 新增）

| 方法 | 对应 Qt API | 真实调用点数 |
|---|---|---:|
| `PkTime()` | `QTime()` | — |
| `PkTime(int h, int m, int s = 0, int ms = 0)` | `QTime(int, int, int, int)` | 0 |
| `hour()`/`minute()`/`second()`/`msec()` | `QTime::hour/minute/second/msec` | 0（无效返回 -1，对齐 Qt） |
| `isValid()`/`isNull()` | `QTime::isValid()/isNull()` | 0 |
| `setHMS(int, int, int, int)` | `QTime::setHMS` | 0 |
| `addSecs(int)`/`addMSecs(int)` | `QTime::addSecs/addMSecs` | 0（跨午夜回绕） |
| `secsTo(const PkTime&)`/`msecsTo(const PkTime&)` | `QTime::secsTo/msecsTo` | 0（无效返回 0） |
| `fromMSecsSinceStartOfDay(int)`/`msecsSinceStartOfDay()` | `QTime::fromMSecsSinceStartOfDay/msecsSinceStartOfDay` | 0 |
| `currentTime()` | `QTime::currentTime()` | 0 |
| `operator==`/`!=`/`<`/`<=`/`>`/`>=` | 同名 | 0 |

`DateFormat` 枚举（`ISODate`/`RFC2822Date`/`ISODateWithMs`）对应真实调用点里
`Qt::ISODate`/`Qt::RFC2822Date`/`Qt::ISODateWithMs` 三个值——`PkDateTime.h` 底部
`namespace Qt` 块直接提供这三个 constexpr 别名值，调用点 `toString(Qt::ISODate)`
一字不改。`compat/QDateTime` 只留 `#define QDateTime PkDateTime`（别名值已由
PkDateTime.h 提供）。

**不做**（保留范围内零调用点，Qt 头文件通读 + 逐处核实后确认没有余项）：
`setTimeSpec()`/`timeZone()`/`toTimeZone()`/`toOffsetFromUtc()`/`toUTC()`/
`toLocalTime()`/`fromString(..., QCalendar)`/`QDateTime::toTimeSpec`、
`QDate::toString` 系列、`QTime::toString` 系列、`QDate::startOfDay/endOfDay`。

**R-29 登记偏离**（探针证实，安全方向）：
- `QDateTime()` 无效实例的 `addDays/addMonths/addYears` 在 Qt 会「愈合」到 julianDay=0
  （date -4714-11-24）；`PkDateTime` 返回无效实例（更安全，真实调用点不会对无效实例调
  `add*`）。
- `toMSecsSinceEpoch()` 对负 epoch 亚秒输入（如 -1999ms）会丢失亚秒精度（返回 -999 而非
  -1999）——calendar 模型（存正 msec 余数 + 整秒重建）的本质限制，真实调用点不做负亚秒
  epoch，`toSecsSinceEpoch`/`secsTo` 不受影响。
- `operator<` 对混合有效/无效实例违反严格弱序（无效既非 `<` 也非 `>` 有效，但 `==` 说它们
  不等）；kernel 用途是纯有效比较，`std::set/map` 不会混合有效/无效。

**用量表口径**：`grep -l`（文件）+ `grep -n`（行）现场数，排除 `tests/`/
`benchmarks/`；`QMetaType::QDateTime`（`kis_meta_data_type_info.cc`、
`kis_meta_data_value.cc`、`kis_exiv2_common.h` 三处）是 `QVariant` 的类型标签
枚举比较，不构造/操作 `QDateTime` 实例，已排除在上表之外（详见下方「架构注记」）。
完整现场测量与逐处核实过程见 `docs/superpowers/plans/R-16.md`「真实 API 用量表」
一节。

## 2. 探针结果（原始输出摘录）

完整原始输出：`docs/superpowers/plans/R-16-probe/probe_time_output.txt`（38 行）。
关键几行（钉死实现语义用的）：

```
QDateTime() isNull                       => [true]
QDateTime() isValid                      => [false]
toString() default                       => [Mon Jan 15 12:30:45 2024]
toString(Qt::ISODate)                    => [2024-01-15T12:30:45]
toString(Qt::RFC2822Date)                => [15 Jan 2024 12:30:45 -0800]
toString(Qt::ISODateWithMs)              => [2024-01-15T12:30:45.000]
fromMSecsSinceEpoch(0) timeSpec == Qt::LocalTime ? true
a.secsTo(b) = 500 (type width test via qint64 cast)
b.secsTo(a) = -500
QElapsedTimer() isValid before start() = false
elapsed() after 20ms sleep = 20
nsecsElapsed() after 20ms sleep (ns) = 20081563
elapsed() right after restart() following 5ms sleep (should be ~0, not ~5) = 0
after invalidate(): isValid = false
```

对应到实现：`isNull() == !isValid()`（不是 AND 语义，`qdatetime.h:77-78` 头文件
事实）、`secsTo()` 是"后减前"（`a.secsTo(b) == b - a`）、`fromMSecsSinceEpoch`
单参默认 `timeSpec()` 是 `LocalTime`（日历字段渲染/解析已按裁决用 C 库
`localtime_r`/`mktime` 落地为 LocalTime，详见 `PkDateTime.h`/`.cpp` 顶部注释与
`oracle/R-16.deviation`「LocalTime 对齐」一节）、`restart()` 真的把计时基点归零
（不是只返回旧值）。

## 3. 对拍结果

`pk/time/oracle/difftest_time.cpp` ↔ 真 Qt 5.15.7，**多时区对拍**（
`run_oracle.sh` 在 `TZ=America/Los_Angeles` 与 `TZ=Asia/Shanghai` 各跑一遍）：

| TZ | total | mismatch | DIFFTAG 数 |
|---|---|---:|---:|
| America/Los_Angeles | 2228 | 186 | 18 |
| Asia/Shanghai | 2228 | 189 | 18 |

两个时区是**同一组 18 条 DIFFTAG**（tag 集合一致，个别 tag 计数因历史 tzdata
差异略不同），全部逐条登记在 `pk/time/oracle/R-16.deviation`。

**时区口径（2026-08-18 裁决）**：`PkDateTime` 的日历字段渲染/解析已从首版的
固定 UTC 改成 **LocalTime**，对齐真 Qt 默认 `timeSpec()==Qt::LocalTime`
（C 库 `localtime_r`/`mktime` 读系统 `TZ`）。早期 `export TZ=UTC` 的对拍会把
两侧的"本地时区"与"UTC"重合，系统性遮蔽"默认 LocalTime vs 实现 UTC"这类差异；
现在改跑两个非 UTC 时区，这类差异真的进 mismatch 表。C 库 tzdata 与 Qt 自带
tzdb 的近似留下两类**历史**差异（历史 DST：1969-06-15 在 LA；历史 LMT：
1900-01-01 在 Shanghai），已在 deviation「LocalTime 对齐」一节如实登记——现代
日期（1970+）两侧一致。

对拍首轮（当时 TZ=UTC）跑出 `mismatch=347`，其中一类差异（`fromString` 系列在
年份落在 `[1678,2261]` 安全窗口之外时会有符号整数回绕，给出"看似合法、实际错得
离谱"的值）被判定为真实根因缺口并在 Task 4 修复（`PkDateTime.cpp` 新增
`yearRepresentable()` 校验）：修复后 `347 → 185`，剩余偏离全部逐条登记，判定均
为"可接受"（真实调用点不会产生这些手挑对抗用例才能触发的极端输入）。

18 条 DIFFTAG 归为三类：
1. **极端 epoch 数值**（`fromMSecsSinceEpoch`/`fromSecsSinceEpoch` 的
   `extreme`/`int64-max`/`int64-min`/`post-1970`/`pre-1970`，7 条）——两侧内部
   精度不同（Qt 存毫秒、`PkDateTime` 存纳秒），各自的整数回绕点不同，都是
   "未定义范围输入下尽力而为"，不是谁错；真实调用点不会传这个量级。
2. **边界长度/字段精确校验**（`fromString-ISODate len=18/19/20`、
   `fromString-custom fmt=.../len=19`、`fromString-default tokens=5`，共
   9 条）——`PkDateTime` 严格按 `PkDateTime.h` 类注释声明的"只认 5 个具体格式串
   /不做通用 format-token 解析器/不做逐月天数精确校验"设计拒绝，真 Qt 的解析器
   更宽松；真实调用点只产出标准长度串，不受影响。其中 `fromString-custom
   .../len=19` 在 LA 多 1 条（历史 DST，1969-06-15）。
3. **上一类根因的渲染回声 + 历史 LMT**（`toString-default`/`toString-fmt` 的
   `post-1970`/`pre-1970`，8 条）——`post-1970` 是对象本身已因超范围回绕而错误、
   `toString()` 只是如实渲染；`pre-1970` 在 Shanghai 额外多 1 条（1900-01-01 的
   历史 LMT +08:05:43 vs Qt 的 +08:00）。

判据有效性另外用四组注入实验自证（改完即刻还原）：A/B 两组松开长度判断
（`!=` 改 `<`）、C 组 `secsTo()` 符号取反、D 组 `elapsed()` 换算系数错三个
数量级——四组全部命中预期数量的未声明 DIFFTAG（B 组net mismatch 数字不变但
DIFFTAG 集合确实变了，印证"tag 集合比聚合计数更可靠"）。多时区判据本身的有效性
由两条历史 tzdata 差异自证（只在对应时区进入 mismatch 表，单时区跑不出来）。
完整推导见 `pk/time/oracle/R-16.deviation`。

## 4. 试接结果（本 Task）

brief 原始指名的两个文件——`libs/resources/KisStoragePlugin.cpp`（target
`kritaresources`）与 `libs/global/KisUsageLogger.cpp`（target `kritaglobal`）——
**都实测过，都编不过**，且都不是卡在 `PkDateTime`/`PkElapsedTimer` 本身：

- **`KisStoragePlugin.cpp`**：`KisStoragePlugin.h` → `KisResourceStorage.h`
  第 13 行 `#include <QDateTime>`——**这一行已经能通过 R-16 的垫片解析**，卡点
  在紧接着第 16 行 `#include <KoResource.h>` → 第 10 行 `#include <QImage>`：
  `QImage` 没有垫片（R-15，`NOT_STARTED`）。
- **`KisUsageLogger.cpp`**：文件第 8 行就是 `#include <QScreen>`，在第 11 行
  `#include <QDateTime>` **之前**。`QScreen` 属于 Qt Widgets/GUI 栈，当前没有
  任何 R 任务声明覆盖。

两处都用 `check_expect_fail`（照抄 `pk/config/tests/graft/graft_check.sh` 的
手法：断言错因正则 + 记录归属，而不是让脚本一直红或悄悄放宽判据）登记进
`graft_check.sh`——一旦 R-15 交付 `QImage` 或未来有任务覆盖 `QScreen`，脚本会
自动报"缺口已消失，请更新登记"。

为了不让"两个 target 全部 `EXPECT_FAIL`、一次真正的 `PASS` 都没有"，额外做了
一轮系统性搜索：自动扫了全仓（排除 `tests/`/`benchmarks/`）全部 **37 个**真实
`QDateTime`/`QElapsedTimer` 调用点文件，**零个**能直接零改动编过；对其中十余个
手工往深处试（叠加已有先例的 stub 头：`kritaimage_export.h`/
`kritaglobal_export.h`/`KoConfig.h`/`config-memory-leak-tracker.h` 这类构建期
生成头，均是既有 stub 先例的自包含副本，不是新造出来放宽判据），逐一撞到
`QImage`/`QScreen`/`QLineF`/`QPolygon`/`pugixml.hpp`（第三方依赖，探测到的路径
挂在 `/tmp` 下不可复现）/freetype+harfbuzz 字体栈/
`KoColorConversionTransformation.h`（Pigment 色彩子系统）等同样未交付的缺口。

**唯一编过的**：`libs/image/kis_timed_signal_threshold.cpp`（target
`kritaimage`）。该文件只包 `kis_timed_signal_threshold.h`
（`QScopedPointer`/`QObject`/`kritaimage_export.h`）与
`QElapsedTimer`/`kis_debug.h`（`QDebug`/`QLoggingCategory`/
`kritaglobal_export.h`），真实调用点用到 `PkElapsedTimer` 的 `isValid()`/
`start()`/`elapsed()`/`invalidate()` 四个成员函数——恰好是 Task 1 交付的核心
API 面，零改动、且只叠加了两个既有先例 stub（`kritaimage_export.h`/
`kritaglobal_export.h`，自包含副本存在 `pk/time/tests/graft/stubs/`），真正跑通
整条 include 链。

`graft_check.sh` 现在跑一个 `check_pass`（`kis_timed_signal_threshold.cpp`）+
两个 `check_expect_fail`（`KisStoragePlugin.cpp`、`KisUsageLogger.cpp`），三个
不同 target（`kritaimage`/`kritaresources`/`kritaglobal`），脚本本身以 exit 0
收尾（`check_expect_fail` 按登记失败也算成功，`check_pass` 才要求真的编过）。

## 5. 架构注记：与 `pk/variant/PkAuxTypes.h` 的 `PkDate`/`PkTime`/`PkDateTime` 重名

`pk/variant/PkAuxTypes.h`（R-06 交付、已 `VERIFIED`，不在本任务 `locks` 内、
不可改）已经定义了全局、无 namespace 的 `PkDate`/`PkTime`/`PkDateTime` 三个
类——**是最小占位实现**（只有 `year()`/`month()`/`day()`、
`hour()`/`minute()`/`second()`/`msec()`、`date()`/`time()`、`isValid()`/
`isNull()`、`operator==`/`!=`，**没有** `fromString`/`toString`/
`currentDateTime`/`fromMSecsSinceEpoch`/`secsTo` 等任何行为 API），目的仅仅是
让 `PkVariant` 能存住一个"日期时间形状的值"（`PkVariant::toDateTime()`，9 处
真实调用点）。

这不是巧合，是同一个"值类型先到者建最小占位、真正的能力所有者稍后落地"
模式——`R线-spec.md`"COW 地基共用一份"一节已经为同类情形定过先例
（`pk/geometry/PkGlobal.h` 临时垫的 `qreal` 全局标量，`R-18` 落地后由 R-03
之后的任务折进来）。R-16 照此先例处理：

- `pk/time` 建**自己的、完整行为的** `PkDate`/`PkTime`/`PkDateTime`——这才是
  `Qt替代品选型.md` §1"时钟 → `std::chrono`"真正要交付的东西，`pk/variant`
  那份占位实现从未打算覆盖 `fromString`/`toString`/时间运算这类行为。
- **不碰 `pk/variant/PkAuxTypes.h`**——不在本任务 `locks` 内，且它已经
  `VERIFIED`。
- **两边类名重复在 R 线现阶段不会造成 ODR 冲突**：R 线的库不接入 Krita 主构建，
  `pk/variant` 与 `pk/time` 各自是独立薄壳工程，没有任何真实 TU 会同时
  `#include` 两边的头（已现场核实：唯一同时触碰"`QVariant` 存的 datetime 值"与
  "datetime 行为 API"的真实调用点是 `kis_exiv2_common.h` 的
  `QLocale::c().toString(variant.toDateTime(), "yyyy:MM:dd hh:mm:ss")`——它读的
  是 `QLocale::toString(QDateTime, format)`，属于未来 `QLocale` 端口的调用
  形态，不属于本任务或 `pk/variant` 任何一边现在要接的调用点）。
- **真正的收口点在 S 线**：`pk/variant` 与 `pk/time` 都被接入真实 Krita 构建、
  且有第三方需要"`PkVariant` 存的 datetime 值也要能调用 `pk/time` 的行为 API"
  时，才会真的踩到重名——那时候的正确做法是把 `pk/variant/PkAuxTypes.h` 里的
  `PkDate`/`PkTime`/`PkDateTime` 三个类**删掉**，改为 `#include` `pk/time` 的
  对应头，`PkVariant::toDateTime()` 直接返回 `pk/time` 的 `PkDateTime`。这条
  折叠动作现在写不了（`pk/variant` 不在本任务 `locks` 内），只记录、不执行。

**建议**：把这条追加进 `R线-spec.md`"待认领的缺口"一节（措辞对齐 R-18/
pk-global 先例），指名归"`pk/variant` 与 `pk/time` 都进入 S 线主构建时的收口
任务"认领；不影响本任务的完成判定——本任务的产出是 `pk/time` 自身，不依赖这条
折叠先行完成。

**旁证**：对拍过程中发现 `pk/variant/PkAuxTypes.cpp` 的 `isNull()` 用的是
`m_date.isNull() && m_time.isNull()`（AND 语义）——与本任务 `isNull() ==
!isValid()`（头文件事实：`qdatetime.h:77-78`）不同构。这恰好印证上面的判断：
`PkAuxTypes.cpp` 那份是"从未打算精确复刻 Qt 语义"的占位实现，`pk/time` 的
`isNull`/`isValid` 语义以头文件事实为准，不参照 `PkAuxTypes.cpp`。

## 6. 覆盖度限制自查

对拍覆盖不到的（详见 `oracle/R-16.deviation`「覆盖度限制」一节）：

1. 历史 tzdata 语义是对 Qt 自带 tzdb 的近似，不是完全对齐——`PkDateTime` 已按
   裁决用 C 库 `localtime_r`/`mktime` 落地 LocalTime（现代日期 1970+ 与 Qt
   一致），但两类历史差异已实测并登记在 `oracle/R-16.deviation`「LocalTime 对齐」
   一节（历史 DST：1969-06-15 LA；历史 LMT：1900-01-01 Shanghai）。当前
   difftest 的 kSecTok 只含 1970 前两个值，更细的历史 DST 转换时刻未逐个枚举——
   那是 tzdb 级精确语义，真实调用点（EXIF/XMP 元数据日期）不依赖。
2. `toSecsSinceEpoch()` 没有独立的 ORACLE-COVER 计数——它被内联进几乎所有比较
   函数，覆盖是真实的，只是不体现在按 API 的计数里。
3. `currentDateTime()`/`currentDateTimeUtc()` 只做"5 秒容忍窗口内且都 valid"
   的关系性对拍，不比较精确到毫秒/纳秒的绝对值——两次独立"现在"调用之间必然
   存在真实时间差，是结构性限制。
4. `elapsed()`/`nsecsElapsed()` 同理只做关系性对拍（归零、非负、与真实睡眠
   同数量级、ns/ms 比值落在 1e5~1e7、`restart()` 真的归零），不比较绝对纳秒
   数字面量；容忍窗口选 30ms——足以抓住"系数错 1000 倍"这类严重缺陷，但保证
   不了个位数毫秒量级的换算系数偏差。
5. `fromString(s, DateFormat fmt)` 只测了 `ISODate` 一支——`RFC2822Date`/
   `ISODateWithMs` 目前没有真实调用点，两支实现本身恒返回无效实例，对拍这
   两支只会得到没有信息量的"两侧都不支持"。
6. 内嵌 NUL 完全不覆盖——字符串 token 全部来自 C 字符串字面量
   （`std::string(const char*)` 走 `strlen`），`fromString` 系列本身也没有能
   表达内嵌 NUL 的重载。

试接覆盖不到的：

- 只证明了三个真实生产 `.cpp` 文件（1 个 `check_pass` + 2 个
  `check_expect_fail`）在**语法层面**（`-fsyntax-only`）零改动可解析/精确卡在
  已知缺口——不做链接、不跑运行时；`PkElapsedTimer`/`PkDateTime` 的行为正确性
  由第 3 节的对拍与 `test_pktime` 的 35 条断言负责，试接只负责"compat 垫片在
  真实调用点上下文里语法上真的生效"。
- `check_pass` 只覆盖了 `PkElapsedTimer` 的 4 个方法（`isValid`/`start`/
  `elapsed`/`invalidate`），没有覆盖到 `PkDateTime` 的任何方法——穷举全仓后
  没有找到一个 `PkDateTime` 真实消费者能在当前已交付的 pk 库集合下零改动编过
  （全部卡在 `QImage`/`QColor`/`QDomDocument`+`pugixml`/GUI Widgets/字体栈/
  Pigment 色彩子系统这类 R-16 范围外的缺口）——这是穷举 37 个候选之后的现场
  结论，不是没找。
- `restart()`/`nsecsElapsed()`（`PkElapsedTimer`）与 `PkDateTime` 全部方法的
  "真实调用点里语法零改动可用"这条断言，目前只能靠 Task 3 的 `date_parser_driver`
  （复刻而非零改动）与第 3 节对拍（编译期链接真实测试程序，不是零改动的生产
  文件）间接支撑。

## 7. 判据③：库内无 Qt 符号

```
$ nm -u -C pk/time/build/libpktime.a | grep -i qt
$ echo "exit=$?"
exit=1
```

无输出（grep 未命中，exit=1 即符合预期）。

## 8. 怎么跑

```bash
# 从 fork 仓库根执行。source krita-ci-env/env 先做一次（构建/对拍都要用到
# 里面的 PATH/CMAKE_PREFIX_PATH），PK_QT_PREFIX 指向该环境的 _install 前缀。

# ① 构建（库 + test_pktime + date_parser_driver）
cmake -S pk/time -B pk/time/build -G Ninja \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build pk/time/build

# ② 单测：35 条断言（9 elapsed-timer + 26 date-time）+ date_parser_driver 六分支
pk/time/build/test_pktime
pk/time/build/date_parser_driver

# ③ 判据③：库里不得有 Qt 未定义符号。必须无输出。
nm -u -C pk/time/build/libpktime.a | grep -i qt

# ④ 对拍（需要真 Qt 5.15，PK_QT_PREFIX 指向 krita-ci-env 的 _install 前缀）
PK_QT_PREFIX=<krita-ci-env>/_install pk/time/oracle/run_oracle.sh

# ⑤ 试接（-fsyntax-only 零改动语法检查，见第 4 节）
pk/time/tests/graft/graft_check.sh
```

> `pk/time/build/` 与 `oracle/build/` 都被顶层 `.gitignore` 排除。
