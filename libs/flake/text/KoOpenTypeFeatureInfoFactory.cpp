/*
 *  SPDX-FileCopyrightText: 2024 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <PkMap.h>

#include "KoOpenTypeFeatureInfoFactory.h"
#include <PkByteArray.h>

namespace {
PkString kernelText(const char *, const char *text)
{
    return PkString(text);
}

PkString kernelText(const char *, const char *text, int value)
{
    return PkString(text).arg(value);
}
}

struct KoOpenTypeFeatureInfoFactory::Private
{
    PkMap<PkString, KoOpenTypeFeatureInfo> infoMap;
};

KoOpenTypeFeatureInfoFactory::KoOpenTypeFeatureInfoFactory()
    : d(new Private)
{
    PkVector<KoOpenTypeFeatureInfo> initialMap;

    // General discretionary features
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("aalt"),
                      kernelText("@title", "Access All Alternates"),
                      kernelText("@tooltip", "Access any possible substitutions."),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB3},
                      true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("calt"),
                      kernelText("@title", "Contextual Alternates"),
                      kernelText("@tooltip", "Replaces glyphs depending on their context within a text."),
                      {KoOpenTypeFeatureInfo::GSUB6}, false));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("clig"),
                      kernelText("@title", "Contextual Ligatures"),
                      kernelText("@tooltip", "Replaces sequences of glyphs with ligatures depending on their context within a text."),
                      {KoOpenTypeFeatureInfo::GSUB8}, false));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("cswh"),
                      kernelText("@title", "Contextual Swash"),
                      kernelText("@tooltip", "Replaces glyphs with swashed glyphs depending on their context within a text."),
                      {KoOpenTypeFeatureInfo::GSUB8}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("dlig"),
                      kernelText("@title", "Discretionary Ligatures"),
                      kernelText("@tooltip", "Replaces sequences of glyphs with ligatures intended for typographic effect."),
                      {KoOpenTypeFeatureInfo::GSUB4}, false));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("falt"),
                      kernelText("@title", "Final Glyph on Line Alternates"),
                      kernelText("@tooltip", "Replaces glyphs with their line-end forms."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("hist"),
                      kernelText("@title", "Historical Forms"),
                      kernelText("@tooltip", "Replaces glyphs with their historical forms."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("hlig"),
                      kernelText("@title", "Historical Forms"),
                      kernelText("@tooltip", "Replaces sequences glyphs with ligatures that were in use in the past, but rare today."),
                      {KoOpenTypeFeatureInfo::GSUB4}, false));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("jalt"),
                      kernelText("@title", "Justification Alternates"),
                      kernelText("@tooltip", "Replaces glyphs with their alternates meant for justification."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("liga"),
                      kernelText("@title", "Standard Ligatures"),
                      kernelText("@tooltip", "Replaces sequences glyphs with common ligatures."),
                      {KoOpenTypeFeatureInfo::GSUB4}, false));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("lfbd"),
                      kernelText("@title", "Left Bounds"),
                      kernelText("@tooltip", "This adjusts glyphs so there is no space at the left side."),
                      {KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("locl"),
                      kernelText("@title", "Localized Forms"),
                      kernelText("@tooltip", "This replaces glyphs with language specific versions."),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("opbd"),
                      kernelText("@title", "Optical Bounds"),
                      kernelText("@tooltip", "Adjusts glyphs so they align by their optical bounds. Doesn't do anything in Krita."),
                      {KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("ornm"),
                      kernelText("@title", "Ornaments"),
                      kernelText("@tooltip", "Replaces glyphs with ornaments for decorative purposes."),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB3}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("rand"),
                      kernelText("@title", "Randomize"),
                      kernelText("@tooltip", "Replaces glyphs with random alternates."),
                      {KoOpenTypeFeatureInfo::GSUB3}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("rtbd"),
                      kernelText("@title", "Right Bounds"),
                      kernelText("@tooltip", "This adjusts glyphs so there is no space at the right side."),
                      {KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("salt"),
                      kernelText("@title", "Stylistic Alternates"),
                      kernelText("@tooltip", "Replaces glyphs with stylistic alternates that do not fit in other categories."),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB3}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("swsh"),
                      kernelText("@title", "Swash"),
                      kernelText("@tooltip", "Replaces glyphs with swashed alternates."),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB3}, true));

    // General required features.
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("ccmp"),
                      kernelText("@title", "Glyph Composition/Decomposition"),
                      kernelText("@tooltip", "This composes or decomposes graphemes so that the resulting glyphs can later be recomposed. Commonly used to handle diacritics."),
                      {KoOpenTypeFeatureInfo::GSUB2}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("kern"),
                      kernelText("@title", "Kerning"),
                      kernelText("@tooltip", "This controls kerning on the font."),
                      {KoOpenTypeFeatureInfo::GPOS2}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("mark"),
                      kernelText("@title", "Mark Positioning"),
                      kernelText("@tooltip", "This controls the positioning of mark glyphs on base glyphs. Commonly used for diacritics."),
                      {KoOpenTypeFeatureInfo::GPOS4, KoOpenTypeFeatureInfo::GPOS5}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("mkmk"),
                      kernelText("@title", "Mark Positioning"),
                      kernelText("@tooltip", "This controls the positioning of mark glyphs onto other mark glyphs. Commonly used for diacritics."),
                      {KoOpenTypeFeatureInfo::GPOS6}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("rclt"),
                      kernelText("@title", "Required Contextual Alternates"),
                      kernelText("@tooltip", "Replaces glyphs contextually with alternates, required to make the font work."),
                      {KoOpenTypeFeatureInfo::GSUB6}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("rlig"),
                      kernelText("@title", "Required Ligatures"),
                      kernelText("@tooltip", "Replaces sequences of glyphs with required ligatures."),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("rvrn"),
                      kernelText("@title", "Required Variation Alternates"),
                      kernelText("@tooltip", "Replaces glyphs in a variable font with ones suited for the current variation."),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("size"),
                      kernelText("@title", "Optical size"),
                      kernelText("@tooltip", "Indicates how much the font is suitable for its current size. Does nothing in Krita."),
                      {KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("vkrn"),
                      kernelText("@title", "Vertical Kerning"),
                      kernelText("@tooltip", "This controls vertical kerning on the font."),
                      {KoOpenTypeFeatureInfo::GPOS2, KoOpenTypeFeatureInfo::GPOS8}));

    // Number and math features.
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("afrc"),
                      kernelText("@title", "Alternative Fractions"),
                      kernelText("@tooltip", "Replaces figures separated by a slash with a nut fraction form."),
                      {KoOpenTypeFeatureInfo::GSUB4}, false));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("dnom"),
                      kernelText("@title", "Denominators"),
                      kernelText("@tooltip", "Replaces figures with forms that are suited to represent the denominator in a fraction."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("dtls"),
                      kernelText("@title", "Dotless Forms"),
                      kernelText("@tooltip", "Replaces dotted letters such as i and j with dotless forms, for use in mathematical formula."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("flac"),
                      kernelText("@title", "Flattened accent forms"),
                      kernelText("@tooltip", "Replaces accents on capital letters with flattened forms."),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("frac"),
                      kernelText("@title", "Fractions"),
                      kernelText("@tooltip", "Replaces figures separated by a slash with a proper diagonal fraction form. If a font has the numerator and denominator features, and the numbers are separated by a 'fraction slash' (U+2044), then this will replace the figures with numerators before the slash and denominators after the slash."),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB4}, false));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("lnum"),
                      kernelText("@title", "Lining Figures"),
                      kernelText("@tooltip", "Replaces oldstyle figures with lining figures, which harmonize well with uppercase letters."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("mgrk"),
                      kernelText("@title", "Mathematical Greek"),
                      kernelText("@tooltip", "Replaces normal Greek script with glyphs intended for mathematical usage."),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("nalt"),
                      kernelText("@title", "Alternate Annotation Forms"),
                      kernelText("@tooltip", "Replaces figures and letters with notational forms, like encircled."),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB3}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("numr"),
                      kernelText("@title", "Numerators"),
                      kernelText("@tooltip", "Replaces figures with forms that are suited to represent the numerator in a fraction."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("onum"),
                      kernelText("@title", "Oldstyle Figures"),
                      kernelText("@tooltip", "Replaces lining figures with oldstyle figures, which harmonize well with lowercase letters."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("ordn"),
                      kernelText("@title", "Ordinals"),
                      kernelText("@tooltip", "Replaces letters that follow figures with their ordinal forms."),
                      {KoOpenTypeFeatureInfo::GSUB6, KoOpenTypeFeatureInfo::GSUB4}, false));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("pnum"),
                      kernelText("@title", "Proportional Figures"),
                      kernelText("@tooltip", "Replaces tabular figures with proportional figures."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("sinf"),
                      kernelText("@title", "Scientific Inferiors"),
                      kernelText("@tooltip", "Replaces glyphs with forms that are suited for displaying the number in chemical formulas."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("ssty"),
                      kernelText("@title", "Math script style alternates"),
                      kernelText("@tooltip", "Replaces glyphs with ones more suited for super- and subscripts."),
                      {KoOpenTypeFeatureInfo::GSUB3}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("subs"),
                      kernelText("@title", "Subscript"),
                      kernelText("@tooltip", "Replaces glyphs with ones more suited for subscript."),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GPOS1, KoOpenTypeFeatureInfo::GPOS2}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("sups"),
                      kernelText("@title", "Superscript"),
                      kernelText("@tooltip", "Replaces glyphs with ones more suited for superscript."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("tnum"),
                      kernelText("@title", "Tabular Figures"),
                      kernelText("@tooltip", "Replaces proportional figures with tabular figures."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("zero"),
                      kernelText("@title", "Slashed Zero"),
                      kernelText("@tooltip", "Replaces the number zero with one that has a slash in the middle, which can help prevent confusion with similar glyphs like the letter 'O'."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));

    // LGC and other bicarmel features.
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("case"),
                      kernelText("@title", "Case-Sensitive Forms"),
                      kernelText("@tooltip", "Adjusts glyphs to work better with text that consists of only capitals or lining figures."),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("cpsp"),
                      kernelText("@title", "Capital Spacing"),
                      kernelText("@tooltip", "Adjusts inter-glyph spacing for all-capital text."),
                      {KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("c2pc"),
                      kernelText("@title", "Petite Capitals From Capitals"),
                      kernelText("@tooltip", "Replaces uppercase letters with petite capital forms, which are closer to the x-height of a font than small capital forms."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("c2sc"),
                      kernelText("@title", "Small Capitals From Capitals"),
                      kernelText("@tooltip", "Replaces uppercase letters with small capital forms."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("pcap"),
                      kernelText("@title", "Petite Capitals"),
                      kernelText("@tooltip", "Replaces lowercase letters with petite capital forms, which are closer to the x-height of a font than small capital forms."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("smcp"),
                      kernelText("@title", "Small Capitals"),
                      kernelText("@tooltip", "Replaces lowercase letters with small capital forms."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("titl"),
                      kernelText("@title", "Titling"),
                      kernelText("@tooltip", "Replaces glyphs with alternate forms suited for header text."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("unic"),
                      kernelText("@title", "Unicase"),
                      kernelText("@tooltip", "Replaces both upper and lowercase glyphs with unicase glyphs."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));

    // Indic and other brahmic-derived script features.
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("abvf"),
                      kernelText("@title", "Above-base Forms"),
                      kernelText("@tooltip", "Controls substitution for above base forms in Khmer and other Indic scripts"),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("abvm"),
                      kernelText("@title", "Above-base Mark Positioning"),
                      kernelText("@tooltip", "Controls above-base mark positioning for Indic scripts."),
                      {KoOpenTypeFeatureInfo::GPOS4, KoOpenTypeFeatureInfo::GPOS5}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("abvs"),
                      kernelText("@title", "Above-base Substitutions"),
                      kernelText("@tooltip", "Controls above-base mark ligatures for Indic scripts."),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("akhn"),
                      kernelText("@title", "Akhand"),
                      kernelText("@tooltip", "Controls Akhand ligatures for Indic scripts."),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("blwf"),
                      kernelText("@title", "Below-base Forms"),
                      kernelText("@tooltip", "Controls substitution for below base forms in Indic scripts."),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("blwm"),
                      kernelText("@title", "Below-base Mark Positioning"),
                      kernelText("@tooltip", "Controls below-base mark positioning for Indic scripts."),
                      {KoOpenTypeFeatureInfo::GPOS4, KoOpenTypeFeatureInfo::GPOS5}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("blws"),
                      kernelText("@title", "Below-base Substitutions"),
                      kernelText("@tooltip", "Controls below-base mark ligatures for Indic scripts."),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("cfar"),
                      kernelText("@title", "Conjunct Form After Ro"),
                      kernelText("@tooltip", "Controls glyphs following conjoined Ro in Khmer script."),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("cjct"),
                      kernelText("@title", "Conjunct Forms"),
                      kernelText("@tooltip", "Controls conjunct forms in Indic scripts."),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("dist"),
                      kernelText("@title", "Distances"),
                      kernelText("@tooltip", "Controls distances in Indic scripts, to avoid collisions."),
                      {KoOpenTypeFeatureInfo::GPOS1, KoOpenTypeFeatureInfo::GPOS2}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("half"),
                      kernelText("@title", "Half Forms"),
                      kernelText("@tooltip", "Replaces consonants in Indic scripts with their half forms."),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("haln"),
                      kernelText("@title", "Halant Forms"),
                      kernelText("@tooltip", "Replaces consonants in Indic scripts with their halant forms."),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("nukt"),
                      kernelText("@title", "Nukta Forms"),
                      kernelText("@tooltip", "Replaces consonants combined with Nukta in Indic scripts with their Nukta forms."),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("pref"),
                      kernelText("@title", "Pre-base Forms"),
                      kernelText("@tooltip", "Replaces the pre-base form of a consonant in Khmer script, when a 'Coeng Ra' situation occurs."),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("pres"),
                      kernelText("@title", "Pre-base Substitutions"),
                      kernelText("@tooltip", "Replaces consonants with their pre-base forms in Indic scripts."),
                      {KoOpenTypeFeatureInfo::GSUB4, KoOpenTypeFeatureInfo::GSUB5}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("pstf"),
                      kernelText("@title", "Post-base Forms"),
                      kernelText("@tooltip", "Replaces consonants with their post-base forms in Gurmukhi, Malayalam, and Khmer."),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("psts"),
                      kernelText("@title", "Post-base Substitutions"),
                      kernelText("@tooltip", "Replaces base glyphs and post-base glyphs with a ligature in Indic scripts."),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("rkrf"),
                      kernelText("@title", "Rakar Forms"),
                      kernelText("@tooltip", "Replaces sequences of glyphs in Indic scripts with their Rakar forms."),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("rphf"),
                      kernelText("@title", "Reph Form"),
                      kernelText("@tooltip", "Replaces Reph forms in Indic scripts with a consonant and Halant sequence."),
                      {KoOpenTypeFeatureInfo::GSUB4}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("vatu"),
                      kernelText("@title", "Vattu Variants"),
                      kernelText("@tooltip", "Replaces ligatures in Indic scripts with a base consonant and Vattu form."),
                      {KoOpenTypeFeatureInfo::GSUB4}));

    // CJK features.
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("chws"),
                      kernelText("@title", "Contextual Half-width Spacing"),
                      kernelText("@tooltip", "Respaces full-width glyphs to be half-width depending on the context, common examples include CJK full-width brackets and other punctuation marks."),
                      {KoOpenTypeFeatureInfo::GPOS2, KoOpenTypeFeatureInfo::GPOS8}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("cpct"),
                      kernelText("@title", "Centered CJK Punctuation"),
                      kernelText("@tooltip", "Adjusts non-centered punctuation to be centered in CJK scripts."),
                      {KoOpenTypeFeatureInfo::GPOS1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("expt"),
                      kernelText("@title", "Expert Forms"),
                      kernelText("@tooltip", "Replaces standard forms in Japanese fonts with expert forms."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("fwid"),
                      kernelText("@title", "Full Widths"),
                      kernelText("@tooltip", "Replaces or respaces proportional glyphs with full-width glyphs."),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GPOS1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("chws"),
                      kernelText("@title", "Contextual Half-width Spacing"),
                      kernelText("@tooltip", "Respaces full-width glyphs to be half-width, common examples include CJK full-width brackets and other punctuation marks."),
                      {KoOpenTypeFeatureInfo::GPOS1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("hkna"),
                      kernelText("@title", "Horizontal Kana Alternates"),
                      kernelText("@tooltip", "Replaces standard kana in Japanese fonts with ones suited for horizontal layout."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("hngl"),
                      kernelText("@title", "Hangul"),
                      kernelText("@tooltip", "Replaces hanja in Korean with the corresponding hangul. Deprecated."),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB3}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("hojo"),
                      kernelText("@title", "Hojo Kanji Forms"),
                      kernelText("@tooltip", "Replaces 'JIS X 0213:2004' form kanji in Japanese fonts with the Hojo ('JIS X 0212-1990') forms."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("hwid"),
                      kernelText("@title", "Half Widths"),
                      kernelText("@tooltip", "Replaces or respaces proportional or full-width glyphs with half-width glyphs."),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("ital"),
                      kernelText("@title", "Italics"),
                      kernelText("@tooltip", "Replaces Latin glyphs in a CJK font with their italic forms."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("jp78"),
                      kernelText("@title", "JIS78 Forms"),
                      kernelText("@tooltip", "Replaces standard form kanji in Japanese fonts with the JIS78 forms."),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB3}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("jp83"),
                      kernelText("@title", "JIS83 Forms"),
                      kernelText("@tooltip", "Replaces standard form kanji in Japanese fonts with the JIS83 forms."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("jp90"),
                      kernelText("@title", "JIS90 Forms"),
                      kernelText("@tooltip", "Replaces standard form kanji in Japanese fonts with the JIS90 forms."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("jp04"),
                      kernelText("@title", "JIS2004 Forms"),
                      kernelText("@tooltip", "Replaces standard form kanji in Japanese fonts with the JIS2004 forms."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("ljmo"),
                      kernelText("@title", "Leading Jamo Forms"),
                      kernelText("@tooltip", "Replaces sequences of leading class Hangul Jamo with a combined leading form."),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("nlck"),
                      kernelText("@title", "NLC Kanji Forms"),
                      kernelText("@tooltip", "Replaces standard form kanji in Japanese fonts with the NLC forms."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("palt"),
                      kernelText("@title", "Proportional Alternate Widths"),
                      kernelText("@tooltip", "Respaces full-width glyphs to fit in a proportional context."),
                      {KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("pkna"),
                      kernelText("@title", "Proportional Kana"),
                      kernelText("@tooltip", "Replaces full-width kana in Japanese fonts with proportional kana."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("pwid"),
                      kernelText("@title", "Proportional Widths"),
                      kernelText("@tooltip", "Replaces uniform-width glyphs with proportional glyphs."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("qwid"),
                      kernelText("@title", "Quarter Widths"),
                      kernelText("@tooltip", "Replaces glyphs with quarter-width glyphs."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("ruby"),
                      kernelText("@title", "Ruby Notation Forms"),
                      kernelText("@tooltip", "Replaces glyphs in CJK fonts with ones suited for the small text in ruby annotations."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("smpl"),
                      kernelText("@title", "Simplified Forms"),
                      kernelText("@tooltip", "Replaces glyphs in CJK fonts with 'simplified' forms."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("tjmo"),
                      kernelText("@title", "Trailing Jamo Forms"),
                      kernelText("@tooltip", "Replaces sequences of trailing class Hangul Jamo with a combined trailing form."),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("tnam"),
                      kernelText("@title", "Traditional Name Forms"),
                      kernelText("@tooltip", "Replaces standard form Kanji in Japanese fonts with the ones traditionally used in names."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("trad"),
                      kernelText("@title", "Traditional Forms"),
                      kernelText("@tooltip", "Replaces glyphs in CJK fonts with 'traditional' forms."),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB3}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("twid"),
                      kernelText("@title", "Third Widths"),
                      kernelText("@tooltip", "Replaces glyphs with third-width glyphs."),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GPOS1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("valt"),
                      kernelText("@title", "Alternate Vertical Metrics"),
                      kernelText("@tooltip", "Repositions glyphs so they look more appropriate in vertical layout."),
                      {KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("halt"),
                      kernelText("@title", "Alternate Half Widths"),
                      kernelText("@tooltip", "Repositions glyphs so they look more appropriate in horizontal layout."),
                      {KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("vchw"),
                      kernelText("@title", "Vertical Contextual Half-width Spacing"),
                      kernelText("@tooltip", "Respaces full-width glyphs to be half-width depending on the context, common examples include CJK full-width brackets and other punctuation marks."),
                      {KoOpenTypeFeatureInfo::GPOS2}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("vert"),
                      kernelText("@title", "Vertical Alternates"),
                      kernelText("@tooltip", "Replaces glyphs in CJK fonts with ones suited for vertical writing."),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("vhal"),
                      kernelText("@title", "Alternate Vertical Half Metrics"),
                      kernelText("@tooltip", "Repositions glyphs so they are half-width in a vertical writing context."),
                      {KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("vjmo"),
                      kernelText("@title", "Vowel Jamo Forms"),
                      kernelText("@tooltip", "Replaces sequences of vowel class Hangul Jamo with a combined vowel form."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("vkna"),
                      kernelText("@title", "Vertical Kana Alternates"),
                      kernelText("@tooltip", "Replaces standard kana in Japanese fonts with ones suited for vertical layout."),
                      {KoOpenTypeFeatureInfo::GSUB1}, true));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("vpal"),
                      kernelText("@title", "Proportional Alternate Vertical Metrics"),
                      kernelText("@tooltip", "Repositions glyphs so they are proportional in a vertical writing context."),
                      {KoOpenTypeFeatureInfo::GPOS1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("vrtr"),
                      kernelText("@title", "Vertical Alternates for Rotation"),
                      kernelText("@tooltip", "Replaces a subset of glyphs with rotated variants for vertical layout."),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("vrt2"),
                      kernelText("@title", "Vertical Alternates and Rotation"),
                      kernelText("@tooltip", "Replaces all horizontal-script glyphs with rotated variants for vertical layout."),
                      {KoOpenTypeFeatureInfo::GSUB1}));

    // Arabic and other RTL joined script features
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("curs"),
                      kernelText("@title", "Cursive Positioning"),
                      kernelText("@tooltip", "Adjusts the position of glyphs so they cursively connect."),
                      {KoOpenTypeFeatureInfo::GPOS3}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("fina"),
                      kernelText("@title", "Terminal Forms"),
                      kernelText("@tooltip", "Replaces the final glyph in a sequence of glyphs in a joined script with its end form."),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB5, KoOpenTypeFeatureInfo::GSUB6, KoOpenTypeFeatureInfo::GSUB8}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("fin2"),
                      kernelText("@title", "Terminal Form #2"),
                      kernelText("@tooltip", "Replaces the Alaph glyph at the end of Syriac words with its appropriate form."),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("fin3"),
                      kernelText("@title", "Terminal Form #3"),
                      kernelText("@tooltip", "Replaces the Alaph glyph at the end of Syriac words with its appropriate form, when neither Terminal forms or Terminal forms 2 are applicable."),
                      {KoOpenTypeFeatureInfo::GSUB5}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("init"),
                      kernelText("@title", "Initial Forms"),
                      kernelText("@tooltip", "Replaces the first glyph in a sequence of glyphs in a joined script with its start form."),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB5, KoOpenTypeFeatureInfo::GSUB6, KoOpenTypeFeatureInfo::GSUB8}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("isol"),
                      kernelText("@title", "Initial Forms"),
                      kernelText("@tooltip", "Replaces a glyph in a sequence of glyphs in a joined script with its isolated form."),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB5, KoOpenTypeFeatureInfo::GSUB6, KoOpenTypeFeatureInfo::GSUB8}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("ltra"),
                      kernelText("@title", "Left-to-right glyph alternates"),
                      kernelText("@tooltip", "Replaces glyphs with their left-to-right alternates."),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("ltrm"),
                      kernelText("@title", "Left-to-right mirrored forms"),
                      kernelText("@tooltip", "Replaces glyphs with their left-to-right mirrored forms."),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("medi"),
                      kernelText("@title", "Medial Forms"),
                      kernelText("@tooltip", "Replaces glyphs in the middle of a sequence of glyphs in a joined script with their medial forms."),
                      {KoOpenTypeFeatureInfo::GSUB1, KoOpenTypeFeatureInfo::GSUB5, KoOpenTypeFeatureInfo::GSUB6, KoOpenTypeFeatureInfo::GSUB8}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("med2"),
                      kernelText("@title", "Medial Forms #2"),
                      kernelText("@tooltip", "Replaces the Alaph glyph in the middle of Syriac words with its appropriate form."),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("mset"),
                      kernelText("@title", "mset"),
                      kernelText("@tooltip", "Positions Arabic combining marks in fonts for Windows 95 using glyph substitution."),
                      {KoOpenTypeFeatureInfo::GSUB5}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("rtla"),
                      kernelText("@title", "Right-to-left glyph alternates"),
                      kernelText("@tooltip", "Replaces glyphs with their right-to-left alternates."),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("rtlm"),
                      kernelText("@title", "Right-to-left mirrored forms"),
                      kernelText("@tooltip", "Replaces glyphs with their right-to-left mirrored forms."),
                      {KoOpenTypeFeatureInfo::GSUB1}));
    initialMap.append(KoOpenTypeFeatureInfo(PkByteArray("stch"),
                      kernelText("@title", "Stretching Glyph Decomposition"),
                      kernelText("@tooltip", "Replaces glyphs that need to stretch with stretchable glyphs."),
                      {KoOpenTypeFeatureInfo::GSUB2}));

    //stylistic sets
    char tag[4] = {'s', 's', '0', '0'};
    for (int i = 1; i <= 20; i++) {
        tag[2] = (i/10) + '0';
        tag[3] = (i%10) + '0';

        initialMap.append(KoOpenTypeFeatureInfo(PkByteArray(tag, 4),
                          kernelText("@title", "Stylistic Set %1", i),
                          kernelText("@tooltip", "Replaces glyphs with alternates."),
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
                          kernelText("@title", "Character Variant %1", i),
                          kernelText("@tooltip", "Replaces glyphs with alternates."),
                          {KoOpenTypeFeatureInfo::GSUB1}, true));

    }

    for (const KoOpenTypeFeatureInfo &feature : initialMap) {
        d->infoMap.insert(PkString::fromUtf8(feature.tag.data(), int(feature.tag.size())), feature);
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
