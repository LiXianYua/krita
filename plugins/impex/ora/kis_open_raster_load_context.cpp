/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "kis_open_raster_load_context.h"

#include <PkXmlDocument.h>

#include <KoStore.h>
#include <KoStoreDevice.h>

#include <kis_image.h>
#include <kis_paint_device.h>
#include <KisPngCodec.h>

KisOpenRasterLoadContext::KisOpenRasterLoadContext(KoStore* _store)
    : m_store(_store)
{
}

KisImageSP KisOpenRasterLoadContext::loadDeviceData(const PkString & filename)
{
    if (m_store->open(filename)) {
        KoStoreDevice io(m_store);
        if (!io.open(PkStream::ReadOnly)) {
            dbgFile << "Could not open for reading:" << filename;
            return 0;
        }
        KisPngCodec pngConv;
        pngConv.buildImage(&io);
        io.close();
        m_store->close();

        return pngConv.image();

    }
    return 0;
}

PkXmlDocument KisOpenRasterLoadContext::loadStack()
{
    m_store->open("stack.xml");
    KoStoreDevice io(m_store);
    PkXmlDocument doc;
    doc.setContent(&io);
    io.close();
    m_store->close();
    return doc;
}
