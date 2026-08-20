/*
 *  SPDX-FileCopyrightText: 2021 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KORESOURCELOADRESULT_H
#define KORESOURCELOADRESULT_H

#include <PkSharedPointer.h>
#include <PkScopedPointer.h>
#include <PkDebug.h>
#include <KoResourceSignature.h>
#include <KoEmbeddedResource.h>

class KoResource;
typedef PkSharedPointer<KoResource> KoResourceSP;

class KRITARESOURCES_EXPORT KoResourceLoadResult
{
public:
    enum Type {
        ExistingResource,
        EmbeddedResource,
        FailedLink
    };
public:
    KoResourceLoadResult(KoResourceSP resource);
    KoResourceLoadResult(KoEmbeddedResource embeddedRresource);
    KoResourceLoadResult(KoResourceSignature signature);

    template <typename T, typename = typename std::is_convertible<T*, KoResource*>::type>
    KoResourceLoadResult(PkSharedPointer<T> resource)
        : KoResourceLoadResult(KoResourceSP(resource))
    {
    }

    KoResourceLoadResult(const KoResourceLoadResult &rhs);
    KoResourceLoadResult& operator=(const KoResourceLoadResult &rhs);

    ~KoResourceLoadResult();

    /**
     * Returns existing resource that has been loaded from the Krita
     * database.
     *
     * Returns non-null pointer only when `type()` is equal to
     * `ExistingResource`
     */
    KoResourceSP resource() const noexcept;

    /**
     * Same as resource(), but returns a resource that is dynamically
     * cast to the destination type T
     */
    template <typename T>
    PkSharedPointer<T> resource() const {
        return this->resource().dynamicCast<T>();
    }

    /**
     * Returns the embedded resource, for which there was no instance
     * has been found in the resource database. This resource should
     * be imported into the database manually.
     *
     * Returns a valid object only when `type()` is equal to
     * `EmbeddedResource`
     */
    KoEmbeddedResource embeddedResource() const noexcept;

    /**
     * Return a signature for the embedded/linked resource. This is
     * the only information available when `type()` is equal to
     * `FailedLink`
     */
    KoResourceSignature signature() const;

    /**
     * Describes the result of the resource loading. A copy of the resource
     * can be either found in the resource database, it can be loaded from
     * some embedded storage (and yet should be imported into the database
     * manually) or it can just fail to be found (e.g. when the resource
     * is not embedded and still not found in the database).
     */
    Type type() const;

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

KRITARESOURCES_EXPORT PkDebug operator<<(PkDebug debug, const KoResourceLoadResult &result);

#endif // KORESOURCELOADRESULT_H
