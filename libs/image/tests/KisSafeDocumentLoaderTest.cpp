#include <PkGlobal.h>
#include "KisSafeDocumentLoaderTest.h"

#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QBuffer>
#include <QImage>
#include <QTest>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <filesystem>
#include <memory>
#include <simpletest.h>
#include <vector>
#include <KoColorSpaceRegistry.h>
#include <KoStore.h>

#include <PkEventLoop.h>
#include <PkImage.h>
#include <PkObject.h>
#include <PkThreadCallQueue.h>
#include <PkString.h>

#include "config-limit-long-tests.h"
#include "kis_safe_document_loader.h"
#include "kis_image.h"
#include "kis_debug.h"

namespace {

namespace fs = std::filesystem;

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

KisSafeDocumentLoader::LoadResult successfulLoadResult()
{
    KisPaintDeviceSP device(new KisPaintDevice(KoColorSpaceRegistry::instance()->rgb8()));
    return {device, 1.0, 1.0, PkSize(1, 1)};
}

void writeBytes(QFile &file, const QByteArray &bytes)
{
    file.reset();
    file.resize(0);
    QCOMPARE(file.write(bytes), bytes.size());
    QVERIFY(file.flush());
}

bool isPrivateRegularFile(const PkString &path)
{
    std::error_code error;
    const fs::path native = fs::u8path(path.PkToUtf8());
    if (!fs::is_regular_file(fs::symlink_status(native, error)) || error) return false;
#ifdef _WIN32
    return true;
#else
    const fs::perms permissions = fs::status(native, error).permissions();
    if (error) return false;
    constexpr fs::perms nonOwner = fs::perms::group_read | fs::perms::group_write |
        fs::perms::group_exec | fs::perms::others_read | fs::perms::others_write |
        fs::perms::others_exec;
    return (permissions & nonOwner) == fs::perms::none;
#endif
}

bool waitForPendingCall(std::size_t previousCount, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (PkThreadCallQueue::pendingCount() > previousCount) return true;
        QTest::qWait(10);
    }
    return PkThreadCallQueue::pendingCount() > previousCount;
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

void KisSafeDocumentLoaderTest::testTemporaryCopiesArePrivateAndArchiveUsesMergedImage()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString plainPath = directory.filePath("source.png");
    QImage image(3, 2, QImage::Format_ARGB32);
    image.fill(Qt::red);
    QVERIFY(image.save(plainPath, "PNG"));

    PkString observedPlainPath;
    bool plainWasPrivate = false;
    KisSafeDocumentLoader plainLoader(
        toPkString(plainPath),
        [&](const PkString &path) {
            observedPlainPath = path;
            plainWasPrivate = isPrivateRegularFile(path);
            return loadImage(path);
        });
    plainLoader.reloadImage();
    QVERIFY(plainWasPrivate);
    QVERIFY(observedPlainPath != toPkString(plainPath));
    QVERIFY(!fs::exists(fs::u8path(observedPlainPath.PkToUtf8())));

    QByteArray pngBytes;
    QBuffer pngBuffer(&pngBytes);
    QVERIFY(pngBuffer.open(QIODevice::WriteOnly));
    QVERIFY(image.save(&pngBuffer, "PNG"));
    pngBuffer.close();

    for (const char *extension : {"kra", "ora"}) {
        const PkString archivePath =
            toPkString(directory.filePath(QStringLiteral("source.%1").arg(extension)));
        {
            std::unique_ptr<KoStore> store(
                KoStore::createStore(archivePath, KoStore::Write, PkByteArray(), KoStore::Zip, false));
            QVERIFY(store);
            QVERIFY(!store->bad());
            QVERIFY(store->open("mergedimage.png"));
            QCOMPARE(store->write(pngBytes.constData(), pngBytes.size()), pngBytes.size());
            QVERIFY(store->close());
            QVERIFY(store->finalize());
        }

        PkString observedMergedPath;
        bool mergedWasPrivate = false;
        KisSafeDocumentLoader archiveLoader(
            archivePath,
            [&](const PkString &path) {
                observedMergedPath = path;
                mergedWasPrivate = isPrivateRegularFile(path);
                return loadImage(path);
            });
        archiveLoader.reloadImage();
        QVERIFY(mergedWasPrivate);
        QVERIFY(observedMergedPath.endsWith(".png"));
        QVERIFY(observedMergedPath != archivePath);
        QVERIFY(!fs::exists(fs::u8path(observedMergedPath.PkToUtf8())));
    }
}

void KisSafeDocumentLoaderTest::testDestroyWithDebouncePending()
{
    QTemporaryFile file("safe-loader-debounce-destroy-XXXXXX.s09gdebounce");
    QVERIFY(file.open());
    writeBytes(file, "a");
    int loadCount = 0;
    {
        std::unique_ptr<KisSafeDocumentLoader> loader(new KisSafeDocumentLoader(
            toPkString(file.fileName()),
            [&](const PkString &) {
                ++loadCount;
                return successfulLoadResult();
            }));
        loader->reloadImage();
        QCOMPARE(loadCount, 1);
        writeBytes(file, "changed");
        const std::size_t pendingBeforeWatcher = PkThreadCallQueue::pendingCount();
        QVERIFY(waitForPendingCall(pendingBeforeWatcher, std::chrono::milliseconds(1500)));
        PkEventLoop::processEvents();
        QVERIFY(loader->hasPendingDebounceForTesting());
    }
    QTest::qWait(700);
    PkEventLoop::processEvents();
    QCOMPARE(loadCount, 1);
}

void KisSafeDocumentLoaderTest::testDestroyWithDelayedLoadPending()
{
    QTemporaryFile file("safe-loader-delayed-destroy-XXXXXX.s09gdelayed");
    QVERIFY(file.open());
    writeBytes(file, "a");
    int loadCount = 0;
    PkString temporaryPath;
    {
        std::unique_ptr<KisSafeDocumentLoader> loader(new KisSafeDocumentLoader(
            toPkString(file.fileName()),
            [&](const PkString &) {
                ++loadCount;
                return successfulLoadResult();
            }));
        loader->reloadImage();
        QCOMPARE(loadCount, 1);
        writeBytes(file, "changed");
        QVERIFY(waitFor([&] {
            temporaryPath = loader->temporaryCopyPathForTesting();
            return !temporaryPath.isEmpty() &&
                fs::exists(fs::u8path(temporaryPath.PkToUtf8()));
        }, std::chrono::milliseconds(1500)));
    }
    QTest::qWait(300);
    PkEventLoop::processEvents();
    QCOMPARE(loadCount, 1);
    QVERIFY(!temporaryPath.isEmpty());
    QVERIFY(!fs::exists(fs::u8path(temporaryPath.PkToUtf8())));
}

void KisSafeDocumentLoaderTest::testDestroyWithWatcherEventQueued()
{
    QTemporaryFile file("safe-loader-queued-watcher-XXXXXX.bin");
    QVERIFY(file.open());
    writeBytes(file, "a");
    int loadCount = 0;
    {
        std::unique_ptr<KisSafeDocumentLoader> loader(new KisSafeDocumentLoader(
            toPkString(file.fileName()),
            [&](const PkString &) {
                ++loadCount;
                return successfulLoadResult();
            }));
        loader->reloadImage();
        QCOMPARE(loadCount, 1);
        writeBytes(file, "queued-change");
        const std::size_t pendingBeforeWatcher = PkThreadCallQueue::pendingCount();
        QVERIFY(waitForPendingCall(pendingBeforeWatcher, std::chrono::milliseconds(1500)));
    }
    PkEventLoop::processEvents();
    QTest::qWait(700);
    PkEventLoop::processEvents();
    QCOMPARE(loadCount, 1);
}

void KisSafeDocumentLoaderTest::testSharedPathSurvivesOneLoaderDestruction()
{
    QTemporaryFile file("safe-loader-shared-path-XXXXXX.bin");
    QVERIFY(file.open());
    writeBytes(file, "a");
    int firstCount = 0;
    int secondCount = 0;
    std::unique_ptr<KisSafeDocumentLoader> first(new KisSafeDocumentLoader(
        toPkString(file.fileName()),
        [&](const PkString &) {
            ++firstCount;
            return successfulLoadResult();
        }));
    KisSafeDocumentLoader second(
        toPkString(file.fileName()),
        [&](const PkString &) {
            ++secondCount;
            return successfulLoadResult();
        });
    first->reloadImage();
    second.reloadImage();
    QCOMPARE(firstCount, 1);
    QCOMPARE(secondCount, 1);

    first.reset();
    writeBytes(file, "changed");
    QVERIFY(waitFor([&] { return secondCount == 2; }, std::chrono::milliseconds(2000)));
    QCOMPARE(firstCount, 1);
}

void KisSafeDocumentLoaderTest::testDebounceAndRetryCounts()
{
    QTemporaryFile debounceFile("safe-loader-debounce-count-XXXXXX.bin");
    QVERIFY(debounceFile.open());
    writeBytes(debounceFile, "a");
    int successCount = 0;
    KisSafeDocumentLoader debounceLoader(
        toPkString(debounceFile.fileName()),
        [&](const PkString &) {
            ++successCount;
            return successfulLoadResult();
        });
    debounceLoader.reloadImage();
    QCOMPARE(successCount, 1);
    for (const QByteArray &contents : {QByteArray("one"), QByteArray("two-two"), QByteArray("three-three")}) {
        writeBytes(debounceFile, contents);
        QTest::qWait(100);
        PkEventLoop::processEvents();
    }
    QVERIFY(waitFor([&] { return successCount == 2; }, std::chrono::milliseconds(2000)));
    QTest::qWait(750);
    PkEventLoop::processEvents();
    QCOMPARE(successCount, 2);

    QTemporaryFile retryFile("safe-loader-retry-count-XXXXXX.bin");
    QVERIFY(retryFile.open());
    writeBytes(retryFile, "not-loadable");
    int attemptCount = 0;
    int failedCount = 0;
    KisSafeDocumentLoader retryLoader(
        toPkString(retryFile.fileName()),
        [&](const PkString &) {
            ++attemptCount;
            return KisSafeDocumentLoader::LoadResult {};
        });
    PkObject receiver;
    PkObject::connect(&retryLoader, &KisSafeDocumentLoader::loadingFailed,
                      &receiver, [&] { ++failedCount; });
    retryLoader.reloadImage();
    QVERIFY(waitFor([&] { return failedCount == 1; }, std::chrono::milliseconds(2500)));
    QCOMPARE(attemptCount, 3);
    QTest::qWait(750);
    PkEventLoop::processEvents();
    QCOMPARE(attemptCount, 3);
    QCOMPARE(failedCount, 1);
}

SIMPLE_TEST_MAIN(KisSafeDocumentLoaderTest)
