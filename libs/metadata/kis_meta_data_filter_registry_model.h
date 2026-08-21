/*
 *  SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _KIS_META_DATA_FILTER_REGISTRY_MODEL_H_
#define _KIS_META_DATA_FILTER_REGISTRY_MODEL_H_

#include "kis_meta_data_filter_registry.h"

#include <PkList.h>
#include <PkString.h>
#include <PkStringList.h>

#include <kritametadata_export.h>

namespace KisMetaData
{

/**
 * 过滤器的启用/禁用选择器。
 * 原实现继承 KoGenericRegistryModel（Qt model），UI 层已删；现保留
 * setEnabledFilters/enabledFilters 两个真实消费者用到的 API。
 */
class KRITAMETADATA_EXPORT FilterRegistryModel
{
public:
    FilterRegistryModel();
    ~FilterRegistryModel();
public:
    /// @return a list of filters that are enabled
    PkList<const Filter*> enabledFilters() const;
    /// enable the filters in the given list; others will be disabled.
    void setEnabledFilters(const PkStringList &enabledFilters);
private:
    struct Private;
    Private* const d;
};

}

#endif
