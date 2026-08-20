/*
 * SPDX-FileCopyrightText: 2015 Stefano Bonicatti <smjert@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/
#include "KoMD5Generator.h"

#include "MD5.h"

#include <PkFileStream.h>

PkString KoMD5Generator::generateHash(const PkByteArray &array)
{
    if (array.isEmpty()) {
        return PkString();
    }

    MD5 md5;
    md5.addData(array.data(), static_cast<std::size_t>(array.size()));
    return PkString(md5.toHex().c_str());
}

PkString KoMD5Generator::generateHash(const PkString &filename)
{
    PkFileStream f(filename);
    if (f.open(PkStream::ReadOnly)) {
        MD5 md5;
        char buffer[8192];
        PkStream::pk_int64 n = 0;
        while ((n = f.read(buffer, sizeof(buffer))) > 0) {
            md5.addData(buffer, static_cast<std::size_t>(n));
        }
        return PkString(md5.toHex().c_str());
    }

    return PkString();
}

PkString KoMD5Generator::generateHash(PkStream *device)
{
    if (!device) {
        return PkString();
    }

    MD5 md5;
    char buffer[8192];
    PkStream::pk_int64 n = 0;
    while ((n = device->read(buffer, sizeof(buffer))) > 0) {
        md5.addData(buffer, static_cast<std::size_t>(n));
    }
    return PkString(md5.toHex().c_str());
}
