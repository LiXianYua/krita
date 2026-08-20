/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef KISRESOURCEQUERYMAPPER_H
#define KISRESOURCEQUERYMAPPER_H

#include <PkImage.h>

#include "KisResourceModel.h"

class PkSqlQuery;

class KisResourceQueryMapper
{
public:
    /** Map the current database row to the ordinary resource record. */
    static KisResourceRecord resourceFromQuery(const PkSqlQuery &query,
                                               bool useResourcePrefix);

    /** Load and cache the thumbnail belonging to the current resource row. */
    static PkImage thumbnailFromResourceQuery(const PkSqlQuery &query,
                                              bool useResourcePrefix);
};

#endif // KISRESOURCEQUERYMAPPER_H
