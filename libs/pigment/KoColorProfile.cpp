/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <algorithm>
#include <array>
#include <cmath>

#include "KoColorProfile.h"
#include "DebugPigment.h"
#include "kis_assert.h"

struct KoColorProfile::Private {
    PkString name;
    PkString info;
    PkString fileName;
    PkString manufacturer;
    PkString copyright;
    int primaries {-1};
    TransferCharacteristics characteristics {TRC_UNSPECIFIED};
};

KoColorProfile::KoColorProfile(const PkString &fileName) : d(new Private)
{
//     dbgPigment <<" Profile filename =" << fileName;
    d->fileName = fileName;
}

KoColorProfile::KoColorProfile(const KoColorProfile& profile)
    : d(new Private(*profile.d))
{
}

KoColorProfile::~KoColorProfile()
{
    delete d;
}

bool KoColorProfile::load()
{
    return false;
}

bool KoColorProfile::save(const PkString & filename)
{
    Q_UNUSED(filename);
    return false;
}


PkString KoColorProfile::name() const
{
    return d->name;
}

PkString KoColorProfile::info() const
{
    return d->info;
}
PkString KoColorProfile::manufacturer() const
{
    return d->manufacturer;
}
PkString KoColorProfile::copyright() const
{
    return d->copyright;
}
PkString KoColorProfile::fileName() const
{
    return d->fileName;
}

void KoColorProfile::setFileName(const PkString &f)
{
    d->fileName = f;
}

ColorPrimaries KoColorProfile::getColorPrimaries() const
{
    if (d->primaries == -1) {
        ColorPrimaries primaries = PRIMARIES_UNSPECIFIED;
        PkVector<qreal> wp = getWhitePointxyY();

        bool match = false;
        if (hasColorants()) {
            PkVector<qreal> col = getColorantsxyY();
            if (col.size()<8) {
                KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(col.size() < 8, PRIMARIES_UNSPECIFIED);
                //too few colorants.
                d->primaries = int(primaries);
                return (primaries);
            }
            PkVector<double> colorants = {wp[0], wp[1], col[0], col[1], col[3], col[4], col[6], col[7]};
            PkVector<double> compare;

            PkVector<ColorPrimaries> primariesList = {PRIMARIES_ITU_R_BT_709_5, PRIMARIES_ITU_R_BT_601_6, PRIMARIES_ITU_R_BT_470_6_SYSTEM_M,
                                                     PRIMARIES_ITU_R_BT_2020_2_AND_2100_0, PRIMARIES_SMPTE_EG_432_1, PRIMARIES_SMPTE_RP_431_2,
                                                     PRIMARIES_SMPTE_ST_428_1, PRIMARIES_GENERIC_FILM, PRIMARIES_SMPTE_240M, PRIMARIES_EBU_Tech_3213_E,
                                                     PRIMARIES_ADOBE_RGB_1998, PRIMARIES_PROPHOTO, PRIMARIES_ITU_R_BT_470_6_SYSTEM_B_G};

            for (ColorPrimaries check: primariesList) {
                colorantsForType(check, compare);
                if (compare.size() <8) {
                    KIS_SAFE_ASSERT_RECOVER(compare.size() < 8) { continue; }
                    //too few colorants, skip.
                }
                match = true;
                for (int i=0; i<colorants.size(); i++) {
                    match = std::fabs(colorants[i] - compare[i]) < 0.00001;
                    if (!match) {
                        break;
                    }
                }
                if (match) {
                    primaries = check;
                }
            }
        }

        d->primaries = int(primaries);
    }
    return ColorPrimaries(d->primaries);
}

PkString KoColorProfile::getColorPrimariesName(ColorPrimaries primaries)
{
    switch (primaries) {
    case PRIMARIES_ITU_R_BT_709_5:
        return PkString("Rec. 709");
    case PRIMARIES_ITU_R_BT_470_6_SYSTEM_M:
        return PkString("BT. 470 System M");
    case PRIMARIES_ITU_R_BT_470_6_SYSTEM_B_G:
        return PkString("BT. 470 System B, G");
    case PRIMARIES_GENERIC_FILM:
        return PkString("Generic Film");
    case PRIMARIES_SMPTE_240M:
        return PkString("SMPTE 240 M");
    case PRIMARIES_ITU_R_BT_2020_2_AND_2100_0:
        return PkString("Rec. 2020");
    case PRIMARIES_ITU_R_BT_601_6:
        return PkString("Rec. 601");
    case PRIMARIES_SMPTE_EG_432_1:
        return PkString("Display P3");
    case PRIMARIES_SMPTE_RP_431_2:
        return PkString("DCI P3");
    case PRIMARIES_SMPTE_ST_428_1:
        return PkString("XYZ primaries");
    case PRIMARIES_EBU_Tech_3213_E:
        return PkString("EBU Tech 3213 E");
    case PRIMARIES_PROPHOTO:
        return PkString("ProPhoto");
    case PRIMARIES_ADOBE_RGB_1998:
        return PkString("A98");
    case PRIMARIES_UNSPECIFIED:
        break;
    }
    return PkString("Unspecified");
}

void KoColorProfile::colorantsForType(ColorPrimaries primaries, PkVector<double> &colorants)
{
    switch (ColorPrimaries(primaries)) {
    case PRIMARIES_UNSPECIFIED:
        break;
    case PRIMARIES_ITU_R_BT_470_6_SYSTEM_M:
        // Unquantized.
        colorants = {0.310, 0.316};
        colorants.append({0.67, 0.33});
        colorants.append({0.21, 0.71});
        colorants.append({0.14, 0.08});
        //Illuminant C
        break;
    case PRIMARIES_ITU_R_BT_470_6_SYSTEM_B_G:
        // Unquantized.
        colorants = {0.3127, 0.3290};
        colorants.append({0.64, 0.33});
        colorants.append({0.29, 0.60});
        colorants.append({0.1500, 0.06});
        break;
    case PRIMARIES_ITU_R_BT_601_6:
        colorants = {0.3127, 0.3290};
        colorants.append({0.630, 0.340});
        colorants.append({0.310, 0.595});
        colorants.append({0.155, 0.070});
        break;
    case PRIMARIES_SMPTE_240M:
        colorants = {0.3127, 0.3290};
        colorants.append({0.630, 0.340});
        colorants.append({0.310, 0.595});
        colorants.append({0.155, 0.070});
        break;
    case PRIMARIES_GENERIC_FILM:
        colorants = {0.310, 0.316};
        colorants.append({0.681, 0.319});
        colorants.append({0.243, 0.692});
        colorants.append({0.145, 0.049});
        //Illuminant C
        break;
    case PRIMARIES_ITU_R_BT_2020_2_AND_2100_0:
        //prequantization courtesy of Elle Stone.
        colorants = {0.3127, 0.3290};
        colorants.append({0.708012540607, 0.291993664388});
        colorants.append({0.169991652439, 0.797007778423});
        colorants.append({0.130997824007, 0.045996550894});
        break;
    case PRIMARIES_SMPTE_ST_428_1:
        colorants = {1.0/3, 1.0/3};
        colorants.append({1.0, 0});
        colorants.append({0, 1.0});
        colorants.append({0, 0});
        break;
    case PRIMARIES_SMPTE_RP_431_2:
        colorants = {0.314, 0.351};
        colorants.append({0.6800, 0.3200});
        colorants.append({0.2650, 0.6900});
        colorants.append({0.1500, 0.0600});
        break;
    case PRIMARIES_SMPTE_EG_432_1:
        colorants = {0.3127, 0.3290};
        colorants.append({0.6800, 0.3200});
        colorants.append({0.2650, 0.6900});
        colorants.append({0.1500, 0.0600});
        break;
    case PRIMARIES_EBU_Tech_3213_E:
        colorants = {0.3127, 0.3290};
        colorants.append({0.63, 0.34});
        colorants.append({0.295, 0.605});
        colorants.append({0.155, 0.077});
        break;
    case PRIMARIES_PROPHOTO:
        //prequantization courtesy of Elle Stone.
        colorants = {0.3457, 0.3585};
        colorants.append({0.7347, 0.2653});
        colorants.append({0.1596, 0.8404});
        colorants.append({0.0366, 0.0001});
        break;
    case PRIMARIES_ADOBE_RGB_1998:
        //prequantization courtesy of Elle Stone.
        colorants = {0.3127, 0.3290};
        colorants.append({0.639996511, 0.329996864});
        colorants.append({0.210005295, 0.710004866});
        colorants.append({0.149997606, 0.060003644});
        break;
    case PRIMARIES_ITU_R_BT_709_5:
    default:
        // Prequantized colorants, courtesy of Elle Stone
        colorants = {0.3127, 0.3290};
        colorants.append({0.639998686, 0.330010138});
        colorants.append({0.300003784, 0.600003357});
        colorants.append({0.150002046, 0.059997204});
        break;

    }
}

TransferCharacteristics KoColorProfile::getTransferCharacteristics() const
{
    // Parse from an estimated gamma
    const PkVector<double> estimatedTRC = getEstimatedTRC();
    const double error = 0.0001;
    // Make sure the TRC is uniform across all channels
    const bool isUniformTRC = (estimatedTRC[0] == estimatedTRC[1] && estimatedTRC[0] == estimatedTRC[2]);
    if (d->characteristics == TRC_UNSPECIFIED && isUniformTRC && hasTRC()) {
        if (isLinear()) {
            d->characteristics = TRC_LINEAR;
        } else if (std::fabs(estimatedTRC[0] - (461.0 / 256.0)) < error) {
            // ICC v2 u8Fixed8Number calculation
            // Or can be prequantized as 1.80078125, courtesy of Elle Stone
            d->characteristics = TRC_GAMMA_1_8;
        } else if (std::fabs(estimatedTRC[0] - (563.0 / 256.0)) < error) {
            // Or can be prequantized as 2.19921875, courtesy of Elle Stone
            d->characteristics = TRC_A98;
        } else if (std::fabs(estimatedTRC[0] - 1.8) < error) {
            d->characteristics = TRC_GAMMA_1_8;
        } else if (std::fabs(estimatedTRC[0] - 2.2) < error) {
            d->characteristics = TRC_ITU_R_BT_470_6_SYSTEM_M;
        } else if (std::fabs(estimatedTRC[0] - 2.4) < error) {
            d->characteristics = TRC_GAMMA_2_4;
        } else if (std::fabs(estimatedTRC[0] - 2.8) < error) {
            d->characteristics = TRC_ITU_R_BT_470_6_SYSTEM_B_G;
        } else {
            // Escort to curve matching if no gamma is matched
            static constexpr std::array<TransferCharacteristics, 12> trcList = {{TRC_ITU_R_BT_709_5,
                                                                                 TRC_ITU_R_BT_470_6_SYSTEM_M,
                                                                                 TRC_ITU_R_BT_470_6_SYSTEM_B_G,
                                                                                 TRC_SMPTE_240M,
                                                                                 TRC_IEC_61966_2_1,
                                                                                 TRC_LOGARITHMIC_100,
                                                                                 TRC_LOGARITHMIC_100_sqrt10,
                                                                                 TRC_PROPHOTO,
                                                                                 TRC_GAMMA_1_8,
                                                                                 TRC_GAMMA_2_4,
                                                                                 TRC_A98,
                                                                                 TRC_LAB_L}};
            const auto characteristic =
                std::find_if(trcList.begin(), trcList.end(), [&](const TransferCharacteristics &check) -> bool {
                    return compareTRC(check, static_cast<float>(error));
                });
            if (characteristic != trcList.end()) {
                d->characteristics = *characteristic;
            }
        }
    }
    return d->characteristics;
}

void KoColorProfile::setCharacteristics(ColorPrimaries primaries, TransferCharacteristics curve)
{
    d->primaries = int(primaries);
    d->characteristics = curve;
}

PkString KoColorProfile::getTransferCharacteristicName(TransferCharacteristics curve)
{
    switch (curve) {
    case TRC_ITU_R_BT_709_5:
    case TRC_ITU_R_BT_601_6:
    case TRC_ITU_R_BT_2020_2_10bit:
        return PkString("rec 709 trc");
    case TRC_ITU_R_BT_2020_2_12bit:
        return PkString("rec 2020 12bit trc");
    case TRC_ITU_R_BT_470_6_SYSTEM_M:
        return PkString("Gamma 2.2");
    case TRC_ITU_R_BT_470_6_SYSTEM_B_G:
        return PkString("Gamma 2.8");
    case TRC_SMPTE_240M:
        return PkString("SMPTE 240 trc");
    case TRC_LINEAR:
        return PkString("Linear");
    case TRC_LOGARITHMIC_100:
        return PkString("Logarithmic 100");
    case TRC_LOGARITHMIC_100_sqrt10:
        return PkString("Logarithmic 100 sqrt10");
    case TRC_IEC_61966_2_4:
        return PkString("IEC 61966 2.4");
    case TRC_ITU_R_BT_1361:
    case TRC_IEC_61966_2_1:
        return PkString("sRGB trc");
    case TRC_SMPTE_ST_428_1:
        return PkString("SMPTE ST 428");
    case TRC_ITU_R_BT_2100_0_PQ:
        return PkString("Perceptual Quantizer");
    case TRC_ITU_R_BT_2100_0_HLG:
        return PkString("Hybrid Log Gamma");
    case TRC_GAMMA_1_8:
        return PkString("Gamma 1.8");
    case TRC_GAMMA_2_4:
        return PkString("Gamma 2.4");
    case TRC_A98:
        return PkString("Gamma A98");
    case TRC_PROPHOTO:
        return PkString("ProPhoto trc");
    case TRC_LAB_L:
        return PkString("Lab L* trc");
    case TRC_UNSPECIFIED:
        break;
    }

    return PkString("Unspecified");
}

void KoColorProfile::setName(const PkString &name)
{
    d->name = name;
}
void KoColorProfile::setInfo(const PkString &info)
{
    d->info = info;
}
void KoColorProfile::setManufacturer(const PkString &manufacturer)
{
    d->manufacturer = manufacturer;
}
void KoColorProfile::setCopyright(const PkString &copyright)
{
    d->copyright = copyright;
}
