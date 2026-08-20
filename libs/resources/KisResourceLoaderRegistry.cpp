/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisResourceLoaderRegistry.h"

#include <KisResourceCacheDb.h>
#include <KisMimeDatabase.h>
#include <PkMap.h>

#include <mutex>

namespace
{
struct RegistrySingletonSlot
{
    std::mutex mutex;
    KisResourceLoaderRegistry *instance = nullptr;
};

RegistrySingletonSlot &registrySingletonSlot()
{
    static RegistrySingletonSlot *slot = new RegistrySingletonSlot;
    return *slot;
}
} // namespace

struct KisResourceLoaderRegistry::Private
{
    PkMap<int, ResourceCacheFixup*> fixups;
};

KisResourceLoaderRegistry::KisResourceLoaderRegistry()
    : PkObject(nullptr)
    , m_d(new Private)
{
}

KisResourceLoaderRegistry::~KisResourceLoaderRegistry()
{
    for (KisResourceLoaderBase *loader : values()) {
        delete loader;
    }
    for (ResourceCacheFixup *fixup : m_d->fixups) {
        delete fixup;
    }
}

KisResourceLoaderRegistry* KisResourceLoaderRegistry::instance()
{
    RegistrySingletonSlot &slot = registrySingletonSlot();
    std::lock_guard<std::mutex> lock(slot.mutex);
    if (!slot.instance) {
        slot.instance = new KisResourceLoaderRegistry;
    }
    return slot.instance;
}

void KisResourceLoaderRegistry::shutdown()
{
    RegistrySingletonSlot &slot = registrySingletonSlot();
    KisResourceLoaderRegistry *oldInstance = nullptr;
    {
        std::lock_guard<std::mutex> lock(slot.mutex);
        oldInstance = slot.instance;
        slot.instance = nullptr;
    }
    delete oldInstance;
}

void KisResourceLoaderRegistry::registerLoader(KisResourceLoaderBase *loader)
{
    add(loader);
}

KisResourceLoaderBase *KisResourceLoaderRegistry::loader(const PkString &resourceType, const PkString &mimetype) const
{
    for (KisResourceLoaderBase *loader : resourceTypeLoaders(resourceType)) {

        if (loader->mimetypes().contains(mimetype)) {
            return loader;
        }
    }
    return 0;
}

PkVector<KisResourceLoaderBase *> KisResourceLoaderRegistry::resourceTypeLoaders(const PkString &resourceType) const
{
    PkVector<KisResourceLoaderBase *> r;
    for (KisResourceLoaderBase *loader : values()) {
        if (loader->resourceType() == resourceType) {
            r << loader;
        }
    }
    return r;
}

void KisResourceLoaderRegistry::registerFixup(int priority, ResourceCacheFixup *fixup)
{
    m_d->fixups.insert(priority, fixup);
}

PkStringList KisResourceLoaderRegistry::executeAllFixups()
{
    PkStringList errorMessages;

    for (ResourceCacheFixup *fixup : m_d->fixups) {
        errorMessages.append(fixup->executeFix());
    }

    return errorMessages;
}

PkStringList KisResourceLoaderRegistry::filters(const PkString &resourceType) const
{
    PkStringList r;
    for (KisResourceLoaderBase *loader : resourceTypeLoaders(resourceType)) {
        r.append(loader->filters());
    }
    r.removeDuplicates();
    r.sort();
    return r;
}

PkStringList KisResourceLoaderRegistry::mimeTypes(const PkString &resourceType) const
{
    PkStringList extensions = KisResourceLoaderRegistry::instance()->filters(resourceType);
    PkStringList mimeTypes;
    for (const PkString &extension : extensions) {
        const PkString suffix = extension.startsWith(PkString("*.")) ? extension.mid(2) : extension;
        mimeTypes.append(KisMimeDatabase::mimeTypeForSuffix(suffix));
    }
    mimeTypes.removeDuplicates();
    mimeTypes.sort();

    return mimeTypes;
}



PkStringList KisResourceLoaderRegistry::resourceTypes() const
{
    PkStringList r;
    for (KisResourceLoaderBase *loader : values()) {
        r.append(loader->resourceType());
    }
    r.removeDuplicates();
    r.sort();

    return r;
}
