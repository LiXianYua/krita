/*
 *  SPDX-FileCopyrightText: 2025 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KOCSSSTYLEPRESET_H
#define KOCSSSTYLEPRESET_H

#include <KoResource.h>
#include <KoSvgTextProperties.h>
#include <kritaflake_export.h>

#include <KisResourceTypes.h>
#include <PkString.h>
#include <pk/port/PkStream.h>

class KoCssStylePreset;
typedef PkSharedPointer<KoCssStylePreset> KoCssStylePresetSP;

/**
 * @brief The KoCssStylePreset class
 *
 * This is a Resource that represents style data.
 * Internally, the style data is stored inside a text shape,
 * allowing us to showcase the style data in context.
 */
class KRITAFLAKE_EXPORT KoCssStylePreset : public KoResource
{
public:
    KoCssStylePreset(const PkString &filename);
    KoCssStylePreset(const KoCssStylePreset &rhs);
    KoCssStylePreset &operator=(const KoCssStylePreset &rhs) = delete;
    ~KoCssStylePreset();

    /// The actual text properties.
    KoSvgTextProperties properties(int ppi = 72, bool removeKraProps = false) const;

    /// set the properties. Call updateThumbnail to update the sample.
    void setProperties(const KoSvgTextProperties &properties);

    /// The description associated with this style.
    PkString description() const;
    void setDescription(const PkString &desc);

    /// Set the style type, type is either "paragraph" or "character".
    PkString styleType() const;
    void setStyleType(const PkString &type);

    /// The sample text that is being styled by this preset.
    PkString sampleText() const;

    /// set the sample. Call updateThumbnail to update the sample.
    void setSampleText(const PkString &text);

    /// The text displayed before the sample. Only relevant when in Character mode.
    PkString beforeText() const;

    /// set the before text. Call updateThumbnail to update the sample.
    void setBeforeText(const PkString &text);

    /// The text displayed after the sample, only relevant when in character mode.
    PkString afterText() const;

    /// set the after text. Call updateThumbnail to update the sample.
    void setAfterText(const PkString &text);

    /// Returns the sample svg metadata. Use updateThumbnail to update it.
    PkString sampleSvg() const;

    /// Returns the size of the shape which the paragraph is set as.
    PkSizeF paragraphSampleSize() const;
    /// Set the size of the shape the paragraph is set in.
    void setParagraphSampleSize(const PkSizeF size);

    /**
     * The resolution that this style is tied to.
     * if this is above 0, then the properties absolute values are scaled by
     * to fit the document resolution.
     * This allows for pixel-relative styles to be created.
     */
    int storedPPIResolution() const;

    void setStoredPPIResolution(const int ppi);

    /**
     * @brief generateSampleShape
     * This generates the sample textshape from the properties and sample text(s).
     */
    KoShape* generateSampleShape() const;

    /// Determines the preferred sample alignment based on the text properties.
    /// It's set up so that the alignment anchor of the text is shown.
    Qt::Alignment alignSample() const;

    /**
     * @brief primaryFontFamily
     * If a style uses a FontFamily, it may not look as expected when that
     * font family is missing. Typically, we'd use linked resources for this,
     * however, embedding fonts is really complex.
     * @return the primary font family for this style, will return empty if
     * the style does not require a font family.
     */
    PkString primaryFontFamily() const;

    void updateAlignSample();

    // KoResource interface
public:
    KoResourceSP clone() const override;
    bool loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface) override;
    bool saveToDevice(PkStream *dev) const override;
    PkString defaultFileExtension() const override;
    void updateThumbnail() override;
    std::pair<PkString, PkString> resourceType() const override;
private:
    struct Private;
    PkScopedPointer<Private> d;
};

#endif // KOCSSSTYLEPRESET_H
