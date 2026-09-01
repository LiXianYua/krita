/*
 *  SPDX-FileCopyrightText: 2015 Jouni Pentikäinen <joupent@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_ONION_SKIN_COMPOSITOR_H
#define KIS_ONION_SKIN_COMPOSITOR_H

#include "kis_types.h"
#include "kritaimage_export.h"

#include <PkObject.h>
#include <PkRect.h>
#include <PkScopedPointer.h>
#include <PkSet.h>
#include <PkSignalCompat.h>

class KRITAIMAGE_EXPORT KisOnionSkinCompositor : public PkObject
{
public:
    KisOnionSkinCompositor();
    ~KisOnionSkinCompositor() override;
    static KisOnionSkinCompositor *instance();

    void composite(const KisPaintDeviceSP sourceDevice, KisPaintDeviceSP targetDevice, const PkRect &rect);

    PkRect calculateFullExtent(const KisPaintDeviceSP device);
    PkRect calculateExtent(const KisPaintDeviceSP device, int time);
    PkRect calculateExtent(const KisPaintDeviceSP device);

    PkRect updateExtentOnAddition(const KisPaintDeviceSP device, int addedTime);

    int configSeqNo() const;

    void setColorLabelFilter(PkSet<int> colors);
    PkSet<int> colorLabelFilter();

public:
    void configChanged();

signals:
    void sigOnionSkinChanged();

private:
    struct Private;
    PkScopedPointer<Private> m_d;

};

#endif
