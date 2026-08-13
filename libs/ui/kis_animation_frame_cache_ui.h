/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_ANIMATION_FRAME_CACHE_UI_H
#define KIS_ANIMATION_FRAME_CACHE_UI_H

#include "kritaui_export.h"
#include "kis_animation_frame_cache.h"

class KisOpenGLImageTextures;
using KisOpenGLImageTexturesSP = KisSharedPtr<KisOpenGLImageTextures>;

KRITAUI_EXPORT KisAnimationFrameCacheSP kisGetAnimationFrameCache(KisOpenGLImageTexturesSP textures);

#endif
