/*
 *  SPDX-FileCopyrightText: 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISREQUIREDRESOURCESOPERATORS_H
#define KISREQUIREDRESOURCESOPERATORS_H

#include "kritaresources_export.h"

#include <KisResourcesInterface.h>
#include <KoResourceLoadResult.h>
#include <PkList.h>
#include <PkSharedPointer.h>
#include <PkVector.h>
#include "kis_assert.h"

template<class T> class KisSharedPtr;
template<class T> class KisPinnedSharedPtr;

namespace KisRequiredResourcesOperators
{

namespace detail {
template <typename T>
struct SharedPointerTraits;

template <typename T>
struct SharedPointerTraits<PkSharedPointer<T>>
{
    using ValueType = T;

    template <typename D, typename S>
    static PkSharedPointer<D> dynamicCast(PkSharedPointer<S> source)
    {
        return source.template dynamicCast<D>();
    }
};

template <typename T>
struct SharedPointerTraits<KisSharedPtr<T>>
{
    using ValueType = T;

    template <typename D, typename S>
    static KisSharedPtr<D> dynamicCast(KisSharedPtr<S> source)
    {
        return KisSharedPtr<D>(dynamic_cast<D *>(source.data()));
    }
};

template <typename T>
struct SharedPointerTraits<KisPinnedSharedPtr<T>>
{
    using ValueType = T;

    template <typename D, typename S>
    static KisPinnedSharedPtr<D> dynamicCast(KisPinnedSharedPtr<S> source)
    {
        return KisPinnedSharedPtr<D>(dynamic_cast<D *>(source.data()));
    }
};

bool KRITARESOURCES_EXPORT isLocalResourcesStorage(KisResourcesInterfaceSP resourcesInterface);
bool KRITARESOURCES_EXPORT assertInGuiThread();
KisResourcesInterfaceSP KRITARESOURCES_EXPORT createLocalResourcesStorage(const PkList<KoResourceSP> &resources);
void KRITARESOURCES_EXPORT addResourceOrWarnIfNotLoaded(KoResourceLoadResult loadedResource, PkList<KoResourceSP> *resources, KisResourcesInterfaceSP resourcesInterface);
}


/**
 * @return true if the configuration has all the necessary resources in
 * local storage. It mean it can be used in a threaded environment.
 *
 * @see createLocalResourcesSnapshot()
 */
template <typename T>
bool hasLocalResourcesSnapshot(const T *object)
{
    return detail::isLocalResourcesStorage(object->resourcesInterface());
}

/**
 * Loads all the required resources either from the current resource interface
 * or from the embedded data. The object first tries to fetch the required
 * resource from the current source, and only if it fails, tries to load
 * it from the embedded data.
 *
 * @param globalResourcesInterface if \p globalResourcesInterface is not null,
 * the resources are fetched from there, not from the internally stored resources
 * interface
 */
template <typename T>
void createLocalResourcesSnapshot(T *object, KisResourcesInterfaceSP globalResourcesInterface = nullptr)
{
    if (!detail::assertInGuiThread()) {
        return;
    }

    KisResourcesInterfaceSP effectiveResourcesInterface =
        globalResourcesInterface ?
            globalResourcesInterface :
            object->resourcesInterface();

    PkVector<KoResourceLoadResult> loadedResources =
        object->requiredResources(effectiveResourcesInterface);

    PkList<KoResourceSP> resources;

    for (const KoResourceLoadResult &loadedResource : loadedResources) {
        detail::addResourceOrWarnIfNotLoaded(loadedResource, &resources, effectiveResourcesInterface);
    }

    object->setResourcesInterface(detail::createLocalResourcesStorage(resources));
}

/**
 * @brief creates an exact copy of the object and loads all the linked
 *        resources into the local storage.
 * @param globalResourcesInterface is an optional override for the
 *        resources interface used for fetching linked resources. If
 *        \p globalResourcesInterface is null, then object->resourcesInterface()
 *        is used.
 *
 * If a filter configuration object already has a resources snapshot, then
 * the function just clones the object without reloading anything.
 */
template <typename TypeSP, typename T = typename detail::SharedPointerTraits<TypeSP>::ValueType>
TypeSP cloneWithResourcesSnapshot(const T* object,
                                  KisResourcesInterfaceSP globalResourcesInterface = nullptr)
{
    auto clonedStorage = object->clone();
    TypeSP cloned = detail::SharedPointerTraits<TypeSP>::template dynamicCast<T>(clonedStorage);

    if (!hasLocalResourcesSnapshot(cloned.data())) {
        createLocalResourcesSnapshot(cloned.data(), globalResourcesInterface);
        KIS_SAFE_ASSERT_RECOVER_NOOP(hasLocalResourcesSnapshot(cloned.data()));
    }

    return cloned;
}

}

#endif // KISREQUIREDRESOURCESOPERATORS_H
