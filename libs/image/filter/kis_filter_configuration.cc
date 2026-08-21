/*
 *  SPDX-FileCopyrightText: 2006 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "filter/kis_filter_configuration.h"
#include "filter/kis_filter.h"

#include <kis_debug.h>

#include <string>

#include <PkAtomic.h>
#include <PkBitArray.h>
#include <PkString.h>
#include <PkVariant.h>
#include <PkXmlDocument.h>
#include <PkXmlElement.h>

#include "filter/kis_filter_registry.h"
#include "kis_transaction.h"
#include "kis_undo_adapter.h"
#include "kis_painter.h"
#include "kis_selection.h"
#include "KoID.h"
#include "kis_types.h"
#include <KisRequiredResourcesOperators.h>


struct Q_DECL_HIDDEN KisFilterConfiguration::Private {
    PkString name;
    qint32 version;
    PkBitArray channelFlags;
    KisResourcesInterfaceSP resourcesInterface = 0;

    Private(const PkString & _name, qint32 _version, KisResourcesInterfaceSP _resourcesInterface)
        : name(_name),
          version(_version),
          resourcesInterface(_resourcesInterface)
    {
    }

    Private(const Private &rhs)
        : name(rhs.name),
          version(rhs.version),
          channelFlags(rhs.channelFlags),
          resourcesInterface(rhs.resourcesInterface)
    {
    }

#ifdef SANITY_CHECK_FILTER_CONFIGURATION_OWNER
    PkAtomicInt sanityUsageCounter;
#endif /* SANITY_CHECK_FILTER_CONFIGURATION_OWNER */
};

KisFilterConfiguration::KisFilterConfiguration(const PkString & name, qint32 version, KisResourcesInterfaceSP resourcesInterface)
    : d(new Private(name, version, resourcesInterface))
{
}

KisFilterConfigurationSP KisFilterConfiguration::clone() const
{
    return new KisFilterConfiguration(*this);
}

KisFilterConfiguration::KisFilterConfiguration(const KisFilterConfiguration & rhs)
    : KisPropertiesConfiguration(rhs)
    , d(new Private(*rhs.d))
{
}

KisFilterConfiguration::~KisFilterConfiguration()
{
    delete d;
}

void KisFilterConfiguration::fromLegacyXML(const PkXmlElement& root)
{
    clearProperties();
    d->name = root.attribute("name");
    d->version = root.attribute("version").toInt();

    PkXmlElement e;
    for (e = root.firstChildElement("property"); !e.isNull(); e = e.nextSiblingElement()) {
        PkString name = e.attribute("name");
        PkString type = e.attribute("type");
        PkString value = e.text();

        // XXX: Convert the variant pro-actively to the right type?
        setProperty(name, PkVariant(value));
    }
}

void KisFilterConfiguration::fromXML(const PkXmlElement& elt)
{
    d->version = elt.attribute("version").toInt();
    KisPropertiesConfiguration::fromXML(elt);
}

void KisFilterConfiguration::toXML(PkXmlDocument& doc, PkXmlElement& elt) const
{
    elt.setAttribute("version", PkString(std::to_string(d->version).c_str()));
    KisPropertiesConfiguration::toXML(doc, elt);
}


const PkString & KisFilterConfiguration::name() const
{
    return d->name;
}

qint32 KisFilterConfiguration::version() const
{
    return d->version;
}

void KisFilterConfiguration::setVersion(qint32 version)
{
    d->version = version;
}

KisResourcesInterfaceSP KisFilterConfiguration::resourcesInterface() const
{
    return d->resourcesInterface;
}

void KisFilterConfiguration::setResourcesInterface(KisResourcesInterfaceSP resourcesInterface)
{
    d->resourcesInterface = resourcesInterface;
}

void KisFilterConfiguration::createLocalResourcesSnapshot(KisResourcesInterfaceSP globalResourcesInterface)
{
    KisRequiredResourcesOperators::createLocalResourcesSnapshot(this, globalResourcesInterface);
}

bool KisFilterConfiguration::hasLocalResourcesSnapshot() const
{
    return KisRequiredResourcesOperators::hasLocalResourcesSnapshot(this);
}

KisFilterConfigurationSP KisFilterConfiguration::cloneWithResourcesSnapshot(KisResourcesInterfaceSP globalResourcesInterface) const
{
    return KisRequiredResourcesOperators::cloneWithResourcesSnapshot<KisFilterConfigurationSP>(this, globalResourcesInterface);
}

PkList<KoResourceLoadResult> KisFilterConfiguration::requiredResources(KisResourcesInterfaceSP globalResourcesInterface) const
{
    PkList<KoResourceLoadResult> result = linkedResources(globalResourcesInterface);
    result += embeddedResources(globalResourcesInterface);
    return result;
}

PkList<KoResourceLoadResult> KisFilterConfiguration::linkedResources(KisResourcesInterfaceSP globalResourcesInterface) const
{
    Q_UNUSED(globalResourcesInterface);
    return {};
}

PkList<KoResourceLoadResult> KisFilterConfiguration::embeddedResources(KisResourcesInterfaceSP globalResourcesInterface) const
{
    Q_UNUSED(globalResourcesInterface);
    return {};
}

bool KisFilterConfiguration::isCompatible(const KisPaintDeviceSP) const
{
    return true;
}

bool KisFilterConfiguration::compareTo(const KisPropertiesConfiguration *rhs) const
{
    const KisFilterConfiguration *otherConfig = dynamic_cast<const KisFilterConfiguration *>(rhs);

    return otherConfig
            && KisPropertiesConfiguration::compareTo(rhs)
            && name() == otherConfig->name()
            && version() == otherConfig->version()
            && channelFlags() == otherConfig->channelFlags();
}

PkBitArray KisFilterConfiguration::channelFlags() const
{
    return d->channelFlags;
}

void KisFilterConfiguration::setChannelFlags(PkBitArray channelFlags)
{
    d->channelFlags = channelFlags;
}

#ifdef SANITY_CHECK_FILTER_CONFIGURATION_OWNER

int KisFilterConfiguration::sanityRefUsageCounter()
{
    return d->sanityUsageCounter.ref();
}

int KisFilterConfiguration::sanityDerefUsageCounter()
{
    return d->sanityUsageCounter.deref();
}

#endif /* SANITY_CHECK_FILTER_CONFIGURATION_OWNER */
