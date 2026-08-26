/*
 *  SPDX-FileCopyrightText: 2025 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <QtCore/QtCore>

#include "psd_text_data_converter.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H

#include <PkFlakeBridge.h>

#include <KoCSSFontInfo.h>
#include <KoSvgTextProperties.h>
#include <KoPathSegment.h>
#include <KoPathPoint.h>
#include <KoFontRegistry.h>
#include <KoShape.h>
#include <KoPathShape.h>

#include <KoColorSpace.h>
#include <KoColor.h>

#include <PkStringList.h>
#include <PkTransform.h>
#include <PkXmlDocument.h>
#include <PkXmlStreamWriter.h>

#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

PkString pkNum(int v)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", v);
    return PkString(buf);
}

PkString pkNum(double v)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%g", v);
    return PkString(buf);
}

PkVariant pkHashValue(const PkVariantHash &hash, const PkString &key)
{
    const auto it = hash.find(key);
    return (it != hash.end()) ? it->second : PkVariant();
}

PkStringList pkSplit(const PkString &s, char16_t sep)
{
    const std::vector<PkString> parts = s.split(sep);
    PkStringList result;
    for (size_t i = 0; i < parts.size(); i++) {
        result.append(parts[i]);
    }
    return result;
}

bool pkListContains(const PkStringList &list, const PkString &s)
{
    for (int i = 0; i < list.size(); i++) {
        if (list.at(i) == s) {
            return true;
        }
    }
    return false;
}

bool fontSlantIsNormal(const KoCSSFontInfo &fontInfo)
{
    return fontInfo.slantMode == 0;
}

#ifdef QT_CORE_LIB
PkStringList pkFromNativeFamilies(const PK_CAT_(Q, StringList) &families)
{
    PkStringList result;
    for (int i = 0; i < families.size(); i++) {
        result.append(toPkString(families.at(i)));
    }
    return result;
}

PK_CAT_(Q, StringList) pkToNativeFamilies(const PkStringList &families)
{
    PK_CAT_(Q, StringList) result;
    for (int i = 0; i < families.size(); i++) {
        result.append(toQString(families.at(i)));
    }
    return result;
}
#else
PkStringList pkFromNativeFamilies(const PkStringList &families) { return families; }
PkStringList pkToNativeFamilies(const PkStringList &families) { return families; }
#endif

} // namespace
struct PsdTextDataConverter::Private {

    PkStringList errors;
    PkStringList warnings;

    void clearErrors() {
        errors.clear();
        warnings.clear();
    }
};
PsdTextDataConverter::PsdTextDataConverter()
    : d(new Private)
{

}

PsdTextDataConverter::~PsdTextDataConverter()
{

}


PkColor PsdTextDataConverter::colorFromPSDStyleSheet(PkVariantHash color, const KoColorSpace *imageCs) {
    PkColor c(Qt::black);
    if (color.count("/Color") > 0) {
        color = color["/Color"].toHash();
    }
    PkXmlDocument doc;
    PkXmlElement root;
    PkVariantList values = pkHashValue(color, "/Values").toList();
    if (pkHashValue(color, "/Type").toInt() == 0) { //graya
        root = doc.createElement("Gray");
        root.setAttribute("g", pkNum(values.at(1).toDouble()));
    } else if (pkHashValue(color, "/Type").toInt() == 2) { // CMYK
        root = doc.createElement("CMYK");
        root.setAttribute("c", pkNum(values.at(1).toDouble()));
        root.setAttribute("m", pkNum(values.at(2).toDouble()));
        root.setAttribute("y", pkNum(values.at(3).toDouble()));
        root.setAttribute("k", pkNum(values.at(4).toDouble()));
    } else if (pkHashValue(color, "/Type").toInt() == 3) { // LAB
        root = doc.createElement("Lab");
        root.setAttribute("L", pkNum(values.at(1).toDouble()));
        root.setAttribute("a", pkNum(values.at(2).toDouble()));
        root.setAttribute("b", pkNum(values.at(3).toDouble()));
    } else if (pkHashValue(color, "/Type").toInt() == 1) {
        root = doc.createElement("RGB");
        root.setAttribute("r", pkNum(values.at(1).toDouble()));
        root.setAttribute("g", pkNum(values.at(2).toDouble()));
        root.setAttribute("b", pkNum(values.at(3).toDouble()));
    }
    KoColor final = KoColor::fromXML(root, "U8");
    if (final.colorSpace()->colorModelId() == imageCs->colorModelId()) {
        final.setProfile(imageCs->profile());
    }
    final.toQColor(&c);
    return c;
}

// language is one of pt, pt-BR, fr, fr-CA, de, de-1901, gsw, nl, en-UK, en-US, fi, it, nb, nn, es, sv
static PkHash <int, PkString> psdLanguageMap {
    {0, "en-US"},   // US English
    {1, "fi"},      // Finnish
    {2, "fr"},      // French
    {3, "fr-CA"},   // Canadian French
    {4, "de"},      // German
    {5, "de-1901"}, // German before spelling reform
    {6, "gsw"},     // Swiss German
    {7, "it"},      // Italian
    {8, "nb"},      //Norwegian
    {9, "nn"},      // Norsk (nynorsk)
    {10, "pt"},     // Portuguese
    {11, "pt-BR"},  // Brazilian Portuguese
    {12, "es"},     // Spansh
    {13, "sv"},     // Swedish
    {14, "en-UK"},  // British English
    {15, "nl"},     // Dutch
    {16, "da"},     // Danish
    //{17, ""},
    {18, "ru"},     // Russian
    //{19, ""},
    //{20, ""},
    //{21, ""},
    {22, "cs"},     // Czech
    {23, "pl"},     // Polish
    //{24, ""},
    {25, "el"},     // Greek
    {26, "tr"},     // Turkish
    //{27, ""},
    {28, "hu"},     // Hungarian
};

PkString PsdTextDataConverter::stylesForPSDStyleSheet(PkString &lang, PkVariantHash PSDStyleSheet, PkMap<int, KoCSSFontInfo> fontNames, PkTransform scale, const KoColorSpace *imageCs) {
    PkStringList styles;

    PkStringList unsupportedStyles;

    int weight = 400;
    bool italic = false;
    PkStringList textDecor;
    PkStringList baselineShift;
    PkStringList fontVariantLigatures;
    PkStringList fontVariantNumeric;
    PkStringList fontVariantCaps;
    PkStringList fontVariantEastAsian;
    PkStringList fontFeatureSettings;
    PkString underlinePos;
    for (const auto &pss : PSDStyleSheet) {
        const PkString key = pss.first;
        const PkVariant &pssVal = pss.second;
        if (key == "/Font") {
            KoCSSFontInfo fontInfo = fontNames.value(pssVal.toInt());
            weight = fontInfo.weight;
            italic = italic? true: !fontSlantIsNormal(fontInfo);
            styles.append(PkString("font-family:")+pkFromNativeFamilies(fontInfo.families).join(","));
            if (fontInfo.width != 100) {
                styles.append(PkString("font-width:")+pkNum(fontInfo.width));
            }
            continue;
        } else if (key == "/FontSize") {
            double val = pssVal.toDouble();
            val = scale.map(PkPointF(val, val)).y();
            styles.append(PkString("font-size:")+pkNum(val));
            continue;
        } else if (key == "/AutoKerning" || key == "/AutoKern") {
            if (!pssVal.toBool()) {
                styles.append("font-kerning: none");
            }
            continue;
        } else if (key == "/Kerning") {
            // adjusts kerning value, we don't support this.
            unsupportedStyles << key;
            continue;
        } else if (key == "/FauxBold") {
            if (pssVal.toBool()) {
                weight = 700;
            }
            continue;
        } else if (key == "/FauxItalic") {
            if (pssVal.toBool()) {
                italic = true;
            }
            // synthetic Italic, bool
            continue;
        } else if (key == "/Leading") {
            bool autoleading = true;
            if (PSDStyleSheet.count("AutoLeading") > 0) {
                autoleading = pkHashValue(PSDStyleSheet, "AutoLeading").toBool();
            }
            if (!autoleading) {
                double fontSize = pkHashValue(PSDStyleSheet, "FontSize").toDouble();
                double val = pssVal.toDouble();
                styles.append(PkString("line-height:")+pkNum(val/fontSize));
            }
            // value for line-height
            continue;
        } else if (key == "/HorizontalScale" || key == "/VerticalScale") {
            // adjusts scale glyphs, we don't support this.
            unsupportedStyles << key;
            continue;
        } else if (key == "/Tracking") {
            // tracking is in 1/1000 of an EM (as is kerning for that matter...)
            double letterSpacing = (0.001 * pssVal.toDouble());
            styles.append(PkString("letter-spacing:")+pkNum(letterSpacing)+PkString("em"));
            continue;
        } else if (key == "/BaselineShift") {
            if (pssVal.toDouble() > 0) {
                double val = pssVal.toDouble();
                val = scale.map(PkPointF(val, val)).y();
                baselineShift.append(pkNum(val));
            }
            continue;
        } else if (key == "/FontCaps") {
            switch (pssVal.toInt()) {
            case 0:
                break;
            case 1:
                fontVariantCaps.append("all-small-caps");
                break;
            case 2:
                styles.append("text-transform:uppercase");
                break;
            default:
                d->warnings << PkString("Unknown value for %1: %2").arg(key).arg(pssVal.toString());
            }
            continue;
        } else if (key == "/FontBaseline") {
            // NOTE: This might also be better done with font-variant-position, though
            // we don't support synthetic font stuff, including super and sub script.
            // Actually, seems like this is specifically font-synthesis
            switch (pssVal.toInt()) {
            case 0:
                break;
            case 1:
                baselineShift.append("super");
                break;
            case 2:
                baselineShift.append("sub");
                break;
            default:
                d->warnings << PkString("Unknown value for %1: %2").arg(key).arg(pssVal.toString());
            }
            continue;
        } else if (key == "/FontOTPosition") {
            // NOTE: This might also be better done with font-variant-position, though
            // we don't support synthetic font stuff, including super and sub script.
            switch (pssVal.toInt()) {
            case 0:
                break;
            case 1:
                styles.append("font-variant-position:super");
                break;
            case 2:
                styles.append("font-variant-position:sub");
                break;
            case 3:
                fontFeatureSettings.append("'numr' 1");
                break;
            case 4:
                fontFeatureSettings.append("'dnum' 1");
                break;
            default:
                d->warnings << PkString("Unknown value for %1: %2").arg(key).arg(pssVal.toString());
            }
            continue;
        } else if (key == "/Underline") {
            if (pssVal.toBool()) {
                textDecor.append("underline");
            }
            continue;
        }  else if (key == "/UnderlinePosition") {
            switch (pssVal.toInt()) {
            case 0:
                break;
            case 1:
                textDecor.append("underline");
                underlinePos = "auto left";
                break;
            case 2:
                textDecor.append("underline");
                underlinePos = "auto right";
                break;
            default:
                d->warnings << PkString("Unknown value for %1: %1").arg(key).arg(pssVal.toString());
            }
            continue;
        } else if (key == "/YUnderline") {
            // Option relating to vertical underline left or right
            if (pssVal.toInt() == 1) {
                underlinePos = "auto left";
            } else if (pssVal.toInt() == 0) {
                underlinePos = "auto right";
            }
            continue;
        } else if (key == "/Strikethrough" || key == "/StrikethroughPosition") {
            if (pssVal.toBool()) {
                textDecor.append("line-through");
            }
            continue;
        } else if (key == "/Ligatures") {
            if (!pssVal.toBool()) {
                fontVariantLigatures.append("no-common-ligatures");
            }
            continue;
        } else if (key == "/DLigatures" || key == "/DiscretionaryLigatures" || key == "/AlternateLigatures") {
            if (pssVal.toBool()) {
                fontVariantLigatures.append("discretionary-ligatures");
            }
            continue;
        } else if (key == "/ContextualLigatures") {
            if (pssVal.toBool()) {
                fontVariantLigatures.append("contextual");
            }
            continue;
        } else if (key == "/Fractions") {
            if (pssVal.toBool()) {
                fontVariantNumeric.append("diagonal-fractions");
            }
            continue;
        } else if (key == "/Ordinals") {
            if (pssVal.toBool()) {
                fontVariantNumeric.append("ordinal");
            }
            continue;
        } else if (key == "/Swash") {
            if (pssVal.toBool()) {
                fontFeatureSettings.append("'swsh' 1");
            }
            continue;
        } else if (key == "/Titling") {
            if (pssVal.toBool()) {
                fontVariantCaps.append("titling-caps");
            }
            continue;
        } else if (key == "/StylisticAlternates") {
            if (pssVal.toBool()) {
                fontFeatureSettings.append("'salt' 1");
            }
            continue;
        } else if (key == "/Ornaments") {
            if (pssVal.toBool()) {
                fontFeatureSettings.append("'ornm' 1");
            }
            continue;
        }  else if (key == "/OldStyle") {
            if (pssVal.toBool() && !pkListContains(fontVariantNumeric, PkString("oldstyle-nums"))) {
                fontVariantNumeric.append("oldstyle-nums");
            }
            continue;
        } else if (key == "/FigureStyle") {
            switch (pssVal.toInt()) {
            case 0:
                break;
            case 1:
                fontVariantNumeric.append("tabular-nums");
                fontVariantNumeric.append("lining-nums");
                break;
            case 2:
                fontVariantNumeric.append("proportional-nums");
                fontVariantNumeric.append("oldstyle-nums");
                break;
            case 3:
                fontVariantNumeric.append("proportional-nums");
                fontVariantNumeric.append("lining-nums");
                break;
            case 4:
                fontVariantNumeric.append("tabular-nums");
                fontVariantNumeric.append("oldstyle-nums");
                break;
            default:
                d->warnings << PkString("Unknown value for %1: %2").arg(key).arg(pssVal.toString());
            }
            continue;
        } else if (key == "/Italics") {
            // This is an educated guess: other italic happens via postscript name.
            if (pssVal.toBool()) {
                fontFeatureSettings.append("'ital' 1");
            }
            continue;
        } else if (key == "/BaselineDirection") {
            int val = pssVal.toInt();
            if (val == 1) {
                styles.append("text-orientation: upright");
            } else if (val == 2) {
                styles.append("text-orientation: mixed");
            } else if (val == 3) { //TCY or tate-chu-yoko
                styles.append("text-combine-upright: all");
            } else {
                d->warnings << PkString("Unknown value for %1: %2").arg(key).arg(pssVal.toString());
            }
            continue;
        } else if (key == "/Tsume" || key == "/LeftAki" || key == "/RightAki" || key == "/JiDori") {
            // Reduce spacing around a single character. Partially related to text-spacing,
            // Tsume is reduction, Aki expansion, and both can be used as part of Mojikumi
            // However, in this particular case, the property seems to just reduce the space
            // of a single character, and may not be possible to support (as in CSS that'd
            // just be padding/margin-reduction, but SVG cannot do that).
            unsupportedStyles << key;
            continue;
        } else if (key == "/StyleRunAlignment") {
            // 3 = roman
            // 5 = em-box top/right, 2 = em-box center, 0 = em-box bottom/left
            // 4 = icf-top/right, 1 icf-bottom/left?
            PkString dominantBaseline;
            switch(pssVal.toInt()) {
            case 3:
                dominantBaseline = "alphabetic";
                break;
            case 2:
                dominantBaseline = "center";
                break;
            case 0:
                dominantBaseline = "ideographic";
                break;
            case 4:
                dominantBaseline = "text-top";
                break;
            case 1:
                dominantBaseline = "text-bottom";
                break;
            default:
                d->warnings << PkString("Unknown value for %1: %2").arg(key).arg(pssVal.toString());
                dominantBaseline = PkString();
            }
            if (!dominantBaseline.isEmpty()) {
                styles.append(PkString("dominant-baseline: ")+dominantBaseline);
                styles.append(PkString("alignment-baseline: ")+dominantBaseline);
            }
            continue;
        } else if (key == "/Language") {
            int val = pssVal.toInt();
            if (psdLanguageMap.contains(val)) {
                lang = psdLanguageMap.value(val);
            } else {
                d->warnings << PkString("Unknown value for %1: %2").arg(key).arg(pssVal.toString());
            }
            continue;
        }  else if (key == "/ProportionalMetrics") {
            if (pssVal.toBool()) {
                fontFeatureSettings.append("'palt' 1");
            }
            continue;
        } else if (key == "/Kana") {
            if (pssVal.toBool()) {
                fontFeatureSettings.append("'hkna' 1");
            }
            continue;
        } else if (key == "/Ruby") {
            if (pssVal.toBool()) {
                fontVariantEastAsian.append("ruby");
            }
        } else if (key == "/JapaneseAlternateFeature") {
            // hojo kanji - 'hojo'
            // nlc kanji - 'nlck'
            // alternate notation - nalt
            // proportional kana - 'pkna'
            // vertical kana - 'vkna'
            // vert alt+rot - vrt2, or vert + vrtr
            int val = pssVal.toInt();
            if (val == 0) {
                continue;
            } else if (val == 1) { // japanese traditional - 'tnam'/'trad'
                fontVariantEastAsian.append("traditional");
            } else if (val == 2) {  // japanese expert - 'expt'
                fontFeatureSettings.append("'expt' 1");
            } else if (val == 3) { // Japanese 78 - jis78
                fontVariantEastAsian.append("jis78");
            } else {
                d->warnings << PkString("Unknown value for %1: %2").arg(key).arg(pssVal.toString());
            }
            continue;
        } else if (key == "/NoBreak") {
            // Prevents word from breaking... I guess word-break???
            if (pssVal.toBool()) {
                styles.append("word-break: keep-all");
            }
            continue;
        } else if (key == "/DirOverride") {
            PkString dir = pssVal.toBool()? "rtl": "ltr";
            if (pssVal.toBool()) {
                styles.append(PkString("direction: ")+dir);
                styles.append("unicode-bidi: isolate");
            }
            continue;
        }  else if (key == "/FillColor") {
            bool fill = true;
            if (PSDStyleSheet.count("/FillFlag") > 0) {
                fill = pkHashValue(PSDStyleSheet, "/FillFlag").toBool();
            }
            if (fill) {
                PkVariantHash color = pssVal.toHash();
                styles.append(PkString("fill:")+colorFromPSDStyleSheet(color, imageCs).name());
            } else {
                styles.append("fill:none");
            }
        } else if (key == "/StrokeColor") {
            bool fill = true;
            if (PSDStyleSheet.count("/StrokeFlag") > 0) {
                fill = pkHashValue(PSDStyleSheet, "/StrokeFlag").toBool();
            }
            if (fill) {
                PkVariantHash color = pssVal.toHash();
                styles.append(PkString("stroke:")+colorFromPSDStyleSheet(color, imageCs).name());
            } else {
                styles.append("stroke:none");
            }
            continue;
        } else if (key == "/OutlineWidth" || key == "/LineWidth") {
            double val = pssVal.toDouble();
            val = scale.map(PkPointF(val, val)).y();
            styles.append(PkString("stroke-width:")+pkNum(val));
        } else if (key == "/FillFirst") {
            // draw fill on top of stroke? paint-order: stroke markers fill, I guess.
            if (pssVal.toBool()) {
                styles.append("paint-order: fill");
            }
            continue;
        } else if (key == "/HindiNumbers") {
            // bool. Looks like this automatically selects hindi numbers for arabic. There also
            // seems to be a more complex option to automatically have arabic numbers for hebrew, and an option for farsi numbers, but this might be a different bool altogether.
            unsupportedStyles << key;
            continue;
        } else if (key == "/Kashida") {
            // number, s related to drawing/inserting Kashida/Tatweel into Arabic justified text... We don't support this.
            // options are none, short, medium, long, stylistic, indesign apparently has a 'naskh' option, which is what toggles jalt usage.
            unsupportedStyles << key;
            continue;
        } else if (key == "/DiacriticPos") {
            // number, which is odd, because it looks like it should be a point.
            // this controls how high or low the diacritic is on arabic text.
            unsupportedStyles << key;
            continue;
        }  else if (key == "/SlashedZero") {
            // font-variant: common-ligatures
            if (pssVal.toBool()) {
                fontVariantNumeric.append("slashed-zero");
            }
            continue;
        } else if (key == "/StylisticSets") {
            int flags = pssVal.toInt();
            for (int i = 1; i <= 20; i++) {
                const int bit = 2^(i-1);
                const PkString tag = i > 9? PkString("ss%1").arg(i):PkString("ss0%1").arg(i);
                if (flags & bit) {
                    fontFeatureSettings.append(PkString("\'%1\' 1").arg(tag));
                }
            }
            continue;
        } else if (key == "/LineCap") {
            switch (pssVal.toInt()) {
            case 0:
                styles.append("stroke-linecap: butt");
                break;
            case 1:
                styles.append("stroke-linecap: round");
                break;
            case 2:
                styles.append("stroke-linecap: square");
                break;
            default:
                styles.append("stroke-linecap: butt");
            }
        } else if (key == "/LineJoin") {
            switch (pssVal.toInt()) {
            case 0:
                styles.append("stroke-linejoin: miter");
                break;
            case 1:
                styles.append("stroke-linejoin: round");
                break;
            case 2:
                styles.append("stroke-linejoin: bevel");
                break;
            default:
                styles.append("stroke-linejoin: miter");
            }
        } else if (key == "/MiterLimit") {
            styles.append(PkString("stroke-miterlimit: ")+pssVal.toString());
        //} else if (key == "/LineDashArray") {
            //"stroke-dasharray"
        } else if (key == "/LineDashOffset") {
            styles.append(PkString("stroke-dashoffset: ")+pssVal.toString());
        } else if (key == "/EnableWariChu" || key == "/WariChuWidowAmount" || key == "/WariChuLineGap" || key == "/WariChuJustification"
                   || key == "/WariChuOrphanAmount" || key == "/WariChuLineCount" || key == "/WariChuSubLineAmount") {
            // Inline cutting note features.
            unsupportedStyles << key;
            continue;
        } else if (key == "/TCYUpDownAdjustment" || key == "/TCYLeftRightAdjustment") {
            // Extra text-combine-upright stuff we don't support.
            unsupportedStyles << key;
            continue;
        }  else if (key == "/Type1EncodingNames" || key == "/ConnectionForms") {
            // no clue what these are
            unsupportedStyles << key;
            continue;
        } else if (key == "/FillOverPrint" || key == "/StrokeOverPrint" || key == "/Blend") {
            // Fill stuff we don't support.
            unsupportedStyles << key;
            continue;
        } else if (key == "/UnderlineOffset") {
            // Needs css text-decor-4 features
            unsupportedStyles << key;
            continue;
        } else {
            if (key != "/FillFlag" && key != "/StrokeFlag" && key != "/AutoLeading") {
                d->warnings << PkString("Unknown PSD character stylesheet style key, %1: %2").arg(key).arg(pssVal.toString());
            }
        }
    }
    if (weight != 400) {
        styles.append(PkString("font-weight:")+pkNum(weight));
    }
    if (italic) {
        styles.append("font-style:italic");
    }
    if (!textDecor.isEmpty()) {
        styles.append(PkString("text-decoration:")+textDecor.join(" "));
    }
    if (!baselineShift.isEmpty()) {
        styles.append(PkString("baseline-shift:")+baselineShift.join(" "));
    }
    if (!fontVariantLigatures.isEmpty()) {
        styles.append(PkString("font-variant-ligatures:")+fontVariantLigatures.join(" "));
    }
    if (!fontVariantNumeric.isEmpty()) {
        styles.append(PkString("font-variant-numeric:")+fontVariantNumeric.join(" "));
    }
    if (!fontVariantCaps.isEmpty()) {
        styles.append(PkString("font-variant-caps:")+fontVariantCaps.join(" "));
    }
    if (!fontVariantEastAsian.isEmpty()) {
        styles.append(PkString("font-variant-east-asian:")+fontVariantEastAsian.join(" "));
    }
    if (!fontFeatureSettings.isEmpty()) {
        styles.append(PkString("font-feature-settings:")+fontFeatureSettings.join(", "));
    }
    if (!underlinePos.isEmpty()) {
        styles.append(PkString("text-decoration-position:")+underlinePos);
    }
    d->warnings << PkString("Unsupported styles: %1").arg(unsupportedStyles.join(","));
    return styles.join("; ");
}

PkString PsdTextDataConverter::stylesForPSDParagraphSheet(PkVariantHash PSDParagraphSheet, PkString &lang, PkMap<int, KoCSSFontInfo> fontNames, PkTransform scaleToPt, const KoColorSpace *imageCs) {
    PkStringList styles;
    PkStringList unsupportedStyles;

    for (const auto &psd : PSDParagraphSheet) {
        const PkString key = psd.first;
        const PkVariant &psdVal = psd.second;
        double val = psdVal.toDouble();
        if (key == "/Justification") {
            PkString textAlign = "start";
            PkString textAnchor = "start";
            switch (psdVal.toInt()) {
            case 0:
                textAlign = "start";
                textAnchor = "start";
                break;
            case 1:
                textAlign = "end";
                textAnchor = "end";
                break;
            case 2:
                textAlign = "center";
                textAnchor = "middle";
                break;
            case 3:
                textAlign = "justify start";
                textAnchor = "start";
                break;
            case 4:
                textAlign = "justify end"; // guess
                textAnchor = "end";
                break;
            case 5:
                textAlign = "justify center"; // guess
                textAnchor = "middle";
                break;
            case 6:
                textAlign = "justify";
                textAnchor = "middle";
                break;
            default:
                textAlign = "start";
            }

            styles.append(PkString("text-align:")+textAlign);
            styles.append(PkString("text-anchor:")+textAnchor);
        } else if (key == "/FirstLineIndent") { //-1296..1296
            val = scaleToPt.map(PkPointF(val, val)).x();
            styles.append(PkString("text-indent:")+pkNum(val));
            continue;
        } else if (key == "/StartIndent") {
            // left margin (also for rtl?), pixels -1296..1296
            unsupportedStyles << key;
            continue;
        } else if (key == "/EndIndent") {
            // right margin (also for rtl?), pixels -1296..1296
            unsupportedStyles << key;
            continue;
        } else if (key == "/SpaceBefore") {
            // top margin for paragraph, pixels -1296..1296
            unsupportedStyles << key;
            continue;
        } else if (key == "/SpaceAfter") {
            // bottom margin for paragraph, pixels -1296..1296
            unsupportedStyles << key;
            continue;
        } else if (key == "/AutoHyphenate") {
            // hyphenate: auto;
            unsupportedStyles << key;
            continue;
        } else if (key == "/HyphenatedWordSize") {
            // minimum wordsize at which to start hyphenating. 2-25
            unsupportedStyles << key;
            continue;
        } else if (key == "/PreHyphen") {
            // minimum number of letters before hyphenation is allowed to start in a word. 1-15
            // CSS-Text-4 hyphenate-limit-chars value 1.
            unsupportedStyles << key;
            continue;
        } else if (key == "/PostHyphen") {
            // minimum amount of letters a hyphnated word is allowed to end with. 1-15
            // CSS-Text-4 hyphenate-limit-chars value 2.
            unsupportedStyles << key;
            continue;
        } else if (key == "/ConsecutiveHyphens") {
            // maximum consecutive lines with hyphenation. 2-25
            // CSS-Text-4 hyphenate-limit-lines.
            unsupportedStyles << key;
            continue;
        } else if (key == "/HyphenateCapitalized") {
            unsupportedStyles << key;
            continue;
        } else if (key == "/HyphenationPreference") {
            unsupportedStyles << key;
            continue;
        } else if (key == "/SingleWordJustification") {
            unsupportedStyles << key;
            continue;
        } else if (key == "/Zone") {
            // Hyphenation zone to control where hyphenation is allowed to start, pixels. 0..8640 for 72ppi
            // CSS-Text-4 hyphenation-limit-zone.
            unsupportedStyles << key;
            continue;
        } else if (key == "/WordSpacing") {
            // val 0 is minimum allowed spacing, and val 2 is maximum allowed spacing, both for justified text.
            // 0 to 1000%, 100% default.
            unsupportedStyles << key;
            continue;
        } else if (key == "/LetterSpacing") {
            // val 0 is minimum allowed spacing, and val 2 is maximum allowed spacing, both for justified text.
            // -100% to 500%, 0% default.
            unsupportedStyles << key;
            continue;
        } else if (key == "/GlyphSpacing") {
            // scaling of the glyphs, list of vals, 50% to 200%, default 100%.
            unsupportedStyles << key;
            continue;
        } else if (key == "/AutoLeading") {
            styles.append(PkString("line-height:")+pkNum(val));
            continue;
        } else if (key == "/LeadingType") {
            // Probably how leading is measured for asian glyphs.
            // 0 = top-to-top, 1 = bottom-to-bottom. CSS can only do the second.
            unsupportedStyles << key;
            continue;
        } else if (key == "/Hanging") {
            // Roman hanging punctuation (?), bool
            continue;
        } else if (key == "/Burasagari" || key == "/BurasagariType") {
            // CJK hanging punctuation, bool
            // options are none, regular (allow-end) and force (force-end).
            if (psdVal.toBool()) {
                styles.append("hanging-punctuation:allow-end");
            }
            continue;
        } else if (key == "/Kinsoku") {
            // line breaking strictness.
            unsupportedStyles << key;
            continue;
        }  else if (key == "/KinsokuOrder") {
            // might be 0 = pushInFirst, 1 = pushOutFirst, 2 = pushOutOnly, if so, Krita only supports 2.
            unsupportedStyles << key;
            continue;
        } else if (key == "/EveryLineComposer") {
            // bool representing which text-wrapping method to use.
            //'single-line' is 'stable/greedy' line breaking,
            //'everyline' uses a penalty based system like Knuth's method.
            unsupportedStyles << key;
            continue;
        } else if (key == "/ComposerEngine") {
            unsupportedStyles << key;
            continue;
        } else if (key == "/KurikaeshiMojiShori") {
            unsupportedStyles << key;
            continue;
        } else if (key == "/MojiKumiTable") {
            unsupportedStyles << key;
            continue;
        }  else if (key == "/DropCaps") {
            unsupportedStyles << key;
            continue;
        } else if (key == "/TabStops" || key == "/AutoTCY" || key == "/KeepTogether" ) {
            unsupportedStyles << key;
            continue;
        } else if (key == "/ParagraphDirection") {
            switch (psdVal.toInt()) {
            case 1:
                styles.append("direction:rtl");
                break;
            case 0:
                styles.append("direction:ltr");
                break;
            default:
                break;
            }
        } else if (key == "/DefaultTabWidth") {
            unsupportedStyles << key;
            continue;
        } else if (key == "/DefaultStyle") {
            styles.append(stylesForPSDStyleSheet(lang, psdVal.toHash(), fontNames, scaleToPt, imageCs));
        } else {
            d->warnings << PkString("Unknown PSD character stylesheet style key, %1: %2").arg(key).arg(psdVal.toString());
        }
    }
    d->warnings << PkString("Unsupported paragraph styles: %1").arg(unsupportedStyles.join(","));

    return styles.join("; ");
}

bool PsdTextDataConverter::convertPSDTextEngineDataToSVG(const PkVariantHash tySh,
                                                                  const PkVariantHash txt2,
                                                                  const KoColorSpace *imageCs,
                                                                  const int textIndex,
                                                                  PkString *svgText,
                                                                  PkString *svgStyles,
                                                                  PkPointF &offset,
                                                                  bool &offsetByAscent,
                                                                  bool &isHorizontal,
                                                                  PkTransform scaleToPt)
{


    PkVariantHash root = tySh;
    bool loadFallback = txt2.empty();
    const PkVariantHash docObjects = pkHashValue(txt2, "/DocumentObjects").toHash();

    PkVariantHash textObject = pkHashValue(docObjects, "/TextObjects").toList().at(textIndex).toHash();
    if (textObject.empty() || loadFallback) {
        textObject = root["/EngineDict"].toHash();
        loadFallback = true;
    }
    if (textObject.empty()) {
        d->errors << "No engine dict found in PSD engine data";
        return false;
    }

    PkMap<int, KoCSSFontInfo> fontNames;
    PkVariantHash resourceDict = loadFallback? pkHashValue(root, "/DocumentResources").toHash(): pkHashValue(txt2, "/DocumentResources").toHash();
    if (resourceDict.empty()) {
        d->errors << "No engine dict found in PSD engine data";
        return false;
    } else {
        // PSD only stores the postscript name, and we'll need a bit more information than that.
        PkVariantList fonts = loadFallback? pkHashValue(resourceDict, "/FontSet").toList()
                                         : pkHashValue(pkHashValue(resourceDict, "/FontSet").toHash(), "/Resources").toList();
        for (int i = 0; i < fonts.size(); i++) {
            PkVariantHash font = loadFallback? fonts.at(i).toHash()
                                            : pkHashValue(pkHashValue(fonts.at(i).toHash(), "/Resource").toHash(), "/Identifier").toHash();
            PkString postScriptName = pkHashValue(font, "/Name").toString();
            PkString foundPostScriptName;
            PK_QSTRING_ qFound;
            KoCSSFontInfo fontInfo = KoFontRegistry::instance()->getCssDataForPostScriptName(toQString(postScriptName),
                                                                    &qFound);
            foundPostScriptName = toPkString(qFound);

            if (postScriptName != foundPostScriptName) {
                fontInfo.families = pkToNativeFamilies(PkStringList({"sans-serif"}));
                d->errors << PkString("Font %1 not found, substituting %2").arg(postScriptName).arg(pkFromNativeFamilies(fontInfo.families).join(","));
            }
            fontNames.insert(i, fontInfo);
        }
    }

    PkString inlineSizeString;
    PkRectF bounds;

    // load text shape
    std::unique_ptr<KoPathShape> textShape;
    double textPathStartOffset = -3;
    double shapePadding = 0.0;
    int textType = 0; ///< 0 = point text, 1 = paragraph text (including text in shape), 2 = text on path.
    bool reversed = false;
    if (loadFallback) {
        PkVariantHash rendered = pkHashValue(textObject, "/Rendered").toHash();
        // rendering info...
        if (!rendered.empty()) {
            PkVariantHash shapeChild = pkHashValue(pkHashValue(rendered, "/Shapes").toHash(), "/Children").toList()[0].toHash();
            textType = pkHashValue(shapeChild, "/ShapeType").toInt();
            if (textType == 1) {
                PkVariantList BoxBounds = pkHashValue(pkHashValue(pkHashValue(shapeChild, "/Cookie").toHash(), "/Photoshop").toHash(), "/BoxBounds").toList();
                if (BoxBounds.size() == 4) {
                    bounds = PkRectF(BoxBounds[0].toDouble(), BoxBounds[1].toDouble(), BoxBounds[2].toDouble(), BoxBounds[3].toDouble());
                    bounds = scaleToPt.mapRect(bounds);
                    if (isHorizontal) {
                        inlineSizeString = PkString(" inline-size:")+pkNum(bounds.width())+PkString(";");
                    } else {
                        inlineSizeString = PkString(" inline-size:")+pkNum(bounds.height())+PkString(";");
                    }
                }
            }
        }
    } else {
        PkVariantHash view = pkHashValue(textObject, "/View").toHash();
        // todo: if multiple frames in frames array, there's multiple shapes in shape-inside.
        PkVariantList frames = pkHashValue(view, "/Frames").toList();
        if (!frames.empty()) {
            int textFrameIndex = pkHashValue(pkHashValue(view, "/Frames").toList().at(0).toHash(), "/Resource").toInt();
            PkVariantList textFrameSet = pkHashValue(pkHashValue(resourceDict, "/TextFrameSet").toHash(), "/Resources").toList();
            PkVariantHash textFrame = pkHashValue(textFrameSet.at(textFrameIndex).toHash(), "/Resource").toHash();


            if (!textFrame.empty()) {
                textType = textFrame["/Data"].toHash()["/Type"].toInt();

                if (textType > 0) {
                    KoPathShape *textCurve = new KoPathShape();
                    PkVariantHash data = pkHashValue(textFrame, "/Data").toHash();
                    PkVariantList points = pkHashValue(pkHashValue(textFrame, "/Bezier").toHash(), "/Points").toList();
                    PkVariantList range = pkHashValue(data, "/TextOnPathTRange").toList();
                    PkVariantList fm = pkHashValue(data, "/FrameMatrix").toList();
                    shapePadding = pkHashValue(data, "/Spacing").toDouble();
                    PkVariantHash pathData = pkHashValue(data, "/PathData").toHash();
                    reversed = pkHashValue(pathData, "/Flip").toBool();

                    PkVariant lineOrientation = pkHashValue(data, "/LineOrientation");
                    if (!lineOrientation.isNull()) {
                        if (lineOrientation.toInt() == 2) {
                            isHorizontal = false;
                        }
                    }
                    PkTransform frameMatrix = scaleToPt;
                    if (fm.size() == 6) {
                        frameMatrix = PkTransform(fm[0].toDouble(), fm[1].toDouble(), fm[2].toDouble(), fm[3].toDouble(), fm[4].toDouble(), fm[5].toDouble());
                        frameMatrix = frameMatrix * scaleToPt;
                    }

                    int length = points.size()/8;

                    PkPointF startPoint;
                    PkPointF endPoint;
                    for (int i = 0; i < length; i++) {
                        int iAdjust = i*8;
                        PkPointF p1(points[iAdjust  ].toDouble(), points[iAdjust+1].toDouble());
                        PkPointF p2(points[iAdjust+2].toDouble(), points[iAdjust+3].toDouble());
                        PkPointF p3(points[iAdjust+4].toDouble(), points[iAdjust+5].toDouble());
                        PkPointF p4(points[iAdjust+6].toDouble(), points[iAdjust+7].toDouble());

                        if (i == 0 || endPoint != frameMatrix.map(p1)) {
                            if (endPoint == startPoint && i > 0) {
                                textCurve->closeMerge();
                            }
                            textCurve->moveTo(toQPointF(frameMatrix.map(p1)));
                            startPoint = frameMatrix.map(p1);
                        }
                        if (p1==p2 && p3==p4) {
                            textCurve->lineTo(toQPointF(frameMatrix.map(p4)));
                        } else {
                            textCurve->curveTo(toQPointF(frameMatrix.map(p2)), toQPointF(frameMatrix.map(p3)), toQPointF(frameMatrix.map(p4)));
                        }
                        endPoint = frameMatrix.map(p4);
                    }
                    if (points.size() > 8) {
                        if (endPoint == startPoint) {
                            textCurve->closeMerge();
                        }
                        textShape.reset(textCurve);
                    } else {
                        delete textCurve;
                    }
                    if (!range.empty()) {
                        textPathStartOffset = range[0].toDouble();
                        int segment = static_cast<int>(std::floor(textPathStartOffset));
                        double t = textPathStartOffset - segment;
                        double length = 0;
                        double totalLength = 0;
                        for (int i=0; i<textShape->subpathPointCount(0); i++) {
                            double l = textShape->segmentByIndex(KoPathPointIndex(0, i)).length();
                            totalLength += l;
                            if (i < segment) {
                                length += l;
                            } else if (i == segment) {
                                length += textShape->segmentByIndex(KoPathPointIndex(0, i)).lengthAt(t);
                            }
                        }
                        textPathStartOffset = (length/totalLength) * 100.0;
                    }
                }
            }
        }
    }
    PkString paragraphStyle = isHorizontal? "writing-mode: horizontal-tb;": "writing-mode: vertical-rl;";
    paragraphStyle += " white-space: pre-wrap;";

    PkString svgBuffer;
    PkString styleBuffer;

    PkXmlStreamWriter svgWriter(&svgBuffer);
    PkXmlStreamWriter stylesWriter(&styleBuffer);
    stylesWriter.writeStartElement("defs");
    if (bounds.isValid()) {
        stylesWriter.writeStartElement("rect");
        stylesWriter.writeAttribute("id", "bounds");
        stylesWriter.writeAttribute("x", pkNum(bounds.x()));
        stylesWriter.writeAttribute("y", pkNum(bounds.y()));
        stylesWriter.writeAttribute("width", pkNum(bounds.width()));
        stylesWriter.writeAttribute("height", pkNum(bounds.height()));
        stylesWriter.writeEndElement();
    }
    if (textShape) {
        stylesWriter.writeStartElement("path");
        stylesWriter.writeAttribute("id", "textShape");
        stylesWriter.writeAttribute("d", toPkString(textShape->toString()));
        stylesWriter.writeAttribute("opacity", "0");
        stylesWriter.writeAttribute("sodipodi:nodetypes", toPkString(textShape->nodeTypes()));
        stylesWriter.writeEndElement();
    }


    // disable auto-formatting to avoid axtra spaces appearing here and there
    svgWriter.setAutoFormatting(false);

    svgWriter.writeStartElement("text");

    PkVariantHash editor = loadFallback? pkHashValue(textObject, "/Editor").toHash() : pkHashValue(textObject, "/Model").toHash();
    PkString text = "";
    if (editor.empty()) {
        d->errors << "No editor dict found in PSD engine data";
        return false;
    } else {
        text = pkHashValue(editor, "/Text").toString();
        text = pkStringReplaceAll(text, PkString("\r"), PkString("\n"), PkCaseSensitive); // return, used for paragraph hard breaks.
        text = pkStringReplaceAll(text, pkCharToString(char16_t(0x03)), PkString("\n"), PkCaseSensitive); // end of text character, used for non-paragraph hard breaks.
    }

    int antiAliasing = 0;
        antiAliasing = loadFallback? pkHashValue(textObject, "/AntiAlias").toInt()
                                   : pkHashValue(pkHashValue(textObject, "/StorySheet").toHash(), "/AntiAlias").toInt();
    //0 = None, 4 = Sharp, 1 = Crisp, 2 = Strong, 3 = Smooth
    if (antiAliasing == 3) {
        svgWriter.writeAttribute("text-rendering", "auto");
    } else if (antiAliasing == 0) {
        svgWriter.writeAttribute("text-rendering", "OptimizeSpeed");
    }

    PkVariantHash paragraphRun = loadFallback? pkHashValue(textObject, "/ParagraphRun").toHash() : pkHashValue(editor, "/ParagraphRun").toHash();
    if (!paragraphRun.empty()) {
        //PkVariantList runLengthArray = pkHashValue(paragraphRun, "RunLengthArray").toList();
        PkVariantList runArray = pkHashValue(paragraphRun, "/RunArray").toList();
        PkString features = loadFallback? "/Properties": "/Features";
        PkVariantHash style = loadFallback? runArray.at(0).toHash() : pkHashValue(runArray.at(0).toHash(), "/RunData").toHash();
        PkVariantHash parasheet = loadFallback? runArray.at(0).toHash()["/ParagraphSheet"].toHash():
                runArray.at(0).toHash()["/RunData"].toHash()["/ParagraphSheet"].toHash();
        PkVariantHash styleSheet = parasheet[features].toHash();

        PkString lang;
        PkString styleString = stylesForPSDParagraphSheet(styleSheet, lang, fontNames, scaleToPt, imageCs);
        if (!lang.isEmpty()) {
            svgWriter.writeAttribute("xml:lang", lang);
        }
        if (textType < 2) {
            if (textShape) {
                offsetByAscent = false;
                paragraphStyle += " shape-inside:url(#textShape);";
                if (shapePadding > 0) {
                    PkPointF sPadding = scaleToPt.map(PkPointF(shapePadding, shapePadding));
                    paragraphStyle += PkString(" shape-padding:")+pkNum(sPadding.x())+PkString(";");
                }
            } else if (styleString.contains("text-align:justify") && bounds.isValid()) {
                offsetByAscent = false;
                paragraphStyle += " shape-inside:url(#bounds);";
            } else if (bounds.isValid()){
                offsetByAscent = true;
                offset = isHorizontal? bounds.topLeft(): bounds.topRight();
                if (styleString.contains("text-anchor:middle")) {
                    offset = isHorizontal? PkPointF(bounds.center().x(), offset.y()):
                                           PkPointF(offset.x(), bounds.center().y());
                } else if (styleString.contains("text-anchor:end")) {
                    offset = isHorizontal? PkPointF(bounds.right(), offset.y()):
                                           PkPointF(offset.x(), bounds.bottom());
                }
                paragraphStyle += inlineSizeString;
                svgWriter.writeAttribute("transform", PkString("translate(%1, %2)").arg(offset.x()).arg(offset.y()));
            }
        }
        paragraphStyle += styleString;
        svgWriter.writeAttribute("style", paragraphStyle);

    }

    bool textPathCreated = false;
    if (textShape && textType == 2) {
        svgWriter.writeStartElement("textPath");
        textPathCreated = true;
        svgWriter.writeAttribute("path", toPkString(textShape->toString()));
        if (reversed) {
            svgWriter.writeAttribute("side", "right");
        }
        svgWriter.writeAttribute("startOffset", pkNum(textPathStartOffset)+PkString("%"));
    }

    PkVariantHash styleRun = loadFallback? pkHashValue(textObject, "/StyleRun").toHash(): pkHashValue(editor, "/StyleRun").toHash();
    if (styleRun.empty()) {
        d->errors << "No styleRun dict found in PSD engine data";
        return false;
    } else {
        PkString features = loadFallback? "/StyleSheetData": "/Features";
        PkVariantList runLengthArray = pkHashValue(styleRun, "/RunLengthArray").toList();
        PkVariantList runArray = pkHashValue(styleRun, "/RunArray").toList();
        if (runArray.empty()) {
            d->errors << "No styleRun dict found in PSD engine data";
            return false;
        } else {
            PkVariantHash style = loadFallback? runArray.at(0).toHash() : runArray.at(0).toHash()["/RunData"].toHash();
            PkVariantHash styleSheet = pkHashValue(pkHashValue(style, "/StyleSheet").toHash(), features).toHash();
            int length = 0;
            int pos = 0;
            for (int i = 0; i < runArray.size(); i++) {
                style = loadFallback? runArray.at(i).toHash() : runArray.at(i).toHash()["/RunData"].toHash();
                int l = loadFallback? runLengthArray.at(i).toInt(): pkHashValue(runArray.at(i).toHash(), "/Length").toInt();

                PkVariantHash newStyleSheet = pkHashValue(pkHashValue(style, "/StyleSheet").toHash(), features).toHash();
                if (newStyleSheet == styleSheet) {
                    length += l;
                } else {
                    svgWriter.writeStartElement("tspan");
                    PkString lang;
                    svgWriter.writeAttribute("style", stylesForPSDStyleSheet(lang, styleSheet, fontNames, scaleToPt, imageCs));
                    if (!lang.isEmpty()) {
                        svgWriter.writeAttribute("xml:lang", lang);
                    }
                    svgWriter.writeCharacters(text.mid(pos, length));
                    svgWriter.writeEndElement();
                    styleSheet = newStyleSheet;
                    pos += length;
                    length = l;
                }
            }
            svgWriter.writeStartElement("tspan");
            PkString lang;
            svgWriter.writeAttribute("style", stylesForPSDStyleSheet(lang, styleSheet, fontNames, scaleToPt, imageCs));
            if (!lang.isEmpty()) {
                svgWriter.writeAttribute("xml:lang", lang);
            }
            svgWriter.writeCharacters(text.mid(pos));
            svgWriter.writeEndElement();
        }
    }

    if (textPathCreated) {
        svgWriter.writeEndElement();
    }

    svgWriter.writeEndElement();//text root element.
    stylesWriter.writeEndElement();

    *svgText = svgBuffer.trimmed();
    *svgStyles = styleBuffer.trimmed();

    return true;
}



void PsdTextDataConverter::gatherFonts(const PkMap<PkString, PkString> cssStyles, const PkString text, PkVariantList &fontSet,
                 PkVector<int> &lengths, PkVector<int> &fontIndices) {
    if (cssStyles.contains("font-family")) {
        PkStringList families = pkSplit(cssStyles.value("font-family"), u',');
        int fontSize = cssStyles.value("font-size", "10").toInt();
        int fontWeight = cssStyles.value("font-weight", "400").toInt();
        int fontWidth = cssStyles.value("font-stretch", "100").toInt();

        KoCSSFontInfo fontInfo;
        fontInfo.families = pkToNativeFamilies(families);
        fontInfo.size = fontSize;
        fontInfo.weight = fontWeight;
        fontInfo.width = fontWidth;
#ifdef QT_CORE_LIB
        PK_QVECTOR_<int> lengthsNative;
        for (int i = 0; i < lengths.size(); i++) {
            lengthsNative.append(lengths.at(i));
        }
        const std::vector<FT_FaceSP> faces = KoFontRegistry::instance()->facesForCSSValues(lengthsNative, fontInfo,
                                                      toQString(text), 72, 72);
        lengths.clear();
        for (int i = 0; i < lengthsNative.size(); i++) {
            lengths.append(lengthsNative.at(i));
        }
#else
        const std::vector<FT_FaceSP> faces = KoFontRegistry::instance()->facesForCSSValues(lengths, fontInfo,
                                                      text, 72, 72);
#endif

        for (uint i = 0; i < faces.size(); i++) {
            const FT_FaceSP &face = faces.at(static_cast<size_t>(i));
            PkString postScriptName = face->family_name;
            if (FT_Get_Postscript_Name(face.data())) {
                postScriptName = FT_Get_Postscript_Name(face.data());
            }

            int fontIndex = -1;
            for(int j=0; j<fontSet.size(); j++) {
                if (fontSet[j].toHash()["/Name"] == postScriptName) {
                    fontIndex = j;
                    break;
                }
            }
            if (fontIndex < 0) {
                PkVariantHash font;
                font["/Name"] = postScriptName;
                font["/Type"] = 1;
                fontSet.push_back(font);
                fontIndex = fontSet.size()-1;
            }
            fontIndices << fontIndex;
        }
    }
}

PkVariantHash PsdTextDataConverter::styleToPSDStylesheet(const PkMap<PkString, PkString> cssStyles,
                                 PkVariantHash parentStyle, PkTransform scaleToPx) {
    PkVariantHash styleSheet = parentStyle;

    const auto cssStyleKeys = cssStyles.keys();
    for (int i = 0; i < cssStyleKeys.size(); i++) {
        const PkString key = cssStyleKeys.at(i);
        PkString val = cssStyles.value(key);

        if (key == "font-size") {
            double size = val.toDouble();
            size = scaleToPx.map(PkPointF(size, size)).x();
            styleSheet["/FontSize"] = size;
        } else if (key == "letter-spacing") {
            double space = val.toDouble();
            space = scaleToPx.map(PkPointF(space, space)).x();
            double size = styleSheet["/FontSize"].toDouble();
            styleSheet["/Tracking"] = (space/size) * 1000.0;
        } else if (key == "line-height") {
            double space = val.toDouble();
            double size = styleSheet["/FontSize"].toDouble();
            styleSheet["/Leading"] = (space*size);
            styleSheet["/AutoLeading"] = false;
        } else if (key == "font-kerning") {
            if (val == "none") {
                styleSheet["/AutoKern"] = 0;
            }
        } else if (key == "baseline-shift") {
            if (val == "super") {
                styleSheet["/FontBaseline"] = 1;
            } else if (val == "super") {
                styleSheet["/FontBaseline"] = 2;
            } else {
                double offset = val.toDouble();
                offset = scaleToPx.map(PkPointF(offset, offset)).y();
                styleSheet["/BaselineShift"] = offset;
            }
        } else if (key == "text-decoration") {
            PkStringList decor = pkSplit(val, u' ');
            for (int i = 0; i < decor.size(); i++) {
                const PkString param = decor.at(i);
                if (param == "underline") {
                    styleSheet["/UnderlinePosition"] = 1;
                    if (cssStyles.value("text-decoration-position").contains("right")) {
                        styleSheet["/UnderlinePosition"] = 2;
                    }
                } else if (param == "line-through"){
                    styleSheet["/StrikethroughPosition"] = 1;
                }
            }
        } else if (key == "font-variant") {
            PkStringList params = pkSplit(val, u' ');
            bool tab = pkListContains(params, PkString("tabular-nums"));
            bool old = pkListContains(params, PkString("oldstyle-nums"));
            for (int i = 0; i < params.size(); i++) {
                const PkString param = params.at(i);
                if (param == "small-caps" || param == "all-small-caps") {
                    styleSheet["/FontCaps"] = 1;
                } else if (param == "titling-caps") {
                    styleSheet["/Titling"] = true;
                } else if (param == "no-common-ligatures"){
                    styleSheet["/Ligatures"] = false;
                } else if (param == "discretionary-ligatures"){
                    styleSheet["/DiscretionaryLigatures"] = true;
                } else if (param == "contextual"){
                    styleSheet["/ContextualLigatures"] = true;
                } else if (param == "diagonal-fractions"){
                    styleSheet["/Fractions"] = true;
                } else if (param == "ordinal"){
                    styleSheet["/Ordinals"] = true;
                } else if (param == "slashed-zero"){
                    styleSheet["/SlashedZero"] = true;
                } else if (param == "super") {
                    styleSheet["/FontOTPosition"] = 1;
                } else if (param == "sub") {
                    styleSheet["/FontOTPosition"] = 2;
                } else if (param == "ruby") {
                    styleSheet["/Ruby"] = true;
                } else if (param == "traditional") {
                    styleSheet["/JapaneseAlternateFeature"] = 1;
                } else if (param == "jis78") {
                    styleSheet["/JapaneseAlternateFeature"] = 3;
                }
            }
            styleSheet["/OldStyle"] = old;
            if (tab && old) {
                styleSheet["/FigureStyle"] = 4;
            } else if (tab) {
                styleSheet["/FigureStyle"] = 1;
            } else if (old) {
                styleSheet["/FigureStyle"] = 2;
            }
        } else if (key == "font-feature-settings") {
            PkStringList params = pkSplit(val, u',');
            for (int i = 0; i < params.size(); i++) {
                const PkString param = params.at(i);
                if (param.trimmed() == "'swsh' 1") {
                    styleSheet["/Swash"] = true;
                } else if (param.trimmed() == "'titl' 1") {
                    styleSheet["/Titling"] = true;
                } else if (param.trimmed() == "'salt' 1") {
                    styleSheet["/StylisticAlternates"] = true;
                } else if (param.trimmed() == "'ornm' 1") {
                    styleSheet["/Ornaments"] = true;
                } else if (param.trimmed() == "'ital' 1") {
                    styleSheet["/Italics"] = true;
                } else if (param.trimmed() == "'numr' 1") {
                    styleSheet["/FontOTPosition"] = 3;
                } else if (param.trimmed() == "'dnum' 1") {
                    styleSheet["/FontOTPosition"] = 4;
                } else if (param.trimmed() == "'expt' 1") {
                    styleSheet["/JapaneseAlternateFeature"] = 2;
                } else if (param.trimmed() == "'hkna' 1") {
                    styleSheet["/Kana"] = true;
                } else if (param.trimmed() == "'palt' 1") {
                    styleSheet["/ProportionalMetrics"] = true;
                }
            }
        } else if (key == "text-orientation") {
            if (val == "upright") {
                styleSheet["/BaselineDirection"] = 1;
            } else if (val == "mixed") {
                styleSheet["/BaselineDirection"] = 2;
            }
        } else if (key == "text-combine-upright") {
            if (val == "all") {
                 styleSheet["/BaselineDirection"] = 3;
            }
        } else if (key == "word-break") {
            styleSheet["/NoBreak"] = val == "keep-all";
        } else if (key == "direction") {
            styleSheet["/DirOverride"] = val == "ltr"? 0 :1;
        } else if (key == "xml:lang") {
            const int langKey = psdLanguageMap.key(val, -1);
            if (langKey != -1) {
                styleSheet["/Language"] = langKey;
            }
        } else if (key == "paint-order") {
            PkStringList decor = pkSplit(val, u' ');
            styleSheet["/FillFirst"] = decor.first() == "fill";
        } else {
            d->errors << "Unsupported css-style:" << key << val;
        }
    }

    return styleSheet;
}

void gatherFills(PkXmlElement el, PkVariantHash &styleDict) {
    if (el.hasAttribute("fill")) {
        if (el.attribute("fill") != "none") {
            PkColor c = PkColor(el.attribute("fill"));
            //double opacity = el.attribute("fill-opacity", "1.0").toDouble();
            styleDict["/FillFlag"] = true;
            styleDict["/FillColor"] = PkVariantHash({ {"/StreamTag", "/SimplePaint"},
                                                     { "/Color", PkVariantHash({
                                                           {"/Type", 1},
                                                           {"/Values", PkVariantList({1.0, c.redF(), c.greenF(), c.blueF()})
                                                           }})}
                                                   });
        } else {
            styleDict["/FillFlag"] = false;
        }
    }
    if (el.hasAttribute("stroke")) {
        if (el.attribute("stroke") != "none" && el.attribute("stroke-width").toDouble() != 0) {
            PkColor c = PkColor(el.attribute("stroke"));
            //double opacity = el.attribute("stroke-opacity").toDouble();
            styleDict["/StrokeFlag"] = true;
            styleDict["/StrokeColor"] = PkVariantHash({ {"/StreamTag", "/SimplePaint"},
                                                       { "/Color", PkVariantHash({
                                                             {"/Type", 1},
                                                             {"/Values", PkVariantList({1.0, c.redF(), c.greenF(), c.blueF()})
                                                             }})}
                                                     });
        } else {
            styleDict["/StrokeFlag"] = false;
        }
    }
    if (el.hasAttribute("stroke-linejoin")) {
        PkString val = el.attribute("stroke-linejoin");
        if (val == "miter") {
            styleDict["/LineJoin"] = 0;
        } else if (val == "round") {
            styleDict["/LineJoin"] = 1;
        } else if (val == "bevel") {
            styleDict["/LineJoin"] = 2;
        }
    }
    if (el.hasAttribute("stroke-linecap")) {
        PkString val = el.attribute("stroke-linecap");
        if (val == "butt") {
            styleDict["/LineCap"] = 0;
        } else if (val == "round") {
            styleDict["/LineCap"] = 1;
        } else if (val == "square") {
            styleDict["/LineCap"] = 2;
        }
    }
    if (el.hasAttribute("stroke-width") && el.attribute("stroke-width").toDouble() != 0) {
        styleDict["/LineWidth"] = el.attribute("stroke-width").toDouble();
    }
}

void PsdTextDataConverter::gatherStyles(PkXmlElement el, PkString &text,
                  PkVariantHash parentStyle,
                  PkMap<PkString, PkString> parentCssStyles,
                  PkVariantList &styles,
                  PkVariantList &fontSet, PkTransform scaleToPx) {
    PkMap<PkString, PkString> cssStyles = parentCssStyles;
    if (el.hasAttribute("style")) {
        PkString style = el.attribute("style");
        PkStringList dummy = pkSplit(style, u';');

        for (int i = 0; i < dummy.size(); i++) {
            PkString style = dummy.at(i);
            PkString key = pkSplit(style, u':').first().trimmed();
            PkString val = pkSplit(style, u':').last().trimmed();
            cssStyles.insert(key, val);
        }
        for (const auto &attribute : KoSvgTextProperties::supportedXmlAttributes()) {
            if (el.hasAttribute(toPkString(attribute))) {
                cssStyles.insert(toPkString(attribute), el.attribute(toPkString(attribute)));
            }
        }
    }

    if (el.firstChild().isText()) {
        PkXmlText textNode = el.firstChild().toText();
        PkString currentText = textNode.data();
        text += currentText;

        PkVariantHash styleDict = styleToPSDStylesheet(cssStyles, parentStyle, scaleToPx);
        gatherFills(el, styleDict);

        PkVector<int> lengths;
        PkVector<int> fontIndices;
        gatherFonts(cssStyles, currentText, fontSet, lengths, fontIndices);
        for (int i = 0; i< fontIndices.size(); i++) {
            PkVariantHash curDict = styleDict;
            curDict["/Font"] = fontIndices.at(i);
            PkVariantHash fDict = {
                {"/StyleSheet", PkVariantHash({{"/Name", ""}, {"/Parent", 0}, {"/Features", curDict}})}
            };
            styles.push_back(PkVariantHash({
                                           {"/Length", lengths.at(i)},
                                           {"/RunData", fDict},
                                       }));
        }

    } else if (el.childNodes().size()>0) {
        PkVariantHash styleDict = styleToPSDStylesheet(cssStyles, parentStyle, scaleToPx);
        gatherFills(el, styleDict);
        PkXmlElement childEl = el.firstChildElement();
        while(!childEl.isNull()) {
            gatherStyles(childEl, text, styleDict, cssStyles, styles, fontSet, scaleToPx);
            childEl = childEl.nextSiblingElement();
        }
    }
}

PkVariantHash PsdTextDataConverter::gatherParagraphStyle(PkXmlElement el,
                                 PkVariantHash defaultProperties,
                                 bool &isHorizontal,
                                 PkString *inlineSize,
                                 PkTransform scaleToPx) {
    PkString cssStyle = el.attribute("style");
    PkStringList dummy = pkSplit(cssStyle, u';');
    PkMap<PkString, PkString> cssStyles;
    for (int i = 0; i < dummy.size(); i++) {
        PkString style = dummy.at(i);
        PkString key = pkSplit(style, u':').first().trimmed();
        PkString val = pkSplit(style, u':').last().trimmed();
        cssStyles.insert(key, val);
    }
    static const PkString s_styleKeys[] = {"text-align", "text-anchor", "writing-mode", "direction", "line-height", "inline-size", "shape-inside"};
    for (const PkString &styleKey : s_styleKeys) {
        if (el.hasAttribute(styleKey)) {
            cssStyles.insert(styleKey, el.attribute(styleKey));
        }
    }
    int alignVal = 0;
    int anchorVal = 0;

    PkVariantHash paragraphStyleSheet = defaultProperties;
    const auto cssStyleKeys = cssStyles.keys();
    for (int i = 0; i < cssStyleKeys.size(); i++) {
        const PkString key = cssStyleKeys.at(i);
        PkString val = cssStyles.value(key);

        if (key == "text-align") {
            if (val == "start") {alignVal = 0;}
            if (val == "center") {alignVal = 2;}
            if (val == "end") {alignVal = 1;}
            if (val == "justify start") {alignVal = 3;}
            if (val == "justify center") {alignVal = 4;}
            if (val == "justify end") {alignVal = 5;}
            if (val == "justify") {alignVal = 6;}
        } else if (key == "text-anchor") {
            if (val == "start") {anchorVal = 0;}
            if (val == "middle") {anchorVal = 2;}
            if (val == "end") {anchorVal = 1;}
        } else if (key == "writing-mode") {
            if (val == "horizontal-tb") {
                isHorizontal = true;
            } else {
                isHorizontal = false;
            }
        } else if (key == "direction") {
            paragraphStyleSheet["/ParagraphDirection"] = val == "ltr"? 0 :1;
        } else if (key == "line-height") {
            paragraphStyleSheet["/AutoLeading"] = val.toDouble();
        } else if (key == "inline-size") {
            *inlineSize = val;
        }
    }
    if (cssStyles.contains("shape-inside")) {
        paragraphStyleSheet["/Justification"] = alignVal;
    } else {
        paragraphStyleSheet["/Justification"] = anchorVal;
    }
    return PkVariantHash{{"/Name", ""}, {"/Parent", 0}, {"/Features", paragraphStyleSheet}};
}

bool PsdTextDataConverter::convertToPSDTextEngineData(const PkString &svgText, PkRectF &boundingBox,
                                                               const PkList<KoShape *> &shapesInside,
                                                               PkVariantHash &txt2,
                                                               int &textIndex,
                                                               PkString &textTotal,
                                                               bool &isHorizontal,
                                                               PkTransform scaleToPx)
{
    PkVariantHash root;

    PkVariantHash model;
    PkVariantHash view;

    PkString text;
    PkVariantList styles;
    PkVariantList fontSet;

    PkVariantList textObjects = pkHashValue(pkHashValue(txt2, "/DocumentObjects").toHash(), "/TextObjects").toList();
    PkVariantHash defaultParagraphProps = pkHashValue(pkHashValue(txt2, "/DocumentObjects").toHash(), "/OriginalNormalParagraphFeatures").toHash();

    const int tIndex = textObjects.size();
    PkVariantHash docResources = pkHashValue(txt2, "/DocumentResources").toHash();
    PkVariantList textFrames = pkHashValue(pkHashValue(docResources, "/TextFrameSet").toHash(), "/Resources").toList();
    const PkVariantList resFontSet = pkHashValue(pkHashValue(docResources, "/FontSet").toHash(), "/Resources").toList();

    for (int i = 0; i < resFontSet.size(); i++) {
        const PkVariant entry = resFontSet.at(i);
        const PkVariantHash docFont = pkHashValue(entry.toHash(), "/Resource").toHash();
        PkVariantHash font = pkHashValue(docFont, "/Identifier").toHash();
        fontSet.push_back(font);
    }

    PkVector<int> lengths;
    PkVector<int> fontIndices;
    const auto nativeAttrs = KoSvgTextProperties::defaultProperties().convertToSvgTextAttributes();
    PkMap<PkString, PkString> defaultCss;
    const auto nativeAttrKeys = nativeAttrs.keys();
    for (int i = 0; i < nativeAttrKeys.size(); i++) {
        defaultCss.insert(toPkString(nativeAttrKeys.at(i)), toPkString(nativeAttrs.value(nativeAttrKeys.at(i))));
    }
    gatherFonts(defaultCss, "", fontSet, lengths, fontIndices);

    // go down the document children to get the style.
    PkXmlDocument doc;
    doc.setContent(svgText);
    gatherStyles(doc.documentElement(), text, PkVariantHash(), PkMap<PkString, PkString>(), styles, fontSet, scaleToPx);

    PkString inlineSize;
    PkVariantHash paragraphStyle = gatherParagraphStyle(doc.documentElement(),
                                                      defaultParagraphProps,
                                                      isHorizontal, &inlineSize,
                                                      scaleToPx);

    text += PkString("\n");
    model["/Text"] = text;

    PkVariantHash paragraphSet;
    paragraphSet["/Length"] = PkVariant(text.size());
    paragraphSet["/RunData"] = PkVariantHash{{"/ParagraphSheet", paragraphStyle}};

    model["/ParagraphRun"] = PkVariantHash{{"/RunArray", PkVariantList({paragraphSet})}};

    PkVariantHash styleRun;
    PkVariantList properStyleRun;
    for (int i = 0; i < styles.size(); i++) {
        PkVariant entry = styles.at(i);
        properStyleRun.push_back(entry);
    }
    styleRun["/RunArray"] = properStyleRun;

    model["/StyleRun"] = styleRun;

    PkVariantHash storySheet;
    storySheet["/UseFractionalGlyphWidths"] = true;
    storySheet["/AntiAlias"] = 1;
    model["/StorySheet"] = storySheet;

    PkRectF bounds;
    if (!(inlineSize.isEmpty() || inlineSize == "auto")) {
        bounds = boundingBox;
        bool ok;
        double inlineSizeVal = inlineSize.toDouble(&ok);
        if (ok) {
            if (isHorizontal) {
                bounds.setWidth(inlineSizeVal);
            } else {
                bounds.setHeight(inlineSizeVal);
            }
        }
    } else {
        bounds = PkRectF();
    }

    int shapeType = bounds.isEmpty()? 0: 1; ///< 0 point, 1 paragraph, 2 text-on-path.
    int writingDirection = isHorizontal? 0: 2;


    const int textFrameIndex = textFrames.size();
    PkVariantHash newTextFrame;
    PkVariantHash newTextFrameData;
    newTextFrameData["/LineOrientation"] = writingDirection;


    PkList<PkPointF> points;

    std::unique_ptr<KoPathShape> textShape;
    for (int i = 0; i < shapesInside.size(); i++) {
        KoShape *shape = shapesInside.at(i);
#ifdef QT_CORE_LIB
        KoPathShape *p = dynamic_cast<KoPathShape*>(shape);
#else
        // Shell (Qt-free test harness): flake RTTI typeinfo isn't linked into the shell
        // (KoShape.cpp isn't compiled here), and this convertToPSDTextEngineData path is not
        // exercised by shell tests. static_cast preserves the downcast; real main-tree build
        // keeps dynamic_cast semantics.
        KoPathShape *p = static_cast<KoPathShape*>(shape);
#endif
        if (p) {
            textShape.reset(p);
            break;
        }
    }
    if (textShape) {
        for (int i = 0; i<textShape->subpathPointCount(0); i++) {
            KoPathSegment s = textShape->segmentByIndex(KoPathPointIndex(0, i));
            points.append(toPkPointF(s.first()->point()));
            points.append(toPkPointF(s.first()->controlPoint2()));
            points.append(toPkPointF(s.second()->controlPoint1()));
            points.append(toPkPointF(s.second()->point()));
        }
    } else if (!bounds.isEmpty()) {
        points.append(bounds.topLeft());
        points.append(bounds.topLeft());
        points.append(bounds.topRight());
        points.append(bounds.topRight());
        points.append(bounds.topRight());
        points.append(bounds.topRight());
        points.append(bounds.bottomRight());
        points.append(bounds.bottomRight());
        points.append(bounds.bottomRight());
        points.append(bounds.bottomRight());
        points.append(bounds.bottomLeft());
        points.append(bounds.bottomLeft());
        points.append(bounds.bottomLeft());
        points.append(bounds.bottomLeft());
        points.append(bounds.topLeft());
        points.append(bounds.topLeft());
    }
    if (!points.isEmpty()) {
        PkVariantList p;
        for(int i = 0; i < points.size(); i++) {
            PkPointF p2 = scaleToPx.map(points.at(i));
            p.push_back(p2.x());
            p.push_back(p2.y());
        }
        newTextFrame["/Bezier"] = PkVariantHash({{"/Points", p}});
        shapeType = 1;
    }
    newTextFrameData["/Type"] = shapeType;
    newTextFrame["/Data"] = newTextFrameData;

    view["/Frames"] = PkVariantList({PkVariantHash({{"/Resource", textFrameIndex}})});

    PkVariantList bbox = {0.0, 0.0, bounds.width(), bounds.height()};
    PkVariantList bbox2 = {bounds.left(), bounds.top(), bounds.right(), bounds.bottom()};

    /*
    PkVariantHash glyphStrike {
        {"/Bounds", bbox},
        {"/GlyphAdjustments", PkVariantHash({{"/Data", PkVariantList()}, {"/RunLengths", PkVariantList()}})},
        {"/Glyphs", PkVariantList()},
        {"/Invalidation", bbox},
        {"/RenderedBounds", bbox},
        {"/VisualBounds", bbox},
        {"/SelectionAscent", 10.0},
        {"/SelectionDescent", -10.0},
        {"/ShadowStylesRun", PkVariantHash({{"/Data", PkVariantList()}, {"/RunLengths", PkVariantList()}})},
        {"/StreamTag", "/GlyphStrike"},
        {"/Transform", PkVariantHash({{"/Origin", PkVariantList({0.0, 0.0})}})}
    };
    PkVariantHash frameStrike {
        {"/Bounds", PkVariantList({0.0, 0.0, 0.0, 0.0})},
        {"/ChildProcession", 2},
        {"/Children", PkVariantList({glyphStrike})},
        {"/StreamTag", "/FrameStrike"},
        {"/Frame", textFrameIndex},
        {"/Transform", PkVariantHash({{"/Origin", PkVariantList({0.0, 0.0})}})}
    };
    PkVariantHash pathStrike {
        {"/Bounds", PkVariantList({0.0, 0.0, 0.0, 0.0})},
        {"/ChildProcession", 0},
        {"/Children", PkVariantList({frameStrike})},
        {"/StreamTag", "/PathSelectGroupCharacter"},
        {"/Transform", PkVariantHash({{"/Origin", PkVariantList({0.0, 0.0})}})}
    };
    view["/Strikes"] = PkVariantList({pathStrike});*/
    PkVariantHash rendered {
        {"/RunData", PkVariantHash({{"/LineCount", 1}})},
        {"/Length", textTotal.size()}
    };
    view["/RenderedData"] = PkVariantHash({{"/RunArray", PkVariantList({rendered})}});


    textFrames.push_back(PkVariantHash({{"/Resource", newTextFrame}}));
    textObjects.push_back(PkVariantHash({{"/Model", model}, {"/View", view}}));

    // default resource dict

    textTotal = text;

    PkVariantList newFontSet;

    for (int i = 0; i < fontSet.size(); i++) {
        const PkVariant entry = fontSet.at(i);
        newFontSet.push_back(PkVariantHash({{"/Resource", PkVariantHash({{"/StreamTag", "/CoolTypeFont"}, {"/Identifier", entry}})}}));
    }

    PkVariantHash docObjects = pkHashValue(txt2, "/DocumentObjects").toHash();
    docObjects["/TextObjects"] = textObjects;
    txt2["/DocumentObjects"] = docObjects;
    docResources["/TextFrameSet"] = PkVariantHash({{"/Resources", textFrames}});
    docResources["/FontSet"] = PkVariantHash({{"/Resources", newFontSet}});
    txt2["/DocumentResources"] = docResources;
    textIndex = tIndex;

    return true;
}

PkStringList PsdTextDataConverter::errors() const
{
    return d->errors;
}

PkStringList PsdTextDataConverter::warnings() const
{
    return d->warnings;
}
