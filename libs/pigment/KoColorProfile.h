/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef _KO_COLOR_PROFILE_H_
#define _KO_COLOR_PROFILE_H_

#include <boost/operators.hpp>
#include <PkAuxTypes.h>
#include <PkGlobal.h>
#include <PkString.h>
#include <PkVector.h>

#include "KoColorProfileConstants.h"
#include "kritapigment_export.h"

/**
 * Contains information needed for color transformation.
 */
class KRITAPIGMENT_EXPORT KoColorProfile : public boost::equality_comparable<KoColorProfile>
{

public:

    /**
     * @param fileName file name to load or save that profile
     */
    explicit KoColorProfile(const PkString &fileName = PkString());
    KoColorProfile(const KoColorProfile& profile);
    virtual ~KoColorProfile();

    /**
     * @return the type of this profile (icc, ctlcs etc)
     */
    virtual PkString type() const {
        return PkString();
    }

    /**
     * Create a copy of this profile.
     * Data that shall not change during the life time of the profile shouldn't be
     * duplicated but shared, like for instance ICC data.
     *
     * Data that shall be changed like a palette or hdr information such as exposure
     * must be duplicated while cloning.
     */
    virtual KoColorProfile* clone() const = 0;

    /**
     * Load the profile in memory.
     * @return true if the profile has been successfully loaded
     */
    virtual bool load();

    /**
     * Override this function to save the profile.
     * @param fileName destination
     * @return true if the profile has been successfully saved
     */
    virtual bool save(const PkString &fileName);

    /**
     * @return true if the profile is valid, false if it isn't been loaded in memory yet, or
     * if the loaded memory is a bad profile
     */
    virtual bool valid() const = 0;

    /**
     * @return the name of this profile
     */
    PkString name() const;
    /**
     * @return the info of this profile
     */
    PkString info() const;
    /** @return manufacturer of the profile
     */
    PkString manufacturer() const;
    /**
     * @return the copyright of the profile
     */
    PkString copyright() const;
    /**
     * @return the filename of the profile (it might be empty)
     */
    PkString fileName() const;
    /**
     * @param filename new filename
     */
    void setFileName(const PkString &filename);

    /**
     * Return version
     */
    virtual float version() const = 0;

    /**
     * @return a string for a color model id.
     */
    virtual PkString colorModelID() const {
        return PkString();
    };
    /**
     * @return true if this profile can be used to convert color from a different profile to this one
     */
    virtual bool isSuitableForOutput() const = 0;
    /**
     * @return true if this profile can be used to convert color from this one to a different one
     */
    virtual bool isSuitableForInput() const = 0;
    /**
     * @return true if you can use this profile can be used in Krita
     */
    virtual bool isSuitableForWorkspace() const = 0;
    /**
     * @return true if this profile is suitable to use for printing
     */
    virtual bool isSuitableForPrinting() const = 0;
    /**
     * @return true if this profile is suitable to use for display
     */
    virtual bool isSuitableForDisplay() const = 0;

    /**
     * @return which rendering intents are supported
     */
    virtual bool supportsPerceptual() const = 0;
    virtual bool supportsSaturation() const = 0;
    virtual bool supportsAbsolute() const = 0;
    virtual bool supportsRelative() const = 0;
    /**
     * @return if the profile has colorants.
     */
    virtual bool hasColorants() const = 0;
    /**
     * @return a qvector <double>(9) with the RGB colorants in XYZ
     */
    virtual PkVector <qreal> getColorantsXYZ() const = 0;
    /**
     * @return a qvector <double>(9) with the RGB colorants in xyY
     */
    virtual PkVector <qreal> getColorantsxyY() const = 0;
    /**
     * @return a qvector <double>(3) with the whitepoint in XYZ
     */
    virtual PkVector <qreal> getWhitePointXYZ() const = 0;
    /**
     * @return a qvector <double>(3) with the whitepoint in xyY
     */
    virtual PkVector <qreal> getWhitePointxyY() const = 0;
    
    /**
     * @return estimated gamma for RGB and Grayscale profiles
     */
    virtual PkVector <qreal> getEstimatedTRC() const = 0;

    /**
     * @return if the profile has a TRC(required for linearisation).
     */
    virtual bool hasTRC() const = 0;
    /**
     * @return if the profile's TRCs are linear.
     */
    virtual bool isLinear() const = 0;
    /**
     * Linearizes first 3 values of PkVector, leaving other values unchanged.
     * Returns the same PkVector if it is not possible to linearize.
     */
    virtual void linearizeFloatValue(PkVector <qreal> & Value) const = 0;
    /**
     * Delinearizes first 3 values of PkVector, leaving other values unchanged.
     * Returns the same PkVector if it is not possible to delinearize.
     * Effectively undoes LinearizeFloatValue.
     */
    virtual void delinearizeFloatValue(PkVector <qreal> & Value) const = 0;
    /**
     * More imprecise versions of the above(limited to 16bit, and can't
     * delinearize above 1.0.) Use this for filters and images.
     */
    virtual void linearizeFloatValueFast(PkVector <qreal> & Value) const = 0;
    virtual void delinearizeFloatValueFast(PkVector <qreal> & Value) const = 0;

    /**
     * Comparing profile's TRC against the other with defined error threshold,
     * returns true if profile TRC is matched.
     */
    virtual bool compareTRC(TransferCharacteristics characteristics, float error) const = 0;

    virtual PkByteArray uniqueId() const = 0;
    
    virtual bool operator==(const KoColorProfile&) const = 0;

    /**
     * @return an array with the raw data of the profile
     */
    virtual PkByteArray rawData() const {
        return PkByteArray();
    }

    /**
     * @brief getColorPrimaries
     * @return colorprimaries, defaults to 'unspecified' if no match is possible.
     */
    virtual ColorPrimaries getColorPrimaries() const;

    /**
     * @brief getColorPrimariesName
     * @param primaries
     * @return human friendly name of the primary.
     */
    static PkString getColorPrimariesName(ColorPrimaries primaries);
    /**
     * @brief colorantsForPrimaries
     * fills a PkVector<float> with the xy values of the whitepoint and red, green, blue colorants for
     * a given predefined value. Will not change the vector when the primaries are set to 'undefined'.
     * @param primaries predefined value.
     * @param colorants the vector to fill.
     */
    static void colorantsForType(ColorPrimaries primaries, PkVector<double> &colorants);

    /**
     * @brief getTransferCharacteristics
     * This function should be subclassed at some point so we can get the value from the lcms profile.
     * @return transferfunction number.
     */
    virtual TransferCharacteristics getTransferCharacteristics() const;

    /**
     * @brief getTransferCharacteristicName
     * @param curve the number
     * @return name of the characteristic
     */
    static PkString getTransferCharacteristicName(TransferCharacteristics curve);

protected:
    /**
     * Allows to define the name of this profile.
     */
    void setName(const PkString &name);
    /**
     * Allows to set the information string of that profile.
     */
    void setInfo(const PkString &info);
    /**
     * Allows to set the manufacturer string of that profile.
     */
    void setManufacturer(const PkString &manufacturer);
    /**
     * Allows to set the copyright string of that profile.
     */
    void setCopyright(const PkString &copyright);

    /**
     * @brief setCharacteristics
     * ideally, we'd read this from the icc profile curve, but that can be tricky, instead
     * we'll set it on profile creation.
     * @param curve
     */
    void setCharacteristics(ColorPrimaries primaries, TransferCharacteristics curve);

private:
    struct Private;
    Private* const d;
};

#endif
