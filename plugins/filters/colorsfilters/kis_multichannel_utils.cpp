/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2004 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2021 Deif Lou <giniba@gmail.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <KoColorSpace.h>
#include <KoChannelInfo.h>
#include <KoColorModelStandardIds.h>
#include <kis_assert.h>
#include <KoCompositeColorTransformation.h>
#include <kis_cubic_curve.h>

#include "../../color/colorspaceextensions/kis_hsv_adjustment.h"

#include "kis_multichannel_utils.h"

namespace KisMultiChannelUtils {

PkVector<VirtualChannelInfo> getVirtualChannels(const KoColorSpace *cs,
                                               int maxChannels,
                                               bool supportsLightness,
                                               bool supportsHue,
                                               bool supportsSaturation)
{
    supportsLightness =
        supportsLightness &&
        cs->colorModelId() != LABAColorModelID &&
        cs->colorModelId() != GrayAColorModelID &&
        cs->colorModelId() != GrayColorModelID &&
        cs->colorModelId() != AlphaColorModelID;

    supportsHue = supportsHue && supportsLightness;
    supportsSaturation = supportsSaturation && supportsLightness;

    PkVector<VirtualChannelInfo> vchannels;

    PkList<KoChannelInfo *> sortedChannels =
        KoChannelInfo::displayOrderSorted(cs->channels());

    if (maxChannels >= 0 && maxChannels == sortedChannels.size()) {
        /**
         * This is the extremely old version of the filter (< Krita 3.0).
         * It has the following curves layout: R, G, B, A.
         */
        supportsLightness = false;
        supportsHue = false;
        supportsSaturation = false;
    } else if (maxChannels >= 0 && maxChannels == sortedChannels.size() + 2) {
        /**
         * This is the old version of the filter (Krita [3.0, 5.0]).
         * It has the following channel layout: RGBA, R, G, B, A, Lightness.
         */
        supportsHue = false;
        supportsSaturation = false;
    } else {
        /**
         * Starting Krita 5.1.0 Krita started to use the latest layout:
         * RGBA, R, G, B, A, Hue, Saturation, Lightness
         */
    }

    if (supportsLightness) {
        vchannels << VirtualChannelInfo(VirtualChannelInfo::ALL_COLORS, -1, 0, cs);
    }

    for (KoChannelInfo *channel : sortedChannels) {
        int pixelIndex = KoChannelInfo::displayPositionToChannelIndex(channel->displayPosition(), cs->channels());
        vchannels << VirtualChannelInfo(VirtualChannelInfo::REAL, pixelIndex, channel, cs);
    }

    if (supportsHue) {
        vchannels << VirtualChannelInfo(VirtualChannelInfo::HUE, -1, 0, cs);
    }

    if (supportsSaturation) {
        vchannels << VirtualChannelInfo(VirtualChannelInfo::SATURATION, -1, 0, cs);
    }

    if (supportsLightness) {
        vchannels << VirtualChannelInfo(VirtualChannelInfo::LIGHTNESS, -1, 0, cs);
    }

    if (maxChannels >= 0 && vchannels.size() > maxChannels) {
        vchannels.resize(maxChannels);
    }

    return vchannels;
}

int findChannel(const PkVector<VirtualChannelInfo> &virtualChannels,
                const VirtualChannelInfo::Type &channelType)
{
    for (int i = 0; i < virtualChannels.size(); i++) {
        if (virtualChannels[i].type() == channelType) {
            return i;
        }
    }
    return -1;
}

KoColorTransformation* createPerChannelTransformationFromTransfers(const KoColorSpace *cs,
                                                                   const PkVector<PkVector<quint16>> &transfers,
                                                                   const PkList<bool> &transferIsIdentity)
{
    /**
     * TODO: What about the order of channels? (DK)
     *
     * Virtual channels are sorted in display order, does Lcms accepts
     * transforms in display order? Why on Earth it works?! Is it
     * documented anywhere?
     */
    const PkVector<VirtualChannelInfo> virtualChannels = getVirtualChannels(cs, transfers.size());

    if (transfers.size() > int(virtualChannels.size())) {
        // We got an illegal number of colorchannels :(
        return 0;
    }

    bool colorsNull = true;
    bool hueNull = true;
    bool saturationNull = true;
    bool lightnessNull = true;
    bool allColorsNull = true;
    int alphaIndexInReal = -1;

    PkVector<PkVector<quint16> > realTransfers;
    PkVector<quint16> hueTransfer;
    PkVector<quint16> saturationTransfer;
    PkVector<quint16> lightnessTransfer;
    PkVector<quint16> allColorsTransfer;

    for (int i = 0; i < virtualChannels.size(); i++) {
        if (virtualChannels[i].type() == VirtualChannelInfo::REAL) {
            realTransfers << transfers[i];

            if (virtualChannels[i].isAlpha()) {
                alphaIndexInReal = realTransfers.size() - 1;
            }

            if (colorsNull && !transferIsIdentity[i]) {
                colorsNull = false;
            }
        } else if (virtualChannels[i].type() == VirtualChannelInfo::HUE) {
            KIS_ASSERT_RECOVER_NOOP(hueTransfer.isEmpty());
            hueTransfer = transfers[i];

            if (hueNull && !transferIsIdentity[i]) {
                hueNull = false;
            }
        } else if (virtualChannels[i].type() == VirtualChannelInfo::SATURATION) {
            KIS_ASSERT_RECOVER_NOOP(saturationTransfer.isEmpty());
            saturationTransfer = transfers[i];

            if (saturationNull && !transferIsIdentity[i]) {
                saturationNull = false;
            }
        } else if (virtualChannels[i].type() == VirtualChannelInfo::LIGHTNESS) {
            KIS_ASSERT_RECOVER_NOOP(lightnessTransfer.isEmpty());
            lightnessTransfer = transfers[i];

            if (lightnessNull && !transferIsIdentity[i]) {
                lightnessNull = false;
            }
        } else if (virtualChannels[i].type() == VirtualChannelInfo::ALL_COLORS) {
            KIS_ASSERT_RECOVER_NOOP(allColorsTransfer.isEmpty());
            allColorsTransfer = transfers[i];

            if (allColorsNull && !transferIsIdentity[i]) {
                allColorsNull = false;
            }
        }
    }

    KoColorTransformation *hueTransform = 0;
    KoColorTransformation *saturationTransform = 0;
    KoColorTransformation *lightnessTransform = 0;
    KoColorTransformation *allColorsTransform = 0;
    KoColorTransformation *colorTransform = 0;

    /**
     * Sometimes the realTransfers are too low, this often happens with faulty config,
     * which in turn leads to trouble when creating a transform.
     */
    int missingTransfers = qMax(0, int(cs->channelCount()-realTransfers.size()));
    for (int i=0; i < missingTransfers; i++) {
        realTransfers.append(KisCubicCurve().uint16Transfer());
    }

    if (!colorsNull) {
        const quint16** transfers = new const quint16*[realTransfers.size()];
        for(int i = 0; i < realTransfers.size(); ++i) {
            transfers[i] = realTransfers[i].constData();

            /**
             * createPerChannelAdjustment() expects alpha channel to
             * be the last channel in the list, so just it here
             */
            KIS_ASSERT_RECOVER_NOOP(i != alphaIndexInReal ||
                                    alphaIndexInReal == (realTransfers.size() - 1));
        }

        colorTransform = cs->createPerChannelAdjustment(transfers);
        delete [] transfers;
    }

    if (!hueNull) {
        PkHash<PkString, PkVariant> params;
        params["curve"] = PkVariant::fromValue(hueTransfer);
        params["channel"] = KisHSVCurve::Hue;
        params["relative"] = false;
        params["lumaRed"]   = cs->lumaCoefficients()[0];
        params["lumaGreen"] = cs->lumaCoefficients()[1];
        params["lumaBlue"]  = cs->lumaCoefficients()[2];

        hueTransform = cs->createColorTransformation("hsv_curve_adjustment", params);
    }

    if (!saturationNull) {
        PkHash<PkString, PkVariant> params;
        params["curve"] = PkVariant::fromValue(saturationTransfer);
        params["channel"] = KisHSVCurve::Saturation;
        params["relative"] = false;
        params["lumaRed"]   = cs->lumaCoefficients()[0];
        params["lumaGreen"] = cs->lumaCoefficients()[1];
        params["lumaBlue"]  = cs->lumaCoefficients()[2];

        saturationTransform = cs->createColorTransformation("hsv_curve_adjustment", params);
    }

    if (!lightnessNull) {
        lightnessTransform = cs->createBrightnessContrastAdjustment(lightnessTransfer.constData());
    }

    if (!allColorsNull) {
        const quint16** allColorsTransfers = new const quint16*[realTransfers.size()];
        for(int i = 0; i < realTransfers.size(); ++i) {
            allColorsTransfers[i] = (i != alphaIndexInReal) ?
                allColorsTransfer.constData() : 0;

            /**
             * createPerChannelAdjustment() expects alpha channel to
             * be the last channel in the list, so just it here
             */
            KIS_ASSERT_RECOVER_NOOP(i != alphaIndexInReal ||
                                    alphaIndexInReal == (realTransfers.size() - 1));
        }

        allColorsTransform = cs->createPerChannelAdjustment(allColorsTransfers);
        delete[] allColorsTransfers;
    }

    PkVector<KoColorTransformation*> allTransforms;
    allTransforms << colorTransform;
    allTransforms << allColorsTransform;
    allTransforms << hueTransform;
    allTransforms << saturationTransform;
    allTransforms << lightnessTransform;

    return KoCompositeColorTransformation::createOptimizedCompositeTransform(allTransforms);
}

}
