/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISRESOURCELOADER_H
#define KISRESOURCELOADER_H

#include <PkString.h>
#include <PkStringList.h>
#include <PkSharedPointer.h>
#include <PkStream.h>

#include <KoResource.h>

#include <kritaresources_export.h>


/**
 * @brief The KisResourceLoader class is an abstract interface
 * class that must be implemented by actual resource classes and
 * registered with the KisResourceLoaderRegistry.
 */
class KRITARESOURCES_EXPORT KisResourceLoaderBase
{
public:

    KisResourceLoaderBase(const PkString &resourceSubType, const PkString &resourceType, const PkString &name, const PkStringList &mimetypes)
    {
        m_resourceSubType = resourceSubType;
        m_resourceType = resourceType;
        m_mimetypes = mimetypes;
        m_name = name;
    }

    virtual ~KisResourceLoaderBase()
    {
    }

    /**
     * @return a set of filters ("*.bla,*.foo") that is suitable for filtering
     * the contents of a directory.
     */
    PkStringList filters() const;

    /**
     * @return the mimetypes this resource can load
     */
    PkStringList mimetypes() const
    {
        return m_mimetypes;
    }

    /**
     * @return the folder in the resource storage where resources
     * of this type are located
     */
    PkString resourceType() const
    {
        return m_resourceType;
    }

    PkString resourceSubType() const
    {
        return id();
    }

    /// For registration in KisResourceLoaderRegistry
    PkString id() const
    {
        return m_resourceSubType;
    }

    /// The user-friendly name of the category
    PkString name() const
    {
        return m_name;
    }

    virtual KoResourceSP create(const PkString &name) = 0;

    bool load(KoResourceSP resource, PkStream &dev, KisResourcesInterfaceSP resourcesInterface)
    {
        Q_ASSERT(dev.isOpen() && dev.isReadable());
        return resource->loadFromDevice(&dev, resourcesInterface);
    }

    /**
     * Load this resource.
     * @return a resource if loading the resource succeeded, 0 otherwise
     */
    KoResourceSP load(const PkString &name, PkStream &dev, KisResourcesInterfaceSP resourcesInterface)
    {
        KoResourceSP resource = create(name);
        return load(resource, dev, resourcesInterface) ? resource : 0;
    }


private:
    PkString m_resourceSubType;
    PkString m_resourceType;
    PkStringList m_mimetypes;
    PkString m_name;

};

template<typename T>
class KisResourceLoader : public KisResourceLoaderBase {
public:
    KisResourceLoader(const PkString &id, const PkString &folder, const PkString &name, const PkStringList &mimetypes)
        : KisResourceLoaderBase(id, folder, name, mimetypes)
    {
    }

    virtual KoResourceSP create(const PkString &name) override
    {
        PkSharedPointer<T> resource = PkSharedPointer<T>::create(name);
        return resource;
    }
};



#endif // KISRESOURCELOADER_H
