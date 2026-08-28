/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2015 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisAnimatedBrushAnnotation.h"

#include <PkAuxTypes.h>
#include <PkMemoryStream.h>

#include <kis_pipebrush_parasite.h>

KisAnimatedBrushAnnotation::KisAnimatedBrushAnnotation(const KisPipeBrushParasite &parasite)
    : KisAnnotation(PkString("ImagePipe Parasite"),
                    PkString("Brush selection information for animated brushes"),
                    PkByteArray())
{
    PkMemoryStream buffer;
    if (!buffer.open(PkStream::WriteOnly)) {
        return;
    }
    if (parasite.saveToDevice(&buffer)) {
        m_annotation = PkByteArray(buffer.data(), static_cast<int>(buffer.size()));
    }
}
