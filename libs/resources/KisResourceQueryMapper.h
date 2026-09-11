/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef KISRESOURCEQUERYMAPPER_H
#define KISRESOURCEQUERYMAPPER_H

#include <PkImage.h>
#include <PkMap.h>
#include <PkString.h>
#include <PkVector.h>

#include "KisResourceModel.h"

class PkSqlQuery;

class KisResourceQueryMapper
{
public:
    /**
     * Everything a thumbnail lookup needs about one row: the absolute
     * location of its storage, the resource type, the file name and the
     * resource id. Collect these while the result cursor still sits on the
     * row -- a cursor cannot be re-read once it has moved on -- so that a
     * whole refresh batch can be served in one go.
     */
    struct ThumbnailRequest {
        PkString storageLocation;
        PkString resourceType;
        PkString filename;
        int resourceId = -1;
    };

    /** Map the current database row to the ordinary resource record. */
    static KisResourceRecord resourceFromQuery(const PkSqlQuery &query,
                                               bool useResourcePrefix);

    /** Load and cache the thumbnail belonging to the current resource row. */
    static PkImage thumbnailFromResourceQuery(const PkSqlQuery &query,
                                              bool useResourcePrefix);

    /**
     * Fetch and cache the thumbnails of a whole refresh batch.
     *
     * Rows that KisResourceThumbnailCache already holds are served from
     * memory; the remaining ids come back from a single `IN (...)` query, so
     * one refresh costs a constant number of statements instead of one per
     * row. Returns the resource id -> image map for the requested rows;
     * resources without a thumbnail are left out.
     */
    static PkMap<int, PkImage> thumbnailsForRequests(
        const PkVector<ThumbnailRequest> &requests);
};

#endif // KISRESOURCEQUERYMAPPER_H
