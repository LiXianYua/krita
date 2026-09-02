/*
 *  SPDX-FileCopyrightText: 2003 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2004, 2010 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2011 Srikanth Tiyyagura <srikanth.tulasiram@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/
#include "LcmsEnginePlugin.h"
#include "LcmsProfileDiscovery.h"

#include <PkStringList.h>

#include <KoBasicHistogramProducers.h>
#include <KoColorSpace.h>
#include <KoColorSpaceEngine.h>
#include <KoColorSpaceRegistry.h>
#include <KoResourcePaths.h>
#include <kis_assert.h>
#include <kis_debug.h>

#include "IccColorSpaceEngine.h"
#include "colorprofiles/LcmsColorProfileContainer.h"

#include "colorspaces/cmyk_u8/CmykU8ColorSpace.h"
#include "colorspaces/cmyk_u16/CmykU16ColorSpace.h"
#include "colorspaces/cmyk_f32/CmykF32ColorSpace.h"

#include "colorspaces/gray_u8/GrayU8ColorSpace.h"
#include "colorspaces/gray_u16/GrayU16ColorSpace.h"
#include "colorspaces/gray_f32/GrayF32ColorSpace.h"

#include "colorspaces/lab_u8/LabU8ColorSpace.h"
#include "colorspaces/lab_u16/LabColorSpace.h"
#include "colorspaces/lab_f32/LabF32ColorSpace.h"

#include "colorspaces/xyz_u8/XyzU8ColorSpace.h"
#include "colorspaces/xyz_u16/XyzU16ColorSpace.h"
#include "colorspaces/xyz_f32/XyzF32ColorSpace.h"

#include "colorspaces/rgb_u8/RgbU8ColorSpace.h"
#include "colorspaces/rgb_u16/RgbU16ColorSpace.h"
#include "colorspaces/rgb_f32/RgbF32ColorSpace.h"

#include "colorspaces/ycbcr_u8/YCbCrU8ColorSpace.h"
#include "colorspaces/ycbcr_u16/YCbCrU16ColorSpace.h"
#include "colorspaces/ycbcr_f32/YCbCrF32ColorSpace.h"

#include "LcmsRGBP2020PQColorSpace.h"

#include <KoConfig.h>

#ifdef HAVE_OPENEXR
#   include <half.h>
#   ifdef HAVE_LCMS24
#       include "colorspaces/gray_f16/GrayF16ColorSpace.h"
#       include "colorspaces/xyz_f16/XyzF16ColorSpace.h"
#       include "colorspaces/rgb_f16/RgbF16ColorSpace.h"
#   endif
#endif

#if defined(HAVE_LCMS_FAST_FLOAT_PLUGIN)
#include <lcms2_fast_float.h>
#endif 

void lcms2LogErrorHandlerFunction(cmsContext /*ContextID*/, cmsUInt32Number ErrorCode, const char *Text)
{
    errorPigment << "Lcms2 error: " << ErrorCode << Text;
}

void registerLcmsEngine()
{
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    KoResourcePaths::addAssetType("icc_profiles", "data", "/color/icc");
    KoResourcePaths::addAssetType("icc_profiles", "data", "/profiles/");

    // Set the lmcs error reporting function
    cmsSetLogErrorHandler(&lcms2LogErrorHandlerFunction);

    KoColorSpaceRegistry *registry = KoColorSpaceRegistry::instance();

    // Initialise color engine
    KoColorSpaceEngineRegistry::instance()->add(new IccColorSpaceEngine);

    PkStringList profileFilenames;
    profileFilenames += KoResourcePaths::findAllAssets("icc_profiles", "*.icm",  KoResourcePaths::Recursive);
    profileFilenames += KoResourcePaths::findAllAssets("icc_profiles", "*.ICM",  KoResourcePaths::Recursive);
    profileFilenames += KoResourcePaths::findAllAssets("icc_profiles", "*.ICC",  KoResourcePaths::Recursive);
    profileFilenames += KoResourcePaths::findAllAssets("icc_profiles", "*.icc",  KoResourcePaths::Recursive);

    PkStringList iccProfileDirs;

#ifdef __APPLE__
    iccProfileDirs.append(LcmsProfileDiscovery::homePath() + "/Library/ColorSync/Profiles/");
    iccProfileDirs.append("/System/Library/ColorSync/Profiles/");
    iccProfileDirs.append("/Library/ColorSync/Profiles/");
#endif
#ifdef _WIN32
    PkString winPath = LcmsProfileDiscovery::environmentPath("windir");
    iccProfileDirs.append(winPath + "/System32/Spool/Drivers/Color/");

#endif
#ifdef __linux__
    iccProfileDirs.append(LcmsProfileDiscovery::homePath() + "./share/color/icc");
#endif

    for (const PkString &iccProfiledir : iccProfileDirs) {
        for (const PkString &entry : LcmsProfileDiscovery::profileEntries(iccProfiledir)) {
            profileFilenames << iccProfiledir + "/" + entry;
        }
    }

    // Load the profiles
    if (!profileFilenames.empty()) {
        for (PkStringList::Iterator it = profileFilenames.begin(); it != profileFilenames.end(); ++it) {

            KoColorProfile *profile = new IccColorProfile(*it);
            KIS_ASSERT(profile);

            profile->load();
            if (profile->valid()) {
                registry->addProfileToMap(profile);
            } else {
                dbgPigment << "Invalid profile : " << *it;
                delete profile;
            }
        }
    }

    // ------------------- LAB ---------------------------------

    KoColorProfile *labProfile = LcmsColorProfileContainer::createFromLcmsProfile(cmsCreateLab4Profile(0));
    registry->addProfile(labProfile);

    registry->add(new LabU8ColorSpaceFactory());
    registry->add(new LabU16ColorSpaceFactory());
    registry->add(new LabF32ColorSpaceFactory());

    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicU8HistogramProducer>
            (KoID("LABAU8HISTO", PkString("L*a*b*/8 Histogram")), LABAColorModelID.id(), Integer8BitsColorDepthID.id()));

    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicU16HistogramProducer>
            (KoID("LABAU16HISTO", PkString("L*a*b*/16 Histogram")), LABAColorModelID.id(), Integer16BitsColorDepthID.id()));

    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicF32HistogramProducer>
            (KoID("LABAF32HISTO", PkString("L*a*b*/32 Histogram")), LABAColorModelID.id(), Float32BitsColorDepthID.id()));

    // ------------------- RGB ---------------------------------

    KoColorProfile *rgbProfile = LcmsColorProfileContainer::createFromLcmsProfile(cmsCreate_sRGBProfile());
    registry->addProfile(rgbProfile);

    registry->add(new LcmsRGBP2020PQColorSpaceFactoryWrapper<RgbU8ColorSpaceFactory>());
    registry->add(new LcmsRGBP2020PQColorSpaceFactoryWrapper<RgbU16ColorSpaceFactory>());
#ifdef HAVE_LCMS24
#ifdef HAVE_OPENEXR
    registry->add(new LcmsRGBP2020PQColorSpaceFactoryWrapper<RgbF16ColorSpaceFactory>());
#endif
#endif
    registry->add(new LcmsRGBP2020PQColorSpaceFactoryWrapper<RgbF32ColorSpaceFactory>());

    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicU8HistogramProducer>
            (KoID("RGBU8HISTO", PkString("RGBA/8 Histogram")), RGBAColorModelID.id(), Integer8BitsColorDepthID.id()));

    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicU16HistogramProducer>
            (KoID("RGBU16HISTO", PkString("RGBA/16 Histogram")), RGBAColorModelID.id(), Integer16BitsColorDepthID.id()));

#ifdef HAVE_LCMS24
#ifdef HAVE_OPENEXR
    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicF16HalfHistogramProducer>
            (KoID("RGBF16HISTO", PkString("RGBA/F16 Histogram")), RGBAColorModelID.id(), Float16BitsColorDepthID.id()));
#endif
#endif

    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicF32HistogramProducer>
            (KoID("RGF328HISTO", PkString("RGBA/F32 Histogram")), RGBAColorModelID.id(), Float32BitsColorDepthID.id()));

    // ------------------- GRAY ---------------------------------

    cmsToneCurve *Gamma = cmsBuildGamma(0, 2.2);
    cmsHPROFILE hProfile = cmsCreateGrayProfile(cmsD50_xyY(), Gamma);
    cmsFreeToneCurve(Gamma);
    KoColorProfile *defProfile = LcmsColorProfileContainer::createFromLcmsProfile(hProfile);
    registry->addProfile(defProfile);

    registry->add(new GrayAU8ColorSpaceFactory());
    registry->add(new GrayAU16ColorSpaceFactory());
#ifdef HAVE_LCMS24
#ifdef HAVE_OPENEXR
    registry->add(new GrayF16ColorSpaceFactory());
#endif
#endif
    registry->add(new GrayF32ColorSpaceFactory());

    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicU8HistogramProducer>
            (KoID("GRAYA8HISTO", PkString("GRAY/8 Histogram")), GrayAColorModelID.id(), Integer8BitsColorDepthID.id()));

    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicU16HistogramProducer>
            (KoID("GRAYA16HISTO", PkString("GRAY/16 Histogram")), GrayAColorModelID.id(), Integer16BitsColorDepthID.id()));
#ifdef HAVE_LCMS24
#ifdef HAVE_OPENEXR
    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicF16HalfHistogramProducer>
            (KoID("GRAYF16HISTO", PkString("GRAYF/F16 Histogram")), GrayAColorModelID.id(), Float16BitsColorDepthID.id()));
#endif
#endif

    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicF32HistogramProducer>
            (KoID("GRAYAF32HISTO", PkString("GRAY/F32 float Histogram")), GrayAColorModelID.id(), Float32BitsColorDepthID.id()));

    // ------------------- CMYK ---------------------------------

    registry->add(new CmykU8ColorSpaceFactory());
    registry->add(new CmykU16ColorSpaceFactory());
    registry->add(new CmykF32ColorSpaceFactory());

    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicU8HistogramProducer>
            (KoID("CMYK8HISTO", PkString("CMYK/8 Histogram")), CMYKAColorModelID.id(), Integer8BitsColorDepthID.id()));

    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicU16HistogramProducer>
            (KoID("CMYK16HISTO", PkString("CMYK/16 Histogram")), CMYKAColorModelID.id(), Integer16BitsColorDepthID.id()));

    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicF32HistogramProducer>
            (KoID("CMYKF32HISTO", PkString("CMYK/F32 Histogram")), CMYKAColorModelID.id(), Float32BitsColorDepthID.id()));

    // ------------------- XYZ ---------------------------------

    KoColorProfile *xyzProfile = LcmsColorProfileContainer::createFromLcmsProfile(cmsCreateXYZProfile());
    registry->addProfile(xyzProfile);

    registry->add(new XyzU8ColorSpaceFactory());
    registry->add(new XyzU16ColorSpaceFactory());
#ifdef HAVE_LCMS24
#ifdef HAVE_OPENEXR
    registry->add(new XyzF16ColorSpaceFactory());
#endif
#endif
    registry->add(new XyzF32ColorSpaceFactory());

    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicU8HistogramProducer>
            (KoID("XYZ8HISTO", PkString("XYZ/8 Histogram")), XYZAColorModelID.id(), Integer8BitsColorDepthID.id()));

    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicU16HistogramProducer>
            (KoID("XYZ16HISTO", PkString("XYZ/16 Histogram")), XYZAColorModelID.id(), Integer16BitsColorDepthID.id()));

#ifdef HAVE_LCMS24
#ifdef HAVE_OPENEXR
    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicF32HistogramProducer>
            (KoID("XYZF16HISTO", PkString("XYZ/F16 Histogram")), XYZAColorModelID.id(), Float16BitsColorDepthID.id()));
#endif
#endif

    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicF32HistogramProducer>
            (KoID("XYZF32HISTO", PkString("XYZF32 Histogram")), XYZAColorModelID.id(), Float32BitsColorDepthID.id()));

    // ------------------- YCBCR ---------------------------------

    //    KoColorProfile *yCbCrProfile = LcmsColorProfileContainer::createFromLcmsProfile(cmsCreateYCBCRProfile());
    //    registry->addProfile(yCbCrProfile);

    registry->add(new YCbCrU8ColorSpaceFactory());
    registry->add(new YCbCrU16ColorSpaceFactory());
    registry->add(new YCbCrF32ColorSpaceFactory());

    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicU8HistogramProducer>
            (KoID("YCBCR8HISTO", PkString("YCbCr/8 Histogram")), YCbCrAColorModelID.id(), Integer8BitsColorDepthID.id()));

    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicU16HistogramProducer>
            (KoID("YCBCR16HISTO", PkString("YCbCr/16 Histogram")), YCbCrAColorModelID.id(), Integer16BitsColorDepthID.id()));

    KoHistogramProducerFactoryRegistry::instance()->add(
        new KoBasicHistogramProducerFactory<KoBasicF32HistogramProducer>
            (KoID("YCBCRF32HISTO", PkString("YCbCr/F32 Histogram")), YCbCrAColorModelID.id(), Float32BitsColorDepthID.id()));

    // Add profile alias for default profile from lcms1
    registry->addProfileAlias("sRGB built-in - (lcms internal)", "sRGB built-in");
    registry->addProfileAlias("gray built-in - (lcms internal)", "gray built-in");
    registry->addProfileAlias("Lab identity built-in - (lcms internal)", "Lab identity built-in");
    registry->addProfileAlias("XYZ built-in - (lcms internal)", "XYZ identity built-in");

#if defined(HAVE_LCMS_FAST_FLOAT_PLUGIN)
    cmsPlugin(cmsFastFloatExtensions());
#endif
}

namespace
{
struct LcmsEngineRegistration
{
    LcmsEngineRegistration()
    {
        KoColorSpaceRegistry::addInitializationCallback(&registerLcmsEngine);
    }
};

LcmsEngineRegistration s_lcmsEngineRegistration;
} // namespace
