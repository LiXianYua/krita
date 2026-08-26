/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2016 Spencer Brown <sbrown655@gmail.com>
 * SPDX-FileCopyrightText: 2020 Deif Lou <ginoba@gmail.com>
 * 
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkXmlDocument.h>

#include <KoResourceLoadResult.h>
#include <KoStopGradient.h>
#include <KoAbstractGradient.h>
#include <KisDitherConfigurationHelper.h>
#include <PkMemoryStream.h>
#include <PkAuxTypes.h>
#include <KoMD5Generator.h>

#include "KisGradientMapFilterConfiguration.h"

KisGradientMapFilterConfiguration::KisGradientMapFilterConfiguration(KisResourcesInterfaceSP resourcesInterface)
    : KisFilterConfiguration(defaultName(), defaultVersion(), resourcesInterface)
{}

KisGradientMapFilterConfiguration::KisGradientMapFilterConfiguration(qint32 version, KisResourcesInterfaceSP resourcesInterface)
    : KisFilterConfiguration(defaultName(), version, resourcesInterface)
{}

KisGradientMapFilterConfiguration::KisGradientMapFilterConfiguration(const KisGradientMapFilterConfiguration &rhs)
    : KisFilterConfiguration(rhs)
{}

KisFilterConfigurationSP KisGradientMapFilterConfiguration::clone() const
{
    return new KisGradientMapFilterConfiguration(*this);
}

PkList<KoResourceLoadResult> KisGradientMapFilterConfiguration::linkedResources(KisResourcesInterfaceSP globalResourcesInterface) const
{
    PkList<KoResourceLoadResult> resources;

    // only the first version of the filter loaded the gradient by name
    if (version() == 1) {
        KoAbstractGradientSP gradient = this->gradient();
        if (gradient) {
            resources << gradient;
        } else {
            PkString md5sum = this->getString("md5sum");
            PkString gradientName = this->getString("gradientName");

            resources << KoResourceSignature(ResourceType::Gradients, md5sum, "", gradientName);
        }
    }

    resources << KisDitherConfigurationHelper::prepareLinkedResources(*this, "dither/", globalResourcesInterface);

    return resources;
}

PkList<KoResourceLoadResult> KisGradientMapFilterConfiguration::embeddedResources(KisResourcesInterfaceSP) const
{
    PkList<KoResourceLoadResult> resources;

    // the second version of the filter embeds the gradient
    if (version() > 1) {
        KoAbstractGradientSP gradient = this->gradient();

        // TODO: check if it is okay to resave the gradient on loading
        PkMemoryStream buffer;
        buffer.open(PkMemoryStream::WriteOnly);
        gradient->saveToDevice(&buffer);

        const PkByteArray bufData(buffer.data(), (int)buffer.size());
        resources << KoEmbeddedResource(KoResourceSignature(ResourceType::Gradients, KoMD5Generator::generateHash(bufData), gradient->filename(), gradient->name()), bufData);
    }

    return resources;
}

KoAbstractGradientSP KisGradientMapFilterConfiguration::gradient(KoAbstractGradientSP fallbackGradient) const
{
    if (version() == 1) {

        PkString md5sum = this->getString("md5sum");
        PkString gradientName = this->getString("gradientName");
        auto source = resourcesInterface()->source<KoAbstractGradient>(ResourceType::Gradients);

        KoAbstractGradientSP resourceGradient = source.bestMatch(md5sum, "", gradientName);

        if (resourceGradient) {
            KoStopGradientSP gradient = KisGradientConversion::toStopGradient(resourceGradient);
            gradient->setValid(true);
            return gradient;
        } else {
            qWarning() << "Could not find gradient" << getString("md5sum") << getString("gradientName");
        }
    } else if (version() == 2) {
        PkXmlDocument document;
        if (document.setContent(getString("gradientXML", ""))) {
            const PkXmlElement gradientElement = document.firstChildElement();
            if (!gradientElement.isNull()) {
                const PkString gradientType = gradientElement.attribute("type");
                KoAbstractGradientSP gradient = nullptr;
                if (gradientType == "stop") {
                    gradient = KoStopGradient::fromXML(gradientElement).clone().dynamicCast<KoAbstractGradient>();
                } else if (gradientType == "segment") {
                    gradient = KoSegmentGradient::fromXML(gradientElement).clone().dynamicCast<KoAbstractGradient>();
                }
                if (gradient) {
                    gradient->setName(gradientElement.attribute("name", ""));
                    gradient->setFilename(gradient->name() + gradient->defaultFileExtension());
                    gradient->setValid(true);
                    return gradient;
                }
            }
        }
    }
    return fallbackGradient ? fallbackGradient : defaultGradient(resourcesInterface());
}

int KisGradientMapFilterConfiguration::colorMode() const
{
    return getInt("colorMode", defaultColorMode());
}

void KisGradientMapFilterConfiguration::setGradient(KoAbstractGradientSP newGradient)
{
    if (!newGradient) {
        setProperty("gradientXML", "");
        return;
    }

    PkXmlDocument document;
    PkXmlElement gradientElement = document.createElement("gradient");
    gradientElement.setAttribute("name", newGradient->name());
    gradientElement.setAttribute("md5sum", newGradient->md5Sum());

    if (newGradient.dynamicCast<KoStopGradient>()) {
        KoStopGradient *gradient = static_cast<KoStopGradient*>(newGradient.data());
        gradient->toXML(document, gradientElement);
    } else if (newGradient.dynamicCast<KoSegmentGradient>()) {
        KoSegmentGradient *gradient = static_cast<KoSegmentGradient*>(newGradient.data());
        gradient->toXML(document, gradientElement);
    }

    document.appendChild(gradientElement);
    setProperty("gradientXML", document.toString());
}

void KisGradientMapFilterConfiguration::setColorMode(int newColorMode)
{
    setProperty("colorMode", newColorMode);
}

void KisGradientMapFilterConfiguration::setDefaults()
{
    setGradient(nullptr);
    setColorMode(defaultColorMode());
    KisDitherConfigurationHelper::factoryConfiguration(*this, "dither/");
}
