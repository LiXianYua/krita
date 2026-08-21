/*
 *  SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "kis_meta_data_filter_registry_model.h"
#include "kis_debug.h"

#include <PkStringHash.h>
#include <vector>

using namespace KisMetaData;

// 用 std::vector<bool> 而非 PkList<bool>：PkArrayContainer 的 operator[] 返回
// T&，对 std::vector<bool> 的位代理（reference）绑不上 bool&，编不过。
struct FilterRegistryModel::Private {
    std::vector<bool> enabled;
};

FilterRegistryModel::FilterRegistryModel()
    : d(new Private)
{
    PkList<PkString> keys = FilterRegistry::instance()->keys();
    for (int i = 0; i < keys.size(); i++) {
        d->enabled.push_back(FilterRegistry::instance()->get(keys[i])->defaultEnabled());
    }
}

FilterRegistryModel::~FilterRegistryModel()
{
    delete d;
}

PkList<const Filter*> FilterRegistryModel::enabledFilters() const
{
    PkList<const Filter*> enabledFilters;
    PkList<PkString> keys = FilterRegistry::instance()->keys();
    for (int i = 0; i < keys.size(); i++) {
        if (d->enabled[i]) {
            enabledFilters.append(FilterRegistry::instance()->get(keys[i]));
        }
    }
    return enabledFilters;
}

void FilterRegistryModel::setEnabledFilters(const PkStringList &enabledFilters)
{
    d->enabled.clear();
    PkList<PkString> keys = FilterRegistry::instance()->keys();
    for (const PkString &key : keys) {
        d->enabled.push_back(enabledFilters.contains(key));
    }
}
