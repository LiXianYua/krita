/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "kis_open_raster_save_context.h"
#include "ora_save_policy.h"

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


bool KisOpenRasterSaveContext::saveStack(const PkXmlDocument& doc)
{
    if (!m_store->open("stack.xml")) {
        dbgFile << "Opening of the stack.xml file failed :";
        return false;
    }

    KoStoreDevice io(m_store);
    const std::string xml = doc.toByteArray().PkToUtf8();
    const bool writeSucceeded = oraWriteAll(
        [&io](const char *data, long long size) { return io.write(data, size); },
        xml.data(), xml.size());
    io.close();
    const bool closeSucceeded = m_store->close();
    if (!writeSucceeded) {
        dbgFile << "Writing stack.xml failed";
    }
    if (!closeSucceeded) {
        dbgFile << "Closing stack.xml failed";
    }
    return writeSucceeded && closeSucceeded;
}
