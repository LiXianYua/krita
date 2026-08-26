/*
 *  SPDX-FileCopyrightText: 2023 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef RESET_TRANSPARENT_H
#define RESET_TRANSPARENT_H


#include <kis_filter.h>


class KisResetTransparentFilter : public KisFilter
{
public:
    KisResetTransparentFilter();
public:
    static inline KoID id() {
        return KoID("resettransparent", PkString("Reset Transparent"));
    }

    bool needsTransparentPixels(const KisFilterConfigurationSP config, const KoColorSpace *cs) const override;

protected:
    void processImpl(KisPaintDeviceSP device,
                     const PkRect& applyRect,
                     const KisFilterConfigurationSP config,
                     KoUpdater* progressUpdater = 0 ) const override;

};

#endif /* RESET_TRANSPARENT_H */
