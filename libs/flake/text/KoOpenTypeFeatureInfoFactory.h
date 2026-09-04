/*
 *  SPDX-FileCopyrightText: 2024 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <PkString.h>
#include <PkStringList.h>
#include <PkVector.h>
#include <PkScopedPointer.h>
#include <kritaflake_export.h>
// [migrate] missing include for Pk/Qt type
#include <PkByteArray.h>

/*
 * KoOpenTypeFeatureInfo provides basic info about
 * each opentype feature, with translatable strings.
 **/
struct KoOpenTypeFeatureInfo
{
    enum OpenTypeTable {
        GSUB1, ///< Single substitution.
        GSUB2, ///< Multiple substitution (1 -> 2)
        GSUB3, ///< Alternate substitution
        GSUB4, ///< Ligature substitution.
        GSUB5, ///< Contextual substitution.
        GSUB6, ///< Chained Contextual substitution.
        GSUB7, ///< Extension substitution.
        GSUB8, ///< Reverse Chained Contextual substitution.
        GPOS1, ///< Single position adjustment.
        GPOS2, ///< Pair position adjustment.
        GPOS3, ///< Cursive attachment.
        GPOS4, ///< Mark-to-base adjustment.
        GPOS5, ///< Mark-to-ligature adjustment.
        GPOS6, ///< Mark-to-mark adjustment.
        GPOS7, ///< Contextual positioning.
        GPOS8, ///< Chained-Contextual positioning.
        GPOS9, ///< Extension positioning.
    };
    KoOpenTypeFeatureInfo() {}
    KoOpenTypeFeatureInfo(const PkByteArray &tag,
                          const PkString &name,
                          const PkString &description,
                          const PkVector<OpenTypeTable> &tables,
                          const bool glyphPalette = false,
                          const int maxValue = 1)
    : tag(tag)
    , name(name)
    , description(description)
    , tables(tables)
    , glyphPalette(glyphPalette)
    , maxValue(maxValue)
    {
    }
    
    PkByteArray tag; ///< 4 char tag.
    
    PkString name; ///< User-friendly name.
    
    PkString description; ///< Description of the feature.

    PkString sample; ///< Sample of the feature, if any. Only used by CVXX features and retrieved from the font.

    PkStringList namedParameters;/// Named parameters. Only used by CVXX features and retrieved from the font.
    
    PkVector<OpenTypeTable> tables; ///< Which table type(s) are recommended for this feature in the official registry.

    bool glyphPalette{false}; ///< Whether the feature should be visible in the glyph palette.

    int maxValue; ///< The maximum value possible, this is by default 1 (on), but for alternate substitution(gsub 3), it can have more inside the font.
    
};

/**
 * @brief The KoOpenTypeFeatureInfoFactory class
 *
 * This returns KoOpenTypeFeatureInfo's for the given tag.
 */

class KRITAFLAKE_EXPORT KoOpenTypeFeatureInfoFactory
{
public:
    KoOpenTypeFeatureInfoFactory();
    ~KoOpenTypeFeatureInfoFactory();
    
    /**
     * @brief infoByTag
     * @param tag -- the opentype tag for a given feature.
     * Note that it is possible for Fonts to have custom features,
     * in this case a generic KoOpenTypeFeatureInfo will be returned.
     * @return KoOpenTypeFeatureInfo for a given tag.
     */
    KoOpenTypeFeatureInfo infoByTag(const PkByteArray &tag) const;

    /**
     * @brief tags
     * @return all the tags we have prepared KoOpenTypeFeatureInfo for.
     */
    PkList<PkString> tags() const;

private:
    struct Private;
    
    PkScopedPointer<Private> d;
};
