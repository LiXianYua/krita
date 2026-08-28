/*
 * SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.com>
 * SPDX-FileCopyrightText: 2020 Ashwin Dhakaita <ashwingpdhakaita@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_MY_PAINTOP_FACTORY_H
#define KIS_MY_PAINTOP_FACTORY_H

#include <kis_paintop_factory.h>

class KisMyPaintOpFactory: public KisPaintOpFactory
{
public:

    KisMyPaintOpFactory() = default;
    ~KisMyPaintOpFactory() override = default;

    KisPaintOp *createOp(const KisPaintOpSettingsSP settings, KisPainter *painter, KisNodeSP node, KisImageSP image) override;
    KisPaintOpSettingsSP createSettings(KisResourcesInterfaceSP resourcesInterface) override;
    PkString id() const override;
    PkString name() const override;
    PkString category() const override;

    PkList<KoResourceLoadResult> prepareLinkedResources(const KisPaintOpSettingsSP settings, KisResourcesInterfaceSP resourcesInterface) override;
    PkList<KoResourceLoadResult> prepareEmbeddedResources(const KisPaintOpSettingsSP settings, KisResourcesInterfaceSP resourcesInterface) override;
    bool lodSizeThresholdSupported() const override;

};

#endif // KIS_MY_PAINTOP_FACTORY_H
