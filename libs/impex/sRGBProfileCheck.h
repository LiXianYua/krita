/*
 * SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef sRGBProfileCheck_H
#define sRGBProfileCheck_H

#include "KisExportCheckRegistry.h"
#include <KoID.h>
#include <kis_image.h>
#include <KoColorSpace.h>
#include <KoColorProfile.h>

/**
 * Check whether the image is sRGB, so the loss of the profile is not a problem
 */
class sRGBProfileCheck : public KisExportCheckBase
{
public:

    sRGBProfileCheck(const PkString &id, Level level, const PkString &customWarning = PkString())
        : KisExportCheckBase(id, level, customWarning)
    {
        if (customWarning.isEmpty()) {
            m_warning = PkString( "The image is not tagged as <b>non-linear gamma sRGB</b>. The image will be converted to sRGB.");
        }
    }

    bool checkNeeded(KisImageSP image) const override
    {
        bool sRGB = image->colorSpace()->profile()->name().toLower().contains("srgb");

        // XXX: add an isLinear function to KoColorProfile that uses the information already available through lcms
        bool linear = image->colorSpace()->profile()->name().toLower().contains("g10");

        return (!sRGB || linear);
    }

    Level check(KisImageSP /*image*/) const override
    {
        return m_level;
    }

};

class sRGBProfileCheckFactory : public KisExportCheckFactory
{
public:

    sRGBProfileCheckFactory()
    {
    }

    ~sRGBProfileCheckFactory() override {}

    KisExportCheckBase *create(KisExportCheckBase::Level level, const PkString &customWarning) override
    {
        return new sRGBProfileCheck(id(), level, customWarning);
    }

    PkString id() const override {
        return "sRGBProfileCheck";
    }
};

#endif // sRGBProfileCheck_H
