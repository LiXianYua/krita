/*
 * SPDX-FileCopyrightText: 2019 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KoQuaZipStore_h
#define KoQuaZipStore_h

#include "KoStore.h"
#include <memory>

class PkStream;
class PkByteArray;
class PkString;

class KoQuaZipStore : public KoStore
{
public:
    KoQuaZipStore(const PkString & _filename, Mode _mode, const PkByteArray & appIdentification,
                  bool writeMimetype = true);

    KoQuaZipStore(PkStream *dev, Mode mode, const PkByteArray & appIdentification,
                  bool writeMimetype = true);

    ~KoQuaZipStore() override;

    void setCompressionEnabled(bool enabled) override;
    PkStream::pk_int64 write(const char* _data, PkStream::pk_int64 _len) override;

    PkStringList directoryList() const override;

protected:
    void init(const PkByteArray& appIdentification);
    bool doFinalize() override;
    bool openWrite(const PkString& name) override;
    bool openRead(const PkString& name) override;
    bool closeWrite() override;
    bool closeRead() override;
    bool enterRelativeDirectory(const PkString& dirName) override;
    bool enterAbsoluteDirectory(const PkString& path) override;
    bool fileExists(const PkString& absPath) const override;

private:
    struct Private;
    const std::unique_ptr<Private> dd;

};

#endif
