#pragma once
//
// R-65 · pk 测试栈的 compat 预激活聚合头。
//
// 由 kritatestsdk_pk 以 `-include` **强制注入**每一个 pk_add_test 目标的每一个 TU
// （brief §2 配方 3）。作用与 .exec/shell/kritaimage/compat/PkCompatAll.h 相同：
// 补 Qt 头之间的“传递 include”习惯 —— 真实 Krita 测试源与被测头写 QString、QList、
// PkRect… 时常常不自己 include，靠 <QObject> → <qglobal.h> → … 那条链子；pk 的
// compat 头不复制这条链，所以在这里对测试面常用的类型做一次预激活。
//
// **顺序**（brief §2 配方 3 / 壳 PkCompatAll.h 的先例）：
//   1. <pk/test/compat/QObject> 与 <pk/test/compat/QTest> 必须最先 —— 测试类的
//      `Q_OBJECT`→`friend PkTestBinder`、`QObject`→`PkTestObject`、`QTest`→`PkTest`、
//      `QCOMPARE`→`PK_COMPARE` 一类别名都靠这两份；
//   2. pk/global/compat/QtGlobal（“超集”）：它内部先拉 pk/test + pk/geometry 两份
//      QtGlobal 再包 PkGlobal.h，于是 qAbs 只定义一次（避开 R-4 的重定义）。
//      QObject 已经拉过 pk/test/compat/QtGlobal，这里再拉超集不会重复定义；
//   3. PkNamespace.h：namespace Qt 的枚举族（Qt::red/blue/… 与 Orientation 等）；
//   4. 其余类型垫片：__has_include 守卫，缺了不炸，真缺会在调用点报清楚的错。
#include <pk/test/compat/QObject>
#include <pk/test/compat/QTest>
#include <pk/global/compat/QtGlobal>
#include <PkNamespace.h>

// Qt::<GlobalColor 枚举名> 别名垫片（R-65 T2+4，impact-map.md §3.7.2）。
// 必须在 PkNamespace.h 之后（它 include PkGlobal.h，依赖 Pk::GlobalColor 已定义）。
// 自守卫：真 Qt qnamespace.h 在场时让位。
#include <QtGlobalColorAliases.h>

// Q_SIGNALS 回写成 public：pk/test/compat/QObject 为了避免槽声明污染把
// `Q_SIGNALS`/`signals` 定义成空，于是被测头里的 `Q_SIGNALS:` 会退化成裸 `:`，
// 在类作用域非法。真 Qt5 里 Q_SIGNALS 展开为 `public`（见壳 PkCompatAll.h 的同款
// 说明）。聚合头在 compat/QObject 之后把访问级别扳回真 Qt 语义。
#ifdef Q_SIGNALS
#undef Q_SIGNALS
#endif
#define Q_SIGNALS public

// QT_VERSION_CHECK：kis 头里按 Qt 版本做预处理分支
// （`#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))`）。无真 Qt 时 QT_VERSION 保持未定义
// （`#if` 里按 0 算，走 Qt5 分支），QT_VERSION_CHECK 不补则函数式宏在 `#if` 里解析
// 失败报 "missing binary operator"。只补 CHECK、不补 VERSION（补 VERSION 会翻到
// Qt6 分支与 pk 类型冲突）。
#ifndef QT_VERSION_CHECK
#define QT_VERSION_CHECK(major, minor, patch) ((major) << 16 | (minor) << 8 | (patch))
#endif

// Qt 容器/几何/字符串垫片的预激活。生产头与被测头大量靠 Qt 的传递 include，
// pk compat 头不复制那条链。__has_include 守卫：垫片不存在时不炸，真需要它的
// 调用点会报 "file not found"，正好指出哪个垫片该补。
#define PKC_HAS(h) (defined(__has_include) && __has_include(h))
#if PKC_HAS(<QString>)
#include <QString>
#endif
#if PKC_HAS(<QByteArray>)
#include <QByteArray>
#endif
#if PKC_HAS(<QStringList>)
#include <QStringList>
#endif
#if PKC_HAS(<QBitArray>)
#include <QBitArray>
#endif
#if PKC_HAS(<QVector>)
#include <QVector>
#endif
#if PKC_HAS(<QList>)
#include <QList>
#endif
#if PKC_HAS(<QSet>)
#include <QSet>
#endif
#if PKC_HAS(<QMap>)
#include <QMap>
#endif
#if PKC_HAS(<QHash>)
#include <QHash>
#endif
#if PKC_HAS(<QPair>)
#include <QPair>
#endif
#if PKC_HAS(<QQueue>)
#include <QQueue>
#endif
#if PKC_HAS(<QStack>)
#include <QStack>
#endif
#if PKC_HAS(<QRect>)
#include <QRect>
#endif
#if PKC_HAS(<QPoint>)
#include <QPoint>
#endif
#if PKC_HAS(<QSize>)
#include <QSize>
#endif
#if PKC_HAS(<QRectF>)
#include <QRectF>
#endif
#if PKC_HAS(<QPointF>)
#include <QPointF>
#endif
#if PKC_HAS(<QSizeF>)
#include <QSizeF>
#endif
#if PKC_HAS(<QPolygon>)
#include <QPolygon>
#endif
#if PKC_HAS(<QPolygonF>)
#include <QPolygonF>
#endif
#if PKC_HAS(<QLineF>)
#include <QLineF>
#endif
#if PKC_HAS(<QPainterPath>)
#include <QPainterPath>
#endif
#if PKC_HAS(<QRegion>)
#include <QRegion>
#endif
#if PKC_HAS(<QTransform>)
#include <QTransform>
#endif
#if PKC_HAS(<QColor>)
#include <QColor>
#endif
#if PKC_HAS(<QImage>)
#include <QImage>
#endif
// QRgb：真 Qt 下由 QtGui 的 <QImage>/<QColor> 传递进来；pk 栈不链 QtGui，
// 于是 `QRgb`/`qRed`/`qGreen`/`qBlue`/`qAlpha`/`qRgb`/`qRgba` 无处解析
// （brief §1.6 L1：28 个 libs/image/tests 可达 target 的首错）。垫片在
// sdk/tests/compat/QRgb，宏映射到 PkRgb 同名函数。**必须在这里 include 才激活**
// （宏不 include 不生效）。放在 <QImage> 之后：真 Qt 在场时 <QImage> 已带 QRgb，
// 本垫片在 pk 栈才是唯一来源。
#if PKC_HAS(<QRgb>)
#include <QRgb>
#endif
#if PKC_HAS(<QVariant>)
#include <QVariant>
#endif
#if PKC_HAS(<QScopedPointer>)
#include <QScopedPointer>
#endif
#if PKC_HAS(<QSharedPointer>)
#include <QSharedPointer>
#endif
#if PKC_HAS(<QWeakPointer>)
#include <QWeakPointer>
#endif
#if PKC_HAS(<QMutex>)
#include <QMutex>
#endif
#if PKC_HAS(<QMutexLocker>)
#include <QMutexLocker>
#endif
#if PKC_HAS(<QReadWriteLock>)
#include <QReadWriteLock>
#endif
#if PKC_HAS(<QReadLocker>)
#include <QReadLocker>
#endif
#if PKC_HAS(<QWriteLocker>)
#include <QWriteLocker>
#endif
#if PKC_HAS(<QAtomicInt>)
#include <QAtomicInt>
#endif
#if PKC_HAS(<QThread>)
#include <QThread>
#endif
#if PKC_HAS(<QSemaphore>)
#include <QSemaphore>
#endif
#if PKC_HAS(<QThreadPool>)
#include <QThreadPool>
#endif
#if PKC_HAS(<QRunnable>)
#include <QRunnable>
#endif
#if PKC_HAS(<QIODevice>)
#include <QIODevice>
#endif
#if PKC_HAS(<QDebug>)
#include <QDebug>
#endif
#if PKC_HAS(<QFlags>)
#include <QFlags>
#endif
#if PKC_HAS(<QMetaType>)
#include <QMetaType>
#endif
// QTime / QRandomGenerator：tiles3 的 kis_chunk_allocator_test 在 .cpp/.h 里直接
// 用 QRandomGenerator（头里 `#include <QRandomGenerator>` 但 .cpp 里用 QTime）。
// 两垫片在 pk/time/compat 与 pk/ 下。
#if PKC_HAS(<QTime>)
#include <QTime>
#endif
#if PKC_HAS(<QRandomGenerator>)
#include <QRandomGenerator>
#endif
// QDom* 族（QtXml）：pk/xml/compat 下已有九个垫片（QDomDocument 等，全是
// `#define QDomXxx PkXmlXxx`），但**宏不 include 不生效**（与 QRgb 同律）。真实测试源
// 直接写 `#include <QDomDocument>` / `#include <QDomElement>` 并用 QDomNode/
// QDomNodeList。实测本 Task 注册面首错落在：
//   libs/image/tests/kis_dom_utils_test.cpp:33,35,52   QDomDocument / QDomElement
//   libs/image/tests/kis_mask_generator_test.cpp:32    QDomElement
// 这里一次预激活整个家族（与真 Qt QtXml 的 qdom.h「一个头带全家族」的传递性对齐）。
// __has_include 守卫：垫片不存在时不炸。
#if PKC_HAS(<QDomDocument>)
#include <QDomDocument>
#endif
#if PKC_HAS(<QDomElement>)
#include <QDomElement>
#endif
#if PKC_HAS(<QDomNode>)
#include <QDomNode>
#endif
#if PKC_HAS(<QDomNodeList>)
#include <QDomNodeList>
#endif
#if PKC_HAS(<QDomAttr>)
#include <QDomAttr>
#endif
#if PKC_HAS(<QDomText>)
#include <QDomText>
#endif
#if PKC_HAS(<QDomDocumentType>)
#include <QDomDocumentType>
#endif
#if PKC_HAS(<QDomImplementation>)
#include <QDomImplementation>
#endif
#if PKC_HAS(<QDomCDATASection>)
#include <QDomCDATASection>
#endif
#if PKC_HAS(<QXmlStreamReader>)
#include <QXmlStreamReader>
#endif
#if PKC_HAS(<QXmlStreamWriter>)
#include <QXmlStreamWriter>
#endif
// QDir：kis_memory_window_test 写 `QDir::currentPath()` 但**不** include <QDir>
// （真 Qt 下由 QtCore 传递进来），所以必须在这里预激活。垫片在 sdk/tests/compat/。
#if PKC_HAS(<QDir>)
#include <QDir>
#endif
// QElapsedTimer：kis_tiled_data_manager_test 的三处 benchmark 段用 `QElapsedTimer timer;`
// 计时，但不 include <QElapsedTimer>（真 Qt 由 QtTest/QtCore 传递）。垫片在 pk/time/compat。
#if PKC_HAS(<QElapsedTimer>)
#include <QElapsedTimer>
#endif
// QtMath：测试源直接调用 qSqrt/qSin/qCos/… 但不 include <cmath>（真 Qt 由 Qt 头
// 传递）。生产头 PkGlobal.h 已提供 qMin/qMax/qAbs/qRound；三角函数族由 QtMath 垫片补。
#if PKC_HAS(<QtMath>)
#include <QtMath>
#endif
// QTest::qSleep 过渡垫片（R-65 修复轮）：真 Qt 的 qSleep 是**纯阻塞 sleep**
// （已探针实测，见 compat/PkTestSleepShim.h 头注与 evidence/qsleep-probe-output.txt），
// 但 pk 测试栈原本没有它，tiles3 组两个真实测试类（kis_tile_data_pooler_test /
// kis_low_memory_tests）因此编不过。垫片定义在 sdk/tests/compat/PkTestSleepShim.h，
// 正家是 pk/test（S0），本行只是过渡。
#include <PkTestSleepShim.h>

#undef PKC_HAS

// qMin / qMax / qBound —— pk 树只提供 pkMin/pkMax/pkBound（pk/global/PkGlobal.h:154-159），
// **没有任何地方提供 Qt 名**（R-65 T2+4 逐树核过：PkGlobal.h 里 qMin/qMax 只出现在注释；
// pk/global/compat/QtGlobal 的头注释声称提供 qRound/qMin/qMax/qBound，实际没有；全树只有
// libs/flake/PkXmlCompat.h:169 有 `#define qMin pkMin`，那是 flake 自己的 compat 头，只对
// flake 的 TU 生效）。薄壳早就在做同样的映射（先例：.exec/shell/kritaimage/compat/PkCompatAll.h）。
// ⚠ pk/global/compat/QtGlobal 的「注释与实现不符」是 pk/ 内的差异，不在本 Task 锁内 ⇒ 只报不改。
// 按实测调用点补齐（不改测试源、不搬整套 QtGlobal）：sdk/tests/qimage_test_util.h:103 的
// `qMax(1, fuzzyAlpha)` 是第一个落点，另有 libs/ 下若干测试面调用点。
// 守卫用 #ifndef：与 flake/PkXmlCompat.h 的同名映射共存（谁先到谁生效，两者都指向 pkXxx）。
#ifndef qMin
#define qMin pkMin
#endif
#ifndef qMax
#define qMax pkMax
#endif
#ifndef qBound
#define qBound pkBound
#endif

// kundo2_noi18n —— Krita 宏 `kundo2_noi18n(text)`，与 `kundo2_i18n` 同族，语义是
// 「显式声明这段文字**不需要翻译**」（`libs/command/kundo2magicstring.h:143-148` 的
// 原文注释就写给 kundo2_text_raw 的：「tells explicitly that we don't need a
// translation ... used either in testing or internal commands」）。
// 本树 `libs/command/kundo2magicstring.h` 已 Pk 化并带有 `kundo2_i18n` 内联函数族
// （该头 :325-330，S-09-g 加的），但**漏了它的兄弟 `kundo2_noi18n`**——全仓
// `grep -rn "define kundo2_noi18n"` 命中 0 处（实测），于是所有调用点编不过。
//
// 语义等价（逐字核对本树实现，非推测）：
//   `kundo2_text_raw(const PkString&)` = `KUndo2MagicString(text)`（该头 :149-152），
//   而 D-6 之后 `kundo2_text`（= i18n 变体）同样是 `KUndo2MagicString(PkString(text))`
//   原文直返（该头 :184-187），二者在本树**完全同义**。故本宏映射到 `kundo2_text_raw`。
//
// 唯一的真实调用点全集（实测 grep，全仓）：
//   本 Task 注册面：libs/image/tests/kis_crop_processing_visitor_test.cpp:39,67
//                   libs/image/tests/kis_transaction_test.cpp:44,85,123,130,206,224,256
//   锁外（不受影响或另有阻塞）：libs/image/tests/kis_transform_worker_test.cpp:171,199,775,802,830
//                   plugins/filters/tests/{kis_all_filter_test.cpp:91,kis_crash_filter_test.cpp:55}
//                   plugins/paintops/libpaintop/tests/*.cpp（5 处）
// 形参覆盖 `const char*` 与 `const PkString&`（后者落在 `kundo2_noi18n(f->name())`
// 这类调用点）。宏定义在**测试栈聚合头**（只对 pk 测试 TU 生效，真 Qt 测试栈
// `kritatestsdk` 不受影响），不动 `libs/command/` 生产头——那超出「只动
// libs/image/tests」的范围。`#ifndef` 守卫：将来 pk 侧补上同名宏时自动让位。
#ifndef kundo2_noi18n
#define kundo2_noi18n(text) kundo2_text_raw(text)
#endif
