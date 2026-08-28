/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _KIS_EXIF_IO_H_
#define _KIS_EXIF_IO_H_

#include <PkString.h>

#include <kis_meta_data_io_backend.h>

class KisExifIO : public KisMetaData::IOBackend
{
public:
    KisExifIO();
    ~KisExifIO() override;
    PkString id() const override
    {
        return "exif";
    }
    PkString name() const override
    {
        return PkString("Exif");
    }
    BackendType type() const override
    {
        return Binary;
    }
    bool supportSaving() const override
    {
        return true;
    }
    bool saveTo(const KisMetaData::Store *store, PkStream *ioDevice, HeaderType headerType = NoHeader) const override;
    bool canSaveAllEntries(KisMetaData::Store *store) const override;
    bool supportLoading() const override
    {
        return true;
    }
    bool loadFrom(KisMetaData::Store *store, PkStream *ioDevice) const override;
};

#endif
