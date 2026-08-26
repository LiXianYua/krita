/*
 *  SPDX-FileCopyrightText: 2023 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISSTROKECOMPATIBILITYINFO_H
#define KISSTROKECOMPATIBILITYINFO_H

#include <kritapaintop_export.h>
#include <boost/operators.hpp>

#include <PkString.h>
#include <KoColor.h>
#include <KoResourceSignature.h>
#include <KoCompositeOpRegistry.h>
#include <PkNodeId.h>


class KisResourcesSnapshot;


struct PAINTOP_EXPORT KisStrokeCompatibilityInfo : public boost::equality_comparable<KisStrokeCompatibilityInfo>
{
    KisStrokeCompatibilityInfo();
    KisStrokeCompatibilityInfo(KisResourcesSnapshot &resourcesSnapshot);

    friend bool operator==(const KisStrokeCompatibilityInfo &lhs, const KisStrokeCompatibilityInfo &rhs);

    KoColor currentFgColor;
    KoColor currentBgColor;
    KoResourceSignature currentPattern;
    KoResourceSignature currentGradient;
    KoResourceSignature currentPreset;
    PkString currentGeneratorXml;
    PkNodeId currentNode;

    qreal opacity {OPACITY_OPAQUE_F};
    PkString compositeOpId {COMPOSITE_OVER};

    PkBitArray channelLockFlags;
};

#endif // KISSTROKECOMPATIBILITYINFO_H
