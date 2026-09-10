/*
   SPDX-FileCopyrightText: 2006, 2011 Boudewijn Rempt (boud@valdyas.org)
   SPDX-FileCopyrightText: 2007, 2010 Thomas Zander <zander@kde.org>
   SPDX-FileCopyrightText: 2008 Carlos Licea <carlos.licea@kdemail.net>
   SPDX-FileCopyrightText: 2011 Jan Hambrecht <jaham@gmx.net>

   SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "KoResourceManager_p.h"

#include <PkVariant.h>
#include <FlakeDebug.h>

#include "KoShape.h"
#include "kis_assert.h"
#include "kis_debug.h"

namespace {

// QMultiHash 的相等键迭代序是**插入序的逆序**（`constFind` 指向最近插入的那条），
// 而 std::multimap 的 `equal_range` 给的是插入序（C++11 起等价键保持插入序）。
// 故凡「按相等键遍历」都走这里，免得每处各自记住要倒着走。
template <typename Map, typename Key, typename Fn>
void forEachEqualKeyReverse(const Map &map, const Key &key, Fn fn)
{
    auto it = map.upper_bound(key);
    const auto begin = map.lower_bound(key);
    while (it != begin) {
        --it;
        fn(*it);
    }
}

} // namespace

void KoResourceManager::slotResourceInternalsChanged(int key)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_resources.contains(key) || m_abstractResources.contains(key));
    notifyDerivedResourcesChanged(key, m_resources[key]);
    notifyDependenciesAboutTargetChange(key, m_resources[key]);
}

void KoResourceManager::slotAbstractResourceChangedExternal(int key, const PkVariant &value)
{
    notifyResourceChanged(key, value);
}

void KoResourceManager::setResource(int key, const PkVariant &value)
{
    notifyResourceChangeAttempted(key, value);

    KoDerivedResourceConverterSP converter =
        m_derivedResources.value(key, KoDerivedResourceConverterSP());

    KoAbstractCanvasResourceInterfaceSP abstractResource =
            m_abstractResources.value(key, KoAbstractCanvasResourceInterfaceSP());

    if (abstractResource) {
        const PkVariant oldValue = abstractResource->value();
        abstractResource->setValue(value);

        if (m_updateMediators.contains(key)) {
            m_updateMediators[key]->connectResource(value);
        }

        if (oldValue != value) {
            notifyResourceChanged(key, value);
        }
    } else if (converter) {
        const int sourceKey = converter->sourceKey();
        const PkVariant oldSourceValue = m_resources.value(sourceKey, PkVariant());

        bool valueChanged = false;
        const PkVariant newSourceValue = converter->writeToSource(value, oldSourceValue, &valueChanged);

        if (valueChanged) {
            notifyResourceChanged(key, value);
        }

        if (oldSourceValue != newSourceValue) {
            m_resources[sourceKey] = newSourceValue;
            notifyResourceChanged(sourceKey, newSourceValue);
        }
    } else if (m_resources.contains(key)) {
        const PkVariant oldValue = m_resources.value(key, PkVariant());
        m_resources[key] = value;

        if (m_updateMediators.contains(key)) {
            m_updateMediators[key]->connectResource(value);
        }

        if (oldValue != value) {
            notifyResourceChanged(key, value);
        }
    } else {
        m_resources[key] = value;
        if (m_updateMediators.contains(key)) {
            m_updateMediators[key]->connectResource(value);
        }
        notifyResourceChanged(key, value);
    }
}

void KoResourceManager::notifyResourceChanged(int key, const PkVariant &value)
{
    resourceChanged(key, value);
    notifyDerivedResourcesChanged(key, value);
    notifyDependenciesAboutTargetChange(key, value);
}

void KoResourceManager::notifyDerivedResourcesChanged(int key, const PkVariant &value)
{
    forEachEqualKeyReverse(m_derivedFromSource, key, [&](const auto &entry) {
        KoDerivedResourceConverterSP converter = entry.second;

        if (converter->notifySourceChanged(value)) {
            notifyResourceChanged(converter->key(), converter->readFromSource(value));
        }
    });
}

void KoResourceManager::notifyResourceChangeAttempted(int key, const PkVariant &value)
{
    resourceChangeAttempted(key, value);
    notifyDerivedResourcesChangeAttempted(key, value);
}

void KoResourceManager::notifyDerivedResourcesChangeAttempted(int key, const PkVariant &value)
{
    forEachEqualKeyReverse(m_derivedFromSource, key, [&](const auto &entry) {
        KoDerivedResourceConverterSP converter = entry.second;
        notifyResourceChangeAttempted(converter->key(), converter->readFromSource(value));
    });
}

void KoResourceManager::notifyDependenciesAboutTargetChange(int targetKey, const PkVariant &targetValue)
{
    forEachEqualKeyReverse(m_dependencyFromTarget, targetKey, [&](const auto &entry) {
        KoActiveCanvasResourceDependencySP dep = entry.second;
        const int sourceKey = dep->sourceKey();

        if (hasResource(sourceKey)) {
            PkVariant sourceValue = resource(sourceKey);

            notifyResourceChangeAttempted(sourceKey, sourceValue);
            if (dep->shouldUpdateSource(sourceValue, targetValue)) {
                notifyResourceChanged(sourceKey, sourceValue);
            }
        }
    });
}

PkVariant KoResourceManager::resource(int key) const
{
    KoAbstractCanvasResourceInterfaceSP abstractResource =
            m_abstractResources.value(key, KoAbstractCanvasResourceInterfaceSP());
    if (abstractResource) {
        return abstractResource->value();
    }

    KoDerivedResourceConverterSP converter =
        m_derivedResources.value(key, KoDerivedResourceConverterSP());

    const int realKey = converter ? converter->sourceKey() : key;
    PkVariant value = m_resources.value(realKey, PkVariant());

    return converter ? converter->readFromSource(value) : value;
}

void KoResourceManager::setResource(int key, const KoColor &color)
{
    PkVariant v;
    v.setValue(color);
    setResource(key, v);
}

void KoResourceManager::setResource(int key, KoShape *shape)
{
    PkVariant v;
    v.setValue(shape);
    setResource(key, v);
}

void KoResourceManager::setResource(int key, const KoUnit &unit)
{
    PkVariant v;
    v.setValue(unit);
    setResource(key, v);
}

KoColor KoResourceManager::koColorResource(int key) const
{
    if (! m_resources.contains(key)) {
        KoColor empty;
        return empty;
    }
    return resource(key).value<KoColor>();
}

KoShape *KoResourceManager::koShapeResource(int key) const
{
    if (! m_resources.contains(key))
        return 0;

    return resource(key).value<KoShape *>();
}


KoUnit KoResourceManager::unitResource(int key) const
{
    return resource(key).value<KoUnit>();
}

bool KoResourceManager::boolResource(int key) const
{
    if (! m_resources.contains(key))
        return false;
    return m_resources[key].toBool();
}

int KoResourceManager::intResource(int key) const
{
    if (! m_resources.contains(key))
        return 0;
    return m_resources[key].toInt();
}

PkString KoResourceManager::stringResource(int key) const
{
    if (! m_resources.contains(key)) {
        PkString empty;
        return empty;
    }
    return resource(key).toString();
}

PkSizeF KoResourceManager::sizeResource(int key) const
{
    if (! m_resources.contains(key)) {
        PkSizeF empty;
        return empty;
    }
    return resource(key).value<PkSizeF>();
}

bool KoResourceManager::hasResource(int key) const
{
    if (m_abstractResources.contains(key)) return true;

    KoDerivedResourceConverterSP converter =
        m_derivedResources.value(key, KoDerivedResourceConverterSP());

    const int realKey = converter ? converter->sourceKey() : key;
    return m_resources.contains(realKey);
}

void KoResourceManager::clearResource(int key)
{
    // we cannot remove a derived resource
    if (m_derivedResources.contains(key)) return;

    // we cannot remove an abstract resource either
    if (m_abstractResources.contains(key)) return;

    if (m_resources.contains(key)) {
        m_resources.remove(key);
        notifyResourceChanged(key, PkVariant());
    }
}

void KoResourceManager::addDerivedResourceConverter(KoDerivedResourceConverterSP converter)
{
    KIS_SAFE_ASSERT_RECOVER_NOOP(!m_derivedResources.contains(converter->key()));

    if (hasAbstractResource(converter->key()))
        warnFlake << "An abstract resource with the same resource ID exists!";

    m_derivedResources.insert(converter->key(), converter);
    m_derivedFromSource.emplace(converter->sourceKey(), converter);
}

bool KoResourceManager::hasDerivedResourceConverter(int key)
{
    return m_derivedResources.contains(key);
}

void KoResourceManager::removeDerivedResourceConverter(int key)
{
    KIS_ASSERT(m_derivedResources.contains(key));

    KoDerivedResourceConverterSP converter = m_derivedResources.value(key);
    m_derivedResources.remove(key);
    const auto range = m_derivedFromSource.equal_range(converter->sourceKey());
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second == converter) {
            m_derivedFromSource.erase(it);
            break;
        }
    }
}

void KoResourceManager::addResourceUpdateMediator(KoResourceUpdateMediatorSP mediator)
{
    KIS_SAFE_ASSERT_RECOVER_NOOP(!m_updateMediators.contains(mediator->key()));
    m_updateMediators.insert(mediator->key(), mediator);
    PkObject::connect(mediator.data(), &KoResourceUpdateMediator::sigResourceChanged, this, &KoResourceManager::slotResourceInternalsChanged);
}

bool KoResourceManager::hasResourceUpdateMediator(int key)
{
    return m_updateMediators.contains(key);
}

void KoResourceManager::removeResourceUpdateMediator(int key)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_updateMediators.contains(key));
    m_updateMediators.remove(key);
}

void KoResourceManager::addActiveCanvasResourceDependency(KoActiveCanvasResourceDependencySP dep)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(!hasActiveCanvasResourceDependency(dep->sourceKey(), dep->targetKey()));

    m_dependencyFromSource.emplace(dep->sourceKey(), dep);
    m_dependencyFromTarget.emplace(dep->targetKey(), dep);
}

bool KoResourceManager::hasActiveCanvasResourceDependency(int sourceKey, int targetKey) const
{
    const auto range = m_dependencyFromSource.equal_range(sourceKey);
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second->targetKey() == targetKey) {
            return true;
        }
    }

    return false;
}

void KoResourceManager::removeActiveCanvasResourceDependency(int sourceKey, int targetKey)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(hasActiveCanvasResourceDependency(sourceKey, targetKey));

    {
        const auto range = m_dependencyFromSource.equal_range(sourceKey);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second->targetKey() == targetKey) {
                it = m_dependencyFromSource.erase(it);
                break;
            }
        }
    }

    {
        const auto range = m_dependencyFromTarget.equal_range(targetKey);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second->sourceKey() == sourceKey) {
                it = m_dependencyFromTarget.erase(it);
                break;
            }
        }
    }
}

bool KoResourceManager::hasAbstractResource(int key)
{
    return m_abstractResources.contains(key);
}

void KoResourceManager::removeAbstractResource(int key)
{
    KIS_ASSERT(hasAbstractResource(key));

    KoAbstractCanvasResourceInterfaceSP resourceInterface = m_abstractResources.value(key);
    PkObject::disconnect(resourceInterface.data(), &KoAbstractCanvasResourceInterface::sigResourceChangedExternal,
               this, &KoResourceManager::slotAbstractResourceChangedExternal);
    m_abstractResources.remove(key);
}

void KoResourceManager::setAbstractResource(KoAbstractCanvasResourceInterfaceSP resourceInterface)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(resourceInterface);

    if (hasDerivedResourceConverter(resourceInterface->key()))
        warnFlake << "A derived resource converter with the same resource ID exists!";

    const PkVariant oldValue = this->resource(resourceInterface->key());

    KoAbstractCanvasResourceInterfaceSP oldResourceInterface =
        m_abstractResources.value(resourceInterface->key());
    if (oldResourceInterface) {
        PkObject::disconnect(oldResourceInterface.data(), &KoAbstractCanvasResourceInterface::sigResourceChangedExternal,
                   this, &KoResourceManager::slotAbstractResourceChangedExternal);
    }

    m_abstractResources[resourceInterface->key()] = resourceInterface;

    PkObject::connect(resourceInterface.data(), &KoAbstractCanvasResourceInterface::sigResourceChangedExternal,
            this, &KoResourceManager::slotAbstractResourceChangedExternal);

    if (oldValue != resourceInterface->value()) {
        notifyResourceChanged(resourceInterface->key(), resourceInterface->value());
    }
}

void KoResourceManager::resourceChanged(int key, const PkVariant &value)
{
    activateSignal<int, const PkVariant &>(
        this, PkMemberFnKey::from(&KoResourceManager::resourceChanged), key, value);
}

void KoResourceManager::resourceChangeAttempted(int key, const PkVariant &value)
{
    activateSignal<int, const PkVariant &>(
        this, PkMemberFnKey::from(&KoResourceManager::resourceChangeAttempted), key, value);
}
