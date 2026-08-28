/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2020 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QDir>
#include <QImage>
#include <QPoint>
#include <QString>
#include <QVector>

#include <PkMap.h>
#include <PkNodeId.h>
#include <KisGlobalResourcesInterface.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoProgressUpdater.h>
#include <KoUpdater.h>
#include <generator/kis_generator_registry.h>
#include <kis_filter_configuration.h>
#include <kis_processing_information.h>
#include <kis_selection.h>
#include <simpletest.h>
#include <testimage.h>
#include <qimage_test_util.h>

#include <dlfcn.h>
#include <cstring>

#include "KisScreentoneGeneratorTest.h"

void KisScreentoneGeneratorTest::initTestCase()
{
    void *module = dlopen(SCREENTONE_MODULE_PATH, RTLD_NOW | RTLD_GLOBAL);
    const char *error = module ? nullptr : dlerror();
    QVERIFY2(module, error ? error : "unknown module load error");
    QVERIFY(KisGeneratorRegistry::instance()->get("screentone"));
}

namespace {

class TestProgressBar final : public KoProgressProxy
{
public:
    int maximum() const override { return m_maximum; }
    void setValue(int value) override { m_value = value; }
    void setRange(int, int maximum) override { m_maximum = maximum; }
    void setFormat(const PkString &) override {}

private:
    int m_maximum = 0;
    int m_value = 0;
};

QImage toTestImage(const PkImage &image)
{
    QImage result(image.width(), image.height(), static_cast<QImage::Format>(image.format()));
    for (int y = 0; y < image.height(); ++y) {
        std::memcpy(result.scanLine(y), image.constScanLine(y),
                    static_cast<std::size_t>(image.bytesPerLine()));
    }
    if (image.colorCount() > 0) {
        QVector<QRgb> table;
        table.reserve(image.colorCount());
        for (int i = 0; i < image.colorCount(); ++i) {
            table.append(image.color(i));
        }
        result.setColorTable(table);
    }
    return result;
}

QString toQString(const PkString &text)
{
    return QString::fromUtf8(text.PkToUtf8().c_str());
}

}

void testGenerate(const PkString &testName, const PkHash<PkString, PkVariant> &properties)
{
    KisGeneratorSP generator = KisGeneratorRegistry::instance()->get("screentone");
    QVERIFY(generator);

    KisFilterConfigurationSP config = generator->defaultConfiguration(KisGlobalResourcesInterface::instance());
    QVERIFY(config);

    KisPaintDeviceSP paintDevice = new KisPaintDevice(KoColorSpaceRegistry::instance()->rgb8());
    KisProcessingInformation processingInformation(paintDevice, PkPoint(0, 0), KisSelectionSP());
    TestProgressBar *testProgressBar = new TestProgressBar();
    KoProgressUpdater *progressUpdater = new KoProgressUpdater(testProgressBar);
    KoUpdaterPtr updater = progressUpdater->startSubtask();

    PkSize testImageSize(256, 256);

    for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
        config->setProperty(it.key(), it.value());
    }

    generator->generate(processingInformation, testImageSize, config, updater);

    const QString qtTestName = toQString(testName);
    QImage referenceImage(QString(FILES_DATA_DIR) + QDir::separator() + qtTestName + ".png");
    QImage deviceImage = toTestImage(paintDevice->convertToQImage(0, 0, 0, testImageSize.width(), testImageSize.height()));

    QPoint differingPoint;
    if (!TestUtil::compareQImages(differingPoint, referenceImage, deviceImage)) {
        deviceImage.save(qtTestName + "_generated.png");
        QFAIL(QString(qtTestName + ": failed to compare images, first different pixel: %1,%2 ")
                  .arg(differingPoint.x()).arg(differingPoint.y()).toLatin1());
    }

    delete progressUpdater;
    delete testProgressBar;
}

void KisScreentoneGeneratorTest::testGenerate01()
{
    PkHash<PkString, PkVariant> properties;

    properties.insert("equalization_mode", 0);
    properties.insert("brightness", 70);
    properties.insert("interpolation", 1);

    testGenerate("test01", properties);
}

void KisScreentoneGeneratorTest::testGenerate02()
{
    PkHash<PkString, PkVariant> properties;

    properties.insert("equalization_mode", 1);
    properties.insert("brightness", 70);
    properties.insert("interpolation", 1);

    testGenerate("test02", properties);
}

void KisScreentoneGeneratorTest::testGenerate03()
{
    PkHash<PkString, PkVariant> properties;

    properties.insert("equalization_mode", 2);
    properties.insert("brightness", 70);
    properties.insert("interpolation", 1);

    testGenerate("test03", properties);
}

void KisScreentoneGeneratorTest::testGenerate04()
{
    PkHash<PkString, PkVariant> properties;

    properties.insert("equalization_mode", 0);
    properties.insert("align_to_pixel_grid", false);
    properties.insert("brightness", 70);
    properties.insert("interpolation", 1);

    testGenerate("test04", properties);
}

void KisScreentoneGeneratorTest::testGenerate05()
{
    PkHash<PkString, PkVariant> properties;

    properties.insert("equalization_mode", 1);
    properties.insert("align_to_pixel_grid", false);
    properties.insert("brightness", 70);
    properties.insert("interpolation", 1);

    testGenerate("test05", properties);
}

void KisScreentoneGeneratorTest::testGenerate06()
{
    PkHash<PkString, PkVariant> properties;

    properties.insert("equalization_mode", 2);
    properties.insert("align_to_pixel_grid", false);
    properties.insert("brightness", 70);
    properties.insert("interpolation", 1);

    testGenerate("test06", properties);
}

void KisScreentoneGeneratorTest::testGenerate07()
{
    PkHash<PkString, PkVariant> properties;

    properties.insert("pattern", 1);
    properties.insert("shape", 1);
    properties.insert("equalization_mode", 0);

    properties.insert("size_mode", 1);
    properties.insert("keep_size_square", false);
    properties.insert("size_x", 100.0);
    properties.insert("rotation", 15.0);

    PkVariant v;
    v.setValue(KoColor(PkColor(255, 0, 0), KoColorSpaceRegistry::instance()->rgb8()));
    properties.insert("foreground_color", v);
    properties.insert("background_opacity", 0);
    properties.insert("brightness", 75);
    properties.insert("contrast", 90);

    properties.insert("interpolation", 1);

    testGenerate("test07", properties);
}

void KisScreentoneGeneratorTest::testGenerate08()
{
    PkHash<PkString, PkVariant> properties;

    properties.insert("pattern", 1);
    properties.insert("shape", 1);
    properties.insert("equalization_mode", 1);

    properties.insert("size_mode", 1);
    properties.insert("keep_size_square", false);
    properties.insert("size_x", 100.0);
    properties.insert("rotation", 15.0);

    PkVariant v;
    v.setValue(KoColor(PkColor(255, 0, 0), KoColorSpaceRegistry::instance()->rgb8()));
    properties.insert("foreground_color", v);
    properties.insert("background_opacity", 0);
    properties.insert("brightness", 75);
    properties.insert("contrast", 90);

    properties.insert("interpolation", 1);

    testGenerate("test08", properties);
}

void KisScreentoneGeneratorTest::testGenerate09()
{
    PkHash<PkString, PkVariant> properties;

    properties.insert("pattern", 1);
    properties.insert("shape", 1);
    properties.insert("equalization_mode", 2);

    properties.insert("size_mode", 1);
    properties.insert("keep_size_square", false);
    properties.insert("size_x", 100.0);
    properties.insert("rotation", 15.0);

    PkVariant v;
    v.setValue(KoColor(PkColor(255, 0, 0), KoColorSpaceRegistry::instance()->rgb8()));
    properties.insert("foreground_color", v);
    properties.insert("background_opacity", 0);
    properties.insert("brightness", 75);
    properties.insert("contrast", 90);

    properties.insert("interpolation", 1);

    testGenerate("test09", properties);
}

KISTEST_MAIN(KisScreentoneGeneratorTest)
