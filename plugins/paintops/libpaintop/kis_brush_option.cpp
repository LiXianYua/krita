/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2008 Sven Langkamp <sven.langkamp@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "kis_brush_option.h"

#include <PkXmlDocument.h>
#include <PkXmlElement.h>

#include "kis_properties_configuration.h"
#include <KisPaintopSettingsIds.h>
#include <kis_brush.h>

#include <KoCanvasResourcesInterface.h>
#include <KoCanvasResourcesIds.h>
#include <KoAbstractGradient.h>
#include <KoResourceLoadResult.h>

void KisBrushOptionProperties::writeOptionSettingImpl(KisPropertiesConfiguration *setting) const
{
    if (!m_brush) return;

    PkXmlDocument d;
    PkXmlElement e = d.createElement("Brush");
    m_brush->toXML(d, e);
    d.appendChild(e);
    setting->setProperty("brush_definition", d.toString());
}

PkXmlElement getBrushXMLElement(const KisPropertiesConfiguration *setting)
{
    PkXmlElement element;

    PkString brushDefinition = setting->getString("brush_definition");

    if (!brushDefinition.isEmpty()) {
        PkXmlDocument d;
        d.setContent(brushDefinition);
        element = d.firstChildElement("Brush");
    }

    return element;
}

void KisBrushOptionProperties::readOptionSettingResourceImpl(const KisPropertiesConfiguration *setting, KisResourcesInterfaceSP resourcesInterface, KoCanvasResourcesInterfaceSP canvasResourcesInterface)
{
    PkXmlElement element = getBrushXMLElement(setting);
    if (!element.isNull()) {
        m_brush = KisBrush::fromXML(element, resourcesInterface);
        if (m_brush && m_brush->applyingGradient() && canvasResourcesInterface) {
            KoAbstractGradientSP gradient = canvasResourcesInterface->resource(KoCanvasResource::CurrentGradient).value<KoAbstractGradientSP>()->cloneAndBakeVariableColors(canvasResourcesInterface);
            m_brush->setGradient(gradient);
        }
    }
}

PkList<KoResourceLoadResult> KisBrushOptionProperties::prepareLinkedResourcesImpl(const KisPropertiesConfiguration *settings, KisResourcesInterfaceSP resourcesInterface) const
{
    PkList<KoResourceLoadResult> resources;
    PkXmlElement element = getBrushXMLElement(settings);
    if (element.isNull()) return resources;

    KoResourceLoadResult result = KisBrush::fromXMLLoadResult(element, resourcesInterface);

    KoResourceSP resource = result.resource();
    if (!resource || !resource->isEphemeral()) {
        resources << result;
    }

    return resources;
}

PkList<KoResourceLoadResult> KisBrushOptionProperties::prepareEmbeddedResourcesImpl(const KisPropertiesConfiguration *settings, KisResourcesInterfaceSP resourcesInterface) const
{
    Q_UNUSED(settings);
    Q_UNUSED(resourcesInterface);
    return {};
}

enumBrushApplication KisBrushOptionProperties::brushApplication(const KisPropertiesConfiguration *settings, KisResourcesInterfaceSP resourcesInterface)
{
    PkList<KoResourceSP> resources;

    PkXmlElement element = getBrushXMLElement(settings);
    if (element.isNull()) return ALPHAMASK;

    KisBrushSP brush = KisBrush::fromXML(element, resourcesInterface);

    return brush ? brush->brushApplication() : ALPHAMASK;
}

#ifdef HAVE_THREADED_TEXT_RENDERING_WORKAROUND

#include "kis_text_brush_factory.h"

bool KisBrushOptionProperties::isTextBrush(const KisPropertiesConfiguration *setting)
{
    static PkString textBrushId = KisTextBrushFactory().id();

    PkXmlElement element = getBrushXMLElement(setting);
    PkString brushType = element.attribute("type");

    return brushType == textBrushId;
}

#endif /* HAVE_THREADED_TEXT_RENDERING_WORKAROUND */

KisBrushSP KisBrushOptionProperties::brush() const
{
    return m_brush;
}

void KisBrushOptionProperties::setBrush(KisBrushSP brush)
{
    m_brush = brush;
}
