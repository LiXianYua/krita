/*
 *  SPDX-FileCopyrightText: 2024 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <PkMap.h>
#include <QDebug>

#include "KoOpenTypeFeatureInfoFactory.h"
#include <klocalizedstring.h>
// [migrate] missing include for Pk/Qt type
#include <PkByteArray.h>
// i18nc 返回 QString，KoOpenTypeFeatureInfo 构造吃 PkString —— 走 designated 边界桥。
#include <PkFlakeBridge.h>

struct Q_DECL_HIDDEN KoOpenTypeFeatureInfoFactory::Private
{
    PkMap<PkString, KoOpenTypeFeatureInfo> infoMap;
};

KoOpenTypeFeatureInfoFactory::KoOpenTypeFeatureInfoFactory()
    : d(new Private)
{
    PkVector<KoOpenTypeFeatureInfo> initialMap;

    // General discretionary features
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("aalt"),
                      toPkString(i18nc("@title", "Access All Alternates")),
                      toPkString(i18nc("@tooltip", "Access any possible substitutions.")),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB3},
                      true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("calt"),
                      toPkString(i18nc("@title", "Contextual Alternates")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs depending on their context within a text.")),
                      {KoOpenTypeFeatureInfo::GSUB6}, false));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("clig"),
                      toPkString(i18nc("@title", "Contextual Ligatures")),
                      toPkString(i18nc("@tooltip", "Replaces sequences of glyphs with ligatures depending on their context within a text.")),
                      {KoOpenTypeFeatureInfo::GSUB8}, false));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("cswh"),
                      toPkString(i18nc("@title", "Contextual Swash")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs with swashed glyphs depending on their context within a text.")),
                      {KoOpenTypeFeatureInfo::GSUB8}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("dlig"),
                      toPkString(i18nc("@title", "Discretionary Ligatures")),
                      toPkString(i18nc("@tooltip", "Replaces sequences of glyphs with ligatures intended for typographic effect.")),
                      {KoOpenTypeFeatureInfo::GSUB4}, false));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("falt"),
                      toPkString(i18nc("@title", "Final Glyph on Line Alternates")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs with their line-end forms.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("hist"),
                      toPkString(i18nc("@title", "Historical Forms")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs with their historical forms.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("hlig"),
                      toPkString(i18nc("@title", "Historical Forms")),
                      toPkString(i18nc("@tooltip", "Replaces sequences glyphs with ligatures that were in use in the past, but rare today.")),
                      {KoOpenTypeFeatureInfo::GSUB4}, false));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("jalt"),
                      toPkString(i18nc("@title", "Justification Alternates")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs with their alternates meant for justification.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("liga"),
                      toPkString(i18nc("@title", "Standard Ligatures")),
                      toPkString(i18nc("@tooltip", "Replaces sequences glyphs with common ligatures.")),
                      {KoOpenTypeFeatureInfo::GSUB4}, false));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("lfbd"),
                      toPkString(i18nc("@title", "Left Bounds")),
                      toPkString(i18nc("@tooltip", "This adjusts glyphs so there is no space at the left side.")),
                      {KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("locl"),
                      toPkString(i18nc("@title", "Localized Forms")),
                      toPkString(i18nc("@tooltip", "This replaces glyphs with language specific versions.")),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("opbd"),
                      toPkString(i18nc("@title", "Optical Bounds")),
                      toPkString(i18nc("@tooltip", "Adjusts glyphs so they align by their optical bounds. Doesn't do anything in Krita.")),
                      {KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("ornm"),
                      toPkString(i18nc("@title", "Ornaments")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs with ornaments for decorative purposes.")),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB3}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("rand"),
                      toPkString(i18nc("@title", "Randomize")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs with random alternates.")),
                      {KoOpenTypeFeatureInfo::GSUB3}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("rtbd"),
                      toPkString(i18nc("@title", "Right Bounds")),
                      toPkString(i18nc("@tooltip", "This adjusts glyphs so there is no space at the right side.")),
                      {KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("salt"),
                      toPkString(i18nc("@title", "Stylistic Alternates")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs with stylistic alternates that do not fit in other categories.")),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB3}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("swsh"),
                      toPkString(i18nc("@title", "Swash")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs with swashed alternates.")),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB3}, true));

    // General required features.
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("ccmp"),
                      toPkString(i18nc("@title", "Glyph Composition/Decomposition")),
                      toPkString(i18nc("@tooltip", "This composes or decomposes graphemes so that the resulting glyphs can later be recomposed. Commonly used to handle diacritics.")),
                      {KoOpenTypeFeatureInfo::GSUB2}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("kern"),
                      toPkString(i18nc("@title", "Kerning")),
                      toPkString(i18nc("@tooltip", "This controls kerning on the font.")),
                      {KoOpenTypeFeatureInfo::GPOS2}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("mark"),
                      toPkString(i18nc("@title", "Mark Positioning")),
                      toPkString(i18nc("@tooltip", "This controls the positioning of mark glyphs on base glyphs. Commonly used for diacritics.")),
                      {KoOpenTypeFeatureInfo::GPOS4, KoOpenTypeFeatureInfo::GPOS5}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("mkmk"),
                      toPkString(i18nc("@title", "Mark Positioning")),
                      toPkString(i18nc("@tooltip", "This controls the positioning of mark glyphs onto other mark glyphs. Commonly used for diacritics.")),
                      {KoOpenTypeFeatureInfo::GPOS6}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("rclt"),
                      toPkString(i18nc("@title", "Required Contextual Alternates")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs contextually with alternates, required to make the font work.")),
                      {KoOpenTypeFeatureInfo::GSUB6}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("rlig"),
                      toPkString(i18nc("@title", "Required Ligatures")),
                      toPkString(i18nc("@tooltip", "Replaces sequences of glyphs with required ligatures.")),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("rvrn"),
                      toPkString(i18nc("@title", "Required Variation Alternates")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs in a variable font with ones suited for the current variation.")),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("size"),
                      toPkString(i18nc("@title", "Optical size")),
                      toPkString(i18nc("@tooltip", "Indicates how much the font is suitable for its current size. Does nothing in Krita.")),
                      {KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("vkrn"),
                      toPkString(i18nc("@title", "Vertical Kerning")),
                      toPkString(i18nc("@tooltip", "This controls vertical kerning on the font.")),
                      {KoOpenTypeFeatureInfo::GPOS2, KoOpenTypeFeatureInfo::GPOS8}));

    // Number and math features.
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("afrc"),
                      toPkString(i18nc("@title", "Alternative Fractions")),
                      toPkString(i18nc("@tooltip", "Replaces figures separated by a slash with a nut fraction form.")),
                      {KoOpenTypeFeatureInfo::GSUB4}, false));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("dnom"),
                      toPkString(i18nc("@title", "Denominators")),
                      toPkString(i18nc("@tooltip", "Replaces figures with forms that are suited to represent the denominator in a fraction.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("dtls"),
                      toPkString(i18nc("@title", "Dotless Forms")),
                      toPkString(i18nc("@tooltip", "Replaces dotted letters such as i and j with dotless forms, for use in mathematical formula.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("flac"),
                      toPkString(i18nc("@title", "Flattened accent forms")),
                      toPkString(i18nc("@tooltip", "Replaces accents on capital letters with flattened forms.")),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("frac"),
                      toPkString(i18nc("@title", "Fractions")),
                      toPkString(i18nc("@tooltip", "Replaces figures separated by a slash with a proper diagonal fraction form. If a font has the numerator and denominator features, and the numbers are separated by a 'fraction slash' (U+2044)), then this will replace the figures with numerators before the slash and denominators after the slash.")),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB4}, false));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("lnum"),
                      toPkString(i18nc("@title", "Lining Figures")),
                      toPkString(i18nc("@tooltip", "Replaces oldstyle figures with lining figures, which harmonize well with uppercase letters.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("mgrk"),
                      toPkString(i18nc("@title", "Mathematical Greek")),
                      toPkString(i18nc("@tooltip", "Replaces normal Greek script with glyphs intended for mathematical usage.")),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("nalt"),
                      toPkString(i18nc("@title", "Alternate Annotation Forms")),
                      toPkString(i18nc("@tooltip", "Replaces figures and letters with notational forms, like encircled.")),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB3}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("numr"),
                      toPkString(i18nc("@title", "Numerators")),
                      toPkString(i18nc("@tooltip", "Replaces figures with forms that are suited to represent the numerator in a fraction.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("onum"),
                      toPkString(i18nc("@title", "Oldstyle Figures")),
                      toPkString(i18nc("@tooltip", "Replaces lining figures with oldstyle figures, which harmonize well with lowercase letters.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("ordn"),
                      toPkString(i18nc("@title", "Ordinals")),
                      toPkString(i18nc("@tooltip", "Replaces letters that follow figures with their ordinal forms.")),
                      {KoOpenTypeFeatureInfo::GSUB6, KoOpenTypeFeatureInfo::GSUB4}, false));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("pnum"),
                      toPkString(i18nc("@title", "Proportional Figures")),
                      toPkString(i18nc("@tooltip", "Replaces tabular figures with proportional figures.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("sinf"),
                      toPkString(i18nc("@title", "Scientific Inferiors")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs with forms that are suited for displaying the number in chemical formulas.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("ssty"),
                      toPkString(i18nc("@title", "Math script style alternates")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs with ones more suited for super- and subscripts.")),
                      {KoOpenTypeFeatureInfo::GSUB3}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("subs"),
                      toPkString(i18nc("@title", "Subscript")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs with ones more suited for subscript.")),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GPOS1, KoOpenTypeFeatureInfo::GPOS2}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("sups"),
                      toPkString(i18nc("@title", "Superscript")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs with ones more suited for superscript.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("tnum"),
                      toPkString(i18nc("@title", "Tabular Figures")),
                      toPkString(i18nc("@tooltip", "Replaces proportional figures with tabular figures.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("zero"),
                      toPkString(i18nc("@title", "Slashed Zero")),
                      toPkString(i18nc("@tooltip", "Replaces the number zero with one that has a slash in the middle, which can help prevent confusion with similar glyphs like the letter 'O'.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));

    // LGC and other bicarmel features.
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("case"),
                      toPkString(i18nc("@title", "Case-Sensitive Forms")),
                      toPkString(i18nc("@tooltip", "Adjusts glyphs to work better with text that consists of only capitals or lining figures.")),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("cpsp"),
                      toPkString(i18nc("@title", "Capital Spacing")),
                      toPkString(i18nc("@tooltip", "Adjusts inter-glyph spacing for all-capital text.")),
                      {KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("c2pc"),
                      toPkString(i18nc("@title", "Petite Capitals From Capitals")),
                      toPkString(i18nc("@tooltip", "Replaces uppercase letters with petite capital forms, which are closer to the x-height of a font than small capital forms.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("c2sc"),
                      toPkString(i18nc("@title", "Small Capitals From Capitals")),
                      toPkString(i18nc("@tooltip", "Replaces uppercase letters with small capital forms.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("pcap"),
                      toPkString(i18nc("@title", "Petite Capitals")),
                      toPkString(i18nc("@tooltip", "Replaces lowercase letters with petite capital forms, which are closer to the x-height of a font than small capital forms.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("smcp"),
                      toPkString(i18nc("@title", "Small Capitals")),
                      toPkString(i18nc("@tooltip", "Replaces lowercase letters with small capital forms.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("titl"),
                      toPkString(i18nc("@title", "Titling")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs with alternate forms suited for header text.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("unic"),
                      toPkString(i18nc("@title", "Unicase")),
                      toPkString(i18nc("@tooltip", "Replaces both upper and lowercase glyphs with unicase glyphs.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));

    // Indic and other brahmic-derived script features.
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("abvf"),
                      toPkString(i18nc("@title", "Above-base Forms")),
                      toPkString(i18nc("@tooltip", "Controls substitution for above base forms in Khmer and other Indic scripts")),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("abvm"),
                      toPkString(i18nc("@title", "Above-base Mark Positioning")),
                      toPkString(i18nc("@tooltip", "Controls above-base mark positioning for Indic scripts.")),
                      {KoOpenTypeFeatureInfo::GPOS4, KoOpenTypeFeatureInfo::GPOS5}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("abvs"),
                      toPkString(i18nc("@title", "Above-base Substitutions")),
                      toPkString(i18nc("@tooltip", "Controls above-base mark ligatures for Indic scripts.")),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("akhn"),
                      toPkString(i18nc("@title", "Akhand")),
                      toPkString(i18nc("@tooltip", "Controls Akhand ligatures for Indic scripts.")),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("blwf"),
                      toPkString(i18nc("@title", "Below-base Forms")),
                      toPkString(i18nc("@tooltip", "Controls substitution for below base forms in Indic scripts.")),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("blwm"),
                      toPkString(i18nc("@title", "Below-base Mark Positioning")),
                      toPkString(i18nc("@tooltip", "Controls below-base mark positioning for Indic scripts.")),
                      {KoOpenTypeFeatureInfo::GPOS4, KoOpenTypeFeatureInfo::GPOS5}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("blws"),
                      toPkString(i18nc("@title", "Below-base Substitutions")),
                      toPkString(i18nc("@tooltip", "Controls below-base mark ligatures for Indic scripts.")),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("cfar"),
                      toPkString(i18nc("@title", "Conjunct Form After Ro")),
                      toPkString(i18nc("@tooltip", "Controls glyphs following conjoined Ro in Khmer script.")),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("cjct"),
                      toPkString(i18nc("@title", "Conjunct Forms")),
                      toPkString(i18nc("@tooltip", "Controls conjunct forms in Indic scripts.")),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("dist"),
                      toPkString(i18nc("@title", "Distances")),
                      toPkString(i18nc("@tooltip", "Controls distances in Indic scripts, to avoid collisions.")),
                      {KoOpenTypeFeatureInfo::GPOS1, KoOpenTypeFeatureInfo::GPOS2}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("half"),
                      toPkString(i18nc("@title", "Half Forms")),
                      toPkString(i18nc("@tooltip", "Replaces consonants in Indic scripts with their half forms.")),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("haln"),
                      toPkString(i18nc("@title", "Halant Forms")),
                      toPkString(i18nc("@tooltip", "Replaces consonants in Indic scripts with their halant forms.")),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("nukt"),
                      toPkString(i18nc("@title", "Nukta Forms")),
                      toPkString(i18nc("@tooltip", "Replaces consonants combined with Nukta in Indic scripts with their Nukta forms.")),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("pref"),
                      toPkString(i18nc("@title", "Pre-base Forms")),
                      toPkString(i18nc("@tooltip", "Replaces the pre-base form of a consonant in Khmer script, when a 'Coeng Ra' situation occurs.")),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("pres"),
                      toPkString(i18nc("@title", "Pre-base Substitutions")),
                      toPkString(i18nc("@tooltip", "Replaces consonants with their pre-base forms in Indic scripts.")),
                      {KoOpenTypeFeatureInfo::GSUB4, KoOpenTypeFeatureInfo::GSUB5}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("pstf"),
                      toPkString(i18nc("@title", "Post-base Forms")),
                      toPkString(i18nc("@tooltip", "Replaces consonants with their post-base forms in Gurmukhi, Malayalam, and Khmer.")),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("psts"),
                      toPkString(i18nc("@title", "Post-base Substitutions")),
                      toPkString(i18nc("@tooltip", "Replaces base glyphs and post-base glyphs with a ligature in Indic scripts.")),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("rkrf"),
                      toPkString(i18nc("@title", "Rakar Forms")),
                      toPkString(i18nc("@tooltip", "Replaces sequences of glyphs in Indic scripts with their Rakar forms.")),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("rphf"),
                      toPkString(i18nc("@title", "Reph Form")),
                      toPkString(i18nc("@tooltip", "Replaces Reph forms in Indic scripts with a consonant and Halant sequence.")),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("vatu"),
                      toPkString(i18nc("@title", "Vattu Variants")),
                      toPkString(i18nc("@tooltip", "Replaces ligatures in Indic scripts with a base consonant and Vattu form.")),
                      {KoOpenTypeFeatureInfo::GSUB4}));

    // CJK features.
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("chws"),
                      toPkString(i18nc("@title", "Contextual Half-width Spacing")),
                      toPkString(i18nc("@tooltip", "Respaces full-width glyphs to be half-width depending on the context, common examples include CJK full-width brackets and other punctuation marks.")),
                      {KoOpenTypeFeatureInfo::GPOS2, KoOpenTypeFeatureInfo::GPOS8}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("cpct"),
                      toPkString(i18nc("@title", "Centered CJK Punctuation")),
                      toPkString(i18nc("@tooltip", "Adjusts non-centered punctuation to be centered in CJK scripts.")),
                      {KoOpenTypeFeatureInfo::GPOS1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("expt"),
                      toPkString(i18nc("@title", "Expert Forms")),
                      toPkString(i18nc("@tooltip", "Replaces standard forms in Japanese fonts with expert forms.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("fwid"),
                      toPkString(i18nc("@title", "Full Widths")),
                      toPkString(i18nc("@tooltip", "Replaces or respaces proportional glyphs with full-width glyphs.")),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GPOS1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("chws"),
                      toPkString(i18nc("@title", "Contextual Half-width Spacing")),
                      toPkString(i18nc("@tooltip", "Respaces full-width glyphs to be half-width, common examples include CJK full-width brackets and other punctuation marks.")),
                      {KoOpenTypeFeatureInfo::GPOS1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("hkna"),
                      toPkString(i18nc("@title", "Horizontal Kana Alternates")),
                      toPkString(i18nc("@tooltip", "Replaces standard kana in Japanese fonts with ones suited for horizontal layout.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("hngl"),
                      toPkString(i18nc("@title", "Hangul")),
                      toPkString(i18nc("@tooltip", "Replaces hanja in Korean with the corresponding hangul. Deprecated.")),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB3}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("hojo"),
                      toPkString(i18nc("@title", "Hojo Kanji Forms")),
                      toPkString(i18nc("@tooltip", "Replaces 'JIS X 0213:2004' form kanji in Japanese fonts with the Hojo ('JIS X 0212-1990')) forms.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("hwid"),
                      toPkString(i18nc("@title", "Half Widths")),
                      toPkString(i18nc("@tooltip", "Replaces or respaces proportional or full-width glyphs with half-width glyphs.")),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("ital"),
                      toPkString(i18nc("@title", "Italics")),
                      toPkString(i18nc("@tooltip", "Replaces Latin glyphs in a CJK font with their italic forms.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("jp78"),
                      toPkString(i18nc("@title", "JIS78 Forms")),
                      toPkString(i18nc("@tooltip", "Replaces standard form kanji in Japanese fonts with the JIS78 forms.")),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB3}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("jp83"),
                      toPkString(i18nc("@title", "JIS83 Forms")),
                      toPkString(i18nc("@tooltip", "Replaces standard form kanji in Japanese fonts with the JIS83 forms.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("jp90"),
                      toPkString(i18nc("@title", "JIS90 Forms")),
                      toPkString(i18nc("@tooltip", "Replaces standard form kanji in Japanese fonts with the JIS90 forms.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("jp04"),
                      toPkString(i18nc("@title", "JIS2004 Forms")),
                      toPkString(i18nc("@tooltip", "Replaces standard form kanji in Japanese fonts with the JIS2004 forms.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("ljmo"),
                      toPkString(i18nc("@title", "Leading Jamo Forms")),
                      toPkString(i18nc("@tooltip", "Replaces sequences of leading class Hangul Jamo with a combined leading form.")),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("nlck"),
                      toPkString(i18nc("@title", "NLC Kanji Forms")),
                      toPkString(i18nc("@tooltip", "Replaces standard form kanji in Japanese fonts with the NLC forms.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("palt"),
                      toPkString(i18nc("@title", "Proportional Alternate Widths")),
                      toPkString(i18nc("@tooltip", "Respaces full-width glyphs to fit in a proportional context.")),
                      {KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("pkna"),
                      toPkString(i18nc("@title", "Proportional Kana")),
                      toPkString(i18nc("@tooltip", "Replaces full-width kana in Japanese fonts with proportional kana.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("pwid"),
                      toPkString(i18nc("@title", "Proportional Widths")),
                      toPkString(i18nc("@tooltip", "Replaces uniform-width glyphs with proportional glyphs.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("qwid"),
                      toPkString(i18nc("@title", "Quarter Widths")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs with quarter-width glyphs.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("ruby"),
                      toPkString(i18nc("@title", "Ruby Notation Forms")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs in CJK fonts with ones suited for the small text in ruby annotations.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("smpl"),
                      toPkString(i18nc("@title", "Simplified Forms")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs in CJK fonts with 'simplified' forms.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("tjmo"),
                      toPkString(i18nc("@title", "Trailing Jamo Forms")),
                      toPkString(i18nc("@tooltip", "Replaces sequences of trailing class Hangul Jamo with a combined trailing form.")),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("tnam"),
                      toPkString(i18nc("@title", "Traditional Name Forms")),
                      toPkString(i18nc("@tooltip", "Replaces standard form Kanji in Japanese fonts with the ones traditionally used in names.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("trad"),
                      toPkString(i18nc("@title", "Traditional Forms")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs in CJK fonts with 'traditional' forms.")),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB3}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("twid"),
                      toPkString(i18nc("@title", "Third Widths")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs with third-width glyphs.")),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GPOS1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("valt"),
                      toPkString(i18nc("@title", "Alternate Vertical Metrics")),
                      toPkString(i18nc("@tooltip", "Repositions glyphs so they look more appropriate in vertical layout.")),
                      {KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("halt"),
                      toPkString(i18nc("@title", "Alternate Half Widths")),
                      toPkString(i18nc("@tooltip", "Repositions glyphs so they look more appropriate in horizontal layout.")),
                      {KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("vchw"),
                      toPkString(i18nc("@title", "Vertical Contextual Half-width Spacing")),
                      toPkString(i18nc("@tooltip", "Respaces full-width glyphs to be half-width depending on the context, common examples include CJK full-width brackets and other punctuation marks.")),
                      {KoOpenTypeFeatureInfo::GPOS2}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("vert"),
                      toPkString(i18nc("@title", "Vertical Alternates")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs in CJK fonts with ones suited for vertical writing.")),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("vhal"),
                      toPkString(i18nc("@title", "Alternate Vertical Half Metrics")),
                      toPkString(i18nc("@tooltip", "Repositions glyphs so they are half-width in a vertical writing context.")),
                      {KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("vjmo"),
                      toPkString(i18nc("@title", "Vowel Jamo Forms")),
                      toPkString(i18nc("@tooltip", "Replaces sequences of vowel class Hangul Jamo with a combined vowel form.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("vkna"),
                      toPkString(i18nc("@title", "Vertical Kana Alternates")),
                      toPkString(i18nc("@tooltip", "Replaces standard kana in Japanese fonts with ones suited for vertical layout.")),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("vpal"),
                      toPkString(i18nc("@title", "Proportional Alternate Vertical Metrics")),
                      toPkString(i18nc("@tooltip", "Repositions glyphs so they are proportional in a vertical writing context.")),
                      {KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("vrtr"),
                      toPkString(i18nc("@title", "Vertical Alternates for Rotation")),
                      toPkString(i18nc("@tooltip", "Replaces a subset of glyphs with rotated variants for vertical layout.")),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("vrt2"),
                      toPkString(i18nc("@title", "Vertical Alternates and Rotation")),
                      toPkString(i18nc("@tooltip", "Replaces all horizontal-script glyphs with rotated variants for vertical layout.")),
                      {KoOpenTypeFeatureInfo::GSUB1}));

    // Arabic and other RTL joined script features
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("curs"),
                      toPkString(i18nc("@title", "Cursive Positioning")),
                      toPkString(i18nc("@tooltip", "Adjusts the position of glyphs so they cursively connect.")),
                      {KoOpenTypeFeatureInfo::GPOS3}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("fina"),
                      toPkString(i18nc("@title", "Terminal Forms")),
                      toPkString(i18nc("@tooltip", "Replaces the final glyph in a sequence of glyphs in a joined script with its end form.")),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB5, KoOpenTypeFeatureInfo::GSUB6, KoOpenTypeFeatureInfo::GSUB8}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("fin2"),
                      toPkString(i18nc("@title", "Terminal Form #2")),
                      toPkString(i18nc("@tooltip", "Replaces the Alaph glyph at the end of Syriac words with its appropriate form.")),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("fin3"),
                      toPkString(i18nc("@title", "Terminal Form #3")),
                      toPkString(i18nc("@tooltip", "Replaces the Alaph glyph at the end of Syriac words with its appropriate form, when neither Terminal forms or Terminal forms 2 are applicable.")),
                      {KoOpenTypeFeatureInfo::GSUB5}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("init"),
                      toPkString(i18nc("@title", "Initial Forms")),
                      toPkString(i18nc("@tooltip", "Replaces the first glyph in a sequence of glyphs in a joined script with its start form.")),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB5, KoOpenTypeFeatureInfo::GSUB6, KoOpenTypeFeatureInfo::GSUB8}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("isol"),
                      toPkString(i18nc("@title", "Initial Forms")),
                      toPkString(i18nc("@tooltip", "Replaces a glyph in a sequence of glyphs in a joined script with its isolated form.")),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB5, KoOpenTypeFeatureInfo::GSUB6, KoOpenTypeFeatureInfo::GSUB8}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("ltra"),
                      toPkString(i18nc("@title", "Left-to-right glyph alternates")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs with their left-to-right alternates.")),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("ltrm"),
                      toPkString(i18nc("@title", "Left-to-right mirrored forms")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs with their left-to-right mirrored forms.")),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("medi"),
                      toPkString(i18nc("@title", "Medial Forms")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs in the middle of a sequence of glyphs in a joined script with their medial forms.")),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB5, KoOpenTypeFeatureInfo::GSUB6, KoOpenTypeFeatureInfo::GSUB8}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("med2"),
                      toPkString(i18nc("@title", "Medial Forms #2")),
                      toPkString(i18nc("@tooltip", "Replaces the Alaph glyph in the middle of Syriac words with its appropriate form.")),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("mset"),
                      toPkString(i18nc("@title", "mset")),
                      toPkString(i18nc("@tooltip", "Positions Arabic combining marks in fonts for Windows 95 using glyph substitution.")),
                      {KoOpenTypeFeatureInfo::GSUB5}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("rtla"),
                      toPkString(i18nc("@title", "Right-to-left glyph alternates")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs with their right-to-left alternates.")),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("rtlm"),
                      toPkString(i18nc("@title", "Right-to-left mirrored forms")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs with their right-to-left mirrored forms.")),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("stch"),
                      toPkString(i18nc("@title", "Stretching Glyph Decomposition")),
                      toPkString(i18nc("@tooltip", "Replaces glyphs that need to stretch with stretchable glyphs.")),
                      {KoOpenTypeFeatureInfo::GSUB2}));

    //stylistic sets
    char tag[4] = {'s', 's', '0', '0'};
    for (int i = 1; i <= 20; i++) {
        tag[2] = (i/10) + '0';
        tag[3] = (i%10) + '0';

        initialMap.append(KoOpenTypeFeatureInfo(PkByteArray(tag, 4),
                          toPkString(i18nc("@title", "Stylistic Set %1", i)),
                          toPkString(i18nc("@tooltip", "Replaces glyphs with alternates.")),
                          {KoOpenTypeFeatureInfo::GSUB1}, true));
    }

    //character variants
    // char *tag{new char[4]{'c', 'v', '0', '0'}};
    tag[0] = 'c';
    tag[1] = 'v';
    for (int i = 1; i <= 99; i++) {
        tag[2] = (i/10) + '0';
        tag[3] = (i%10) + '0';

        initialMap.append(KoOpenTypeFeatureInfo(PkByteArray(tag, 4),
                          toPkString(i18nc("@title", "Character Variant %1", i)),
                          toPkString(i18nc("@tooltip", "Replaces glyphs with alternates.")),
                          {KoOpenTypeFeatureInfo::GSUB1}, true));

    }

    Q_FOREACH(KoOpenTypeFeatureInfo feature, initialMap) {
        d->infoMap.insert(feature.tag, feature);
    }
}

KoOpenTypeFeatureInfoFactory::~KoOpenTypeFeatureInfoFactory()
{
}

KoOpenTypeFeatureInfo KoOpenTypeFeatureInfoFactory::infoByTag(const PkByteArray &tag) const
{
    KoOpenTypeFeatureInfo def(tag, PkString(), PkString(), {});
    return d->infoMap.value(PkString::fromUtf8(tag.data(), int(tag.size())), def);
}

PkList<PkString> KoOpenTypeFeatureInfoFactory::tags() const
{
    return d->infoMap.keys();
}
