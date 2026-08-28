/*
 * SPDX-FileCopyrightText: 2009 Shawn T. Rutledge (shawn.t.rutledge@gmail.com)
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef GIFLIBCODEC_H
#define GIFLIBCODEC_H

#include <PkImage.h>
#include <PkStream.h>

class GifLibCodec
{
public:
    explicit GifLibCodec(PkStream *device);
    bool canRead() const;
    bool read(PkImage *image);
    bool write(const PkImage &image);
    static bool canRead(PkStream *device);

private:
    PkStream *m_device = nullptr;
};

#endif // GIFLIBCODEC_H
