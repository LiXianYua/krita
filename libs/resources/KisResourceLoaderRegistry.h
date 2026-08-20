/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISRESOURCELOADERREGISTRY_H
#define KISRESOURCELOADERREGISTRY_H

#include <PkObject.h>
#include <PkScopedPointer.h>
#include <PkStringList.h>
#include <PkVector.h>

#include <KoGenericRegistry.h>
#include "KisResourceLoader.h"

#include <kritaresources_export.h>

/**
 * @brief The KisResourceLoaderRegistry class manages the loader plugins for resources. Every resource can be loaded
 * by a KisResourceLoader instance. A loader corresponds to a particular file type. Resources are organized in
 * folders that represent the main type of a certain resource (brushes) and subtypes, that identify a particular
 * resource format (gbr, gih, png, svg).
 * 
 * KisResourceLoaderRegistry has full knowledge of all resource types that are defined for Krita.
 */
class KRITARESOURCES_EXPORT KisResourceLoaderRegistry : public PkObject, public KoGenericRegistry<KisResourceLoaderBase*>
{
public:
    ~KisResourceLoaderRegistry() override;

    static KisResourceLoaderRegistry *instance();

    /**
     * Destroy all registered loaders and fixups before their plugins unload.
     * The call is idempotent. A later instance() call creates an empty registry;
     * loaders and fixups must then be registered again.
     *
     * Application teardown must be serialized with all registry users.
     */
    static void shutdown();

    /**
     * Adds the given loader and registers its type in the database, if it hasn't been registered yet.
     */
    void registerLoader(KisResourceLoaderBase* loader);

    /// @return the first loader for the given resource type and mimetype
    KisResourceLoaderBase *loader(const PkString &resourceType, const PkString &mimetype) const;

    /**
     * @return a list of filename extensions that can be present for the given resource type
     */
    PkStringList filters(const PkString &resourceType) const;

    /**
     * @return a list of mimetypes that can be loaded for the given resource type
     */
    PkStringList mimeTypes(const PkString &resourceType) const;

    /**
     * @return the list of folders for which resource loaders have been registered
     */
    PkStringList resourceTypes() const;

    /**
     * @return a list of loader plugins that can handle the resources stored in the folder. A folder can contain multiple subtypes.
     */
    PkVector<KisResourceLoaderBase*> resourceTypeLoaders(const PkString &resourceType) const;

    /**
     * Sometimes the database needs updates without changing
     * the schema of the database. E.g. when we need to update
     * the resources' metadata. In such case, fix up should
     * be created.
     */
    struct ResourceCacheFixup {
        virtual ~ResourceCacheFixup() {};
        virtual PkStringList executeFix() = 0;
    };

    void registerFixup(int priority, ResourceCacheFixup *fixup);
    PkStringList executeAllFixups();

private:

    KisResourceLoaderRegistry();
    KisResourceLoaderRegistry(const KisResourceLoaderRegistry&) = delete;
    KisResourceLoaderRegistry &operator=(const KisResourceLoaderRegistry&) = delete;
private:

    struct Private;
    PkScopedPointer<Private> m_d;
};

#endif // KISRESOURCELOADERREGISTRY_H
