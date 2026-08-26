/*
 *  SPDX-FileCopyrightText: 2008 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "kis_brush_registry.h"

#include <PkString.h>

#include <kis_debug.h>

#include "KoResourceServer.h"
#include "kis_auto_brush_factory.h"
#include "kis_text_brush_factory.h"
#include "kis_predefined_brush_factory.h"

KisBrushRegistry::KisBrushRegistry()
{
    add(new KisAutoBrushFactory());
    add(new KisPredefinedBrushFactory("gbr_brush"));
    add(new KisPredefinedBrushFactory("abr_brush"));
    add(new KisTextBrushFactory());
    add(new KisPredefinedBrushFactory("png_brush"));
    add(new KisPredefinedBrushFactory("svg_brush"));
}

KisBrushRegistry::~KisBrushRegistry()
{
    for (const PkString &id : keys()) {
        delete get(id);
    }
    dbgRegistry << "deleting KisBrushRegistry";
}

KisBrushRegistry* KisBrushRegistry::instance()
{
    static KisBrushRegistry s_instance;
    return &s_instance;
}


KoResourceLoadResult KisBrushRegistry::createBrush(const PkXmlElement& element, KisResourcesInterfaceSP resourcesInterface)
{
    PkString brushType = element.attribute("type");

    if (brushType.isEmpty()) {
        return KoResourceSignature(ResourceType::Brushes, "", "unknown", "unknown");
    }

    KisBrushFactory *factory = get(brushType);
    if (!factory) {
        return KoResourceSignature(ResourceType::Brushes, "", "unknown", "unknown");
    }

    return factory->createBrush(element, resourcesInterface);
}

KoResourceLoadResult KisBrushRegistry::createBrush(const KisBrushModel::BrushData &data, KisResourcesInterfaceSP resourcesInterface)
{
    PkXmlDocument doc;
    PkXmlElement element = doc.createElement("brush_definition");
    toXML(doc, element, data);
    return createBrush(element, resourcesInterface);
}

std::optional<KisBrushModel::BrushData> KisBrushRegistry::createBrushModel(const PkXmlElement& element, KisResourcesInterfaceSP resourcesInterface)
{
    PkString brushType = element.attribute("type");

    if (brushType.isEmpty()) {
        return std::nullopt;
    }

    KisBrushFactory *factory = get(brushType);

    if (!factory) {
        return std::nullopt;
    }

    return factory->createBrushModel(element, resourcesInterface);
}

void KisBrushRegistry::toXML(PkXmlDocument &doc, PkXmlElement &element, const KisBrushModel::BrushData &model)
{
    PkString brushType;

    if (model.type == KisBrushModel::Auto) {
        brushType = "auto_brush";
    } else if (model.type == KisBrushModel::Text) {
        brushType = "kis_text_brush";
    } else {
        brushType = model.predefinedBrush.subtype;
    }

    KIS_SAFE_ASSERT_RECOVER_RETURN(!brushType.isEmpty());

    KisBrushFactory *factory = get(brushType);
    KIS_SAFE_ASSERT_RECOVER_RETURN(factory);

    factory->toXML(doc, element, model);
}
