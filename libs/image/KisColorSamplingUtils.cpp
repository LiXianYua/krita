/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisColorSamplingUtils.h"

#include <KoColor.h>
#include <KoColorSpace.h>
#include <KoMixColorsOp.h>

#include "kis_global.h"
#include "kis_paint_device.h"
#include "kis_sequential_iterator.h"

bool KisColorSamplingUtils::sampleColor(KoColor &outColor,
                                        KisPaintDeviceSP device,
                                        const QPoint &position,
                                        const KoColor *blendColor,
                                        int radius,
                                        int blend,
                                        bool pure)
{
    KIS_ASSERT(device);

    static bool firstTime = true;
    if (firstTime) {
        pure = true;
        firstTime = false;
    }

    const KoColorSpace *colorSpace = device->colorSpace();
    KoColor sampledColor = KoColor::createTransparent(colorSpace);

    const bool oldSupportsWraparound = device->supportsWraproundMode();
    device->setSupportsWraparoundMode(true);

    if (!pure && radius > 1) {
        QScopedPointer<KoMixColorsOp::Mixer> mixer(colorSpace->mixColorsOp()->createMixer());
        const int effectiveRadius = radius - 1;
        const QRect sampleRect(position.x() - effectiveRadius,
                               position.y() - effectiveRadius,
                               2 * effectiveRadius + 1,
                               2 * effectiveRadius + 1);
        KisSequentialConstIterator iterator(device, sampleRect);
        const int radiusSquared = pow2(effectiveRadius);

        int consecutivePixels = iterator.nConseqPixels();
        while (iterator.nextPixels(consecutivePixels)) {
            const QPoint realPosition(iterator.x(), iterator.y());
            if (kisSquareDistance(realPosition, position) < radiusSquared) {
                mixer->accumulateAverage(iterator.oldRawData(), consecutivePixels);
            }
        }
        mixer->computeMixedColor(sampledColor.data());
    } else {
        device->pixel(position.x(), position.y(), &sampledColor);
    }

    device->setSupportsWraparoundMode(oldSupportsWraparound);

    if (!pure && blendColor && blend < 100) {
        const quint8 blendScaled = static_cast<quint8>(blend * 2.55f);
        const quint8 *colors[2] = {blendColor->data(), sampledColor.data()};
        qint16 weights[2] = {qint16(255 - blendScaled), qint16(blendScaled)};
        device->colorSpace()->mixColorsOp()->mixColors(colors, weights, 2, sampledColor.data());
    }

    sampledColor.convertTo(device->compositionSourceColorSpace());
    const bool validColorSampled = sampledColor.opacityU8() != OPACITY_TRANSPARENT_U8;
    if (validColorSampled) {
        outColor = sampledColor;
    }
    return validColorSampled;
}
