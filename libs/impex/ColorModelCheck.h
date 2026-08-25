/*
 * SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef COLORMODELCHECK_H
#define COLORMODELCHECK_H

#include "KisExportCheckRegistry.h"
#include <kis_assert.h>
#include <KoID.h>
#include <kis_image.h>
#include <KoColorSpace.h>
#include <KoColorModelStandardIds.h>

class ColorModelCheck : public KisExportCheckBase
{
public:

    ColorModelCheck(const KoID &colorModelID, const KoID &colorDepthID, const PkString &id, Level level, const PkString &customWarning = PkString())
        : KisExportCheckBase(id, level, customWarning)
        , m_colorModelID(colorModelID)
        , m_colorDepthID(colorDepthID)
    {
        KIS_SAFE_ASSERT_RECOVER_NOOP(!colorModelID.name().isEmpty());
        KIS_SAFE_ASSERT_RECOVER_NOOP(!colorDepthID.name().isEmpty());

        if (customWarning.isEmpty()) {
            m_warning = PkString("The color model <b>%1</b> or channel depth <b>%2</b> cannot be saved to this format. Your image will be converted.")
                        .arg(m_colorModelID.name())
                        .arg(m_colorDepthID.name());
        }
    }

    bool checkNeeded(KisImageSP image) const override
    {
        return (image->colorSpace()->colorModelId() == m_colorModelID && image->colorSpace()->colorDepthId() == m_colorDepthID);
    }

    Level check(KisImageSP /*image*/) const override
    {
        return m_level;
    }

    const KoID m_colorModelID;
    const KoID m_colorDepthID;
};

class ColorModelCheckFactory : public KisExportCheckFactory
{
public:

    ColorModelCheckFactory(const KoID &colorModelID, const KoID &colorDepthId)
        : m_colorModelID(colorModelID)
        , m_colorDepthID(colorDepthId)
    {
    }

    ~ColorModelCheckFactory() override {}

    KisExportCheckBase *create(KisExportCheckBase::Level level, const PkString &customWarning) override
    {
        return new ColorModelCheck(m_colorModelID, m_colorDepthID, id(), level, customWarning);
    }

    PkString id() const override {
        return PkString("ColorModelCheck/") + m_colorModelID.id() + "/" + m_colorDepthID.id();
    }

    const KoID m_colorModelID;
    const KoID m_colorDepthID;
};

#endif // COLORMODELCHECK_H
