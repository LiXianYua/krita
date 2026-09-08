#include <PkGlobal.h>
#include "KisSafeDocumentLoaderTest.h"

#include <QTemporaryFile>
#include <QImage>
#include <QTest>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <simpletest.h>
#include <vector>
#include <KoColorSpaceRegistry.h>

#include <PkEventLoop.h>
#include <PkImage.h>
#include <PkObject.h>
#include <PkString.h>

#include "config-limit-long-tests.h"
#include "kis_safe_document_loader.h"
#include "kis_image.h"
#include "kis_debug.h"

namespace {

PkString toPkString(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return PkString::PkFromUtf8(utf8.constData(), utf8.size());
}

QString toQString(const PkString &value)
{
    const std::string utf8 = value.PkToUtf8();
    return QString::fromUtf8(utf8.data(), int(utf8.size()));
}

bool waitFor(const std::function<bool()> &condition, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do {
        PkEventLoop::processEvents();
        if (condition()) return true;
        QTest::qWait(10);
    } while (std::chrono::steady_clock::now() < deadline);

    PkEventLoop::processEvents();
    return condition();
}

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

KisSafeDocumentLoader::LoadResult loadImage(const PkString &path)
{
    const QImage image(toQString(path));
    if (image.isNull()) {
        return {};
    }

    KisPaintDeviceSP device(new KisPaintDevice(KoColorSpaceRegistry::instance()->rgb8()));
    device->convertFromQImage(toPkImage(image), 0);
    return {device, 1.0, 1.0, PkSize(image.width(), image.height())};
}

void writeToFile(QFile &file, QColor /*color*/)
{
    file.reset();
    file.resize(0);
    QImage img(10,10,QImage::Format_ARGB32);
    img.fill(Pk::black);
    img.save(&file, "PNG");
    file.flush();
}

}

void KisSafeDocumentLoaderTest::test()
{
    QTemporaryFile file("safe_loader_test_XXXXXX.png");

    KIS_ASSERT(file.open());
    writeToFile(file, Pk::black);

    KisSafeDocumentLoader loader(toPkString(file.fileName()), loadImage);
    PkObject receiver;
    int finishedCount = 0;
    int failedCount = 0;
    PkObject::connect(&loader, &KisSafeDocumentLoader::loadingFinished,
                      &receiver, [&](KisPaintDeviceSP, qreal, qreal, PkSize) {
                          ++finishedCount;
                      });
    PkObject::connect(&loader, &KisSafeDocumentLoader::loadingFailed,
                      &receiver, [&] { ++failedCount; });

    // reloadImage() is synchronous
    loader.reloadImage();
    QCOMPARE(finishedCount, 1);
    QCOMPARE(failedCount, 0);

    writeToFile(file, Pk::white);

    QVERIFY(waitFor([&] { return finishedCount == 2; }, std::chrono::milliseconds(2500)));
    QCOMPARE(failedCount, 0);
    finishedCount = 0;

    file.reset();
    file.resize(0);
    file.write("blah-blah-try-read-me");
    file.flush();

    QVERIFY(waitFor([&] { return failedCount == 1; }, std::chrono::milliseconds(6000)));
    QCOMPARE(finishedCount, 0);

}

void KisSafeDocumentLoaderTest::testFileLost()
{
    QTemporaryFile file("safe_loader_test_XXXXXX.png");

    KIS_ASSERT(file.open());
    writeToFile(file, Pk::black);

    KisSafeDocumentLoader loader(toPkString(file.fileName()), loadImage);
    PkObject receiver;
    int finishedCount = 0;
    int failedCount = 0;
    std::vector<bool> existsStates;
    PkObject::connect(&loader, &KisSafeDocumentLoader::loadingFinished,
                      &receiver, [&](KisPaintDeviceSP, qreal, qreal, PkSize) {
                          ++finishedCount;
                      });
    PkObject::connect(&loader, &KisSafeDocumentLoader::loadingFailed,
                      &receiver, [&] { ++failedCount; });
    PkObject::connect(&loader, &KisSafeDocumentLoader::fileExistsStateChanged,
                      &receiver, [&](bool exists) { existsStates.push_back(exists); });

    // reloadImage() is synchronous
    loader.reloadImage();
    QCOMPARE(finishedCount, 1);
    QCOMPARE(failedCount, 0);
    QCOMPARE(existsStates.size(), std::size_t(0));
    finishedCount = 0;

    file.close();
    file.remove();

    QVERIFY(waitFor([&] { return existsStates.size() == 1; }, std::chrono::milliseconds(15000)));
    QCOMPARE(existsStates[0], false);
    existsStates.clear();

    KIS_ASSERT(file.open());
    writeToFile(file, Pk::white);

    QVERIFY(waitFor([&] { return existsStates.size() == 1; }, std::chrono::milliseconds(3500)));
    QCOMPARE(finishedCount, 0);
    QCOMPARE(failedCount, 0);
    QCOMPARE(existsStates[0], true);
    existsStates.clear();

    QVERIFY(waitFor([&] { return finishedCount == 1; }, std::chrono::milliseconds(1500)));
    QCOMPARE(failedCount, 0);
    QCOMPARE(existsStates.size(), std::size_t(0));
    finishedCount = 0;


    writeToFile(file, Pk::yellow);
    QVERIFY(waitFor([&] { return finishedCount == 1; }, std::chrono::milliseconds(1500)));
    QCOMPARE(failedCount, 0);
    QCOMPARE(existsStates.size(), std::size_t(0));
}

void KisSafeDocumentLoaderTest::testQueuedDeliveryHonorsReceiverLifetime()
{
    QTemporaryFile file("safe_loader_test_XXXXXX.png");
    KIS_ASSERT(file.open());
    writeToFile(file, Pk::black);

    KisSafeDocumentLoader loader(toPkString(file.fileName()), loadImage);
    int deliveryCount = 0;

    {
        PkObject receiver;
        PkObject::connect(&loader, &KisSafeDocumentLoader::loadingFinished,
                          &receiver,
                          [&](KisPaintDeviceSP, qreal, qreal, PkSize size) {
                              QCOMPARE(size, PkSize(10, 10));
                              ++deliveryCount;
                          },
                          PkConnectionType::Queued);
        loader.reloadImage();
        QCOMPARE(deliveryCount, 0);
        PkEventLoop::processEvents();
        QCOMPARE(deliveryCount, 1);
    }

    {
        std::unique_ptr<PkObject> receiver(new PkObject);
        PkObject::connect(&loader, &KisSafeDocumentLoader::loadingFinished,
                          receiver.get(),
                          [&](KisPaintDeviceSP, qreal, qreal, PkSize) {
                              ++deliveryCount;
                          },
                          PkConnectionType::Queued);
        loader.reloadImage();
        receiver.reset();
        PkEventLoop::processEvents();
        QCOMPARE(deliveryCount, 1);
    }
}

SIMPLE_TEST_MAIN(KisSafeDocumentLoaderTest)
