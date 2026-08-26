/*
 *  SPDX-FileCopyrightText: 2008 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_all_filter_test.h"
#include "KisImageResolutionProxy.h"
#include "filter/kis_filter.h"
#include "filter/kis_filter_configuration.h"
#include "filter/kis_filter_registry.h"
#include "kis_default_bounds.h"
#include "kis_pixel_selection.h"
#include "kis_processing_information.h"
#include "kis_selection.h"
#include "kis_transaction.h"
#include <KisGlobalResourcesInterface.h>
#include <KoColorSpaceRegistry.h>
#include <qimage_test_util.h>
#include <simpletest.h>
#include <testing_timed_default_bounds.h>
#include <KisPortingUtils.h>

bool testFilterSrcNotIsDev(KisFilterSP f)
{
    const KoColorSpace * cs = KoColorSpaceRegistry::instance()->rgb8();

    PkImage qimage(PkString(FILES_DATA_DIR) + '/' + "carrot.png");
    PkImage result(PkString(FILES_DATA_DIR) + '/' + "carrot_" + f->id() + ".png");
    KisPaintDeviceSP dev = new KisPaintDevice(cs);
    dev->setDefaultBounds(new TestUtil::TestingTimedDefaultBounds(qimage.rect()));

    KisPaintDeviceSP dstdev = new KisPaintDevice(cs);
    dstdev->setDefaultBounds(new TestUtil::TestingTimedDefaultBounds(qimage.rect()));

    dev->convertFromQImage(qimage, 0, 0, 0);

    // Get the predefined configuration from a file
    KisFilterConfigurationSP  kfc = f->defaultConfiguration(KisGlobalResourcesInterface::instance());

    PkFileStream file(PkString(FILES_DATA_DIR) + '/' + f->id() + ".cfg");
    if (!file.open(PkStream::ReadOnly | PkStream::Text)) {
        //qDebug() << "creating new file for " << f->id();
        if (file.open(PkStream::WriteOnly | PkStream::Text)) {
            PkTextStream out(&file);
            KisPortingUtils::setUtf8OnStream(out);
            out << kfc->toXML();
        } else {
            qDebug() << "Could not open" << file.fileName() << "for writing:" <<  file.errorString();
        }
    } else {
        PkString s;
        PkTextStream in(&file);
        KisPortingUtils::setUtf8OnStream(in);
        s = in.readAll();
        //qDebug() << "Read for " << f->id() << "\n" << s;
        kfc->fromXML(s);
    }
    dbgKrita << f->id();// << "\n" << kfc->toXML() << "\n";

    kfc->createLocalResourcesSnapshot(KisGlobalResourcesInterface::instance());
    f->process(dev, dstdev, 0, PkRect(PkPoint(0,0), qimage.size()), kfc);

    PkPoint errpoint;

    PkImage actualResult = dstdev->convertToQImage(0, 0, 0, qimage.width(), qimage.height());

    if (!TestUtil::compareQImages(errpoint, result, actualResult, 1, 1)) {
        qDebug() << "Failed compare result images for: " << f->id();
        qDebug() << errpoint;
        actualResult.save(PkString("carrot_%1.png").arg(f->id()));
        result.save(PkString("carrot_%1_expected.png").arg(f->id()));
        return false;
    }
    return true;
}

bool testFilter(KisFilterSP f)
{
    const KoColorSpace * cs = KoColorSpaceRegistry::instance()->rgb8();

    PkImage qimage(PkString(FILES_DATA_DIR) + '/' + "carrot.png");
    PkString resultFileName = PkString(FILES_DATA_DIR) + '/' + "carrot_" + f->id() + ".png";
    PkImage result(resultFileName);

    //if (!f->id().contains("hsv")) return true;

    KisPaintDeviceSP dev = new KisPaintDevice(cs);
    dev->setDefaultBounds(new TestUtil::TestingTimedDefaultBounds(qimage.rect()));
    dev->convertFromQImage(qimage, 0, 0, 0);
    KisTransaction * cmd = new KisTransaction(kundo2_noi18n(f->name()), dev);

    // Get the predefined configuration from a file
    KisFilterConfigurationSP  kfc = f->defaultConfiguration(KisGlobalResourcesInterface::instance());

    PkFileStream file(PkString(FILES_DATA_DIR) + '/' + f->id() + ".cfg");
    if (!file.open(PkStream::ReadOnly | PkStream::Text)) {
        //qDebug() << "creating new file for " << f->id();
        if (file.open(PkStream::WriteOnly | PkStream::Text)) {
            PkTextStream out(&file);
            KisPortingUtils::setUtf8OnStream(out);
            out << kfc->toXML();
        } else {
            qDebug() << "Could not open" << file.fileName() << "for writing:" <<  file.errorString();
        }
    } else {
        PkString s;
        PkTextStream in(&file);
        KisPortingUtils::setUtf8OnStream(in);
        s = in.readAll();
        //qDebug() << "Read for " << f->id() << "\n" << s;
        const bool validConfig = kfc->fromXML(s);


        if (!validConfig) {
            qDebug() << PkString("Couldn't parse XML settings for filter %1").arg(f->id()).toLatin1();
            return false;
        }
    }
    dbgKrita << f->id();// << "\n" << kfc->toXML() << "\n";
    kfc->createLocalResourcesSnapshot(KisGlobalResourcesInterface::instance());

    f->process(dev, PkRect(PkPoint(0,0), qimage.size()), kfc);

    PkPoint errpoint;

    delete cmd;

    PkImage actualResult = dev->convertToQImage(0, 0, 0, qimage.width(), qimage.height());

    if (!TestUtil::compareQImages(errpoint, result, actualResult, 1, 1)) {
        qDebug() << "Failed compare result images for: " << f->id();
        qDebug() << errpoint;
        actualResult.save(PkString("carrot_%1.png").arg(f->id()));
        result.save(PkString("carrot_%1_expected.png").arg(f->id()));
        return false;
    }
    return true;
}


bool testFilterWithSelections(KisFilterSP f)
{
    const KoColorSpace * cs = KoColorSpaceRegistry::instance()->rgb8();

    PkImage qimage(PkString(FILES_DATA_DIR) + '/' + "carrot.png");
    PkImage result(PkString(FILES_DATA_DIR) + '/' + "carrot_" + f->id() + ".png");
    KisPaintDeviceSP dev = new KisPaintDevice(cs);
    dev->setDefaultBounds(new TestUtil::TestingTimedDefaultBounds(qimage.rect()));
    dev->convertFromQImage(qimage, 0, 0, 0);

    // Get the predefined configuration from a file
    KisFilterConfigurationSP  kfc = f->defaultConfiguration(KisGlobalResourcesInterface::instance());

    PkFileStream file(PkString(FILES_DATA_DIR) + '/' + f->id() + ".cfg");
    if (!file.open(PkStream::ReadOnly | PkStream::Text)) {
        //qDebug() << "creating new file for " << f->id();
        if (file.open(PkStream::WriteOnly | PkStream::Text)) {
            PkTextStream out(&file);
            KisPortingUtils::setUtf8OnStream(out);
            out << kfc->toXML();
        } else {
            qDebug() << "Could not open" << file.fileName() << "for writing:" <<  file.errorString();
        }
    } else {
        PkString s;
        PkTextStream in(&file);
        KisPortingUtils::setUtf8OnStream(in);
        s = in.readAll();
        //qDebug() << "Read for " << f->id() << "\n" << s;
        kfc->fromXML(s);
    }
    dbgKrita << f->id();// << "\n"; << kfc->toXML() << "\n";

    KisSelectionSP sel1 = new KisSelection(new KisSelectionDefaultBounds(dev), KisImageResolutionProxy::identity());
    sel1->pixelSelection()->select(qimage.rect());

    kfc->createLocalResourcesSnapshot(KisGlobalResourcesInterface::instance());
    f->process(dev, dev, sel1, PkRect(PkPoint(0,0), qimage.size()), kfc);

    PkPoint errpoint;

    PkImage actualResult = dev->convertToQImage(0, 0, 0, qimage.width(), qimage.height());

    if (!TestUtil::compareQImages(errpoint, result, actualResult, 1, 1)) {
        qDebug() << "Failed compare result images for: " << f->id();
        qDebug() << errpoint;
        actualResult.save(PkString("carrot_%1.png").arg(f->id()));
        result.save(PkString("carrot_%1_expected.png").arg(f->id()));
        return false;
    }

    return true;
}

void KisAllFilterTest::testAllFilters()
{
    PkStringList excludeFilters;
    excludeFilters << "colortransfer";
    excludeFilters << "gradientmap";
    excludeFilters << "phongbumpmap";
    excludeFilters << "raindrops";

    // halftone has some bezier curve painting drifts, so
    // let's just exclude it
    excludeFilters << "halftone";

    PkStringList failures;
    PkStringList successes;

    PkList<PkString> filterList = KisFilterRegistry::instance()->keys();
    std::sort(filterList.begin(), filterList.end());
    for (PkList<PkString>::Iterator it = filterList.begin(); it != filterList.end(); ++it) {
        if (excludeFilters.contains(*it)) continue;

        if (testFilter(KisFilterRegistry::instance()->value(*it)))
            successes << *it;
        else
            failures << *it;
    }
    dbgKrita << "Success: " << successes;
    if (failures.size() > 0) {
        QFAIL(PkString("Failed filters:\n\t %1").arg(failures.join("\n\t")).toLatin1());
    }
}

void KisAllFilterTest::testAllFiltersSrcNotIsDev()
{
    PkStringList excludeFilters;
    excludeFilters << "colortransfer";
    excludeFilters << "gradientmap";
    excludeFilters << "phongbumpmap";
    excludeFilters << "raindrops";

    // halftone has some bezier curve painting drifts, so
    // let's just exclude it
    excludeFilters << "halftone";

    PkStringList failures;
    PkStringList successes;

    PkList<PkString> filterList = KisFilterRegistry::instance()->keys();
    std::sort(filterList.begin(), filterList.end());
    for (PkList<PkString>::Iterator it = filterList.begin(); it != filterList.end(); ++it) {
        if (excludeFilters.contains(*it)) continue;

        if (testFilterSrcNotIsDev(KisFilterRegistry::instance()->value(*it)))
            successes << *it;
        else
            failures << *it;
    }
    dbgKrita << "Src!=Dev Success: " << successes;
    if (failures.size() > 0) {
        QFAIL(PkString("Src!=Dev Failed filters:\n\t %1").arg(failures.join("\n\t")).toLatin1());
    }

}

void KisAllFilterTest::testAllFiltersWithSelections()
{
    PkStringList excludeFilters;
    excludeFilters << "colortransfer";
    excludeFilters << "gradientmap";
    excludeFilters << "phongbumpmap";
    excludeFilters << "raindrops";

    // halftone has some bezier curve painting drifts, so
    // let's just exclude it
    excludeFilters << "halftone";

    PkStringList failures;
    PkStringList successes;

    PkList<PkString> filterList = KisFilterRegistry::instance()->keys();
    std::sort(filterList.begin(), filterList.end());
    for (PkList<PkString>::Iterator it = filterList.begin(); it != filterList.end(); ++it) {
        if (excludeFilters.contains(*it)) continue;

        if (testFilterWithSelections(KisFilterRegistry::instance()->value(*it)))
            successes << *it;
        else
            failures << *it;
    }
    dbgKrita << "Success: " << successes;
    if (failures.size() > 0) {
        QFAIL(PkString("Failed filters with selections:\n\t %1").arg(failures.join("\n\t")).toLatin1());
    }
}

#include <testimage.h>
KISTEST_MAIN(KisAllFilterTest)
