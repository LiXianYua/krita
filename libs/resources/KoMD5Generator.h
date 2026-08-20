/*
 * SPDX-FileCopyrightText: 2015 Stefano Bonicatti <smjert@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/
#ifndef KOMD5GENERATOR_H
#define KOMD5GENERATOR_H

#include <PkString.h>
#include <PkAuxTypes.h>

#include <kritaresources_export.h>

class PkStream;

class KRITARESOURCES_EXPORT KoMD5Generator
{
public:
    /**
     * @brief generateHash reads the given file and generates
     * a hex-encoded md5sum for the file.
     * @param filename the file to open
     * @return a hex-encoded string representation of the md5sum
     */
    static PkString generateHash(const PkString &filename);

    /**
     * @brief generateHash calculates the md5sum of the given bytes
     * @param PkByteArray the contents to be calculated
     * @return a hex-encoded string representation of the md5sum
     */
    static PkString generateHash(const PkByteArray &array);

    /**
     * @brief generateHash calculates the md5sum of the given device
     * @param PkStream
     * @return a hex-encoded string representation of the md5sum
     */
    static PkString generateHash(PkStream *device);
};

#endif
