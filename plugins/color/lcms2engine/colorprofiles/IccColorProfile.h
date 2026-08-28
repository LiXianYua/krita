/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2021 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef _KO_ICC_COLOR_PROFILE_H_
#define _KO_ICC_COLOR_PROFILE_H_

#include "KoColorProfile.h"
#include "KoChannelInfo.h"
#include <PkScopedPointer.h>

class LcmsColorProfileContainer;

/**
 * This class contains an ICC color profile.
 */
class IccColorProfile : public KoColorProfile
{
public:

    using KoColorProfile::save;

    /**
     * Contains the data associated with a profile. This is
     * shared through internal representation.
     */
    class Data
    {
    public:
        Data();
        explicit Data(const PkByteArray &rawData);
        ~Data();
        PkByteArray rawData();
        void setRawData(const PkByteArray &);
    private:
        struct Private;
        PkScopedPointer<Private> const d;
    };
    /**
     * This class should be used to wrap the ICC profile
     * representation coming from various CMS engine.
     */
    class Container
    {
    public:
        Container();
        virtual ~Container();
    public:
        virtual PkString name() const = 0;
        virtual PkString info() const = 0;
        virtual PkString manufacturer() const = 0;
        virtual PkString copyright() const = 0;
        virtual bool valid() const = 0;
        virtual bool isSuitableForOutput() const = 0;
        virtual bool isSuitableForInput() const = 0;
        virtual bool isSuitableForWorkspace() const = 0;
        virtual bool isSuitableForPrinting() const = 0;
        virtual bool isSuitableForDisplay() const = 0;
        virtual bool hasColorants() const = 0;
        virtual PkVector <double> getColorantsXYZ() const = 0;
        virtual PkVector <double> getColorantsxyY() const = 0;
        virtual PkVector <double> getWhitePointXYZ() const = 0;
        virtual PkVector <double> getWhitePointxyY() const = 0;
        virtual PkVector <double> getEstimatedTRC() const = 0;
        virtual bool compareTRC(TransferCharacteristics characteristics, float error) const = 0;
        virtual PkByteArray getProfileUniqueId() const = 0;
    };
public:

    explicit IccColorProfile(const PkString &fileName = PkString());
    explicit IccColorProfile(const PkByteArray &rawData);
    explicit IccColorProfile(const PkVector<double> &colorants,
                             const ColorPrimaries colorPrimariesType = PRIMARIES_UNSPECIFIED,
                             const TransferCharacteristics transferFunction = TRC_LINEAR);
    IccColorProfile(const IccColorProfile &rhs);
    ~IccColorProfile() override;

    KoColorProfile *clone() const override;

    bool load() override;
    virtual bool save();

    /**
    * @return an array with the raw data of the profile
    */
    PkByteArray rawData() const override;
    bool valid() const override;
    float version() const override;
    PkString colorModelID() const override;
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
    PkVector <qreal> getColorantsXYZ() const override;
    PkVector <qreal> getColorantsxyY() const override;
    PkVector <qreal> getWhitePointXYZ() const override;
    PkVector <qreal> getWhitePointxyY() const override;
    PkVector <qreal> getEstimatedTRC() const override;
    bool compareTRC(TransferCharacteristics characteristics, float error) const override;
    void linearizeFloatValue(PkVector <qreal> & Value) const override;
    void delinearizeFloatValue(PkVector <qreal> & Value) const override;
    void linearizeFloatValueFast(PkVector <qreal> & Value) const override;
    void delinearizeFloatValueFast(PkVector <qreal> & Value) const override;
    PkByteArray uniqueId() const override;
    bool operator==(const KoColorProfile &) const override;
    PkString type() const override
    {
        return "icc";
    }

    /**
     * Returns the set of min/maxes for each channel in this profile.
     * These (sometimes approximate) min and maxes are suitable
     * for UI building.
     * Furthermore, then only apply to the floating point uses of this profile,
     * and not the integer variants.
     */
    const PkVector<KoChannelInfo::DoubleRange> &getFloatUIMinMax(void) const;

protected:
    void setRawData(const PkByteArray &rawData);
public:
    LcmsColorProfileContainer *asLcms() const;
protected:
    bool init();
private:
    struct Private;
    PkScopedPointer<Private> d;
};

#endif
