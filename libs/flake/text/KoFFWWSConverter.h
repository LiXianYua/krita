/*
 *  SPDX-FileCopyrightText: 2024 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KOFFWWSCONVERTER_H
#define KOFFWWSCONVERTER_H

#include <PkHash.h>
#include <KoFontLibraryResourceUtils.h>
#include "KoCSSFontInfo.h"
#include "PkFontProvider.h"
#include <kritaflake_export.h>

#include <KoSvgText.h>
#include <QLocale>
#include <PkDateTime.h>

#include <optional>
// [migrate] missing include for Pk/Qt type
#include <PkScopedPointer.h>

/// This struct represents a CSS-compatible font family, containing all
/// sorts of info useful for the GUI.
struct KoFontFamilyWWSRepresentation {
    PkString fontFamilyName;
    PkString typographicFamilyName;

    PkHash<QLocale, PkString> localizedFontFamilyNames;
    PkHash<QLocale, PkString> localizedTypographicFamily;
    PkHash<QLocale, PkString> localizedTypographicStyles;

    PkDateTime lastModified; ///< Value of the most recently modified font family. Used for updates.

    PkHash<PkString, PkString> sampleStrings; /// sample string used to generate the preview;
    PkList<QLocale> supportedLanguages;

    PkHash<PkString, KoSvgText::FontFamilyAxis> axes;
    PkList<KoSvgText::FontFamilyStyleInfo> styles;

    KoSvgText::FontFormatType type = KoSvgText::UnknownFontType;
    bool isVariable = false;
    bool colorClrV0 = false;
    bool colorClrV1 = false;
    bool colorSVG = false;
    bool colorBitMap = false;
};


/**
 * @brief The KoFFWWSConverter class
 * This class takes fontconfig patterns and tries to sort them into
 * a hierarchy of typographic/wws/font files font families, as well
 * as retrieving all sorts of information to display in the GUI.
 *
 * This is necessary because by default FontConfig patterns don't
 * differentiate between fonts family names, which means that some
 * fonts are not selectable by CSS values alone.
 */
class KRITAFLAKE_EXPORT KoFFWWSConverter
{
public:
    KoFFWWSConverter();
    ~KoFFWWSConverter();

    struct FontFileEntry {
        PkString fileName;
        int fontIndex;
    };

    /// Add a font from a PkFontProvider enumeration entry (handle + familyName + languages).
    /// The provider is passed along so languages can be resolved into writing-system
    /// samples per codepoint (R-12 评审 M-5：端口只给逐码点 coversCodepoint()，没有整个
    /// 字符集粒度)。
    bool addFontFromEntry(const PkFontProvider::FontEntry &entry, FT_LibrarySP freeTypeLibrary, const PkFontProvider *provider);

    /// Add a font from a filename and index.
    /// This will use freetype and harfbuzz to figure out the family name(s), styles
    /// and other font features.
    bool addFontFromFile(const PkString &filename, const int index, FT_LibrarySP freeTypeLibrary);

    void addSupportedLanguagesByFile(const PkString &filename, const int index, const PkList<QLocale> &supportedLanguages, const PkFontProvider *provider, const PkFontProvider::FontHandle &handle);

    /// Sort any straggling fonts into WWSFamilies.
    void sortIntoWWSFamilies();

    /// This adds a CSS generic family. Call this before sortIntoWWSFamilies.
    void addGenericFamily(const PkString &name);

    /// Collects all WWSFamilies (that is, CSS compatible groupings of font files) and return them.
    PkList<KoFontFamilyWWSRepresentation> collectFamilies() const;

    /// Gets a single WWSFamily representation for a given CSS Family Name, used by KoFontStorage.
    std::optional<KoFontFamilyWWSRepresentation> representationByFamilyName(const PkString &familyName) const;

    /// Used to find the closest corresponding resource when the family name doesn't match.
    std::optional<PkString> wwsNameByFamilyName(const PkString familyName) const;

    /**
     * @brief candidatesForCssValues
     * Search the nodes for the most appropriate font for the given css values.
     * We want to give these preferential treatment to whatever fontconfig matches for us.
     * @return list of QPairs representing the filenames and file indices for the candidates.
     */
    PkVector<FontFileEntry> candidatesForCssValues(const KoCSSFontInfo info,
                                       quint32 xRes = 72,
                                       quint32 yRes = 72) const;

    /// Print out the font family hierarchy into the debug output.
    void debugInfo() const;

private:
    struct Private;
    PkScopedPointer<Private> d;
};

#endif // KOFFWWSCONVERTER_H
