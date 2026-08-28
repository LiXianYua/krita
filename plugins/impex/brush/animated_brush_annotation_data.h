/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ANIMATED_BRUSH_ANNOTATION_DATA_H
#define ANIMATED_BRUSH_ANNOTATION_DATA_H

#include <PkAuxTypes.h>

#include <functional>

class PkStream;

PkByteArray captureAnimatedBrushAnnotation(const std::function<bool(PkStream *)> &serialize);

#endif
