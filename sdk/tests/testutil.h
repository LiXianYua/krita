/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] testutil.h 阻塞登记（S-06 Task 9）
//
// 本文件不进薄壳，保留 Qt 类型。经 qimage_test_util.h 依赖 QImage 文件 I/O；
// TestNode 用 Q_OBJECT；另有 GUI 应用对象调用点。PATTERN-1 一处
// qApp->processEvents() 已删除；PATTERN-2 一处 QTest::qWait 保留待 S-08
// flush 方法。
// 关闭条件：PkImage 文件 I/O（R-15）+ TestNode 的 Q_OBJECT 端口化。
//
// R-75 更新（2026-09-12）：**PkImage 文件 I/O 已交付**（R-15 / R-75 Task 1）⇒
// 上面第一个关闭条件已满足。随之打开的是 ReferenceImageChecker（它唯一依赖就是
// checkQImage* 的文件 I/O，见下方 struct 前的注）。其余关闭条件（TestNode 的
// Q_OBJECT 端口化）仍未满足，本文件的 [GAP] 未整体关闭。


#ifndef TEST_UTIL
#define TEST_UTIL

#include <QDebug>
#include <QImage>
#include <QProcessEnvironment>
#include <QRect>
#ifndef KRITA_TESTSDK_PK_NATIVE
// R-65（Task 2+4）· impact-map.md §3.14：这一行是 28 个可达 target 的**真根因**。
// `QtCore/qtestsupport_core.h` 是**真 Qt 头**，它经 qcoreapplication.h → qglobal.h
// 带来真 Qt 的 `qglobal.h`，与 pk/test/compat/QtGlobal 的 `qAbs` 撞
// `redefinition of 'qAbs'`。真 Qt 头之所以还能解析得到：pk 编译行里仍有
// `-F <CI 前缀>/lib`（KF5 frameworks 需要，**不能删**——删了连坐一批本来就绿的
// 目标），于是 `QtCore/…` 经 .framework 形式解析得到。
//
// pk 分支不拉真 Qt 头。它原本提供的 `QTest::qWait/qSleep/qWaitFor` 由
// sdk/tests/compat/PkTestSleepShim.h 那族垫片在 pk 栈上补（QTest→PkTest 由
// pk/test/compat/QTest 的宏完成；qWait 的 pk 等价物 = 显式 pump 调用队列，
// 见该垫片头注）。真 Qt 测试栈（kritatestsdk）不定义 KRITA_TESTSDK_PK_NATIVE，
// 照旧拉真头、行为一个字不变。
#include <QtCore/qtestsupport_core.h>
#endif

#include <simpletest.h>
#include <QTime>
#include <QDir>

#ifdef KRITA_TESTSDK_PK_NATIVE
// R-82：`pk/test/README.md:77` 把 `qWait` 登记为 **S0 缺口**，
// `PkTestCompatAll.h` 拉的 `PkTestSleepShim.h` 只补了 `qSleep`。
// **订正（全分支评审 §4）**：pk 栈上并非完全没有对应物 —— `sdk/tests/simpletest.h`
// 里已有 `PkTest::qWait`。但它在 `testutil.h` 的 include 顺序里**声明得太晚**
// （本头先被包含，`simpletest.h` 在其后）⇒ 在本文件里直接写 `QTest::qWait` 会报
// `no member named qWait in namespace PkTest`（实测）。所以这里用同目录、自足的
// `KritaTestSdk::waitFor`（其类头注释即写明「替代 QTest::qWait」）。
// pk 的等价物**已经在本目录里**：`KritaTestSdk::waitFor()` 的类头注释明写它
// 「**替代** Qt 的 `QTest::qWait()`，因为 Qt 的事件循环不驱动 pk 队列」——
// 它按 5 ms 分片 sleep、每片后 `PkThreadCallQueue::processPendingCalls()`。
// 所以这里**不新增垫片、也不动 pk/test**（那会把 S0 的正家实现提前做掉）。
// 唯一调用点是下面 `MaskParent::waitForImageAndShapeLayers()`（:590）。
#include <PkThreadCallQueuePumpHost.h>
#endif

#include <PkImage.h>
#include <PkRect.h>
#include <PkString.h>

#include <KoResource.h>
#include <KoTestConfig.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoColorProfile.h>
#include <KoProgressProxy.h>
#include <kis_paint_device.h>
#include <kis_node.h>
#include <kis_undo_adapter.h>
#include "kis_node_graph_listener.h"
#include "kis_iterator_ng.h"
#include "kis_image.h"
#include "testing_nodes.h"

#ifndef FILES_DATA_DIR
#define FILES_DATA_DIR "."
#endif

#ifndef FILES_DEFAULT_DATA_DIR
#define FILES_DEFAULT_DATA_DIR "."
#endif

#include "qimage_test_util.h"

/**
 * Compare values and return false on failure
 * (normal QCOMPARE returns void)
 */
#define KIS_COMPARE_RF(expr, ref) \
    if ((expr) != (ref)) { \
        qDebug() << "Compared values are not the same at line" << __LINE__; \
        qDebug() << "    Actual  : " << #expr << "=" << (expr); \
        qDebug() << "    Expected: " << #ref << "=" << (ref); \
        return false; \
    }

/**
 * Compare two float numbers by rounding them up to \p prec
 * decimals after the point.
 */
#define KIS_COMPARE_FLT(actual, expected, prec) \
do {\
        const qreal multiplier = pow(10, prec); \
        if (!QTest::qCompare(qRound(actual * multiplier) / multiplier, qRound(expected * multiplier) / multiplier, #actual, #expected, __FILE__, __LINE__))\
        return;\
} while (false)


/**
 * Routines that are useful for writing efficient tests
 */

namespace TestUtil
{

inline QString diagnosticQString(const PkString &text)
{
    const std::string utf8 = text.PkToUtf8();
    return QString::fromUtf8(utf8.data(), static_cast<int>(utf8.size()));
}

// ---------------------------------------------------------------------------
// R-65 Task 3 · pk 栈守卫（testutil.h）
//
// pk 栈上 `QString`→`PkString`、`QImage`→`PkImage`（pk/*/compat 的宏），于是
// **QImage 族与 PkImage 族的签名会塌成同一个**，且 PkImage↔QImage 的桥接需要
// PkImage 的**文件/缓冲区**能力（R-15 未交付）。逐条守卫（真 Qt 栈不定义
// KRITA_TESTSDK_PK_NATIVE，行为一个字不变）：
//
//   * pkStringFromQString —— `QByteArray` + `PkString::toUtf8()`（pk 只提供
//     PkToUtf8()）；唯一调用点是下面 findNode 的 QString 重载；
//   * diagnosticQImage —— `PkImage` 的**缓冲区构造**（PkImage 只提供
//     (width,height,format)/(size,format) 两种构造）；
//   * PkImage 的 checkQImage/checkQImageExternal 重载 —— 在 pk 栈上与
//     qimage_test_util.h 的同名重载**签名完全相同**（QString→PkString 后
//     两边都是 (const PkImage&, const PkString&, const PkString&, const PkString&,
//     int, int, int)）⇒ 报 "redefinition of default argument"；且它们经
//     diagnosticQImage 桥到上面那一族文件 I/O；
//   * findNode(KisNodeSP, const QString&) —— 在 pk 栈上与 PkString 重载同签名
//     ⇒ 报 "redefinition of 'findNode'"（PkString 重载已完整覆盖该语义）；
//   * ReferenceImageChecker —— 它存在的唯一目的就是调 checkQImage* 读写
//     参考图（文件 I/O）；
//   * MaskParent(const QRect&) —— QRect→PkRect 后与默认参数版同签名 ⇒
//     "constructor cannot be redeclared"。
// diagnosticQString 不守卫：它只用 PkToUtf8()/PkString::fromUtf8()，pk 栈上
// 编得过，且被 TestProgressBar::format() 真实使用。
#ifndef KRITA_TESTSDK_PK_NATIVE
inline PkString pkStringFromQString(const QString &text)
{
    const QByteArray utf8 = text.toUtf8();
    return PkString::PkFromUtf8(utf8.constData(), utf8.size());
}

inline QImage diagnosticQImage(const PkImage &image)
{
    if (image.isNull()) return QImage();
    const QImage source(reinterpret_cast<const uchar *>(image.constBits()),
                        image.width(), image.height(), image.bytesPerLine(),
                        static_cast<QImage::Format>(image.format()));
    return source.convertToFormat(QImage::Format_ARGB32).copy();
}
#endif // !KRITA_TESTSDK_PK_NATIVE

// ---------------------------------------------------------------------------
// R-82 · pk 栈分支：同上两个符号的 pk 版。
//
// **这是 R-77 留下的一个真缺陷**（R-82 判定阶段发现、主会话复核属实）：
// R-77 把 `sdk/tests/filestest.h` 的 pk 活跃体打开时，那两个函数体里调了
// `pkStringFromQString(...)`（filestest.h:109/149/301/367/410/457/461）与
// `diagnosticQImage(...)`（filestest.h:170），**而这两个符号正好被上面那个
// `#ifndef` 守卫掉** ⇒ pk 栈上「守卫打开了、函数却不存在」，两处互相矛盾。
//
// 守卫它们的理由（上面 R-65 段的注释）在 pk 栈上并不成立：
//   * `pkStringFromQString` 的难点是 `QByteArray` + `PkString::toUtf8()`；
//     但在 pk 栈上 `QString` 就是 `PkString`（`#define QString PkString`），
//     这个转换**退化成恒等**，根本不需要 QByteArray；
//   * `diagnosticQImage` 的难点是 `QImage` 的**缓冲区构造**；但在 pk 栈上
//     `QImage` 就是 `PkImage`，参数与返回同型 ⇒ 直接 `convertToFormat` 即可
//     （真 Qt 版做的就是「转 ARGB32 + copy」，pk 版保留格式归一那一步，
//      省略 `.copy()`：`convertToFormat` 已返回值语义的新对象，调用点随后
//      还会 `convertTo(Format_ARGB32)`，无可观察差异）。
// 两栈的**可观察语义一致**：Qt 栈仍走上面那份原始实现，一个字节未动。
// ---------------------------------------------------------------------------
#ifdef KRITA_TESTSDK_PK_NATIVE
inline PkString pkStringFromQString(const PkString &text)
{
    return text;
}

inline PkImage diagnosticQImage(const PkImage &image)
{
    if (image.isNull()) return PkImage();
    return image.convertToFormat(PkImage::Format_ARGB32);
}
#endif // KRITA_TESTSDK_PK_NATIVE

inline PkImage pkImageFromQImage(const QImage &image)
{
    const QImage converted = image.convertToFormat(QImage::Format_ARGB32);
    PkImage result(converted.width(), converted.height(), PkImage::Format_ARGB32);
    const size_t rowBytes = static_cast<size_t>(converted.width()) * sizeof(QRgb);
    for (int y = 0; y < converted.height(); ++y) {
        memcpy(result.scanLine(y), converted.constScanLine(y), rowBytes);
    }
    return result;
}

#ifndef KRITA_TESTSDK_PK_NATIVE
inline bool checkQImage(const PkImage &image, const QString &testName,
                        const QString &prefix, const QString &caseName,
                        int fuzzy = 0, int fuzzyAlpha = -1, int maxNumFailingPixels = 0)
{
    return checkQImage(diagnosticQImage(image), testName, prefix, caseName,
                       fuzzy, fuzzyAlpha, maxNumFailingPixels);
}

inline bool checkQImage(const PkImage &image, const QString &testName,
                        const QString &prefix, const PkString &caseName,
                        int fuzzy = 0, int fuzzyAlpha = -1, int maxNumFailingPixels = 0)
{
    return checkQImage(image, testName, prefix, diagnosticQString(caseName),
                       fuzzy, fuzzyAlpha, maxNumFailingPixels);
}

inline bool checkQImage(const PkImage &image, const QString &testName,
                        const QString &prefix, const char *caseName,
                        int fuzzy = 0, int fuzzyAlpha = -1, int maxNumFailingPixels = 0)
{
    return checkQImage(image, testName, prefix, QString::fromUtf8(caseName),
                       fuzzy, fuzzyAlpha, maxNumFailingPixels);
}

inline bool checkQImageExternal(const PkImage &image, const QString &testName,
                                const QString &prefix, const QString &caseName,
                                int fuzzy = 0, int fuzzyAlpha = -1, int maxNumFailingPixels = 0)
{
    return checkQImageExternal(diagnosticQImage(image), testName, prefix, caseName,
                               fuzzy, fuzzyAlpha, maxNumFailingPixels);
}
#endif // !KRITA_TESTSDK_PK_NATIVE

inline KisNodeSP findNode(KisNodeSP root, const PkString &name) {
    if(root->name() == name) return root;

    KisNodeSP child = root->firstChild();
    while (child) {
        if((root = findNode(child, name))) return root;
        child = child->nextSibling();
    }

    return KisNodeSP();
}

#ifndef KRITA_TESTSDK_PK_NATIVE
inline KisNodeSP findNode(KisNodeSP root, const QString &name)
{
    return findNode(root, pkStringFromQString(name));
}
#endif // !KRITA_TESTSDK_PK_NATIVE

inline void dumpNodeStack(KisNodeSP node, PkString prefix = PkString("\t"))
{
    qDebug() << node->name().PkToUtf8().c_str();
    KisNodeSP child = node->firstChild();

    while (child) {

        if (child->childCount() > 0) {
            dumpNodeStack(child, prefix + "\t");
        } else {
            qDebug() << prefix.PkToUtf8().c_str() << child->name().PkToUtf8().c_str();
        }
        child = child->nextSibling();
    }
}

class TestProgressBar : public KoProgressProxy {
public:
    TestProgressBar()
        : m_min(0), m_max(0), m_value(0)
    {}

    int maximum() const override {
        return m_max;
    }
    void setValue(int value) override {
        m_value = value;
    }
    void setRange(int min, int max) override {
        m_min = min;
        m_max = max;
    }
    void setFormat(const PkString &format) override {
        m_format = format;
    }

    void setAutoNestedName(const PkString &name) override {
        m_autoNestedName = name;
        KoProgressProxy::setAutoNestedName(name);
    }

    int min() { return m_min; }
    int max() { return m_max; }
    int value() { return m_value; }
    QString format() const { return diagnosticQString(m_format); }
    PkString autoNestedName() { return m_autoNestedName; }


private:
    int m_min;
    int m_max;
    int m_value;
    PkString m_format;
    PkString m_autoNestedName;
};

inline bool comparePaintDevices(QPoint & pt, const KisPaintDeviceSP dev1, const KisPaintDeviceSP dev2)
{
    //     QTime t;
    //     t.start();

    PkRect rc1 = dev1->exactBounds();
    PkRect rc2 = dev2->exactBounds();

    if (rc1 != rc2) {
        pt.setX(-1);
        pt.setY(-1);
    }

    KisHLineConstIteratorSP iter1 = dev1->createHLineConstIteratorNG(0, 0, rc1.width());
    KisHLineConstIteratorSP iter2 = dev2->createHLineConstIteratorNG(0, 0, rc1.width());

    int pixelSize = dev1->pixelSize();

    for (int y = 0; y < rc1.height(); ++y) {

        do {
            if (memcmp(iter1->oldRawData(), iter2->oldRawData(), pixelSize) != 0)
                return false;
        } while (iter1->nextPixel() && iter2->nextPixel());

        iter1->nextRow();
        iter2->nextRow();
    }
    //     qDebug() << "comparePaintDevices time elapsed:" << t.elapsed();
    return true;
}

template <typename channel_type>
inline bool comparePaintDevicesClever(const KisPaintDeviceSP dev1, const KisPaintDeviceSP dev2, channel_type alphaThreshold = 0)
{
    PkRect rc1 = dev1->exactBounds();
    PkRect rc2 = dev2->exactBounds();

    if (rc1 != rc2) {
        qDebug() << "Devices have different size" << rc1.x() << rc1.y() << rc1.width() << rc1.height()
                 << rc2.x() << rc2.y() << rc2.width() << rc2.height();
        return false;
    }

    KisHLineConstIteratorSP iter1 = dev1->createHLineConstIteratorNG(0, 0, rc1.width());
    KisHLineConstIteratorSP iter2 = dev2->createHLineConstIteratorNG(0, 0, rc1.width());

    int pixelSize = dev1->pixelSize();

    for (int y = 0; y < rc1.height(); ++y) {

        do {
            if (memcmp(iter1->oldRawData(), iter2->oldRawData(), pixelSize) != 0) {
                const channel_type* p1 = reinterpret_cast<const channel_type*>(iter1->oldRawData());
                const channel_type* p2 = reinterpret_cast<const channel_type*>(iter2->oldRawData());

                if (p1[3] < alphaThreshold && p2[3] < alphaThreshold) continue;

                qDebug() << "Failed compare paint devices:" << iter1->x() << iter1->y();
                qDebug() << "src:" << p1[0] << p1[1] << p1[2] << p1[3];
                qDebug() << "dst:" << p2[0] << p2[1] << p2[2] << p2[3];
                return false;
            }
        } while (iter1->nextPixel() && iter2->nextPixel());

        iter1->nextRow();
        iter2->nextRow();
    }

    return true;
}

#ifdef FILES_OUTPUT_DIR

// R-65 Task 3 · pk 栈守卫（R-75 已打开）：
// ReferenceImageChecker 存在的唯一目的就是调 checkQImage* 读写参考图（文件
// I/O）。R-65 时 pk 栈上那族被守卫掉（PkImage 无文件 I/O），本类跟着被守卫。
// R-75：R-15 交付 PkImage 文件 I/O 后，qimage_test_util.h 的 checkQImage* 族在
// pk 栈上打开，本类的守卫前提消失 ⇒ 一并打开。仅删掉 `#ifndef`/`#endif`，
// 代码体除 checkDevice 里那两处 `#ifdef`/`#else` 分叉（pk 栈上 `QImage ==
// PkImage`，`diagnosticQImage` 不存在、无需桥接）外零 diff。
struct ReferenceImageChecker
{
    enum StorageType {
        InternalStorage = 0,
        ExternalStorage
    };

    ReferenceImageChecker(const QString &prefix, const QString &testName, StorageType storageType = ExternalStorage)
        : m_storageType(storageType),
          m_prefix(prefix),
          m_testName(testName),
          m_success(true),
          m_maxFailingPixels(100),
          m_fuzzy(1)
        {
        }


    void setMaxFailingPixels(int value) {
        m_maxFailingPixels = value;
    }

    void setFuzzy(int fuzzy){
        m_fuzzy = fuzzy;
    }

    bool testPassed() const {
        return m_success;
    }

    inline bool checkDevice(KisPaintDeviceSP device, KisImageSP image, const QString &caseName) {
        bool result = false;


        if (m_storageType == ExternalStorage) {
            const PkImage converted = device->convertToQImage(0, image->bounds());
#ifdef KRITA_TESTSDK_PK_NATIVE
            // pk 栈：`QImage` 是 `PkImage` 的宏（pk/image/compat/QImage），
            // checkQImageExternal 直接收 PkImage；diagnosticQImage（PkImage→QImage
            // 桥）在 pk 栈上不存在，也不需要。
            result = checkQImageExternal(converted,
                                         m_testName,
                                         m_prefix,
                                         caseName, m_fuzzy, m_fuzzy, m_maxFailingPixels);
#else
            result = checkQImageExternal(diagnosticQImage(converted),
                                         m_testName,
                                         m_prefix,
                                         caseName, m_fuzzy, m_fuzzy, m_maxFailingPixels);
#endif
        } else {
            const PkImage converted = device->convertToQImage(0, image->bounds());
#ifdef KRITA_TESTSDK_PK_NATIVE
            result = checkQImage(converted,
                                 m_testName,
                                 m_prefix,
                                 caseName, m_fuzzy, m_fuzzy, m_maxFailingPixels);
#else
            result = checkQImage(diagnosticQImage(converted),
                                 m_testName,
                                 m_prefix,
                                 caseName, m_fuzzy, m_fuzzy, m_maxFailingPixels);
#endif
        }

        m_success &= result;
        return result;
    }

    inline bool checkImage(KisImageSP image, const QString &testName) {
        bool result = checkDevice(image->projection(), image, testName);

        m_success &= result;
        return result;
    }

private:
    bool m_storageType;

    QString m_prefix;
    QString m_testName;

    bool m_success;
    int m_maxFailingPixels;
    int m_fuzzy;
};


#endif

inline quint8 alphaDevicePixel(KisPaintDeviceSP dev, qint32 x, qint32 y)
{
    KisHLineConstIteratorSP iter = dev->createHLineConstIteratorNG(x, y, 1);
    const quint8 *pix = iter->oldRawData();
    return *pix;
}

inline void alphaDeviceSetPixel(KisPaintDeviceSP dev, qint32 x, qint32 y, quint8 s)
{
    KisHLineIteratorSP iter = dev->createHLineIteratorNG(x, y, 1);
    quint8 *pix = iter->rawData();
    *pix = s;
}

inline bool checkAlphaDeviceFilledWithPixel(KisPaintDeviceSP dev, const QRect &rc, quint8 expected)
{
    KisHLineIteratorSP it = dev->createHLineIteratorNG(rc.x(), rc.y(), rc.width());

    for (int y = rc.y(); y < rc.y() + rc.height(); y++) {
        for (int x = rc.x(); x < rc.x() + rc.width(); x++) {

            if(*((quint8*)it->rawData()) != expected) {
                errKrita << "At point:" << x << y;
                errKrita << "Expected pixel:" << expected;
                errKrita << "Actual pixel:  " << *((quint8*)it->rawData());
                return false;
            }
            it->nextPixel();
        }
        it->nextRow();
    }
    return true;
}

class TestNode : public DefaultNode
{
    Q_OBJECT
public:
    KisNodeSP clone() const override {
        return KisNodeSP(new TestNode(*this));
    }
};

class TestGraphListener : public KisNodeGraphListener
{
public:

    void aboutToAddANode(KisNode *parent, int index) override {
        KisNodeGraphListener::aboutToAddANode(parent, index);
        beforeInsertRow = true;
    }

    void nodeHasBeenAdded(KisNode *parent, int index, KisNodeAdditionFlags flags) override {
        KisNodeGraphListener::nodeHasBeenAdded(parent, index, flags);
        afterInsertRow = true;
    }

    void aboutToRemoveANode(KisNode *parent, int index) override {
        KisNodeGraphListener::aboutToRemoveANode(parent, index);
        beforeRemoveRow  = true;
    }

    void nodeHasBeenRemoved(KisNode *parent, int index) override {
        KisNodeGraphListener::nodeHasBeenRemoved(parent, index);
        afterRemoveRow = true;
    }

    void aboutToMoveNode(KisNode *parent, int oldIndex, int newIndex) override {
        KisNodeGraphListener::aboutToMoveNode(parent, oldIndex, newIndex);
        beforeMove = true;
    }

    void nodeHasBeenMoved(KisNode *parent, int oldIndex, int newIndex) override {
        KisNodeGraphListener::nodeHasBeenMoved(parent, oldIndex, newIndex);
        afterMove = true;
    }

    bool beforeInsertRow;
    bool afterInsertRow;
    bool beforeRemoveRow;
    bool afterRemoveRow;
    bool beforeMove;
    bool afterMove;

    void resetBools() {
        beforeRemoveRow = false;
        afterRemoveRow = false;
        beforeInsertRow = false;
        afterInsertRow = false;
        beforeMove = false;
        afterMove = false;
    }
};

}

#include <QApplication>
#include <kis_paint_layer.h>
#include "kis_undo_stores.h"
#include "kis_layer_utils.h"

namespace TestUtil {

struct MaskParent
{
    MaskParent(const PkRect &_imageRect = PkRect(0,0,512,512))
        : imageRect(_imageRect) {
        const KoColorSpace * cs = KoColorSpaceRegistry::instance()->rgb8();
        undoStore = new KisSurrogateUndoStore();
        image = new KisImage(undoStore, imageRect.width(), imageRect.height(), cs, "test image");
        layer = KisPaintLayerSP(new KisPaintLayer(image, "paint1", OPACITY_OPAQUE_U8));
        image->addNode(KisNodeSP(layer.data()));
    }

    // R-65 Task 3 · pk 栈守卫：QRect→PkRect 后本重载与上面带默认参数的
    // MaskParent(const PkRect&) 签名完全相同 ⇒ "constructor cannot be redeclared"。
    // PkRect 重载已完整覆盖该语义（PkRect 可由 x/y/width/height 构造）。
#ifndef KRITA_TESTSDK_PK_NATIVE
    explicit MaskParent(const QRect &_imageRect)
        : MaskParent(PkRect(_imageRect.x(), _imageRect.y(),
                            _imageRect.width(), _imageRect.height()))
    {
    }
#endif // !KRITA_TESTSDK_PK_NATIVE

    void waitForImageAndShapeLayers() {
        // PATTERN-1（sdk/tests/README.md「事件循环测试改造模式」）：
        // waitForDone() 已经是同步等待，原 qApp->processEvents() 是历史遗留
        // 保险动作；S-06 按模式删除。
        image->waitForDone();
        KisLayerUtils::forceAllDelayedNodesUpdate(image->root());
        /**
         * Shape updates have two channels of compression, 100ms each.
         * One in KoShapeManager, the other one in KisShapeLayerCanvas.
         * Therefore we should wait for a decent amount of time for all
         * of them to land.
         */

        do {
            // PATTERN-2（sdk/tests/README.md「事件循环测试改造模式」）：
            // 等待 KoShapeManager/KisShapeLayerCanvas 的 100ms QTimer 去抖，
            // 需要 S-08 交付显式同步 flush 方法后才能去掉这个轮询，不能
            // 简单换成 sleep（不会让挂起的 QTimer 触发，语义假绿）。
#ifdef KRITA_TESTSDK_PK_NATIVE
            KritaTestSdk::waitFor(500);
#else
            QTest::qWait(500);
#endif
        } while (!image->tryBarrierLock(true));
        image->unlock();
    }

    KisSurrogateUndoStore *undoStore;
    const PkRect imageRect;
    KisImageSP image;
    KisPaintLayerSP layer;
};

}

namespace TestUtil {

struct MeasureDistributionStats {
    MeasureDistributionStats(int numBins, const QString &name = QString())
        : m_numBins(numBins),
          m_name(name)
    {
        reset();
    }

    void reset() {
        m_values.clear();
        m_values.resize(m_numBins);
    }

    void addValue(int value) {
        addValue(value, 1);
    }

    void addValue(int value, int increment) {
        KIS_SAFE_ASSERT_RECOVER_RETURN(value >= 0);

        if (value >= m_numBins) {
            m_values[m_numBins - 1] += increment;
        } else {
            m_values[value] += increment;
        }
    }

    // R-65 Task 3 · pk 栈守卫：本诊断打印用 `PkString::arg(double, int, char, int)`
    // 四参重载（`…arg(qreal(…)*100.0, 7, 'g', 2)`），pk/string/PkString.h 只提供
    // arg(PkString×1..3)/arg(int)/arg(int,int)/arg(double)（pk/ 在本任务锁外，
    // 只报不改）。`MeasureDistributionStats` 全仓**零消费者**（唯一出现处就是
    // 本文件），故 pk 栈上不编译 print() 零影响；真 Qt 栈行为一个字不变。
#ifndef KRITA_TESTSDK_PK_NATIVE
    void print() {
        qCritical() << "============= Stats ==============";

        if (!m_name.isEmpty()) {
            qCritical() << "Name:" << m_name;
        }

        int total = 0;

        for (int i = 0; i < m_numBins; i++) {
            total += m_values[i];
        }

        for (int i = 0; i < m_numBins; i++) {
            if (!m_values[i]) continue;

            const QString lastMarker = i == m_numBins - 1 ? "> " : "  ";

            const QString line =
                QString("  %1%2: %3 (%4%)")
                    .arg(lastMarker)
                    .arg(i, 3)
                    .arg(m_values[i], 5)
                    .arg(qreal(m_values[i]) / total * 100.0, 7, 'g', 2);

            qCritical() << line;
        }
        qCritical() << "----                          ----";
        qCritical() << QString("Total: %1").arg(total);
        qCritical() << "==================================";
    }
#endif // !KRITA_TESTSDK_PK_NATIVE

private:
    QVector<int> m_values;
    int m_numBins = 0;
    QString m_name;
};

QStringList getHierarchy(KisNodeSP root, const QString &prefix = "");
bool checkHierarchy(KisNodeSP root, const QStringList &expected);

}

#endif
