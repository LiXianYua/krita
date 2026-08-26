/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISMASKINGBRUSHCOMPOSITEOPFACTORY_H
#define KISMASKINGBRUSHCOMPOSITEOPFACTORY_H

#include <QtGlobal>
#include <KoChannelInfo.h>

#include <kritapaintop_export.h>

class KisMaskingBrushCompositeOpBase;

class PAINTOP_EXPORT KisMaskingBrushCompositeOpFactory
{
public:
    static KisMaskingBrushCompositeOpBase* create(const PkString &id, KoChannelInfo::enumChannelValueType channelType,
                                                  int pixelSize, int alphaOffset);
    static KisMaskingBrushCompositeOpBase* create(const PkString &id, KoChannelInfo::enumChannelValueType channelType,
                                                  int pixelSize, int alphaOffset, qreal strength,
                                                  bool useSoftTexturing = false);

    static KisMaskingBrushCompositeOpBase* createForAlphaSrc(const PkString &id,
                                                             KoChannelInfo::enumChannelValueType channelType,
                                                             int pixelSize, int alphaOffset);
    static KisMaskingBrushCompositeOpBase* createForAlphaSrc(const PkString &id,
                                                             KoChannelInfo::enumChannelValueType channelType,
                                                             int pixelSize, int alphaOffset, qreal strength,
                                                             bool useSoftTexturing = false);

    static PkStringList supportedCompositeOpIds();
};

#endif // KISMASKINGBRUSHCOMPOSITEOPFACTORY_H
