/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2015 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisAnimatedBrushAnnotation.h"
#include "animated_brush_annotation_data.h"

#include <kis_pipebrush_parasite.h>

KisAnimatedBrushAnnotation::KisAnimatedBrushAnnotation(const KisPipeBrushParasite &parasite)
    : KisAnnotation(PkString("ImagePipe Parasite"),
                    PkString("Brush selection information for animated brushes"),
                    PkByteArray())
{
    m_annotation = captureAnimatedBrushAnnotation(
        [&parasite](PkStream *stream) { return parasite.saveToDevice(stream); });
}
