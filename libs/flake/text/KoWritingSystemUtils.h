/*
 *  SPDX-FileCopyrightText: 2024 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KOWRITINGSYSTEMUTILS_H
#define KOWRITINGSYSTEMUTILS_H

#include <PkChar.h>
#include <kritaflake_export.h>
// [migrate] missing include for Pk/Qt type
#include <PkMap.h>
// [migrate] missing include for Pk/Qt type
#include <PkString.h>
// [migrate] missing include for Pk/Qt type
#include <PkStringList.h>
/**
 * @brief The KoScriptUtils class
 *
 * Collection of utility functions to wrangle the different
 * PkChar scripts, BCP-47 locales and ISO 15924 tags.
 */

class KRITAFLAKE_EXPORT KoWritingSystemUtils
{
public:
    static PkString scriptTagForQCharScript(PkChar::Script script);
    static PkChar::Script qCharScriptForScriptTag(const PkString &tag);
    static PkString scriptTagForLanguage(const PkString &language);

    /**
     * This returns a map of samples and an associated tag. Note that the Sample is the first entry, the tag the second.
     * This is because Latin, for example, has multiple sample strings associated depending on Latin coverage in the font.
     * String it is stored with is s_<ISO 15924> tag for scripts and l_<BCP 47 Language> tag for languages.
     * This way we can have samples per language as is useful for vietnamese.
     */
    static PkMap<PkString, PkString> samples();

    static PkString sampleTagForLocale(const PkString &locale);

    /**
     * @brief The Bcp47Locale class
     * This holds a parsed BCP47 locale without relying on a platform locale database.
     *
     * @see ietf rfc5646
     */
    struct KRITAFLAKE_EXPORT Bcp47Locale {
        PkStringList languageTags;
        PkString scriptTag;
        PkString regionTag;
        PkStringList variantTags;
        PkStringList extensionTags;
        PkStringList privateUseTags;

        bool isValid() const;
        PkString toPosixLocaleFormat() const;
        PkString toString() const;
    };

    // Parse a BCP 47 string into a locale;
    static Bcp47Locale parseBcp47Locale(const PkString &locale);

};

#endif // KOWRITINGSYSTEMUTILS_H
