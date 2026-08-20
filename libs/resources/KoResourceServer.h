/* This file is part of the KDE project

   SPDX-FileCopyrightText: 1999 Matthias Elter <elter@kde.org>
   SPDX-FileCopyrightText: 2003 Patrick Julien <freak@codepimps.org>
   SPDX-FileCopyrightText: 2005 Sven Langkamp <sven.langkamp@gmail.com>
   SPDX-FileCopyrightText: 2007 Jan Hambrecht <jaham@gmx.net>
   SPDX-FileCopyrightText: 2011 Srikanth Tiyyagura <srikanth.tulasiram@gmail.com>
   SPDX-FileCopyrightText: 2013 Sascha Suelzer <s.suelzer@gmail.com>
   SPDX-FileCopyrightText: 2003-2019 Boudewijn Rempt <boud@valdyas.org>

   SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef KORESOURCESERVER_H
#define KORESOURCESERVER_H

#include <PkList.h>
#include <PkSharedPointer.h>
#include <PkString.h>
#include <PkVector.h>

#include "KoResource.h"

#include <KisGlobalResourcesInterface.h>
#include <KisResourceLocator.h>
#include <KisResourceModel.h>
#include <KisTagModel.h>
#include <ResourceDebug.h>

#include <filesystem>

template <class T>
class KoResourceServerObserver
{
public:
    virtual ~KoResourceServerObserver() = default;

    virtual void unsetResourceServer() = 0;
    virtual void resourceAdded(PkSharedPointer<T> resource) = 0;
    virtual void removingResource(PkSharedPointer<T> resource) = 0;
    virtual void resourceChanged(PkSharedPointer<T> resource) = 0;
};

/** Compatibility facade around the ordinary resource and tag data models. */
template <class T>
class KoResourceServer
{
public:
    using ObserverType = KoResourceServerObserver<T>;

    explicit KoResourceServer(const PkString &type)
        : m_resourceModel(new KisResourceModel(type))
        , m_tagModel(new KisTagModel(type))
        , m_type(type)
    {
    }

    virtual ~KoResourceServer()
    {
        delete m_resourceModel;
        delete m_tagModel;
        for (ObserverType *observer : m_observers) {
            observer->unsetResourceServer();
        }
    }

    KisResourceModel *resourceModel() const
    {
        return m_resourceModel;
    }

    PkSharedPointer<T> firstResource() const
    {
        const PkVector<KoResourceSP> resources = m_resourceModel->resources();
        return resources.isEmpty()
            ? PkSharedPointer<T>()
            : resources.first().template dynamicCast<T>();
    }

    int resourceCount() const
    {
        return m_resourceModel->records().size();
    }

    bool addResource(PkSharedPointer<T> resource, bool save = true)
    {
        if (!resource || !resource->valid()) {
            warnResource << "Tried to add an invalid resource";
            return false;
        }
        if (m_resourceModel->addResource(
                resource, save ? PkString() : PkString("memory"))) {
            notifyResourceAdded(resource);
            return true;
        }
        return false;
    }

    bool removeResourceFromServer(PkSharedPointer<T> resource)
    {
        if (m_resourceModel->setResourceInactive(resource)) {
            notifyRemovingResource(resource);
            return true;
        }
        return false;
    }

    PkString saveLocation() const
    {
        return KisResourceLocator::instance()->resourceLocationBase() + m_type;
    }

    KoResourceSP importResourceFile(const PkString &filename,
                                    bool allowOverwrite)
    {
        return m_resourceModel->importResourceFile(filename, allowOverwrite);
    }

    void removeResourceFile(const PkString &filename)
    {
        const PkString baseName(
            std::filesystem::path(filename.PkToUtf8()).filename().string().c_str());
        PkSharedPointer<T> resource = resourceByFilename(baseName);
        if (!resource) {
            warnResource << "Resource file does not exist" << filename;
            return;
        }
        removeResourceFromServer(resource);
    }

    void addObserver(ObserverType *observer)
    {
        if (observer && !m_observers.contains(observer)) {
            m_observers.append(observer);
        }
    }

    void removeObserver(ObserverType *observer)
    {
        const int index = m_observers.indexOf(observer);
        if (index >= 0) {
            m_observers.removeAt(index);
        }
    }

    PkSharedPointer<T> resource(const PkString &md5,
                                const PkString &fileName,
                                const PkString &name)
    {
        return KisGlobalResourcesInterface::instance()
            ->source<T>(m_type).bestMatch(md5, fileName, name);
    }

    bool updateResource(PkSharedPointer<T> resource)
    {
        const bool result = m_resourceModel->updateResource(resource);
        notifyResourceChanged(resource);
        return result;
    }

    bool reloadResource(PkSharedPointer<T> resource)
    {
        const bool result = m_resourceModel->reloadResource(resource);
        notifyResourceChanged(resource);
        return result;
    }

    PkVector<KisTagSP> assignedTagsList(KoResourceSP resource) const
    {
        return resource
            ? m_resourceModel->tagsForResource(resource->resourceId())
            : PkVector<KisTagSP>();
    }

private:
    PkSharedPointer<T> resourceByFilename(const PkString &filename) const
    {
        if (filename.isEmpty()) {
            return PkSharedPointer<T>();
        }
        const PkVector<KoResourceSP> resources =
            m_resourceModel->resourcesForFilename(filename);
        return resources.isEmpty()
            ? PkSharedPointer<T>()
            : resources.first().template dynamicCast<T>();
    }

    PkSharedPointer<T> resourceByName(const PkString &name) const
    {
        if (name.isEmpty()) {
            return PkSharedPointer<T>();
        }
        const PkVector<KoResourceSP> resources =
            m_resourceModel->resourcesForName(name);
        return resources.isEmpty()
            ? PkSharedPointer<T>()
            : resources.first().template dynamicCast<T>();
    }

    PkSharedPointer<T> resourceByMD5(const PkString &md5) const
    {
        if (md5.isEmpty()) {
            return PkSharedPointer<T>();
        }
        const PkVector<KoResourceSP> resources =
            m_resourceModel->resourcesForMD5(md5);
        return resources.isEmpty()
            ? PkSharedPointer<T>()
            : resources.first().template dynamicCast<T>();
    }

    void notifyResourceAdded(PkSharedPointer<T> resource)
    {
        for (ObserverType *observer : m_observers) {
            observer->resourceAdded(resource);
        }
    }

    void notifyRemovingResource(PkSharedPointer<T> resource)
    {
        for (ObserverType *observer : m_observers) {
            observer->removingResource(resource);
        }
    }

    void notifyResourceChanged(PkSharedPointer<T> resource)
    {
        for (ObserverType *observer : m_observers) {
            observer->resourceChanged(resource);
        }
    }

    PkList<ObserverType *> m_observers;
    KisResourceModel *m_resourceModel = nullptr;
    KisTagModel *m_tagModel = nullptr;
    PkString m_type;
};

#endif // KORESOURCESERVER_H
