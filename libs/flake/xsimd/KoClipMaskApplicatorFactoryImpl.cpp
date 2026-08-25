/*
 *  SPDX-FileCopyrightText: 2023 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtCore/QtGlobal>
#include <QtGui/QtGui>

#include "KoClipMaskApplicatorFactoryImpl.h"

#ifndef DISABLE_CLIP_MASK_PAINTER_ON_MACOS

template<>
KoClipMaskApplicatorBase * KoClipMaskApplicatorFactoryImpl::create<xsimd::current_arch>()
{
    return new KoClipMaskApplicator<xsimd::current_arch>();
}

#else

template<>
KoClipMaskApplicatorBase * KoClipMaskApplicatorFactoryImpl::create<xsimd::generic>()
{
    return new KoClipMaskApplicator<xsimd::generic>();
}

#endif
