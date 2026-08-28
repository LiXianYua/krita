/*
 *  SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _KIS_XMP_IO_H_
#define _KIS_XMP_IO_H_

#include <PkString.h>

#include <kis_meta_data_io_backend.h>

class KisXMPIO : public KisMetaData::IOBackend
{
public:
    KisXMPIO();
    ~KisXMPIO() override;
    PkString id() const override
    {
        return "xmp";
    }
    PkString name() const override
    {
        return PkString("XMP");
    }
    BackendType type() const override
    {
        return Text;
    }
    bool supportSaving() const override
    {
        return true;
    }
    bool saveTo(const KisMetaData::Store *store, PkStream *ioDevice, HeaderType headerType = NoHeader) const override;
    bool canSaveAllEntries(KisMetaData::Store *) const override
    {
        return true;
    }
    bool supportLoading() const override
    {
        return true;
    }
    bool loadFrom(KisMetaData::Store *store, PkStream *ioDevice) const override;
};

#endif
