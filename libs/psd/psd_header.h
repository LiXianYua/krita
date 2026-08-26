/*
 *  SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PSD_HEADER_H
#define PSD_HEADER_H

#include "kritapsd_export.h"

#include <cstdint>
#include <kis_debug.h>
#include <psd.h>

class PkStream;

class KRITAPSD_EXPORT PSDHeader
{
public:
    PSDHeader();

    /**
     * Reads a psd header from the given device.
     *
     * @return false if:
     *   <li>reading failed
     *   <li>if the 8BPS signature is not found
     *   <li>if the version is not 1 or 2
     */
    bool read(PkStream &device);

    /**
     * write the header data to the given device
     *
     * @return false if writing failed or if this is not a valid header
     */
    bool write(PkStream &device);

    bool valid();

    PkString signature; // 8PBS
    std::uint16_t version; // 1 or 2
    std::uint16_t nChannels; // 1 - 56
    std::uint32_t height; // 1-30,000 or 1 - 300,000
    std::uint32_t width; // 1-30,000 or 1 - 300,000
    std::uint16_t channelDepth; // 1, 8, 16. XXX: check whether 32 is used!
    psd_color_mode colormode;
    psd_byte_order byteOrder;
    bool tiffStyleLayerBlock; // if true, treat layer section as 4-byte aligned blocks

    PkString error;
};

KRITAPSD_EXPORT PkDebug operator<<(PkDebug dbg, const PSDHeader &header);

#endif // PSD_HEADER_H
