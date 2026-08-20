/*
 *  SPDX-FileCopyrightText: 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisGlobalResourcesInterface.h"

#include <KisResourceModel.h>
#include <KisResourceModelProvider.h>

#include <PkMutex.h>

#include "kis_assert.h"


namespace {
class GlobalResourcesSource : public KisResourcesInterface::ResourceSourceAdapter
{
public:
    GlobalResourcesSource(const PkString &resourceType, KisAllResourcesModel *model)
        : KisResourcesInterface::ResourceSourceAdapter(resourceType)
        , m_model(model)
    {}

    ~GlobalResourcesSource() override
    {
    }
protected:
    PkVector<KoResourceSP> resourcesForFilename(const PkString &filename) const override {
        PkVector<KoResourceSP> res = m_model->resourcesForFilename(filename);
        return res;
    }

    PkVector<KoResourceSP> resourcesForName(const PkString &name) const override {
        PkVector<KoResourceSP> res = m_model->resourcesForName(name);
        return res;
    }

    PkVector<KoResourceSP> resourcesForMD5(const PkString &md5) const override {
        PkVector<KoResourceSP> res = m_model->resourcesForMD5(md5);
        return res;
    }
public:
    KoResourceSP fallbackResource() const override {
        const PkVector<KisResourceRecord> records = m_model->records();
        return records.isEmpty() ? KoResourceSP()
                                 : m_model->resourceForId(records.first().id);
    }

private:
    KisAllResourcesModel *m_model;
};
}

KisResourcesInterfaceSP KisGlobalResourcesInterface::instance()
{
    static PkMutex mutex;
    static KisResourcesInterfaceSP d;
    PkMutexLocker locker(&mutex);
    if (!d) {
        d.reset(new KisGlobalResourcesInterface());
    }
    return d;
}

KisResourcesInterface::ResourceSourceAdapter *KisGlobalResourcesInterface::createSourceImpl(const PkString &type) const
{
    KisResourcesInterface::ResourceSourceAdapter *source =
        new GlobalResourcesSource(type, KisResourceModelProvider::resourceModel(type));

    KIS_ASSERT(source);
    return source;
}
