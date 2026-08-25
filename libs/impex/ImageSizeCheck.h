/*
 * SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef CHECKIMAGESIZE_H
#define CHECKIMAGESIZE_H

#include "KisExportCheckRegistry.h"
#include <KoID.h>
#include <kis_image.h>
#include "kritaimpex_export.h"

class KRITAIMPEX_EXPORT ImageSizeCheck : public KisExportCheckBase
{
public:

    ImageSizeCheck(int maxWidth, int maxHeight, const PkString &id, Level level, const PkString &customWarning = PkString())
        : KisExportCheckBase(id, level, customWarning, true)
        , m_maxW(maxWidth)
        , m_maxH(maxHeight)
    {
        if (customWarning.isEmpty()) {
            m_warning = PkString("This image is larger than <b>%1 x %2</b>. Images this size cannot be saved to this format.").arg(m_maxW).arg(m_maxH);
        }
    }

    bool checkNeeded(KisImageSP image) const override
    {
        return image->width() >= m_maxW && image->height() >= m_maxH;
    }

    Level check(KisImageSP /*image*/) const override
    {
        return m_level;
    }

    int m_maxW;
    int m_maxH;
};

class KRITAIMPEX_EXPORT ImageSizeCheckFactory : public KisExportCheckFactory
{
public:

    ImageSizeCheckFactory() {}

    ~ImageSizeCheckFactory() override {}

    KisExportCheckBase *create( KisExportCheckBase::Level level, const PkString &customWarning = PkString()) override
    {
        return new ImageSizeCheck(100000000, 100000000, id(), level, customWarning);
    }

    KisExportCheckBase *create(int maxWidth, int maxHeight, KisExportCheckBase::Level level, const PkString &customWarning = PkString())
    {
        return new ImageSizeCheck(maxWidth, maxHeight, id(), level, customWarning);
    }

    PkString id() const override {
        return "ImageSizeCheck";
    }
};


#endif // CHECKIMAGESIZE_H
