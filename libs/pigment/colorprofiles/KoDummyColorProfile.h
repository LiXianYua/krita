/*
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef _KO_DUMMY_COLOR_PROFILE_H_
#define _KO_DUMMY_COLOR_PROFILE_H_

#include "KoColorProfile.h"

class KoDummyColorProfile : public KoColorProfile
{
public:
    KoDummyColorProfile();
    ~KoDummyColorProfile() override;
    KoColorProfile* clone() const override;
    bool valid() const override;
    float version() const override;
    bool isSuitableForOutput() const override;
    bool isSuitableForInput() const override;
    bool isSuitableForWorkspace() const override;
    bool isSuitableForPrinting() const override;
    bool isSuitableForDisplay() const override;
    bool supportsPerceptual() const override;
    bool supportsSaturation() const override;
    bool supportsAbsolute() const override;
    bool supportsRelative() const override;
    bool hasColorants() const override;
    bool hasTRC() const override;
    bool isLinear() const override;
    PkVector <double> getColorantsXYZ() const override;
    PkVector <double> getColorantsxyY() const override;
    PkVector <double> getWhitePointXYZ() const override;
    PkVector <double> getWhitePointxyY() const override;
    PkVector <double> getEstimatedTRC() const override;
    bool compareTRC(TransferCharacteristics characteristics, float error) const override;
    void linearizeFloatValue(PkVector <double> & Value) const override;
    void delinearizeFloatValue(PkVector <double> & Value) const override;
    void linearizeFloatValueFast(PkVector <double> & Value) const override;
    void delinearizeFloatValueFast(PkVector <double> & Value) const override;
    bool operator==(const KoColorProfile&) const override;
    PkByteArray uniqueId() const override;
};

#endif
