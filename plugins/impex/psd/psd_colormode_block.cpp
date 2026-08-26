/*
 *  SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "psd_colormode_block.h"

#include <psd_utils.h>
#include <PkColor.h>

PSDColorModeBlock::PSDColorModeBlock(psd_color_mode colormode)
    : blocksize(0)
    , colormode(colormode)
{
}

bool PSDColorModeBlock::read(PkStream &io)
{
    // get length
    psdread(io, blocksize);

    if (blocksize == 0) {
        if (colormode == Indexed || colormode == DuoTone) {
            error = "Blocksize of 0 and Indexed or DuoTone colormode";
            return false;
        }
        else {
            return true;
        }
    }

    if (colormode == Indexed && blocksize != 768) {
        error = PkString("Indexed mode, but block size is %1.").arg(static_cast<int>(blocksize));
        return false;
    }

    data.resize(blocksize);
    const auto bytesRead = io.read(data.data(), blocksize);
    if (bytesRead != blocksize) return false;

    if (colormode == Indexed) {
        int i = 0;
        while (i <= 767) {
            colormap.append(PkColor::fromRgb(static_cast<quint8>(data.constData()[i]),
                                             static_cast<quint8>(data.constData()[i + 1]),
                                             static_cast<quint8>(data.constData()[i + 2])));
            i += 3;
        }
    }
    else {
        duotoneSpecification = data;
    }
    return valid();
}

bool PSDColorModeBlock::write(PkStream &io)
{
    if (!valid()) {
        error = "Cannot write an invalid Color Mode Block";
        return false;
    }
    if (colormap.size() > 0 && colormode == Indexed) {
        error = "Cannot write indexed color data";
        return false;
    }
    else if (duotoneSpecification.size() > 0 && colormode == DuoTone) {
        quint32 blocksize = duotoneSpecification.size();
        psdwrite(io, blocksize);
        if (io.write(duotoneSpecification.constData(), blocksize) != blocksize) {
            error = "Failed to write duotone specification";
            return false;
        }
    }
    else {
        quint32 i = 0;
        psdwrite(io, i);
    }
    return true;
}

bool PSDColorModeBlock::valid()
{
    if (blocksize == 0 && (colormode == Indexed || colormode == DuoTone)) {
        error = "Blocksize of 0 and Indexed or DuoTone colormode";
        return false;
    }
    if (colormode == Indexed && blocksize != 768) {
        error = PkString("Indexed mode, but block size is %1.").arg(static_cast<int>(blocksize));
        return false;
    }
    if (colormode == DuoTone && blocksize == 0) {
        error = PkString("DuoTone mode, but data block is empty");
        return false;
    }
    if ((quint32)data.size() != blocksize) {
        error = PkString("Data size is %1, but block size is %2").arg(data.size()).arg(static_cast<int>(blocksize));
        return false;
    }
    return true;
}
