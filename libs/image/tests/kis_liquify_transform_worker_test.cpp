/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkGlobal.h>
#include "kis_liquify_transform_worker_test.h"

#include <simpletest.h>
#include <QPainter>

#include <KoColor.h>
#include <KoProgressUpdater.h>
#include <KoUpdater.h>

#include <testutil.h>
#include <kis_liquify_transform_worker.h>
#include <kis_algebra_2d.h>
#include <quazip.h>
#include <quazipfile.h>
#include <PkXmlDocument.h>


// copied from KisLiquifyTransformWorkerBenchmark
KisLiquifyTransformWorker* getWorkerFromIODeviceXml(QIODevice* device)
{
    const QByteArray xml = device->readAll();
    PkXmlDocument doc;
    doc.setContent(PkString::PkFromUtf8(xml.constData(), xml.size()));

    PkXmlElement rootElement = doc.documentElement();
    PkXmlElement data = rootElement.firstChildElement("data");
    return KisLiquifyTransformWorker::fromXML(data);
}

// copied from KisLiquifyTransformWorkerBenchmark
KisLiquifyTransformWorker* getWorkerFromXml(QString filename)
{

    qDebug() << ppVar(filename);
    filename = TestUtil::fetchDataFileLazy(filename);

    qDebug() << ppVar(filename);


    if (filename.endsWith("zip")) {
        QuaZip zipArchive(filename);
        bool result = zipArchive.open(QuaZip::mdUnzip);
        if (!result) return nullptr;
        QStringList filenames = zipArchive.getFileNameList();
        if (filenames.length() < 1) {
            return nullptr;
        }
        // it should be the first and only file
        QString correctFilename = filenames[0];
        if (!zipArchive.setCurrentFile(correctFilename)) {
            qWarning() << "Could not set current file" << zipArchive.getZipError() << correctFilename;
            return nullptr;
        }

        QuaZipFile zipFile = QuaZipFile(&zipArchive);
        if (!zipFile.open(QIODevice::ReadOnly)) {
            qWarning() << "It could not open the file" << zipArchive.getZipError();
            return nullptr;
        }
        return getWorkerFromIODeviceXml(&zipFile);

    } else {
        QFile file(filename);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Could not find the file" << filename;
            return nullptr;
        }

        return getWorkerFromIODeviceXml(&file);

    }

}


void KisLiquifyTransformWorkerTest::testPoints()
{
    TestUtil::TestProgressBar bar;
    KoProgressUpdater pu(&bar);
    KoUpdaterPtr updater = pu.startSubtask();

    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    QImage image(TestUtil::fetchDataFileLazy("test_transform_quality_second.png"));

    KisPaintDeviceSP dev = new KisPaintDevice(cs);
    dev->convertFromQImage(TestUtil::pkImageFromQImage(image), 0);

    KisPaintDeviceSP srcDev = new KisPaintDevice(*dev);

    const int pixelPrecision = 8;

    KisLiquifyTransformWorker worker(dev->exactBounds(),
                                     updater,
                                     pixelPrecision);


    QBENCHMARK_ONCE {
        worker.translatePoints(PkPointF(100,100),
                               PkPointF(50, 0),
                               50, false, 0.2);

        worker.scalePoints(PkPointF(400,100),
                           0.9,
                           50, false, 0.2);

        worker.undoPoints(PkPointF(400,100),
                           1.0,
                           50);

        worker.scalePoints(PkPointF(400,300),
                           0.5,
                           50, false, 0.2);

        worker.scalePoints(PkPointF(100,300),
                           -0.5,
                           30, false, 0.2);

        worker.rotatePoints(PkPointF(100,500),
                            M_PI / 4,
                            50, false, 0.2);
    }

    worker.run(srcDev, dev);

    PkImage result = dev->convertToQImage(0);
    TestUtil::checkQImage(result, "liquify_transform_test", "liquify_dev", "unity");
}

void KisLiquifyTransformWorkerTest::testPointsQImage()
{
    TestUtil::TestProgressBar bar;
    KoProgressUpdater pu(&bar);
    KoUpdaterPtr updater = pu.startSubtask();

    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    QImage image(TestUtil::fetchDataFileLazy("test_transform_quality_second.png"));

    KisPaintDeviceSP dev = new KisPaintDevice(cs);
    dev->convertFromQImage(TestUtil::pkImageFromQImage(image), 0);

    KisPaintDeviceSP srcDev = new KisPaintDevice(*dev);

    const int pixelPrecision = 8;

    KisLiquifyTransformWorker worker(dev->exactBounds(),
                                     updater,
                                     pixelPrecision);


    worker.translatePoints(PkPointF(100,100),
                           PkPointF(50, 0),
                           50, false, 0.2);

    PkRect rc = dev->exactBounds();
    dev->setX(50);
    dev->setY(50);
    worker.run(srcDev, dev);
    rc |= dev->exactBounds();

    PkImage resultDev = dev->convertToQImage(0, rc.x(), rc.y(), rc.width(), rc.height());
    TestUtil::checkQImage(resultDev, "liquify_transform_test", "liquify_qimage", "refDevice");

    const PkTransform imageToThumbTransform =
        PkTransform::fromScale(0.5, 0.5);

    QImage srcImage(image);
    image = QImage(image.size(), QImage::Format_ARGB32);
    QPainter gc(&image);
    gc.setTransform(QTransform::fromScale(0.5, 0.5));
    gc.drawImage(QPoint(), srcImage);

    PkPointF newOffset;
    PkImage result = worker.runOnImage(TestUtil::pkImageFromQImage(image),
                                       PkPointF(10, 10), imageToThumbTransform,
                                       &newOffset);


    TestUtil::checkQImage(result, "liquify_transform_test", "liquify_qimage", "resultImage");
}

void KisLiquifyTransformWorkerTest::testIdentityTransform()
{
    TestUtil::TestProgressBar bar;
    KoProgressUpdater pu(&bar);
    KoUpdaterPtr updater = pu.startSubtask();

    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();

    PkRect rc(0,0,13,23);

    KisPaintDeviceSP dev = new KisPaintDevice(cs);
    dev->fill(rc, KoColor(Pk::blue, cs));

    KisPaintDeviceSP srcDev = new KisPaintDevice(*dev);

    const int pixelPrecision = 8;

    KisLiquifyTransformWorker worker(dev->exactBounds(),
                                     updater,
                                     pixelPrecision);

    worker.run(srcDev, dev);

    PkImage result = dev->convertToQImage(0, rc);
    TestUtil::checkQImage(result, "liquify_transform_test", "liquify_dev", "identity");
}

void KisLiquifyTransformWorkerTest::testMaskRendering_data()
{
    QTest::addColumn<QString>("shortTestName");
    QTest::addColumn<QString>("baseImageFilename");
    QTest::addColumn<QString>("configFilename");
    QTest::addColumn<QString>("resultImageFilename");
    QTest::addColumn<bool>("isQImageTest");


    QTest::addRow("whole image") << "wholeImage" << "liquify_mask_rendering/liquify_mask_rendering_base_image.png" << "liquify_mask_rendering/liquify_mask_rendering_mask_config.zip" << "liquify_mask_rendering/liquify_mask_rendering_resultImage.png" << false;
    QTest::addRow("just small part inside") << "smallPart" << "liquify_mask_rendering/liquify_mask_rendering_base_image.png"
                                            << "liquify_mask_rendering/liquify_mask_rendering_small_change_mask_config.zip" << "liquify_mask_rendering/liquify_mask_rendering_small_change_resultImage.png" << false;

    QTest::addRow("whole image with bg") << "wholeImageBg" << "liquify_mask_rendering/liquify_mask_rendering_base_with_background.png"
                                         << "liquify_mask_rendering/liquify_mask_rendering_mask_config.zip" << "liquify_mask_rendering/liquify_mask_rendering_with_background_resultImage.png" << false;
    QTest::addRow("just small part inside with bg") << "smallPartBg" << "liquify_mask_rendering/liquify_mask_rendering_base_with_background.png"
                                                    << "liquify_mask_rendering/liquify_mask_rendering_small_change_mask_config.zip" << "liquify_mask_rendering/liquify_mask_rendering_with_background_small_change_resultImage.png" << false;

    QTest::addRow("16bit") << "16bit" << "liquify_mask_rendering/liquify_mask_rendering_16bit_base_image.png"
                           << "liquify_mask_rendering/liquify_mask_rendering_16bit_mask_config.zip" << "liquify_mask_rendering/liquify_mask_rendering_16bit_resultImage.png" << false;

    QTest::addRow("16bit small change") << "16bitSmall" << "liquify_mask_rendering/liquify_mask_rendering_16bit_base_image.png"
                           << "liquify_mask_rendering/liquify_mask_rendering_16bit_small_change_mask_config.zip" << "liquify_mask_rendering/liquify_mask_rendering_16bit_small_change_resultImage.png" << false;



    QTest::addRow("whole image, qimage") << "wholeImageQImage" << "liquify_mask_rendering/liquify_mask_rendering_base_image.png" << "liquify_mask_rendering/liquify_mask_rendering_mask_config.zip"
                                         << "liquify_mask_rendering/liquify_mask_rendering_qimage_resultImage.png" << true;
    QTest::addRow("just small part inside, qimage") << "smallPartQImage" << "liquify_mask_rendering/liquify_mask_rendering_base_image.png"
                                            << "liquify_mask_rendering/liquify_mask_rendering_small_change_mask_config.zip" << "liquify_mask_rendering/liquify_mask_rendering_small_change_qimage_resultImage.png" << true;

    QTest::addRow("whole image with bg, qimage") << "wholeImageBgQImage" << "liquify_mask_rendering/liquify_mask_rendering_base_with_background.png"
                                         << "liquify_mask_rendering/liquify_mask_rendering_mask_config.zip" << "liquify_mask_rendering/liquify_mask_rendering_with_background_qimage_resultImage.png" << true;
    QTest::addRow("just small part inside with bg, qimage") << "smallPartBgQImage" << "liquify_mask_rendering/liquify_mask_rendering_base_with_background.png"
                                                    << "liquify_mask_rendering/liquify_mask_rendering_small_change_mask_config.zip" << "liquify_mask_rendering/liquify_mask_rendering_with_background_small_change_qimage_resultImage.png" << true;

    /*
    // Since the liquify transform worker only uses Qimage for instant preview, it only supports rgba 8bit images, so it won't work
    QTest::addRow("16bit") << "liquify_mask_rendering/liquify_mask_rendering_16bit_base_image.png"
                           << "liquify_mask_rendering/liquify_mask_rendering_16bit_mask_config.zip" << "liquify_mask_rendering/liquify_mask_rendering_16bit_resultImage.png" << true;

    QTest::addRow("16bit small change") << "liquify_mask_rendering/liquify_mask_rendering_16bit_base_image.png"
                           << "liquify_mask_rendering/liquify_mask_rendering_16bit_small_change_mask_config.zip" << "liquify_mask_rendering/liquify_mask_rendering_16bit_small_change_resultImage.png" << true;
    */


}


QImage convertPaintDeviceTo16BitQImage(KisPaintDeviceSP dev, QSize size) {
    if (dev->pixelSize() != 2*4) {
        qWarning() << "The Paint Device should be 16 bit too";
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(
            false,
            TestUtil::diagnosticQImage(dev->convertToQImage(dev->colorSpace()->profile())));
    }

    quint8 *data = 0;
    try {
        data = new quint8 [size.width() * size.height() * dev->pixelSize()];
    } catch (const std::bad_alloc&) {
        qWarning() << "KisPaintDevice::convertToQImage std::bad_alloc for " << size << " * " << dev->pixelSize();
        //delete[] data; // data is not allocated, so don't free it
        return QImage();
    }
    Q_CHECK_PTR(data);

    dev->readBytes(data, PkRect(PkPoint(), PkSize(size.width(), size.height())));
    QImage img = QImage(size, QImage::Format_RGBA64);
    memcpy(img.bits(), const_cast<quint8 *>(data), size.width() * size.height() * dev->pixelSize());
    delete[] data;

    return img.rgbSwapped();
}

void convert16BitQImageToPaintDevice(KisPaintDeviceSP dev, QImage image) {

    if (image.format() != QImage::Format_RGBA64) {
        qWarning() << "The Paint Device should be 16 bit too";
        KIS_SAFE_ASSERT_RECOVER_RETURN(false);
    }

    image = image.rgbSwapped();

    dev->writeBytes(image.constBits(), 0, 0, image.width(), image.height());
}

KisPaintDeviceSP getPaintDeviceFromQImage(QImage image, const KoColorSpace* cs) {

    KisPaintDeviceSP dev = new KisPaintDevice(cs);

    if (image.format() == QImage::Format_RGBA64) {
        convert16BitQImageToPaintDevice(dev, image);
    } else {
        dev->convertFromQImage(TestUtil::pkImageFromQImage(image), cs->profile());
    }
    return dev;
}

void KisLiquifyTransformWorkerTest::testMaskRendering()
{
    QFETCH(QString, shortTestName);
    QFETCH(QString, baseImageFilename);
    QFETCH(QString, configFilename);
    QFETCH(QString, resultImageFilename);
    QFETCH(bool, isQImageTest);


    QString tname = QTest::currentDataTag();
    qDebug() << ppVar(tname);



    QString testName = shortTestName;

    QImage image(TestUtil::fetchDataFileLazy(baseImageFilename));
    QImage expected(TestUtil::fetchDataFileLazy(resultImageFilename));

    QString maskFilenameZip = configFilename;

    QScopedPointer<KisLiquifyTransformWorker> liquifyWorker;

    liquifyWorker.reset(getWorkerFromXml(maskFilenameZip));

    bool onQImage = isQImageTest;

    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    if (image.format() == QImage::Format_RGBA64) {
        cs = KoColorSpaceRegistry::instance()->rgb16();
    }


    KisPaintDeviceSP dev = getPaintDeviceFromQImage(image, cs);
    KisPaintDeviceSP dstDev = new KisPaintDevice(dev->colorSpace());

    QImage result;

    if (onQImage) {

        PkPointF p;
        result = TestUtil::diagnosticQImage(
            liquifyWorker->runOnImage(TestUtil::pkImageFromQImage(image),
                                      p, PkTransform(), &p));

    } else {
        liquifyWorker->run(dev, dstDev);
        //ENTER_FUNCTION() << ppVar(dstDev->exactBounds()) << ppVar(image.size());

        if (expected.format() == QImage::Format_RGBA64) {
            result = convertPaintDeviceTo16BitQImage(dstDev, image.size());
        }
        else {
            result = TestUtil::diagnosticQImage(
                dstDev->convertToQImage(cs->profile(), 0, 0,
                                        image.width(), image.height()));
        }
    }

    //ENTER_FUNCTION() << ppVar(result);


    image.save(testName + "_handsaved_original.png");
    result.save(testName + "_handsaved_result.png");
    expected.save(testName + "_handsaved_expected.png");


    QPoint diffPoint;
    if (expected.format() == QImage::Format_RGBA64) {
        KisPaintDeviceSP expDev = getPaintDeviceFromQImage(expected, cs);

        dstDev->crop(0, 0, expected.width(), expected.height()); // needed in case the transform would move the content out of the canvas
        qDebug() << "src = expDev, dst = dstDev";
        QVERIFY(TestUtil::comparePaintDevicesClever<uint16_t>(expDev, dstDev));

    } else {

        QVERIFY(TestUtil::compareQImages(diffPoint, expected, result, 0, 0, 0, true));
    }
}

SIMPLE_TEST_MAIN(KisLiquifyTransformWorkerTest)
