/*
 * SPDX-FileCopyrightText: 2019 Agata Cacko <cacko.azh@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_RESOURCE_SEARCH_BOX_FILTER_H
#define KIS_RESOURCE_SEARCH_BOX_FILTER_H

#include <PkString.h>
#include <PkStringList.h>

#include "kritaresources_export.h"

/** Parses the resource search syntax and matches names plus tag names. */
class KRITARESOURCES_EXPORT KisResourceSearchBoxFilter
{
public:
    KisResourceSearchBoxFilter();
    ~KisResourceSearchBoxFilter();

    void setFilter(const PkString &filter);
    bool matchesResource(const PkString &resourceName,
                         const PkStringList &tagList) const;
    bool isEmpty() const;

private:
    void initializeFilterData();
    void clearFilterData();

    class Private;
    Private *const d;
};

#endif // KIS_RESOURCE_SEARCH_BOX_FILTER_H
