/*
 *  SPDX-FileCopyrightText: 2015 Thorsten Zachmann <zachmann@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "filter/kis_color_transformation_configuration.h"

#include <map>

#include <PkMutex.h>
#include <PkThread.h>
#include "filter/kis_color_transformation_filter.h"

struct KisColorTransformationConfiguration::Private {
    Private()
    {}

    ~Private()
    {
        destroyCache();
    }

    void destroyCache()
    {
        PkMutexLocker locker(&mutex);
        for (auto &pair : colorTransformation) {
            delete pair.second;
        }
        colorTransformation.clear();
    }

    // XXX: Threadlocal storage!!!
    std::map<PkThreadId, KoColorTransformation*> colorTransformation;
    PkMutex mutex;
};

KisColorTransformationConfiguration::KisColorTransformationConfiguration(const PkString & name, qint32 version, KisResourcesInterfaceSP resourcesInterface)
    : KisFilterConfiguration(name, version, resourcesInterface)
    , d(new Private())
{
}

KisColorTransformationConfiguration::KisColorTransformationConfiguration(const KisColorTransformationConfiguration &rhs)
    : KisFilterConfiguration(rhs)
    , d(new Private())
{
}

KisColorTransformationConfiguration::~KisColorTransformationConfiguration()
{
    delete d;
}

KisFilterConfigurationSP KisColorTransformationConfiguration::clone() const
{
    return new KisColorTransformationConfiguration(*this);
}

/**
 * Invalidate the cache by default when setProperty is called. This forces
 * regenerating the color transforms also when a property of this object
 * changes, not only when the object is copied
 */
void KisColorTransformationConfiguration::setProperty(const PkString &name, const PkVariant &value)
{
    KisFilterConfiguration::setProperty(name, value);
    invalidateColorTransformationCache();
}

KoColorTransformation* KisColorTransformationConfiguration::colorTransformation(const KoColorSpace *cs, const KisColorTransformationFilter *filter) const
{
    PkMutexLocker locker(&d->mutex);
    const PkThreadId threadId = PkThread::currentThreadId();
    auto it = d->colorTransformation.find(threadId);
    KoColorTransformation *transformation = (it != d->colorTransformation.end()) ? it->second : 0;
    if (!transformation) {
        KisFilterConfigurationSP config(clone().data());
        transformation = filter->createTransformation(cs, config);
        d->colorTransformation.emplace(threadId, transformation);
    }
    locker.unlock();
    return transformation;
}

void KisColorTransformationConfiguration::invalidateColorTransformationCache()
{
    d->destroyCache();
}
