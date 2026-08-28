/*
 * This file is part of the KDE project
 *  SPDX-FileCopyrightText: 2000 Matthias Elter <elter@kde.org>
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2007 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _KO_LCMS_COLORPROFILE_H
#define _KO_LCMS_COLORPROFILE_H

#include "IccColorProfile.h"

#include <lcms2.h>

#include <PkAuxTypes.h>
#include <PkString.h>

/**
 * This class contains an LCMS color profile. Don't use it outside LcmsColorSpace.
 */
class LcmsColorProfileContainer : public IccColorProfile::Container
{
    friend class IccColorProfile;
protected:
    LcmsColorProfileContainer(IccColorProfile::Data *);
private:
    /**
     * Create a byte array from a lcms profile.
     */
    static PkByteArray lcmsProfileToByteArray(const cmsHPROFILE profile);

public:
    /**
     * @param profile lcms memory structure with the profile, it is freed after the call
     *                to this function
     * @return an ICC profile created from an LCMS profile
     */
    static IccColorProfile *createFromLcmsProfile(const cmsHPROFILE profile);
public:

    ~LcmsColorProfileContainer() override;

    /**
     * @return the ICC color space signature
     */
    cmsColorSpaceSignature colorSpaceSignature() const;
    /**
     * @return the class of the color space signature
     */
    cmsProfileClassSignature deviceClass() const;
    /**
     * @return the name of the manufacturer
     */
    PkString manufacturer() const override;
    /**
     * @return the embedded copyright
     */
    PkString copyright() const override;
    /**
     * @return the structure to use with LCMS functions
     */
    cmsHPROFILE lcmsProfile() const;

    bool valid() const override;
    virtual float version() const;

    bool isSuitableForOutput() const override;
    bool isSuitableForInput() const override;
    bool isSuitableForWorkspace() const override;

    bool isSuitableForPrinting() const override;

    bool isSuitableForDisplay() const override;

    virtual bool supportsPerceptual() const;
    virtual bool supportsSaturation() const;
    virtual bool supportsAbsolute() const;
    virtual bool supportsRelative() const;

    bool hasColorants() const override;
    virtual bool hasTRC() const;
    bool isLinear() const;
    PkVector <double> getColorantsXYZ() const override;
    PkVector <double> getColorantsxyY() const override;
    PkVector <double> getWhitePointXYZ() const override;
    PkVector <double> getWhitePointxyY() const override;
    PkVector <double> getEstimatedTRC() const override;
    virtual void LinearizeFloatValue(PkVector <double> & Value) const;
    virtual void DelinearizeFloatValue(PkVector <double> & Value) const;
    virtual void LinearizeFloatValueFast(PkVector <double> & Value) const;
    virtual void DelinearizeFloatValueFast(PkVector <double> & Value) const;
    PkString name() const override;
    PkString info() const override;
    PkByteArray getProfileUniqueId() const override;

    bool compareTRC(TransferCharacteristics characteristics, float error) const override;

    static cmsToneCurve* transferFunction(TransferCharacteristics transferFunction);

protected:
    LcmsColorProfileContainer();

private:
    bool init();

    class Private;
    Private *const d;
};

#endif // _KO_LCMS_COLORPROFILE_H
