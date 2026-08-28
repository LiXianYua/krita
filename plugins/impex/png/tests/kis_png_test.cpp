/*
 * SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_png_test.h"
#include <KisDocumentRegistry.h>

#include <algorithm>

#include <simpletest.h>
#include <QCoreApplication>

#include "filestest.h"

#include <testui.h>

#include <KisPngCodec.h>
#include <PkConfigGroup.h>
#include <PkMemoryStream.h>
#include <PkSharedConfig.h>

#include "../kis_dlg_png_import.h"

#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing the importing of files in krita"
#endif


const PkString PngMimetype = "image/png";

namespace
{

class FixedPngImportProfilePolicy final : public KisPngImportProfilePolicy
{
public:
    explicit FixedPngImportProfilePolicy(const PkString &profileName)
        : m_profileName(profileName)
    {
    }

    PkString chooseColorProfile(const KisPngImportProfileRequest &) override
    {
        return m_profileName;
    }

private:
    const PkString m_profileName;
};

}

void KisPngTest::testFiles()
{
    TestUtil::testFiles(PkString(FILES_DATA_DIR) + "/sources", PkStringList(), PkString(), 1);
}

void KisPngTest::testWriteonly()
{
    TestUtil::testImportFromWriteonly(PngMimetype);
}

void roudTripHdrImage(const KoColorSpace *savingColorSpace)
{
    const KoColorSpace * scRGBF32 =
        KoColorSpaceRegistry::instance()->colorSpace(
            RGBAColorModelID.id(),
            Float32BitsColorDepthID.id(),
            KoColorSpaceRegistry::instance()->p709G10Profile());

    KoColor fillColor(scRGBF32);
    float *pixelPtr = reinterpret_cast<float*>(fillColor.data());

    pixelPtr[0] = 2.7;
    pixelPtr[1] = 1.6;
    pixelPtr[2] = 0.8;
    pixelPtr[3] = 0.9;

    {
        PkScopedPointer<KisDocument> doc(KisDocumentRegistry::instance()->createDocument());

        KisImageSP image = new KisImage(0, 3, 3, scRGBF32, "png test");
        KisPaintLayerSP paintLayer0 = new KisPaintLayer(image, "paint0", OPACITY_OPAQUE_U8);
        paintLayer0->paintDevice()->fill(image->bounds(), fillColor);
        image->addNode(paintLayer0, image->root());

        // convert image color space before saving
        image->convertImageColorSpace(savingColorSpace, KoColorConversionTransformation::internalRenderingIntent(), KoColorConversionTransformation::internalConversionFlags());
        image->waitForDone();

        KisImportExportManager manager(doc.data());
        doc->setFileBatchMode(true);
        doc->setCurrentImage(image);

        KisPropertiesConfigurationSP exportConfiguration = new KisPropertiesConfiguration();
        exportConfiguration->setProperty("saveAsHDR", true);
        exportConfiguration->setProperty("saveSRGBProfile", false);
        exportConfiguration->setProperty("forceSRGB", false);
        doc->exportDocumentSync("test.png", "image/png", exportConfiguration);
    }

    {
        PkScopedPointer<KisDocument> doc(KisDocumentRegistry::instance()->createDocument());
        KisImportExportManager manager(doc.data());
        doc->setFileBatchMode(true);

        KisImportExportErrorCode loadingStatus =
            manager.importDocument("test.png", PkString());

        QVERIFY(loadingStatus.isOk());

        KisImageSP image = doc->image();
        image->initialRefreshGraph();

        KoColor resultColor;

//        qDebug() << ppVar(image->colorSpace()) << image->colorSpace()->profile()->name();
//        image->projection()->pixel(1, 1, &resultColor);
//        qDebug() << ppVar(resultColor);

        image->convertImageColorSpace(scRGBF32, KoColorConversionTransformation::internalRenderingIntent(), KoColorConversionTransformation::internalConversionFlags());
        image->waitForDone();

        image->projection()->pixel(1, 1, &resultColor);
//        qDebug() << ppVar(resultColor);

        const float tolerance = savingColorSpace->colorDepthId() == Integer8BitsColorDepthID ? 0.02 : 0.01;
        bool resultIsValid = true;
        float *resultPtr = reinterpret_cast<float*>(resultColor.data());
        for (int i = 0; i < 4; i++) {
            resultIsValid &= qAbs(resultPtr[i] - pixelPtr[i]) < tolerance;
        }

        if (!resultIsValid) {
            qDebug() << ppVar(fillColor) << ppVar(resultColor);
        }
        QVERIFY(resultIsValid);
    }
}

void KisPngTest::testSaveHDR()
{
    PkVector<KoID> colorDepthIds;
#ifdef HAVE_OPENEXR
    colorDepthIds << Float16BitsColorDepthID;
#endif
    colorDepthIds << Float32BitsColorDepthID;

    PkVector<const KoColorProfile*> profiles;
    const KoColorProfile *profile = KoColorSpaceRegistry::instance()->p709G10Profile();
    if (!profile) {
        qWarning() << "Could not get a p709G10 Profile";
    }
    else {
        profiles << profile;
    }
    profile = KoColorSpaceRegistry::instance()->p2020G10Profile();
    if (!profile) {
        qWarning() << "Could not get a p2020G10 Profile";
    }
    else {
        profiles << profile;
    }
    profile = KoColorSpaceRegistry::instance()->p2020PQProfile();;
    if (!profile) {
        qWarning() << "Could not get a p2020PQ Profile";
    }
    else {
        profiles << profile;
    }

    Q_FOREACH(const KoID &depth, colorDepthIds) {
        Q_FOREACH(const KoColorProfile *profile, profiles) {
            if (profile) {
                roudTripHdrImage(
                    KoColorSpaceRegistry::instance()->colorSpace(
                                RGBAColorModelID.id(),
                                depth.id(),
                                profile));
            }
        }
    }

    roudTripHdrImage(
        KoColorSpaceRegistry::instance()->colorSpace(
                    RGBAColorModelID.id(),
                    Integer16BitsColorDepthID.id(),
                    KoColorSpaceRegistry::instance()->p2020PQProfile()));

    roudTripHdrImage(
        KoColorSpaceRegistry::instance()->colorSpace(
                    RGBAColorModelID.id(),
                    Integer8BitsColorDepthID.id(),
                    KoColorSpaceRegistry::instance()->p2020PQProfile()));
}

void KisPngTest::testImportProfileModel()
{
    KoColorSpaceRegistry *registry = KoColorSpaceRegistry::instance();
    const PkString colorSpaceId = registry->colorSpaceId(RGBAColorModelID, Integer16BitsColorDepthID);
    const PkString defaultProfile = registry->defaultProfileForColorSpace(colorSpaceId);
    QVERIFY(!defaultProfile.isEmpty());

    PkConfigGroup config = PkSharedConfig::openConfig()->group(PkString());
    config.writeEntry(PkString("pngImportProfile"), defaultProfile);

    KisDlgPngImport model(PkString("fixture.png"),
                          RGBAColorModelID.id(),
                          Integer16BitsColorDepthID.id());

    QVERIFY(model.sourcePath() == PkString("fixture.png"));
    const PkStringList &profiles = model.profiles();
    QVERIFY(profiles.size() > 1);
    QVERIFY(std::is_sorted(profiles.begin(), profiles.end()));
    QVERIFY(std::find(profiles.begin(), profiles.end(), defaultProfile) != profiles.end());
    QVERIFY(model.profile() == defaultProfile);

    const auto alternate = std::find_if(profiles.begin(), profiles.end(),
                                        [&defaultProfile](const PkString &profile) {
                                            return profile != defaultProfile;
                                        });
    QVERIFY(alternate != profiles.end());
    QVERIFY(model.selectProfile(*alternate));
    QVERIFY(model.profile() == *alternate);
    QVERIFY(!model.selectProfile(PkString("not-a-registered-profile")));
    QVERIFY(model.profile() == *alternate);

    config.deleteEntry(PkString("pngImportProfile"));
}

void KisPngTest::testHeadlessCodecUsesImportProfilePolicy()
{
    const KoColorProfile *sourceProfile = KoColorSpaceRegistry::instance()->p709SRGBProfile();
    const KoColorProfile *selectedProfile = KoColorSpaceRegistry::instance()->p2020G10Profile();
    QVERIFY(sourceProfile);
    QVERIFY(selectedProfile);

    const KoColorSpace *sourceColorSpace =
        KoColorSpaceRegistry::instance()->colorSpace(
            RGBAColorModelID.id(),
            Integer16BitsColorDepthID.id(),
            sourceProfile);
    QVERIFY(sourceColorSpace);

    const PkRect imageRect(0, 0, 2, 2);
    KisPaintDeviceSP device = new KisPaintDevice(sourceColorSpace);
    KoColor fillColor(Qt::red, sourceColorSpace);
    device->fill(imageRect, fillColor);

    PkMemoryStream encodedPng;
    QVERIFY(encodedPng.open(PkStream::ReadWrite));

    KisPNGOptions options;
    options.saveSRGBProfile = false;
    options.tryToSaveAsIndexed = false;
    vKisAnnotationSP annotations;

    KisPngCodec writer;
    QVERIFY(writer.buildFile(&encodedPng,
                             imageRect,
                             1.0,
                             1.0,
                             device,
                             annotations.begin(),
                             annotations.end(),
                             options,
                             nullptr).isOk());

    QVERIFY(encodedPng.seek(0));
    FixedPngImportProfilePolicy policy(selectedProfile->name());
    KisPngCodecContext context;
    context.importProfilePolicy = &policy;
    KisPngCodec reader(context);

    QVERIFY(reader.buildImage(&encodedPng).isOk());
    QVERIFY(reader.image());
    QCOMPARE(reader.image()->colorSpace()->profile()->name(), selectedProfile->name());
}

KISTEST_MAIN(KisPngTest)
