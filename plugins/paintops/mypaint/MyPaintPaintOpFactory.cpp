/*
 * SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.com>
 * SPDX-FileCopyrightText: 2020 Ashwin Dhakaita <ashwingpdhakaita@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MyPaintPaintOpFactory.h"

#include <KoResourceLoadResult.h>
#include <kis_image.h>
#include <kis_node.h>

#include "MyPaintPaintOp.h"
#include "MyPaintPaintOpPreset.h"
#include "MyPaintPaintOpSettings.h"

KisPaintOp* KisMyPaintOpFactory::createOp(const KisPaintOpSettingsSP settings, KisPainter *painter, KisNodeSP node, KisImageSP image) {

    return new KisMyPaintPaintOp(settings, painter, node, image);
}

KisPaintOpSettingsSP KisMyPaintOpFactory::createSettings(KisResourcesInterfaceSP resourcesInterface) {

    KisPaintOpSettingsSP settings = new KisMyPaintOpSettings(resourcesInterface);
    return settings;
}

PkString KisMyPaintOpFactory::id() const {

    return "mypaintbrush";
}

PkString KisMyPaintOpFactory::name() const {

    return "MyPaint";
}

PkString KisMyPaintOpFactory::category() const {

    return KisPaintOpFactory::categoryStable();
}

PkList<KoResourceLoadResult> KisMyPaintOpFactory::prepareLinkedResources(const KisPaintOpSettingsSP, KisResourcesInterfaceSP)
{
    return {};
}

PkList<KoResourceLoadResult> KisMyPaintOpFactory::prepareEmbeddedResources(const KisPaintOpSettingsSP, KisResourcesInterfaceSP)
{
    return {};
}

bool KisMyPaintOpFactory::lodSizeThresholdSupported() const
{
    return true;
}
