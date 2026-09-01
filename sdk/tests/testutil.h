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


#ifndef TEST_UTIL
#define TEST_UTIL

#include <QProcessEnvironment>

#include <simpletest.h>
#include <QTime>
#include <QDir>

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

inline QImage diagnosticQImage(const PkImage &image)
{
    if (image.isNull()) return QImage();
    return QImage(reinterpret_cast<const uchar *>(image.constBits()), image.width(), image.height(),
                  image.bytesPerLine(), QImage::Format_ARGB32).copy();
}

inline PkImage pkImageFromQImage(const QImage &image)
{
    PkImage result(image.width(), image.height(), PkImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        memcpy(result.scanLine(y), image.constScanLine(y), static_cast<size_t>(image.bytesPerLine()));
    }
    return result;
}

inline bool checkQImage(const PkImage &image, const QString &testName,
                        const QString &prefix, const QString &caseName,
                        int fuzzy = 0, int fuzzyAlpha = 0, int maxNumFailingPixels = 0)
{
    return checkQImage(diagnosticQImage(image), testName, prefix, caseName,
                       fuzzy, fuzzyAlpha, maxNumFailingPixels);
}

inline bool checkQImageExternal(const PkImage &image, const QString &testName,
                                const QString &prefix, const QString &caseName,
                                int fuzzy = 0, int fuzzyAlpha = 0, int maxNumFailingPixels = 0)
{
    return checkQImageExternal(diagnosticQImage(image), testName, prefix, caseName,
                               fuzzy, fuzzyAlpha, maxNumFailingPixels);
}

inline KisNodeSP findNode(KisNodeSP root, const PkString &name) {
    if(root->name() == name) return root;

    KisNodeSP child = root->firstChild();
    while (child) {
        if((root = findNode(child, name))) return root;
        child = child->nextSibling();
    }

    return KisNodeSP();
}

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
    PkString format() { return m_format; }
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
            const QImage diagnostic(reinterpret_cast<const uchar *>(converted.constBits()), converted.width(), converted.height(), converted.bytesPerLine(), QImage::Format_ARGB32);
            result = checkQImageExternal(diagnostic.copy(),
                                         m_testName,
                                         m_prefix,
                                         caseName, m_fuzzy, m_fuzzy, m_maxFailingPixels);
        } else {
            const PkImage converted = device->convertToQImage(0, image->bounds());
            const QImage diagnostic(reinterpret_cast<const uchar *>(converted.constBits()), converted.width(), converted.height(), converted.bytesPerLine(), QImage::Format_ARGB32);
            result = checkQImage(diagnostic.copy(),
                                 m_testName,
                                 m_prefix,
                                 caseName, m_fuzzy, m_fuzzy, m_maxFailingPixels);
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
    MaskParent(const QRect &_imageRect = QRect(0,0,512,512))
        : imageRect(_imageRect) {
        const KoColorSpace * cs = KoColorSpaceRegistry::instance()->rgb8();
        undoStore = new KisSurrogateUndoStore();
        image = new KisImage(undoStore, imageRect.width(), imageRect.height(), cs, "test image");
        layer = KisPaintLayerSP(new KisPaintLayer(image, "paint1", OPACITY_OPAQUE_U8));
        image->addNode(KisNodeSP(layer.data()));
    }

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
            QTest::qWait(500);
        } while (!image->tryBarrierLock(true));
        image->unlock();
    }

    KisSurrogateUndoStore *undoStore;
    const QRect imageRect;
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

            qCritical() << qPrintable(line);
        }
        qCritical() << "----                          ----";
        qCritical() << qPrintable(QString("Total: %1").arg(total));
        qCritical() << "==================================";
    }

private:
    QVector<int> m_values;
    int m_numBins = 0;
    QString m_name;
};

QStringList getHierarchy(KisNodeSP root, const QString &prefix = "");
bool checkHierarchy(KisNodeSP root, const QStringList &expected);

}

#endif
