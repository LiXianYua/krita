/*
 *  SPDX-FileCopyrightText: 2008 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_crash_filter_test.h"
#include "filter/kis_filter.h"
#include "filter/kis_filter_configuration.h"
#include "filter/kis_filter_registry.h"
#include "kis_pixel_selection.h"
#include "kis_processing_information.h"
#include "kis_selection.h"
#include "kis_transaction.h"
#include <KisGlobalResourcesInterface.h>
#include <KoColorProfile.h>
#include <KoColorSpaceRegistry.h>
#include <simpletest.h>
#include <testing_timed_default_bounds.h>
#include <KisPortingUtils.h>

bool KisCrashFilterTest::applyFilter(const KoColorSpace * cs,  KisFilterSP f)
{

    PkImage qimage(PkString(FILES_DATA_DIR) + '/' + "carrot.png");

    KisPaintDeviceSP dev = new KisPaintDevice(cs);
    dev->setDefaultBounds(new TestUtil::TestingTimedDefaultBounds(qimage.rect()));
    dev->convertFromQImage(qimage, 0, 0, 0);

    // Get the predefined configuration from a file
    KisFilterConfigurationSP  kfc = f->defaultConfiguration(KisGlobalResourcesInterface::instance());

    PkFileStream file(PkString(FILES_DATA_DIR) + '/' + f->id() + ".cfg");
    if (!file.open(PkStream::ReadOnly | PkStream::Text)) {
        dbgKrita << "creating new file for " << f->id();
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
        kfc->fromXML(s);
    }
    dbgKrita << f->id() << ", " << cs->id() << ", " << cs->profile()->name();// << kfc->toXML() << "\n";

    {
        kfc->createLocalResourcesSnapshot(KisGlobalResourcesInterface::instance());
        KisTransaction t(kundo2_noi18n(f->name()), dev);
        f->process(dev, PkRect(PkPoint(0,0), qimage.size()), kfc);
    }

    return true;

}

bool KisCrashFilterTest::testFilter(KisFilterSP f)
{
    PkList<const KoColorSpace*> colorSpaces = KoColorSpaceRegistry::instance()->allColorSpaces(KoColorSpaceRegistry::AllColorSpaces, KoColorSpaceRegistry::OnlyDefaultProfile);
    bool ok = false;
    Q_FOREACH (const KoColorSpace* colorSpace, colorSpaces) {

        // Alpha color spaces are never processed directly. They are
        // first converted into GrayA color space
        if (colorSpace->id().startsWith("ALPHA", Qt::CaseInsensitive)) {
            continue;
        }

        ok = applyFilter(colorSpace, f);
    }

    return ok;
}

void KisCrashFilterTest::testCrashFilters()
{
    PkStringList excludeFilters;
    excludeFilters << "colortransfer";
    excludeFilters << "gradientmap";
    excludeFilters << "phongbumpmap";
    excludeFilters << "perchannel";
    excludeFilters << "height to normal";


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

#include <testimage.h>
KISTEST_MAIN(KisCrashFilterTest)
