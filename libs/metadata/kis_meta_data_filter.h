/*
 *  SPDX-FileCopyrightText: 2007-2008 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _KIS_META_DATA_FILTER_H_
#define _KIS_META_DATA_FILTER_H_

#include <kritametadata_export.h>
class PkString;

namespace KisMetaData
{
class Store;
/**
 * This class is a base class for filtering a meta data store to alter some
 * information. For instance, remove author information or change edition
 * date.
 */
class Filter
{
public:
    virtual ~Filter();
    /// @return true if the filter is enabled by default when exporting
    virtual bool defaultEnabled() const = 0;
    /// @return the id of this filter
    virtual PkString id() const = 0;
    /// @return the name of this filter
    virtual PkString name() const = 0;
    /// @return a description of this filter
    virtual PkString description() const = 0;
    /**
     * Apply a filter on a meta data store.
     */
    virtual void filter(KisMetaData::Store*) const = 0;
};
}

#endif
