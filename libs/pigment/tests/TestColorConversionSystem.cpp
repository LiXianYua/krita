/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <simpletest.h>
#include "TestColorConversionSystem.h"


#include <DebugPigment.h>
#include <KoColorConversionSystem.h>
#include <KoColorModelStandardIds.h>
#include <KoColorProfile.h>
#include <KoColorSpaceRegistry.h>
#include <kistest.h>

#include <PkContainerAlgo.h>

#include <random>

TestColorConversionSystem::TestColorConversionSystem()
{
    Q_FOREACH (const KoID& modelId, KoColorSpaceRegistry::instance()->colorModelsList(KoColorSpaceRegistry::AllColorSpaces)) {
        Q_FOREACH (const KoID& depthId, KoColorSpaceRegistry::instance()->colorDepthList(modelId, KoColorSpaceRegistry::AllColorSpaces)) {
            PkList< const KoColorProfile * > profiles =
                KoColorSpaceRegistry::instance()->profilesFor(
                    KoColorSpaceRegistry::instance()->colorSpaceId(modelId, depthId));
            Q_FOREACH (const KoColorProfile * profile, profiles) {
                listModels.append(ModelDepthProfile(modelId.id(), depthId.id(), profile->name()));
            }
        }
    }
    //listModels.append(ModelDepthProfile(AlphaColorModelID.id(), Integer8BitsColorDepthID.id(), ""));
}

void TestColorConversionSystem::testConnections()
{
    Q_FOREACH (const ModelDepthProfile& srcCS, listModels) {
        Q_FOREACH (const ModelDepthProfile& dstCS, listModels) {
            PK_VERIFY2(KoColorSpaceRegistry::instance()->colorConversionSystem()->existsPath(srcCS.model, srcCS.depth, srcCS.profile, dstCS.model, dstCS.depth, dstCS.profile) , PkString("No path between %1 / %2 and %3 / %4").arg(srcCS.model).arg(srcCS.depth).arg(dstCS.model).arg(dstCS.depth).PkToUtf8());
        }
    }
}

void TestColorConversionSystem::testGoodConnections()
{
    int countFail = 0;
    Q_FOREACH (const ModelDepthProfile& srcCS, listModels) {
        Q_FOREACH (const ModelDepthProfile& dstCS, listModels) {
            if (!KoColorSpaceRegistry::instance()->colorConversionSystem()->existsGoodPath(srcCS.model, srcCS.depth, srcCS.profile , dstCS.model, dstCS.depth, dstCS.profile)) {
                ++countFail;
                dbgPigment << "No good path between \"" << srcCS.model << " " << srcCS.depth << " " << srcCS.profile << "\" \"" << dstCS.model << " " << dstCS.depth << " " << dstCS.profile << "\"";
            }
        }
    }
    int failed = 0;
    if (!KoColorSpaceRegistry::instance()->colorSpace( RGBAColorModelID.id(), Float32BitsColorDepthID.id(), 0) && KoColorSpaceRegistry::instance()->colorSpace( "KS6", Float32BitsColorDepthID.id(), 0) ) {
        failed = 42;
    }
    PK_VERIFY2(countFail == failed, PkString("%1 tests have fails (it should have been %2)").arg(countFail).arg(failed).PkToUtf8());
}

#include <KoColor.h>

#include <KoColorConversionSystem_p.h>
#include <kis_debug.h>



std::vector<KoColorConversionSystem::NodeKey> TestColorConversionSystem::calcPath(const std::vector<KoColorConversionSystem::NodeKey> &expectedPath) {

    const KoColorConversionSystem *system = KoColorSpaceRegistry::instance()->colorConversionSystem();

    KoColorConversionSystem::Path path =
        system->findBestPath(expectedPath.front(), expectedPath.back());

    std::vector<KoColorConversionSystem::NodeKey> realPath;

    Q_FOREACH (const KoColorConversionSystem::Vertex *vertex, path.vertexes) {
        if (!vertex->srcNode->isEngine) {
            realPath.push_back(vertex->srcNode->key());
        }
    }
    realPath.push_back(path.vertexes.last()->dstNode->key());

    return realPath;
};

void TestColorConversionSystem::testAlphaConnectionPaths()
{
    const KoColorSpace *alpha8 = KoColorSpaceRegistry::instance()->alpha8();

    using NodeKey = KoColorConversionSystem::NodeKey;

    std::vector<NodeKey> expectedPath;

       // to Alpha8 conversions. Everything should go via GrayA color space,
    // we expect alpha colorspace be just a flattened of graya color space
    // with srgb tone curve.

    expectedPath =
        {{GrayAColorModelID.id(), Integer8BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"},
         {alpha8->colorModelId().id(), alpha8->colorDepthId().id(), alpha8->profile()->name()}};
    PK_COMPARE(calcPath(expectedPath), expectedPath);

    expectedPath =
        {{GrayAColorModelID.id(), Integer16BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"},
         {alpha8->colorModelId().id(), alpha8->colorDepthId().id(), alpha8->profile()->name()}};
    PK_COMPARE(calcPath(expectedPath), expectedPath);

#ifdef HAVE_OPENEXR
    expectedPath =
        {{GrayAColorModelID.id(), Float16BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"},
         {alpha8->colorModelId().id(), alpha8->colorDepthId().id(), alpha8->profile()->name()}};
    PK_COMPARE(calcPath(expectedPath), expectedPath);
#endif

    expectedPath =
        {{GrayAColorModelID.id(), Float32BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"},
         {alpha8->colorModelId().id(), alpha8->colorDepthId().id(), alpha8->profile()->name()}};
    PK_COMPARE(calcPath(expectedPath), expectedPath);

    expectedPath =
        {{RGBAColorModelID.id(), Integer8BitsColorDepthID.id(), KoColorSpaceRegistry::instance()->p709SRGBProfile()->name()},
         {GrayAColorModelID.id(), Integer8BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"},
         {alpha8->colorModelId().id(), alpha8->colorDepthId().id(), alpha8->profile()->name()}};
    PK_COMPARE(calcPath(expectedPath), expectedPath);

    expectedPath =
        {{RGBAColorModelID.id(), Integer16BitsColorDepthID.id(), KoColorSpaceRegistry::instance()->p709SRGBProfile()->name()},
         {GrayAColorModelID.id(), Integer8BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"},
         {alpha8->colorModelId().id(), alpha8->colorDepthId().id(), alpha8->profile()->name()}};
    PK_COMPARE(calcPath(expectedPath), expectedPath);

    expectedPath =
        {{RGBAColorModelID.id(), Integer8BitsColorDepthID.id(), KoColorSpaceRegistry::instance()->p709SRGBProfile()->name()},
         {GrayAColorModelID.id(), Integer8BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"},
         {AlphaColorModelID.id(), Integer16BitsColorDepthID.id(), alpha8->profile()->name()}};
    PK_COMPARE(calcPath(expectedPath), expectedPath);

    expectedPath =
        {{RGBAColorModelID.id(), Integer16BitsColorDepthID.id(), KoColorSpaceRegistry::instance()->p709SRGBProfile()->name()},
         {GrayAColorModelID.id(), Integer16BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"},
         {AlphaColorModelID.id(), Integer16BitsColorDepthID.id(), alpha8->profile()->name()}};
    PK_COMPARE(calcPath(expectedPath), expectedPath);

    // from Alpha8 conversions. Everything should go via GrayA color space

    expectedPath =
        {{alpha8->colorModelId().id(), alpha8->colorDepthId().id(), alpha8->profile()->name()},
         {GrayAColorModelID.id(), Integer8BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"}};
    PK_COMPARE(calcPath(expectedPath), expectedPath);

    expectedPath =
        {{alpha8->colorModelId().id(), alpha8->colorDepthId().id(), alpha8->profile()->name()},
         {GrayAColorModelID.id(), Integer16BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"}};
    PK_COMPARE(calcPath(expectedPath), expectedPath);

#ifdef HAVE_OPENEXR
    expectedPath =
        {{alpha8->colorModelId().id(), alpha8->colorDepthId().id(), alpha8->profile()->name()},
         {GrayAColorModelID.id(), Float16BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"}};
    PK_COMPARE(calcPath(expectedPath), expectedPath);
#endif

    expectedPath =
        {{alpha8->colorModelId().id(), alpha8->colorDepthId().id(), alpha8->profile()->name()},
         {GrayAColorModelID.id(), Float32BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"}};
    PK_COMPARE(calcPath(expectedPath), expectedPath);

    expectedPath =
        {{alpha8->colorModelId().id(), alpha8->colorDepthId().id(), alpha8->profile()->name()},
         {GrayAColorModelID.id(), Integer8BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"},
         {RGBAColorModelID.id(), Integer8BitsColorDepthID.id(), KoColorSpaceRegistry::instance()->p709SRGBProfile()->name()}};
    PK_COMPARE(calcPath(expectedPath), expectedPath);


    expectedPath =
        {{alpha8->colorModelId().id(), alpha8->colorDepthId().id(), alpha8->profile()->name()},
         {GrayAColorModelID.id(), Integer8BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"},
         {RGBAColorModelID.id(), Integer16BitsColorDepthID.id(), KoColorSpaceRegistry::instance()->p709SRGBProfile()->name()}};
    PK_COMPARE(calcPath(expectedPath), expectedPath);

    expectedPath =
        {{AlphaColorModelID.id(), Integer16BitsColorDepthID.id(), alpha8->profile()->name()},
         {GrayAColorModelID.id(), Integer8BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"},
         {RGBAColorModelID.id(), Integer8BitsColorDepthID.id(), KoColorSpaceRegistry::instance()->p709SRGBProfile()->name()}};
    PK_COMPARE(calcPath(expectedPath), expectedPath);

    expectedPath =
        {{AlphaColorModelID.id(), Integer16BitsColorDepthID.id(), alpha8->profile()->name()},
         {GrayAColorModelID.id(), Integer16BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"},
         {RGBAColorModelID.id(), Integer16BitsColorDepthID.id(), KoColorSpaceRegistry::instance()->p709SRGBProfile()->name()}};
    PK_COMPARE(calcPath(expectedPath), expectedPath);
}

void TestColorConversionSystem::testAlphaConversions()
{
    const KoColorSpace *alpha8 = KoColorSpaceRegistry::instance()->alpha8();
    const KoColorSpace *rgb8 = KoColorSpaceRegistry::instance()->rgb8();
    const KoColorSpace *rgb16 = KoColorSpaceRegistry::instance()->rgb16();

    {
        KoColor c(PkColor(255,255,255,255), alpha8);
        PK_COMPARE(c.opacityU8(), quint8(255));
        c.convertTo(rgb8);
        PK_COMPARE(c.toQColor(), PkColor(255,255,255));
        c.convertTo(alpha8);
        PK_COMPARE(c.opacityU8(), quint8(255));
    }

    {
        KoColor c(PkColor(255,255,255,0), alpha8);
        c.convertTo(rgb8);
        PK_COMPARE(c.toQColor(), PkColor(0,0,0,255));
        c.convertTo(alpha8);
        PK_COMPARE(c.opacityU8(), quint8(0));
    }

    {
        KoColor c(PkColor(255,255,255,128), alpha8);
        c.convertTo(rgb8);
        PK_COMPARE(c.toQColor(), PkColor(128,128,128,255));
        c.convertTo(alpha8);
        PK_COMPARE(c.opacityU8(), quint8(128));
    }

    {
        KoColor c(PkColor(255,255,255,255), alpha8);
        PK_COMPARE(c.opacityU8(), quint8(255));
        c.convertTo(rgb16);
        PK_COMPARE(c.toQColor(), PkColor(255,255,255));
        c.convertTo(alpha8);
        PK_COMPARE(c.opacityU8(), quint8(255));
    }

    {
        KoColor c(PkColor(255,255,255,0), alpha8);
        c.convertTo(rgb16);
        PK_COMPARE(c.toQColor(), PkColor(0,0,0,255));
        c.convertTo(alpha8);
        PK_COMPARE(c.opacityU8(), quint8(0));
    }

    {
        KoColor c(PkColor(255,255,255,128), alpha8);
        c.convertTo(rgb16);
        PK_COMPARE(c.toQColor(), PkColor(128,128,128,255));
        c.convertTo(alpha8);
        PK_COMPARE(c.opacityU8(), quint8(128));
    }
}

void TestColorConversionSystem::testAlphaU16Conversions()
{
    KoColorSpaceRegistry::instance();
    const KoColorSpace *alpha16 = KoColorSpaceRegistry::instance()->alpha16();
    const KoColorSpace *rgb8 = KoColorSpaceRegistry::instance()->rgb8();
    const KoColorSpace *rgb16 = KoColorSpaceRegistry::instance()->rgb16();

    {
        KoColor c(PkColor(255,255,255,255), alpha16);
        PK_COMPARE(c.opacityU8(), quint8(255));
        c.convertTo(rgb8);
        PK_COMPARE(c.toQColor(), PkColor(255,255,255));
        c.convertTo(alpha16);
        PK_COMPARE(c.opacityU8(), quint8(255));
    }

    {
        KoColor c(PkColor(255,255,255,0), alpha16);
        c.convertTo(rgb8);
        PK_COMPARE(c.toQColor(), PkColor(0,0,0,255));
        c.convertTo(alpha16);
        PK_COMPARE(c.opacityU8(), quint8(0));
    }

    {
        KoColor c(PkColor(255,255,255,128), alpha16);
        c.convertTo(rgb8);
        PK_COMPARE(c.toQColor(), PkColor(128,128,128,255));
        c.convertTo(alpha16);
        PK_COMPARE(c.opacityU8(), quint8(128));
    }

    {
        KoColor c(PkColor(255,255,255,255), alpha16);
        PK_COMPARE(c.opacityU8(), quint8(255));
        c.convertTo(rgb16);
        PK_COMPARE(c.toQColor(), PkColor(255,255,255));
        c.convertTo(alpha16);
        PK_COMPARE(c.opacityU8(), quint8(255));
    }

    {
        KoColor c(PkColor(255,255,255,0), alpha16);
        c.convertTo(rgb16);
        PK_COMPARE(c.toQColor(), PkColor(0,0,0,255));
        c.convertTo(alpha16);
        PK_COMPARE(c.opacityU8(), quint8(0));
    }

    {
        KoColor c(PkColor(255,255,255,128), alpha16);
        c.convertTo(rgb16);
        PK_COMPARE(c.toQColor(), PkColor(128,128,128,255));
        c.convertTo(alpha16);
        PK_COMPARE(c.opacityU8(), quint8(128));
    }
}

void TestColorConversionSystem::testGrayAConnectionPaths()
{
    using NodeKey = KoColorConversionSystem::NodeKey;

    std::vector<NodeKey> expectedPath;

    expectedPath =
       {{GrayAColorModelID.id(), Integer8BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"},
        {GrayAColorModelID.id(), Integer16BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"}};
    PK_COMPARE(calcPath(expectedPath), expectedPath);


    expectedPath =
       {{GrayAColorModelID.id(), Integer8BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"},
        {RGBAColorModelID.id(), Integer8BitsColorDepthID.id(), KoColorSpaceRegistry::instance()->p709SRGBProfile()->name()}};
    PK_COMPARE(calcPath(expectedPath), expectedPath);

    expectedPath =
       {{GrayAColorModelID.id(), Integer16BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"},
        {GrayAColorModelID.id(), Integer8BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"}};
    PK_COMPARE(calcPath(expectedPath), expectedPath);


    expectedPath =
       {{RGBAColorModelID.id(), Integer8BitsColorDepthID.id(), KoColorSpaceRegistry::instance()->p709SRGBProfile()->name()},
        {GrayAColorModelID.id(), Integer8BitsColorDepthID.id(), "Gray-D50-elle-V2-srgbtrc.icc"}};
    PK_COMPARE(calcPath(expectedPath), expectedPath);


}

void TestColorConversionSystem::testGrayAConversions()
{
    KoColorSpaceRegistry::instance();
    const KoColorSpace *graya8 = KoColorSpaceRegistry::instance()->graya8();
    const KoColorSpace *graya16 = KoColorSpaceRegistry::instance()->graya16();
    const KoColorSpace *rgb8 = KoColorSpaceRegistry::instance()->rgb8();

    {
        KoColor c(Qt::transparent, graya8);
        PK_COMPARE(c.opacityU8(), quint8(0));
        c.convertTo(graya16);
        PK_COMPARE(c.opacityU8(), quint8(0));
        PK_COMPARE(c.toQColor(), PkColor(Qt::transparent));
        c.convertTo(graya8);
        PK_COMPARE(c.opacityU8(), quint8(0));

        c.convertTo(rgb8);
        PK_COMPARE(c.opacityU8(), quint8(0));
        PK_COMPARE(c.toQColor(), PkColor(Qt::transparent));
    }

    {
        KoColor c(PkColor(255,255,255), graya8);
        PK_COMPARE(c.opacityU8(), quint8(255));
        c.convertTo(graya16);
        PK_COMPARE(c.opacityU8(), quint8(255));
        PK_COMPARE(c.toQColor(), PkColor(Qt::white));
        c.convertTo(graya8);
        PK_COMPARE(c.opacityU8(), quint8(255));

        c.convertTo(rgb8);
        PK_COMPARE(c.opacityU8(), quint8(255));
        PK_COMPARE(c.toQColor(), PkColor(Qt::white));
    }

    {
        KoColor c(PkColor(180,180,180), graya8);
        PK_COMPARE(c.opacityU8(), quint8(255));
        c.convertTo(graya16);
        PK_COMPARE(c.opacityU8(), quint8(255));
        PK_COMPARE(c.toQColor(), PkColor(180,180,180));
        c.convertTo(graya8);
        PK_COMPARE(c.opacityU8(), quint8(255));

        c.convertTo(rgb8);
        PK_COMPARE(c.opacityU8(), quint8(255));
        PK_COMPARE(c.toQColor(), PkColor(180,180,180));
    }
}

void TestColorConversionSystem::benchmarkAlphaToRgbConversion()
{
    const KoColorSpace *alpha8 = KoColorSpaceRegistry::instance()->alpha8();
    const KoColorSpace *rgb8 = KoColorSpaceRegistry::instance()->rgb8();

    const int numPixels = 1024 * 4096;
    PkByteArray srcBuf;
    srcBuf.resize(numPixels * alpha8->pixelSize());
    PkByteArray dstBuf;
    dstBuf.resize(numPixels * rgb8->pixelSize());

    std::mt19937 rng(42);
    for (int i = 0; i < srcBuf.size(); i++) {
        srcBuf.data()[i] = static_cast<char>(rng() % 256);
    }

    {
        alpha8->convertPixelsTo((quint8*)srcBuf.data(),
                                (quint8*)dstBuf.data(),
                                rgb8,
                                numPixels,
                                KoColorConversionTransformation::IntentPerceptual,
                                KoColorConversionTransformation::Empty);
    }
}

void TestColorConversionSystem::benchmarkRgbToAlphaConversion()
{
    const KoColorSpace *alpha8 = KoColorSpaceRegistry::instance()->alpha8();
    const KoColorSpace *rgb8 = KoColorSpaceRegistry::instance()->rgb8();

    const int numPixels = 1024 * 4096;
    PkByteArray srcBuf;
    srcBuf.resize(numPixels * rgb8->pixelSize());
    PkByteArray dstBuf;
    dstBuf.resize(numPixels * alpha8->pixelSize());

    std::mt19937 rng(42);
    for (int i = 0; i < srcBuf.size(); i++) {
        srcBuf.data()[i] = static_cast<char>(rng() % 256);
    }

    {
        rgb8->convertPixelsTo((quint8*)srcBuf.data(),
                              (quint8*)dstBuf.data(),
                              alpha8,
                              numPixels,
                              KoColorConversionTransformation::IntentPerceptual,
                              KoColorConversionTransformation::Empty);
    }
}

void TestColorConversionSystem::testCmykBitnessConversion()
{
    const KoColorSpace *cmyk8 =
        KoColorSpaceRegistry::instance()->colorSpace(CMYKAColorModelID.id(),
                                                     Integer8BitsColorDepthID.id(),
                                                     "Chemical proof");

    const KoColorSpace *cmyk16 =
        KoColorSpaceRegistry::instance()->colorSpace(CMYKAColorModelID.id(),
                                                     Integer16BitsColorDepthID.id(),
                                                     "Chemical proof");

//    ENTER_FUNCTION() << ppVar(cmyk8);
//    ENTER_FUNCTION() << ppVar(cmyk8->profile()->name());
//    ENTER_FUNCTION() << ppVar(cmyk8->profile()->fileName());

//    ENTER_FUNCTION() << ppVar(cmyk16);
//    ENTER_FUNCTION() << ppVar(cmyk16->profile()->name());
//    ENTER_FUNCTION() << ppVar(cmyk16->profile()->fileName());


    KoColor color(PkColor(177, 180, 42, 255), cmyk8);
//    qDebug() << ppVar(color);
    color.convertTo(cmyk16);
//    qDebug() << ppVar(color);
    KoColor color2 = color.convertedTo(cmyk8);
//    qDebug() << ppVar(color2);

    /**
     * For some reason out CMYK color spaces don't support rount-tripping
     * to-from 16-bit representation. So the code that relies on that should
     * use KoOptimizedCmykPixelDataScalerU8ToU16Factory::create().
     */
    PK_VERIFY(color != color2);

}


KISTEST_MAIN(TestColorConversionSystem)
