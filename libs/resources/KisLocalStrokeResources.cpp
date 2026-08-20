/*
 *  SPDX-FileCopyrightText: 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisLocalStrokeResources.h"
#include "KisResourcesInterface_p.h"

#include "kis_assert.h"
#include "ResourceDebug.h"

namespace {
class LocalResourcesSource : public KisResourcesInterface::ResourceSourceAdapter
{
public:
    LocalResourcesSource(const PkString &resourceType, const PkList<KoResourceSP> &cachedResources)
        : KisResourcesInterface::ResourceSourceAdapter(resourceType)
        , m_resourceType(resourceType)
        , m_cachedResources(cachedResources)
    {
    }
protected:
    PkVector<KoResourceSP> resourcesForFilename(const PkString &filename) const override {
        PkVector<KoResourceSP> resources;
        PK_FOREACH(KoResourceSP res, m_cachedResources) {
            if (res->filename() == filename && res->resourceType().first == m_resourceType) {
                resources.append(res);
            }
        }
        return resources;
    }

    PkVector<KoResourceSP> resourcesForName(const PkString &name) const override {
        PkVector<KoResourceSP> resources;
        PK_FOREACH(KoResourceSP res, m_cachedResources) {
            if (res->name() == name && res->resourceType().first == m_resourceType) {
                resources.append(res);
            }
        }
        return resources;
    }

    PkVector<KoResourceSP> resourcesForMD5(const PkString &md5) const override {
        PkVector<KoResourceSP> resources;
        PK_FOREACH(KoResourceSP res, m_cachedResources) {
            if (res->md5Sum() == md5 && res->resourceType().first == m_resourceType) {
                resources.append(res);
            }
        }
        return resources;
    }

public:

    KoResourceSP fallbackResource() const override {
        auto it = std::find_if(m_cachedResources.begin(),
                               m_cachedResources.end(),
                               [this] (KoResourceSP res) {
                return res->resourceType().first == this->m_resourceType;
    });
        return it != m_cachedResources.end() ? *it : KoResourceSP();
    }

private:
    const PkString m_resourceType;
    const PkList<KoResourceSP> &m_cachedResources;
};
}

class KisLocalStrokeResourcesPrivate : public KisResourcesInterfacePrivate
{
public:
    KisLocalStrokeResourcesPrivate(const PkList<KoResourceSP> &_localResources)
        : localResources(_localResources)
    {

        // sanity check that we don't have any null resources
        KIS_SAFE_ASSERT_RECOVER(!localResources.contains(KoResourceSP())) {
            localResources.removeAll(KoResourceSP());
        }


    }
    PkList<KoResourceSP> localResources;
};


KisLocalStrokeResources::KisLocalStrokeResources()
    : KisResourcesInterface(new KisLocalStrokeResourcesPrivate({}))
{
}

KisLocalStrokeResources::KisLocalStrokeResources(const PkList<KoResourceSP> &localResources)
    : KisResourcesInterface(new KisLocalStrokeResourcesPrivate(localResources))
{
}

void KisLocalStrokeResources::addResource(KoResourceSP resource)
{
    KisLocalStrokeResourcesPrivate *const d = d_func();
    KIS_SAFE_ASSERT_RECOVER(resource)
    {
        warnResource << "Attempted to insert a null resource into the local style resource server";
        return;
    }
    d->localResources.append(resource);
}

void KisLocalStrokeResources::removeResource(KoResourceSP resource)
{
    KisLocalStrokeResourcesPrivate *const d = d_func();
    d->localResources.removeAll(resource);
}

KisLocalStrokeResources *KisLocalStrokeResources::clone() const
{
    const KisLocalStrokeResourcesPrivate *const d = d_func();
    return new KisLocalStrokeResources(d->localResources);
}

KisResourcesInterface::ResourceSourceAdapter *KisLocalStrokeResources::createSourceImpl(const PkString &type) const
{
    const KisLocalStrokeResourcesPrivate *const d = d_func();
    return new LocalResourcesSource(type, d->localResources);
}

PkList<KoResourceSP> KisLocalStrokeResources::resources() const
{
    const KisLocalStrokeResourcesPrivate *const d = d_func();
    return d->localResources;
}
