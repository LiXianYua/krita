/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "kis_open_raster_save_context.h"

#include <PkXmlDocument.h>

#include <KoStore.h>
#include <KoStoreDevice.h>
#include <KoColorSpaceRegistry.h>
#include <kundo2command.h>
#include <kis_paint_layer.h>
#include <kis_paint_device.h>
#include <kis_image.h>

#include <kis_meta_data_store.h>

#include <KisPngCodec.h>

KisOpenRasterSaveContext::KisOpenRasterSaveContext(KoStore* store)
    : m_id(0)
    , m_store(store)
{
}

PkString KisOpenRasterSaveContext::saveDeviceData(KisPaintDeviceSP dev, KisMetaData::Store* metaData, const PkRect &imageRect, const qreal xRes, const qreal yRes)
{
    PkString filename = PkString("data/layer%1.png").arg(m_id++);
    if (KisPngCodec::saveDeviceToStore(filename, imageRect, xRes, yRes, dev, m_store, metaData)) {
        return filename;
    }
    return "";
}


void KisOpenRasterSaveContext::saveStack(const PkXmlDocument& doc)
{
    if (m_store->open("stack.xml")) {
        KoStoreDevice io(m_store);
        const std::string xml = doc.toByteArray().PkToUtf8();
        long long written = 0;
        while (written < static_cast<long long>(xml.size())) {
            const long long count = io.write(xml.data() + written,
                                             static_cast<long long>(xml.size()) - written);
            if (count <= 0) {
                dbgFile << "Writing stack.xml failed";
                break;
            }
            written += count;
        }
        io.close();
        m_store->close();
    } else {
        dbgFile << "Opening of the stack.xml file failed :";
    }
}
