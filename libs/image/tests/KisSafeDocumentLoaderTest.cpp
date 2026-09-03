#include "KisSafeDocumentLoaderTest.h"

#include <QSignalSpy>
#include <QTemporaryFile>
#include <QImage>
#include <cstdint>
#include <cstring>
#include <simpletest.h>
#include <vector>
#include <KoColorSpaceRegistry.h>

#include <PkImage.h>

#include "config-limit-long-tests.h"
#include "kis_safe_document_loader.h"
#include "kis_image.h"
#include "kis_debug.h"

// 迁移回归修复：KisSafeDocumentLoader 的 loadingFinished(KisPaintDeviceSP,…) /
// loadingFailed(KisImageSP) 在跨线程 queued 发射时，QSignalSpy 需要 KisPaintDeviceSP /
// KisImageSP 已注册 Qt 元类型才能接收参数。原未迁移 Krita 靠同线程直连计数，迁移后
// 线程模型变化，queued 信号序列化要求该元类型。以 typedef 名注册（moc 记录的信号
// 参数名即 typedef 名）。注意：不能放进 kis_paint_device.h / kis_image.h —— 那里加
// <QMetaType> 会把 QtCore 拉进 kritaimage 的 compat 层 TU，触发 qAbs 重定义导致整库
// 编译失败；此测试 TU 已正常包含 Qt 头，故在此声明。
#include <QMetaType>
Q_DECLARE_METATYPE(KisPaintDeviceSP)
Q_DECLARE_METATYPE(KisImageSP)

// Q_DECLARE_METATYPE 只是声明 trait；QSignalSpy 构造时按参数类型名查 QMetaType
// 注册库，必须显式 qRegisterMetaType 把构造/析构落库，queued 信号才能被序列化接收。
// 放在文件作用域静态初始化器里，确保在 QTest::qExec 连接 spy 之前完成注册。
static const bool s_registeredSignalMetatypes = []() {
    qRegisterMetaType<KisPaintDeviceSP>();
    qRegisterMetaType<KisImageSP>();
    return true;
}();

namespace {

// 真 Qt QImage -> PkImage 桥接（同 libs/canvas 匿名命名空间里的 toPkImage，
// 此处因测试只链 kritaimage/kritatestsdk、够不到 canvas 而本地复刻）。
PkImage toPkImage(const QImage &image)
{
    PkImage result(image.width(), image.height(),
                   static_cast<PkImage::Format>(image.format()));
    for (int y = 0; y < image.height(); ++y) {
        std::memcpy(result.scanLine(y), image.constScanLine(y),
                    static_cast<std::size_t>(image.bytesPerLine()));
    }
    if (image.colorCount() > 0) {
        std::vector<std::uint32_t> colorTable;
        colorTable.reserve(static_cast<std::size_t>(image.colorCount()));
        for (int i = 0; i < image.colorCount(); ++i) {
            colorTable.push_back(static_cast<std::uint32_t>(image.color(i)));
        }
        result.setColorTable(colorTable);
    }
    return result;
}

KisSafeDocumentLoader::LoadResult loadImage(const QString &path)
{
    const QImage image(path);
    if (image.isNull()) {
        return {};
    }

    KisPaintDeviceSP device(new KisPaintDevice(KoColorSpaceRegistry::instance()->rgb8()));
    device->convertFromQImage(toPkImage(image), 0);
    return {device, 1.0, 1.0, image.size()};
}

void writeToFile(QFile &file, QColor /*color*/)
{
    file.reset();
    file.resize(0);
    QImage img(10,10,QImage::Format_ARGB32);
    img.fill(Qt::black);
    img.save(&file, "PNG");
    file.flush();
}

}

void KisSafeDocumentLoaderTest::test()
{
    QTemporaryFile file("safe_loader_test_XXXXXX.png");

    KIS_ASSERT(file.open());
    writeToFile(file, Qt::black);

    KisSafeDocumentLoader loader(file.fileName(), loadImage);

    QSignalSpy spy(&loader, &KisSafeDocumentLoader::loadingFinished);
    QSignalSpy spyFailed(&loader, &KisSafeDocumentLoader::loadingFailed);

    // reloadImage() is synchronous
    loader.reloadImage();
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spyFailed.size(), 0);

    writeToFile(file, Qt::white);

    spy.wait(1500);
    QCOMPARE(spy.size(), 2);
    QCOMPARE(spyFailed.size(), 0);

    spy.clear();

    file.reset();
    file.resize(0);
    file.write("blah-blah-try-read-me");
    file.flush();

    spyFailed.wait(6000);

    QCOMPARE(spy.size(), 0);
    QCOMPARE(spyFailed.size(), 1);

}

void KisSafeDocumentLoaderTest::testFileLost()
{
    QTemporaryFile file("safe_loader_test_XXXXXX.png");

    KIS_ASSERT(file.open());
    writeToFile(file, Qt::black);

    KisSafeDocumentLoader loader(file.fileName(), loadImage);

    QSignalSpy spy(&loader, &KisSafeDocumentLoader::loadingFinished);
    QSignalSpy spyFailed(&loader, &KisSafeDocumentLoader::loadingFailed);
    QSignalSpy spyExistsState(&loader, &KisSafeDocumentLoader::fileExistsStateChanged);

    // reloadImage() is synchronous
    loader.reloadImage();
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spyFailed.size(), 0);
    QCOMPARE(spyExistsState.size(), 0);

    spy.clear();

    file.close();
    file.remove();

    spyExistsState.wait(15000);
    QCOMPARE(spyExistsState.size(), 1);
    QCOMPARE(spyExistsState[0][0].toBool(), false);
    spyExistsState.clear();

    KIS_ASSERT(file.open());
    writeToFile(file, Qt::white);

    spyExistsState.wait(3500);
    QCOMPARE(spy.size(), 0);
    QCOMPARE(spyFailed.size(), 0);
    QCOMPARE(spyExistsState.size(), 1);
    QCOMPARE(spyExistsState[0][0].toBool(), true);
    spyExistsState.clear();

    spy.wait(1500);
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spyFailed.size(), 0);
    QCOMPARE(spyExistsState.size(), 0);
    spy.clear();


    writeToFile(file, Qt::yellow);
    spy.wait(1500);
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spyFailed.size(), 0);
    QCOMPARE(spyExistsState.size(), 0);
}

SIMPLE_TEST_MAIN(KisSafeDocumentLoaderTest)
