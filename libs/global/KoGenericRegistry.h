/* This file is part of the KDE project
 *  SPDX-FileCopyrightText: 2004 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2006 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _KO_GENERIC_REGISTRY_H_
#define _KO_GENERIC_REGISTRY_H_

#include <PkList.h>
#include <PkString.h>
#include <PkHash.h>
#include <PkStringHash.h>

template <typename T> using KoRegistryList = PkList<T>;
template <typename K, typename V> using KoRegistryHash = PkHash<K, V>;
using KoRegistryString = PkString;

#include "kis_assert.h"

/**
 * Base class for registry objects.
 *
 * Registered objects are owned by the registry.
 *
 * Items are mapped by the target's native string type as a unique Id.
 *
 * Example of use:
 * @code
 * class KoMyClassRegistry : public KoGenericRegistry<MyClass*> {
 * public:
 *   static KoMyClassRegistry * instance();
 * private:
 *  static KoMyClassRegistry* s_instance;
 * };
 *
 * KoMyClassRegistry *KoMyClassRegistry::s_instance = 0;
 * KoMyClassRegistry * KoMyClassRegistry::instance()
 * {
 *    if(s_instance == 0)
 *    {
 *      s_instance = new KoMyClassRegistry;
 *    }
 *    return s_instance;
 * }
 *
 * @endcode
 */
template<typename T>
class KoGenericRegistry
{
private:
    static KoRegistryString registryKey(const KoRegistryString &id)
    {
        return id;
    }

    // Some transitional factories still expose QString ids. Convert only at
    // that boundary; registry keys and containers remain Pk-owned.
    template <typename String>
    static auto registryKey(const String &id)
        -> decltype(id.toUtf8(), KoRegistryString())
    {
        const auto utf8 = id.toUtf8();
        return PkString::PkFromUtf8(utf8.constData(), utf8.size());
    }

public:
    KoGenericRegistry() { }
    virtual ~KoGenericRegistry()
    {
        m_doubleEntries.clear();
        m_hash.clear();
    }

public:
    /**
     * Add an object to the registry. If it is a PkObject, make sure it isn't in the
     * PkObject ownership hierarchy, since the registry itself is responsible for
     * deleting it.
     *
     * @param item the item to add (NOTE: T must have an PkString id() const   function)
     */
    void add(T item)
    {
        KIS_SAFE_ASSERT_RECOVER_RETURN(item);

        const KoRegistryString id = registryKey(item->id());
        KIS_SAFE_ASSERT_RECOVER_NOOP(!m_aliases.contains(id));

        if (m_hash.contains(id)) {
            m_doubleEntries << value(id);
            remove(id);
        }
        m_hash.insert(id, item);
    }

    /**
     * add an object to the registry
     * @param id the id of the object
     * @param item the item to add
     */
    void add(const KoRegistryString &id, T item)
    {
        KIS_SAFE_ASSERT_RECOVER_RETURN(item);
        KIS_SAFE_ASSERT_RECOVER_NOOP(!m_aliases.contains(id));

        if (m_hash.contains(id)) {
            m_doubleEntries << value(id);
            remove(id);
        }
        m_hash.insert(id, item);
    }

    /**
     * This function removes an item from the registry
     */
    void remove(const KoRegistryString &id)
    {
        m_hash.remove(id);
    }

    void addAlias(const KoRegistryString &alias, const KoRegistryString &id)
    {
        KIS_SAFE_ASSERT_RECOVER_NOOP(!m_hash.contains(alias));
        m_aliases[alias] = id;
    }

    void removeAlias(const KoRegistryString &alias)
    {
        m_aliases.remove(alias);
    }

    /**
     * Retrieve the object from the registry based on the unique
     * identifier string.
     *
     * @param id the id
     */
    T get(const KoRegistryString &id) const
    {
        return value(id);
    }

    /**
     * @return if there is an object stored in the registry identified
     * by the id.
     * @param id the unique identifier string
     */
    bool contains(const KoRegistryString &id) const
    {
        bool result = m_hash.contains(id);

        if (!result && m_aliases.contains(id)) {
            result = m_hash.contains(m_aliases.value(id));
        }

        return result;
    }

    /**
     * Retrieve the object from the registry based on the unique identifier string
     * @param id the id
     */
    const T value(const KoRegistryString &id) const
    {
        T result = m_hash.value(id);

        if (!result && m_aliases.contains(id)) {
            result = m_hash.value(m_aliases.value(id));
        }

        return result;
    }

    /**
     * @return a list of all keys
     */
    KoRegistryList<KoRegistryString> keys() const
    {
        return m_hash.keys();
    }

    int count() const
    {
        return m_hash.count();
    }

    KoRegistryList<T> values() const
    {
        return m_hash.values();
    }

    KoRegistryList<T> doubleEntries() const
    {
        return m_doubleEntries;
    }

    typename KoRegistryHash<KoRegistryString, T>::const_iterator constBegin() const {
        return m_hash.constBegin();
    }

    typename KoRegistryHash<KoRegistryString, T>::const_iterator constEnd() const {
        return m_hash.constEnd();
    }

private:

    KoRegistryList<T> m_doubleEntries;

private:

    KoRegistryHash<KoRegistryString, T> m_hash;
    KoRegistryHash<KoRegistryString, KoRegistryString> m_aliases;
};

#endif
