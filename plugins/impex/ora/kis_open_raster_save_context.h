/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_OPEN_RASTER_SAVE_CONTEXT_H_
#define _KIS_OPEN_RASTER_SAVE_CONTEXT_H_

#include <kis_types.h>
#include <PkRect.h>

class PkXmlDocument;
class KoStore;

#include <kis_meta_data_entry.h>

class KisOpenRasterSaveContext
{
public:
    KisOpenRasterSaveContext(KoStore *store);
    PkString saveDeviceData(KisPaintDeviceSP dev, KisMetaData::Store *metaData, const PkRect &imageRect, qreal xRes, qreal yRes);
    bool saveStack(const PkXmlDocument& doc);
private:
    int m_id;
    KoStore *m_store;

};


#endif
