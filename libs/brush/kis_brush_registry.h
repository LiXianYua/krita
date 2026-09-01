/*
 *  SPDX-FileCopyrightText: 2008 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_BRUSH_REGISTRY_H_
#define KIS_BRUSH_REGISTRY_H_

#include <PkObject.h>

#include "kis_types.h"
#include "KoGenericRegistry.h"

#include <kritabrush_export.h>

#include "kis_brush.h"
#include "kis_brush_factory.h"
#include "KisBrushModel.h"

class PkXmlElement;

class BRUSH_EXPORT KisBrushRegistry : public PkObject, public KoGenericRegistry<KisBrushFactory*>
{

public:
    KisBrushRegistry();
    ~KisBrushRegistry() override;

    static KisBrushRegistry* instance();

    KoResourceLoadResult createBrush(const PkXmlElement& element, KisResourcesInterfaceSP resourcesInterface);
    KoResourceLoadResult createBrush(const KisBrushModel::BrushData &data, KisResourcesInterfaceSP resourcesInterface);
    std::optional<KisBrushModel::BrushData> createBrushModel(const PkXmlElement& element, KisResourcesInterfaceSP resourcesInterface);
    void toXML(PkXmlDocument &doc, PkXmlElement& element, const KisBrushModel::BrushData &model);

private:
    KisBrushRegistry(const KisBrushRegistry&);
    KisBrushRegistry operator=(const KisBrushRegistry&);
};

#endif // KIS_GENERATOR_REGISTRY_H_
