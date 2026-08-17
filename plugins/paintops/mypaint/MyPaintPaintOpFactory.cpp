/*
 * SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.com>
 * SPDX-FileCopyrightText: 2020 Ashwin Dhakaita <ashwingpdhakaita@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MyPaintPaintOpFactory.h"

#include <qmath.h>
#include <QJsonObject>
#include <QJsonDocument>

#include <KoResourceLoadResult.h>
#include <kis_image.h>
#include <kis_node.h>

#include "MyPaintPaintOp.h"
#include "MyPaintPaintOpPreset.h"
#include "MyPaintPaintOpSettings.h"

class KisMyPaintOpFactory::Private {
};

KisMyPaintOpFactory::KisMyPaintOpFactory()
    : m_d(new Private)
{
}

KisMyPaintOpFactory::~KisMyPaintOpFactory() {

    delete m_d;
}

KisPaintOp* KisMyPaintOpFactory::createOp(const KisPaintOpSettingsSP settings, KisPainter *painter, KisNodeSP node, KisImageSP image) {

    KisPaintOp* op = new KisMyPaintPaintOp(settings, painter, node, image);
    Q_CHECK_PTR(op);
    return op;
}

KisPaintOpSettingsSP KisMyPaintOpFactory::createSettings(KisResourcesInterfaceSP resourcesInterface) {

    KisPaintOpSettingsSP settings = new KisMyPaintOpSettings(resourcesInterface);
    return settings;
}

QString KisMyPaintOpFactory::id() const {

    return "mypaintbrush";
}

QString KisMyPaintOpFactory::name() const {

    return "MyPaint";
}

QIcon KisMyPaintOpFactory::icon() {

    return KisPaintOpFactory::icon();
}

QString KisMyPaintOpFactory::category() const {

    return KisPaintOpFactory::categoryStable();
}

QList<KoResourceLoadResult> KisMyPaintOpFactory::prepareLinkedResources(const KisPaintOpSettingsSP settings, KisResourcesInterfaceSP resourcesInterface)
{
    Q_UNUSED(settings)
    Q_UNUSED(resourcesInterface);

    return {};
}

QList<KoResourceLoadResult> KisMyPaintOpFactory::prepareEmbeddedResources(const KisPaintOpSettingsSP settings, KisResourcesInterfaceSP resourcesInterface)
{
    Q_UNUSED(settings)
    Q_UNUSED(resourcesInterface);

    return {};
}

bool KisMyPaintOpFactory::lodSizeThresholdSupported() const
{
    return true;
}
