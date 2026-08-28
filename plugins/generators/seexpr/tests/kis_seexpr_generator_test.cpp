/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2020 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QDir>
#include <QImage>
#include <QPoint>
#include <QString>
#include <QVector>

#include <PkMap.h>
#include <PkNodeId.h>
#include <KisGlobalResourcesInterface.h>
#include <KisImageResolutionProxy.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoProgressUpdater.h>
#include <KoUpdater.h>
#include <generator/kis_generator_registry.h>
#include <kis_default_bounds.h>
#include <kis_fill_painter.h>
#include <kis_filter_configuration.h>
#include <kis_processing_information.h>
#include <kis_selection.h>
#include <resources/KisSeExprScript.h>
#include <simpletest.h>
#include <testimage.h>
#include <qimage_test_util.h>

#include <dlfcn.h>
#include <cstring>


#include "kis_seexpr_generator_test.h"

#define BASE_SCRIPT                                                                                                                                                                                                                            \
    "$val=voronoi(5*[$u,$v,.5],4,.6,.2); \n \
$color=ccurve($val,\n\
    0.000, [0.141, 0.059, 0.051], 4,\n\
    0.185, [0.302, 0.176, 0.122], 4,\n\
    0.301, [0.651, 0.447, 0.165], 4,\n\
    0.462, [0.976, 0.976, 0.976], 4);\n\
$color\n\
"

void KisSeExprGeneratorTest::initTestCase()
{
    void *module = dlopen(SEEXPR_MODULE_PATH, RTLD_NOW | RTLD_GLOBAL);
    const char *error = module ? nullptr : dlerror();
    QVERIFY2(module, error ? error : "unknown module load error");
    QVERIFY(KisGeneratorRegistry::instance()->get("seexpr"));
}

namespace {

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

}

void KisSeExprGeneratorTest::testGenerationFromScript()
{
    KisGeneratorSP generator = KisGeneratorRegistry::instance()->get("seexpr");
    QVERIFY(generator);

    KisFilterConfigurationSP config = generator->defaultConfiguration(KisGlobalResourcesInterface::instance());
    QVERIFY(config);

    config->setProperty("script", BASE_SCRIPT);

    PkPoint point(0, 0);
    PkSize testSize(256, 256);

    KisDefaultBoundsBaseSP bounds(new KisWrapAroundBoundsWrapper(new KisDefaultBounds(), PkRect(point.x(), point.y(), testSize.width(), testSize.height())));
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP dev = new KisPaintDevice(cs);
    dev->setDefaultBounds(bounds);
    dev->setSupportsWraparoundMode(true);

    KisFillPainter fillPainter(dev);
    fillPainter.fillRect(point.x(), point.y(), 256, 256, config);

    QImage qimage(QString(FILES_DATA_DIR) + QDir::separator() + "noisecolor2.png");

    QPoint errpoint;
    QImage deviceImage = toTestImage(dev->convertToQImage(nullptr, point.x(), point.y(), testSize.width(), testSize.height()));
    if (!TestUtil::compareQImages(errpoint, qimage, deviceImage, 1)) {
        deviceImage.save("filtertest.png");
        QFAIL(QString("Failed to create image, first different pixel: %1,%2 ").arg(errpoint.x()).arg(errpoint.y()).toLatin1());
    }
}

void KisSeExprGeneratorTest::testGenerationFromKoResource()
{
    KisGeneratorSP generator = KisGeneratorRegistry::instance()->get("seexpr");
    QVERIFY(generator);

    KisFilterConfigurationSP config = generator->defaultConfiguration(KisGlobalResourcesInterface::instance());
    QVERIFY(config);

    auto resource = new KisSeExprScript(TestUtil::fetchDataFileLazy("Disney_noisecolor2.kse"));
    resource->load(KisGlobalResourcesInterface::instance());
    Q_ASSERT(resource->valid());

    const QByteArray scriptUtf8 = resource->script().toUtf8();
    config->setProperty("script", PkString(scriptUtf8.constData()));

    PkPoint point(0, 0);
    PkSize testSize(256, 256);

    KisDefaultBoundsBaseSP bounds(new KisWrapAroundBoundsWrapper(new KisDefaultBounds(), PkRect(point.x(), point.y(), testSize.width(), testSize.height())));
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP dev = new KisPaintDevice(cs);
    dev->setDefaultBounds(bounds);
    dev->setSupportsWraparoundMode(true);

    KisFillPainter fillPainter(dev);
    fillPainter.fillRect(point.x(), point.y(), 256, 256, config);

    QImage qimage(QString(FILES_DATA_DIR) + QDir::separator() + "noisecolor2.png");

    QPoint errpoint;
    QImage deviceImage = toTestImage(dev->convertToQImage(nullptr, point.x(), point.y(), testSize.width(), testSize.height()));
    if (!TestUtil::compareQImages(errpoint, qimage, deviceImage, 1)) {
        deviceImage.save("filtertest.png");
        QFAIL(QString("Failed to create image, first different pixel: %1,%2 ").arg(errpoint.x()).arg(errpoint.y()).toLatin1());
    }
}

KISTEST_MAIN(KisSeExprGeneratorTest)
