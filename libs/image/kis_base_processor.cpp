/*
 *  SPDX-FileCopyrightText: 2004, 2006-2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_base_processor.h"

#include <PkString.h>

#include "kis_bookmarked_configuration_manager.h"
#include "filter/kis_filter_configuration.h"
#include "kis_paint_device.h"
#include "kis_selection.h"
#include <KisGlobalResourcesInterface.h>


class KisBaseProcessorConfigurationFactory : public KisSerializableConfigurationFactory
{
public:
    KisBaseProcessorConfigurationFactory(KisBaseProcessor* _generator) : m_generator(_generator) {}
    ~KisBaseProcessorConfigurationFactory() override {}

    KisSerializableConfigurationSP createDefault() override {
        return m_generator->defaultConfiguration(KisGlobalResourcesInterface::instance());
    }
    KisSerializableConfigurationSP create(const PkXmlElement& e) override {
        KisSerializableConfigurationSP config = m_generator->factoryConfiguration(KisGlobalResourcesInterface::instance());
        config->fromXML(e);
        return config;
    }
private:
    KisBaseProcessor* m_generator;
};

struct Q_DECL_HIDDEN KisBaseProcessor::Private {
    Private()
            : bookmarkManager(0)
            , supportsPainting(false)
            , supportsAdjustmentLayers(true)
            , supportsThreading(true)
            , showConfigurationWidget(true)
            , colorSpaceIndependence(FULLY_INDEPENDENT) {
    }

    KisBookmarkedConfigurationManager* bookmarkManager;

    KoID id;
    KoID category; // The category in the filter menu this filter fits
    PkString entry; // the translated accelerated menu text
    bool supportsPainting;
    bool supportsAdjustmentLayers;
    bool supportsThreading;
    bool showConfigurationWidget;
    ColorSpaceIndependence colorSpaceIndependence;
};

KisBaseProcessor::KisBaseProcessor(const KoID& id, const KoID & category, const PkString & entry)
        : d(new Private)
{
    d->id = id;
    d->category = category;
    d->entry = entry;
}

void KisBaseProcessor::init(const PkString& configEntryGroup)
{
    d->bookmarkManager = new KisBookmarkedConfigurationManager(configEntryGroup, new KisBaseProcessorConfigurationFactory(this));
}

KisBaseProcessor::~KisBaseProcessor()
{
    delete d->bookmarkManager;
    delete d;
}

KisFilterConfigurationSP KisBaseProcessor::factoryConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    return new KisFilterConfiguration(id(), 1, resourcesInterface);
}

KisFilterConfigurationSP KisBaseProcessor::defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    return factoryConfiguration(resourcesInterface);
}

KisBookmarkedConfigurationManager* KisBaseProcessor::bookmarkManager()
{
    return d->bookmarkManager;
}

const KisBookmarkedConfigurationManager* KisBaseProcessor::bookmarkManager() const
{
    return d->bookmarkManager;
}

PkString KisBaseProcessor::id() const
{
    return d->id.id();
}

PkString KisBaseProcessor::name() const
{
    return d->id.name();
}

KoID KisBaseProcessor::menuCategory() const
{
    return d->category;
}

PkString KisBaseProcessor::menuEntry() const
{
    return d->entry;
}

bool KisBaseProcessor::supportsPainting() const
{
    return d->supportsPainting;
}

bool KisBaseProcessor::supportsAdjustmentLayers() const
{
    return d->supportsAdjustmentLayers;
}

bool KisBaseProcessor::supportsThreading() const
{
    return d->supportsThreading;
}

ColorSpaceIndependence KisBaseProcessor::colorSpaceIndependence() const
{
    return d->colorSpaceIndependence;
}

void KisBaseProcessor::setSupportsPainting(bool v)
{
    d->supportsPainting = v;
}

void KisBaseProcessor::setSupportsAdjustmentLayers(bool v)
{
    d->supportsAdjustmentLayers = v;
}

void KisBaseProcessor::setSupportsThreading(bool v)
{
    d->supportsThreading = v;
}

void KisBaseProcessor::setColorSpaceIndependence(ColorSpaceIndependence v)
{
    d->colorSpaceIndependence = v;
}

bool KisBaseProcessor::showConfigurationWidget()
{
    return d->showConfigurationWidget;
}

void KisBaseProcessor::setShowConfigurationWidget(bool v)
{
    d->showConfigurationWidget = v;
}
