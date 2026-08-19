/* This file is part of the KDE project
 *
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _KO_GENERIC_REGISTRY_MODEL_H_
#define _KO_GENERIC_REGISTRY_MODEL_H_

#include "KoGenericRegistry.h"

/**
 * This is a model that you can use to display the content of a registry.
 *
 * @param T is the type of the data in the registry
 */
template<typename T>
class KoGenericRegistryModel : public PkAbstractListModel
{

public:

    KoGenericRegistryModel(KoGenericRegistry<T>* registry);

    ~KoGenericRegistryModel() override;

public:

    /**
     * @return the number of elements in the registry
     */
    int rowCount(const PkModelIndex &parent = PkModelIndex()) const override;

    /**
     * When role == Pk::DisplayRole, this function will return the name of the
     * element.
     */
    PkVariant data(const PkModelIndex &index, int role = Pk::DisplayRole) const override;

    /**
     * @return the element at the given index
     */
    T get(const PkModelIndex &index) const;

private:

    KoGenericRegistry<T>* m_registry;
};

// -- Implementation --
template<typename T>
KoGenericRegistryModel<T>::KoGenericRegistryModel(KoGenericRegistry<T>* registry) : m_registry(registry)
{
}

template<typename T>
KoGenericRegistryModel<T>::~KoGenericRegistryModel()
{
}

template<typename T>
int KoGenericRegistryModel<T>::rowCount(const PkModelIndex &/*parent*/) const
{
    return m_registry->keys().size();
}

template<typename T>
PkVariant KoGenericRegistryModel<T>::data(const PkModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return PkVariant();
    }
    if (role == Pk::DisplayRole || role == Pk::EditRole) {
        return PkVariant(get(index)->name());
    }
    return PkVariant();
}

template<typename T>
T KoGenericRegistryModel<T>::get(const PkModelIndex &index) const
{
    return m_registry->get(m_registry->keys()[index.row()]);
}

#endif
