/*
 *  SPDX-FileCopyrightText: 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISRESOURCESINTERFACE_H
#define KISRESOURCESINTERFACE_H

#include "kritaresources_export.h"

#include <PkString.h>
#include <PkSharedPointer.h>
#include <PkVector.h>
#include <PkContainerAlgo.h>
#include <KoResource.h>
#include <KoResourceLoadResult.h>

class KisResourcesInterfacePrivate;

/**
 * @brief a provider-like interface class for accessing resource sources in Krita.
 *
 * Main differences to KoResourceServer and KisResourceModel:
 *
 *  1) It is a polymorphic class. Therefore, we are not obliged to pass
       a pointer to the global gui-only resource storage everywhere. Instead,
       we can create temporary storages and pass them to the strokes, when needed.

    2) The class doesn't depend on any specific resource types. Its baseline
       implementation of resourceInterface->source(type) returns a source
       working with KoResourceSP only. But when needed, the caller may request
       a typed version via resourcesInterface->source<KisBrush>(type). It
       will instantiate a templated wrapper **in the caller's** object file,
       not in kritaresources library. It solves linking problem:
       we have a source for KisBrush objects in kritaresources library,
       even though this library doesn't link kritabrush.

    3) Since strokes may have local storages for the resources, we operate
       with resources sources using shared pointers, not raw pointers.
 */
class KRITARESOURCES_EXPORT KisResourcesInterface
{
public:
    class KRITARESOURCES_EXPORT ResourceSourceAdapter
    {
    public:
        ResourceSourceAdapter(const PkString &type);
        virtual ~ResourceSourceAdapter();
//protected:
        friend class KisResourcesInterface;
        virtual PkVector<KoResourceSP> resourcesForFilename(const PkString& filename) const = 0;
        virtual PkVector<KoResourceSP> resourcesForName(const PkString& name) const = 0;
        virtual PkVector<KoResourceSP> resourcesForMD5(const PkString& md5) const = 0;
public:
        /**
         * @brief bestMatch retrieves a resource, preferably by md5, but with filename and name
         * as fallback for older files that do not store the md5sum. If the resource is
         * not found by md5 and the md5 isn't empty, then it will try to fallback to searching
         * by filename, but will show a warning in case sanity checks are enabled.
         *
         * If multiple resources with the same md5 exist, then it prefers the one
         * with the same filename and name.
         *
         * @return a resource, or 0 of the resource doesn't exist.
         */

        KoResourceSP bestMatch(const PkString md5, const PkString filename, const PkString name);

        /**
         * @brief exactMatch retrieves a resource, preferably by md5, but with filename and name
         * as fallback for older files that do not store the md5sum. If the resource is
         * not found by md5 and the md5 isn't empty, then nullptr is returned (that is the
         * difference to bestMatch).
         *
         * If multiple resources with the same md5 exist, then it prefers the one
         * with the same filename and name.
         *
         * @return a resource, or 0 of the resource doesn't exist.
         */
        KoResourceSP exactMatch(const PkString md5, const PkString filename, const PkString name);

        /**
         * Same as bestMatch(), but returns KoResourceLoadResult. In case the
         * resource is not found in the backend storage, the load-result
         * will be set in FailedLink state
         *
         */
        KoResourceLoadResult bestMatchLoadResult(const PkString md5, const PkString filename, const PkString name);

        virtual KoResourceSP fallbackResource() const = 0;

    private:
        KoResourceSP findResource(const PkString md5, const PkString filename, const PkString name, bool exactMatch);
    private:
        ResourceSourceAdapter(const ResourceSourceAdapter &) = delete;
        ResourceSourceAdapter &operator=(const ResourceSourceAdapter &) = delete;
        const PkString m_type;
    };

    template <typename T>
    class TypedResourceSourceAdapter
    {
    public:
        TypedResourceSourceAdapter(ResourceSourceAdapter *adapter)
            : m_source(adapter)
        {
        }
private:
        PkVector<PkSharedPointer<T>> resourcesForFilename(const PkString& filename) const
        {
            PkVector<PkSharedPointer<T>> r;
            PK_FOREACH(KoResourceSP resource, m_source->resourcesForFilename(filename)) {
                r.append(resource.dynamicCast<T>());
            }
            return r;
        }

        PkVector<PkSharedPointer<T>> resourcesForName(const PkString& name) const
        {
            PkVector<PkSharedPointer<T>> r;
            PK_FOREACH(KoResourceSP resource, m_source->resourcesForName(name)) {
                r.append(resource.dynamicCast<T>());
            }
            return r;
        }

        PkVector<PkSharedPointer<T>> resourcesForMD5(const PkString& md5) const
        {
            PkVector<PkSharedPointer<T>> r;
            PK_FOREACH(KoResourceSP resource, m_source->resourcesForMD5(md5)) {
                r.append(resource.dynamicCast<T>());
            }
            return r;
        }
public:
        /**
         * @brief bestMatch retrieves a resource, preferably by md5, but with filename and name
         * as fallback for older files that do not store the md5sum. If the resource is
         * not found by md5 and the md5 isn't empty, then it will try to fallback to searching
         * by filename, but will show a warning in case sanity checks are enabled.
         *
         * If multiple resources with the same md5 exist, then it prefers the one
         * with the same filename and name.
         *
         * @return a resource, or 0 of the resource doesn't exist.
         */

        PkSharedPointer<T>  bestMatch(const PkString md5, const PkString filename, const PkString name) {
            return m_source->bestMatch(md5, filename, name).dynamicCast<T>();
        }

        /**
         * @brief exactMatch retrieves a resource, preferably by md5, but with filename and name
         * as fallback for older files that do not store the md5sum. If the resource is
         * not found by md5 and the md5 isn't empty, then nullptr is returned (that is the
         * difference to bestMatch).
         *
         * If multiple resources with the same md5 exist, then it prefers the one
         * with the same filename and name.
         *
         * @return a resource, or 0 of the resource doesn't exist.
         */
        PkSharedPointer<T>  exactMatch(const PkString md5, const PkString filename, const PkString name) {
            return m_source->exactMatch(md5, filename, name).dynamicCast<T>();
        }

        /**
         * Same as bestMatch(), but returns KoResourceLoadResult. In case the
         * resource is not found in the backend storage, the load-result
         * will be set in FailedLink state
         *
         */
        KoResourceLoadResult bestMatchLoadResult(const PkString md5, const PkString filename, const PkString name) {
            return m_source->bestMatchLoadResult(md5, filename, name);
        }

        PkSharedPointer<T> fallbackResource() const
        {
            return m_source->fallbackResource().dynamicCast<T>();
        }

    protected:
        ResourceSourceAdapter *m_source;
    };

public:
    KisResourcesInterface();
    virtual ~KisResourcesInterface();

    /**
     * A basic implementation that returns a source for a specific type
     * of the resource. Please take into account that this source object will
     * return un-casted resources of type KoResourceSP. If you want to have a
     * proper resource (in most of the cases), use a `server<T>(type)` instead.
     */
    ResourceSourceAdapter& source(const PkString &type) const;

    /**
     * The main fetcher of resource source for resources of a specific type.
     *
     * Usage:
     *
     * \code{.cpp}
     *
     * auto source = resourceInterface->source<KisBrush>(ResourceType::Brushes);
     * KisBrushSP brush = source.resourceByMd5(md5)
     *
     * \endcode
     *
     */
    template<typename T>
    TypedResourceSourceAdapter<T> source(const PkString &type) const {
        return TypedResourceSourceAdapter<T>(&this->source(type));
    }

protected:
    KisResourcesInterface(KisResourcesInterfacePrivate *dd);
    virtual ResourceSourceAdapter* createSourceImpl(const PkString &type) const = 0;

protected:
    KisResourcesInterfacePrivate *d_ptr;

private:
    KisResourcesInterfacePrivate *d_func() const { return d_ptr; }
    friend class KisResourcesInterfacePrivate;
};

using KisResourcesInterfaceSP = PkSharedPointer<KisResourcesInterface>;

#endif // KISRESOURCESINTERFACE_H
