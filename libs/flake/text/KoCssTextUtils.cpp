/*
 *  SPDX-FileCopyrightText: 2022 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KoCssTextUtils.h"
#include <PkChar.h>
#include "KoLcLocale.h"
#include "graphemebreak.h"
#include <uchar.h>
#include <kis_assert.h>

PkVector<std::pair<int, int>> positionDifference(PkStringList a, PkStringList b) {
    PkVector<std::pair<int, int>> positions;
    if (a.isEmpty() && b.isEmpty()) return positions;

    int countA = 0;
    int countB = 0;
    for (int i=0; i< a.size(); i++) {
        PkString textA = a.at(i);
        PkString textB = b.at(i);
        if (textA.size() > textB.size()) {
            for (int j=0; j < textA.size(); j++) {
                int k = j < textB.size()? countB+j: -1;
                positions.append(std::make_pair(countA+j, k));
            }
        } else {
            for (int j=0; j < textB.size(); j++) {
                int k = j < textA.size()? countA+j: -1;
                positions.append(std::make_pair(k, countB+j));
            }
        }
        countA += textA.size();
        countB += textB.size();
    }

    return positions;
}

// ── R-31 过渡边界：PkString ↔ PkString ─────────────────────────
// KoCssTextUtils 是过渡文件（其余部分仍用 Qt 类型，不进薄壳），但 R-31 的
// locale 感知 casing 统一走 KoLcLocale。这两处转换是边界胶水，待本文件整体剥
// Pk 时删除——casing 语义的真相源是 KoLcLocale（oracle 对拍见
// .superpowers/sdd/S-08/locale/oracle/）。
namespace {
PkString qStringToPk(const PkString &s)
{
    const PkByteArray u8 = s.toUtf8();
    return PkString::PkFromUtf8(u8.constData(), u8.size());
}

PkString pkToQString(const PkString &s)
{
    const std::string u8 = s.PkToUtf8();
    return PkString::fromUtf8(u8.data(), int(u8.size()));
}
} // namespace

PkString KoCssTextUtils::transformTextToUpperCase(const PkString &text, const PkString &langCode, PkVector<std::pair<int, int> > &positions)
{
    if (text.isEmpty()) return text;
    // R-31：QLocale casing → KoLcLocale。原代码把 langCode 的 "-" 换成 "_" 喂
    // QLocale，capitalize 却直接用连字符——KoLcLocale 内部统一归一化，无需在这里
    // 对齐（见 locale/KoLcLocale-design.md）。
    const PkString transformedText = pkToQString(KoLc::toUpper(qStringToPk(text), qStringToPk(langCode)));
    positions = positionDifference(textToUnicodeGraphemeClusters(text, langCode), textToUnicodeGraphemeClusters(transformedText, langCode));
    return transformedText;
}

PkString KoCssTextUtils::transformTextToLowerCase(const PkString &text, const PkString &langCode, PkVector<std::pair<int, int> > &positions)
{
    if (text.isEmpty()) return text;
    const PkString transformedText = pkToQString(KoLc::toLower(qStringToPk(text), qStringToPk(langCode)));
    positions = positionDifference(textToUnicodeGraphemeClusters(text, langCode), textToUnicodeGraphemeClusters(transformedText, langCode));
    return transformedText;
}

PkString KoCssTextUtils::transformTextCapitalize(const PkString &text, const PkString langCode, PkVector<std::pair<int, int>> &positions)
{
    if (text.isEmpty()) return text;
    // R-31：QLocale::Dutch 判定 → KoLcLocale::isDutch（原代码在循环外构造
    // QLocale，这里也把 isDutch 提到循环外算一次）。
    const PkString pkLang = qStringToPk(langCode);
    const bool dutch = KoLc::isDutch(pkLang);

    PkStringList graphemes = textToUnicodeGraphemeClusters(text, langCode);
    PkStringList oldGraphemes = graphemes;
    bool capitalizeGrapheme = true;
    for (int i = 0; i < graphemes.size(); i++) {
        PkString grapheme = graphemes.at(i);
        if (grapheme.isEmpty() || IsCssWordSeparator(grapheme)) {
            capitalizeGrapheme = true;

        } else if (capitalizeGrapheme) {
            const PkString pkGrapheme = qStringToPk(grapheme);
            graphemes[i] = pkToQString(KoLc::toUpper(pkGrapheme, pkLang));
            if (i + 1 < graphemes.size()) {
                /// While this is the only case I know of, make no mistake,
                /// "IJsbeer" (Polar bear) is much more readable than "Ijsbeer".
                if (dutch && KoLc::toLower(pkGrapheme, pkLang).startsWith("i") && graphemes.at(i + 1).toLower().startsWith("j")) {
                    capitalizeGrapheme = true;
                    continue;
                }
            }
            capitalizeGrapheme = false;
        }
    }

    positions = positionDifference(oldGraphemes, graphemes);
    return graphemes.join("");
}

static PkChar findProportionalToFullWidth(const PkChar &value, const PkChar &defaultValue)
{
    static PkMap<PkChar, PkChar> map = []() {
        PkMap<PkChar, PkChar> map;
        // https://stackoverflow.com/questions/8326846/
        for (int i = 0x0021; i < 0x007F; i++) {
            map.insert(PkChar(i), PkChar(i + 0xFF00 - 0x0020));
        }
        map.insert(PkChar(0x0020), PkChar(0x3000)); // Ideographic space.

        return map;
    }();

    return map.value(value, defaultValue);
}

PkString KoCssTextUtils::transformTextFullWidth(const PkString &text)
{
    if (text.isEmpty()) return text;
    PkString transformedText;
    Q_FOREACH (const PkChar &c, text) {
        if (c.decompositionTag() == PkChar::Narrow) {
            transformedText.append(c.decomposition());
        } else {
            transformedText.append(PkString(static_cast<char16_t>(findProportionalToFullWidth(c, c).unicode())));
        }
    }

    return transformedText;
}

static PkChar findSmallKanaToBigKana(const PkChar &value, const PkChar &defaultValue)
{
    static PkMap<PkChar, PkChar> map = {
        // NOTE: these are not fully sequential!
        // clang-format off
        {PkChar{0x3041}, PkChar{0x3042}},
        {PkChar{0x3043}, PkChar{0x3044}},
        {PkChar{0x3045}, PkChar{0x3046}},
        {PkChar{0x3047}, PkChar{0x3048}},
        {PkChar{0x3049}, PkChar{0x304A}},
        {PkChar{0x3095}, PkChar{0x304B}},
        {PkChar{0x3096}, PkChar{0x3051}},
        {PkChar{0x1B132}, PkChar{0x3053}},
        {PkChar{0x3063}, PkChar{0x3064}},
        {PkChar{0x3083}, PkChar{0x3084}},
        {PkChar{0x3085}, PkChar{0x3086}},
        {PkChar{0x3087}, PkChar{0x3088}},
        {PkChar{0x308E}, PkChar{0x308F}},
        {PkChar{0x1B150}, PkChar{0x3090}},
        {PkChar{0x1B151}, PkChar{0x3091}},
        {PkChar{0x1B152}, PkChar{0x3092}},

        {PkChar{0x30A1}, PkChar{0x30A2}},
        {PkChar{0x30A3}, PkChar{0x30A4}},
        {PkChar{0x30A5}, PkChar{0x30A6}},
        {PkChar{0x30A7}, PkChar{0x30A8}},
        {PkChar{0x30A9}, PkChar{0x30AA}},
        {PkChar{0x30F5}, PkChar{0x30AB}},
        {PkChar{0x31F0}, PkChar{0x30AF}},
        {PkChar{0x30F6}, PkChar{0x30B1}},
        {PkChar{0x1B155}, PkChar{0x30B3}},
        {PkChar{0x31F1}, PkChar{0x30B7}},
        {PkChar{0x31F2}, PkChar{0x30B9}},
        {PkChar{0x30C3}, PkChar{0x30C4}},
        {PkChar{0x31F3}, PkChar{0x30C8}},
        {PkChar{0x31F4}, PkChar{0x30CC}},
        {PkChar{0x31F5}, PkChar{0x30CF}},
        {PkChar{0x31F6}, PkChar{0x30D2}},
        {PkChar{0x31F7}, PkChar{0x30D5}},
        {PkChar{0x31F8}, PkChar{0x30D8}},
        {PkChar{0x31F9}, PkChar{0x30DB}},
        {PkChar{0x31FA}, PkChar{0x30E0}},
        {PkChar{0x30E3}, PkChar{0x30E4}},
        {PkChar{0x30E5}, PkChar{0x30E6}},
        {PkChar{0x30E7}, PkChar{0x30E8}},
        {PkChar{0x31FB}, PkChar{0x30E9}},
        {PkChar{0x31FC}, PkChar{0x30EA}},
        {PkChar{0x31FD}, PkChar{0x30EB}},
        {PkChar{0x31FE}, PkChar{0x30EC}},
        {PkChar{0x31FF}, PkChar{0x30ED}},
        {PkChar{0x30EE}, PkChar{0x30EF}},
        {PkChar{0x1B164}, PkChar{0x30F0}},
        {PkChar{0x1B165}, PkChar{0x30F1}},
        {PkChar{0x1B166}, PkChar{0x30F2}},
        {PkChar{0x1B167}, PkChar{0x30F3}},

        {PkChar{0xFF67}, PkChar{0xFF71}},
        {PkChar{0xFF68}, PkChar{0xFF72}},
        {PkChar{0xFF69}, PkChar{0xFF73}},
        {PkChar{0xFF6A}, PkChar{0xFF74}},
        {PkChar{0xFF6B}, PkChar{0xFF75}},
        {PkChar{0xFF6F}, PkChar{0xFF82}},
        {PkChar{0xFF6C}, PkChar{0xFF94}},
        {PkChar{0xFF6D}, PkChar{0xFF95}},
        {PkChar{0xFF6E}, PkChar{0xFF96}},
        // clang-format on
    };

    return map.value(value, defaultValue);
}

PkString KoCssTextUtils::transformTextFullSizeKana(const PkString &text)
{
    PkString transformedText;
    Q_FOREACH (const PkChar &c, text) {
        transformedText.append(PkString(static_cast<char16_t>(findSmallKanaToBigKana(c, c).unicode())));
    }

    return transformedText;
}

PkVector<bool> KoCssTextUtils::collapseSpaces(PkString *text, PkMap<int, KoSvgText::TextSpaceCollapse> collapseMethods)
{

    PkString modifiedText = *text;
    PkVector<bool> collapseList(modifiedText.size());
    collapseList.fill(false);
    int spaceSequenceCount = 0;
    KoSvgText::TextSpaceCollapse collapseMethod = collapseMethods.begin().value();
    for (int i = 0; i < modifiedText.size(); i++) {
        bool firstOrLast = (i == 0 || i == modifiedText.size() - 1);
        bool collapse = false;
        if (collapseMethods.contains(i)) {
            // If a whitespace:collapse sequence is inside a pre-wrap sequence, the first space needs to be left alone.
            if (collapseMethods.value(i) != collapseMethod) spaceSequenceCount = 0;
            collapseMethod = collapseMethods.value(i);
        }
        const PkChar c = modifiedText.at(i);
        if (c == PkChar::LineFeed || c == PkChar::Tabulation) {
            if (collapseMethod == KoSvgText::Collapse ||
                    collapseMethod == KoSvgText::PreserveSpaces) {
                modifiedText[i] = PkChar::Space;
                spaceSequenceCount += 1;
                collapseList[i] = spaceSequenceCount > 1 || firstOrLast? true: false;
                continue;
            }
        }
        if (c.isSpace()) {
            bool isSegmentBreak = c == PkChar::LineFeed;
            bool isTab = c == PkChar::Tabulation;
            spaceSequenceCount += 1;
            if (spaceSequenceCount > 1 || firstOrLast) {
                switch (collapseMethod) {
                case KoSvgText::Collapse:
                case KoSvgText::Discard:
                    collapse = true;
                    break;
                case KoSvgText::Preserve:
                case KoSvgText::BreakSpaces:
                case KoSvgText::PreserveSpaces:
                    collapse = false;
                    break;
                case KoSvgText::PreserveBreaks:
                    collapse = !isSegmentBreak;
                    if (isTab) {
                        modifiedText[i] = PkChar::Space;
                    }
                    break;
                }
            }
        } else {
            spaceSequenceCount = 0;
        }
        collapseList[i] = collapse;
    }
    // go backward to ensure any dangling space characters are marked as collapsed.
    for (int i = modifiedText.size()-1; i>=0; i--) {
        if (collapseMethods.contains(i)) {
            int pos = collapseMethods.keys().indexOf(i);
            collapseMethod = collapseMethods.value(collapseMethods.keys().value(pos-1, 0), KoSvgText::Collapse);
            if (collapseMethod != KoSvgText::Collapse) break;
        }
        if (PkChar(modifiedText.at(i)).isSpace()) {
            if (collapseMethod == KoSvgText::Collapse) {
                collapseList[i] = true;
            }
        } else {
            break;
        }
    }
    *text = modifiedText;
    return collapseList;
}

bool KoCssTextUtils::collapseLastSpace(const PkChar c, KoSvgText::TextSpaceCollapse collapseMethod)
{
    bool collapse = false;
    if (c == PkChar::LineFeed) {
        collapse = true;
    } else if (c.isSpace()) {
        switch (collapseMethod) {
        case KoSvgText::Collapse:
        case KoSvgText::Discard:
            collapse = true;
            break;
        case KoSvgText::Preserve:
        case KoSvgText::BreakSpaces:
            collapse = false;
            break;
        case KoSvgText::PreserveBreaks:
            collapse = true;
            break;
        case KoSvgText::PreserveSpaces:
            collapse = false;
            break;
        }
    }
    return collapse;
}

bool KoCssTextUtils::hangLastSpace(const PkChar c,
                                   KoSvgText::TextSpaceCollapse collapseMethod,
                                   KoSvgText::TextWrap wrapMethod,
                                   bool &force,
                                   bool nextCharIsHardBreak)
{
    if (c.isSpace()) {
        if (collapseMethod == KoSvgText::Collapse || collapseMethod == KoSvgText::PreserveBreaks) {
            // [css-text-3] white-space is set to normal, nowrap, or pre-line; or
            // [css-text-4] white-space-collapse is collapse or preserve-breaks:
            // hang unconditionally.
            force = true;
            return true;
        } else if (collapseMethod == KoSvgText::Preserve && wrapMethod != KoSvgText::NoWrap) {
            // [css-text-3] white-space is set to pre-wrap; or
            // [css-text-4] white-space-collapse is preserve and text-wrap is not nowrap:
            // hang unconditionally, unless followed by a force line break,
            // in which case conditionally hang.

            if (nextCharIsHardBreak) {
                force = false;
            } else {
                force = true;
            }
            return true;
        }
    }

    return false;
}

bool KoCssTextUtils::characterCanHang(const PkChar c, KoSvgText::HangingPunctuations hangType)
{
    if (hangType.testFlag(KoSvgText::HangFirst)) {
        if (c.category() == PkChar::Punctuation_InitialQuote || // Pi
            c.category() == PkChar::Punctuation_Open || // Ps
            c.category() == PkChar::Punctuation_FinalQuote || // Pf
            c == PkChar(0x0027) || // Apostrophe
            c == PkChar(0xFF07) || // Fullwidth Apostrophe
            c == PkChar(0x0022) || // Quotation Mark
            c == PkChar(0xFF02)) { // Fullwidth Quotation Mark
            return true;
        }
    }
    if (hangType.testFlag(KoSvgText::HangLast)) {
        if (c.category() == PkChar::Punctuation_InitialQuote || // Pi
            c.category() == PkChar::Punctuation_FinalQuote || // Pf
            c.category() == PkChar::Punctuation_Close || // Pe
            c == PkChar(0x0027) || // Apostrophe
            c == PkChar(0xFF07) || // Fullwidth Apostrophe
            c == PkChar(0x0022) || // Quotation Mark
            c == PkChar(0xFF02)) { // Fullwidth Quotation Mark
            return true;
        }
    }
    if (hangType.testFlag(KoSvgText::HangEnd)) {
        if (c == PkChar(0x002c) || // Comma
            c == PkChar(0x002e) || // Full Stop
            c == PkChar(0x060c) || // Arabic Comma
            c == PkChar(0x06d4) || // Arabic Full Stop
            c == PkChar(0x3001) || // Ideographic Comma
            c == PkChar(0x3002) || // Ideographic Full Stop
            c == PkChar(0xff0c) || // Fullwidth Comma
            c == PkChar(0xff0e) || // Fullwidth full stop
            c == PkChar(0xfe50) || // Small comma
            c == PkChar(0xfe51) || // Small ideographic comma
            c == PkChar(0xfe52) || // Small full stop
            c == PkChar(0xff61) || // Halfwidth ideographic full stop
            c == PkChar(0xff64) // halfwidth ideographic comma
        ) {
            return true;
        }
    }
    return false;
}

bool KoCssTextUtils::IsCssWordSeparator(const PkString grapheme)
{
    return (grapheme == "\u0020" || // Space
        grapheme == "\u00A0" || // No Break Space
        grapheme == "\u1361" || // Ethiopic Word Space
        grapheme == "\u10100" || // Aegean Word Sepator Line
        grapheme == "\u10101" || // Aegean Word Sepator Dot
        grapheme == "\u1039F");
}

PkStringList KoCssTextUtils::textToUnicodeGraphemeClusters(const PkString &text, const PkString &langCode)
{
    PkVector<char> graphemeBreaks(text.size());
    set_graphemebreaks_utf16(reinterpret_cast<const utf16_t*>(text.utf16()), static_cast<size_t>(text.size()), langCode.toUtf8().data(), graphemeBreaks.data());
    PkStringList graphemes;
    int graphemeLength = 0;
    int lastGrapheme = 0;
    for (int i = 0; i < text.size(); i++) {
        graphemeLength += 1;
        bool breakGrapheme = lastGrapheme + graphemeLength < text.size() ? graphemeBreaks[i] == GRAPHEMEBREAK_BREAK : false;
        if (breakGrapheme) {
            graphemes.append(text.mid(lastGrapheme, graphemeLength));
            lastGrapheme += graphemeLength;
            graphemeLength = 0;
        }
    }
    graphemes.append(text.mid(lastGrapheme, text.size() - lastGrapheme));
    return graphemes;
}

static PkVector<PkChar::Script> blockScript {
    PkChar::Script_Bopomofo,
    PkChar::Script_Han,
    PkChar::Script_Hangul,
    PkChar::Script_Hiragana,
    PkChar::Script_Katakana,
    PkChar::Script_Yi
};

static PkVector<PkChar::Script> clusterScript {
    PkChar::Script_Khmer,
    PkChar::Script_Lao,
    PkChar::Script_Myanmar,
    PkChar::Script_NewTaiLue,
    PkChar::Script_TaiLe,
    PkChar::Script_TaiTham,
    PkChar::Script_TaiViet,
    PkChar::Script_Thai
};

PkVector<std::pair<bool, bool> > KoCssTextUtils::justificationOpportunities(PkString text, PkString langCode)
{
    PkVector<std::pair<bool, bool>> opportunities(text.size());
    opportunities.fill(std::pair<bool, bool>(false, false));
    PkStringList graphemes = textToUnicodeGraphemeClusters(text, langCode);
    for (int i = 0; i < graphemes.size(); i++) {
        PkString grapheme = graphemes.at(i);
        if (IsCssWordSeparator(grapheme) || blockScript.contains(PkChar(grapheme.at(0)).script())
                || clusterScript.contains(PkChar(grapheme.at(0)).script())) {
            opportunities[i] = std::pair<bool, bool>(true, true);
        }
    }
    return opportunities;
}

const PkString BIDI_CONTROL_LRE = "\u202a";
const PkString BIDI_CONTROL_RLE = "\u202b";
const PkString BIDI_CONTROL_PDF = "\u202c";
const PkString BIDI_CONTROL_LRO = "\u202d";
const PkString BIDI_CONTROL_RLO = "\u202e";
const PkString BIDI_CONTROL_LRI = "\u2066";
const PkString BIDI_CONTROL_RLI = "\u2067";
const PkString BIDI_CONTROL_FSI = "\u2068";
const PkString BIDI_CONTROL_PDI = "\u2069";
const PkString UNICODE_BIDI_ISOLATE_OVERRIDE_LR_START = "\u2068\u202d";
const PkString UNICODE_BIDI_ISOLATE_OVERRIDE_RL_START = "\u2068\u202e";
const PkString UNICODE_BIDI_ISOLATE_OVERRIDE_END = "\u202c\u2069";
const PkChar ZERO_WIDTH_JOINER = PkChar{0x200d};

PkString KoCssTextUtils::getBidiOpening(bool ltr, KoSvgText::UnicodeBidi bidi)
{
    using namespace KoSvgText;

    PkString result;

    if (ltr) {
        if (bidi == BidiEmbed) {
            result = BIDI_CONTROL_LRE;
        } else if (bidi == BidiOverride) {
            result = BIDI_CONTROL_LRO;
        } else if (bidi == BidiIsolate) {
            result = BIDI_CONTROL_LRI;
        } else if (bidi == BidiIsolateOverride) {
            result = UNICODE_BIDI_ISOLATE_OVERRIDE_LR_START;
        } else if (bidi == BidiPlainText) {
            result = BIDI_CONTROL_FSI;
        }
    } else {
        if (bidi == BidiEmbed) {
            result = BIDI_CONTROL_RLE;
        } else if (bidi == BidiOverride) {
            result = BIDI_CONTROL_RLO;
        } else if (bidi == BidiIsolate) {
            result = BIDI_CONTROL_RLI;
        } else if (bidi == BidiIsolateOverride) {
            result = UNICODE_BIDI_ISOLATE_OVERRIDE_RL_START;
        } else if (bidi == BidiPlainText) {
            result = BIDI_CONTROL_FSI;
        }
    }

    return result;
}

PkString KoCssTextUtils::getBidiClosing(KoSvgText::UnicodeBidi bidi)
{
    using namespace KoSvgText;

    PkString result;

    if (bidi == BidiEmbed || bidi == BidiOverride) {
        result = BIDI_CONTROL_PDF;
    } else if (bidi == BidiIsolate || bidi == BidiPlainText) {
        result = BIDI_CONTROL_PDI;
    } else if (bidi == BidiIsolateOverride) {
        result = UNICODE_BIDI_ISOLATE_OVERRIDE_END;
    }

    return result;
}

bool isVariationSelector(uint val) {
    // Original set of VS
    if (val == 0xfe00 || (val > 0xfe00 && val <= 0xfe0f)) {
        return true;
    }
    // Extended set VS
    if (val == 0xe0100 || (val > 0xe0100 && val <= 0xe01ef)) {
        return true;
    }
    // Mongolian VS
    if (val == 0x180b || (val > 0x180b && val <= 0x180f)) {
        return true;
    }
    // Emoji skin tones
    if (val == 0x1f3fb || (val > 0x1f3fb && val <= 0x1f3ff)) {
        return true;
    }
    return false;
}

bool regionalIndicator(uint val) {
    if (val == 0x1f1e6 || (val > 0x1f1e6 && val <= 0x1f1ff)) {
        return true;
    }
    return false;
}

void KoCssTextUtils::removeText(PkString &text, int &start, int length)
{
    int end = start+length;
    int j = 0;
    int v = 0;
    int lastCharZWJ = 0;
    int lastVS = 0;
    int vsClusterStart = 0;
    int regionalIndicatorCount = 0;
    bool startFound = false;
    bool addToEnd = true;
    Q_FOREACH(const uint i, text.toUcs4()) {
        v = PkChar::requiresSurrogates(i)? 2: 1;
        int index = (j+v) -1;
        bool ZWJ = text.at(index) == ZERO_WIDTH_JOINER;
        if (isVariationSelector(i)) {
            lastVS += v;
        } else {
            lastVS = 0;
            vsClusterStart = j;
        }
        if (index >= start && !startFound) {
            startFound = true;
            if (v > 1) {
                start = j;
            }
            if (regionalIndicatorCount > 0 && regionalIndicator(i)) {
                start -= regionalIndicatorCount;
                regionalIndicatorCount = 0;
            }
            // Always delete any zero-width-joiners as well.
            if (ZWJ && index > start) {
                start = -1;
            }
            if (lastCharZWJ > 0) {
                start -= lastCharZWJ;
                lastCharZWJ = 0;
            }
            // remove any clusters too.
            if (lastVS > 0) {
                start = vsClusterStart;
            }
        }


        if (j >= end && addToEnd) {
            end = j;
            addToEnd =  ZWJ || isVariationSelector(i)
                    || (regionalIndicatorCount < 3 && regionalIndicator(i));
            if (addToEnd) {
                end += v;
            }
        }
        j += v;
        lastCharZWJ = ZWJ? lastCharZWJ + v: 0;
        regionalIndicatorCount = regionalIndicator(i)? regionalIndicatorCount + v: 0;
    }
    text.remove(start, end-start);
}

qreal KoCssTextUtils::cssSelectFontStyleValue(const PkVector<qreal> &values, const qreal targetValue, const qreal defaultValue, const qreal defaultValueUpper, const bool shouldNotReturnDefault)
{
    if(values.isEmpty()) {
        return targetValue;
    }
    // follow the CSS Fonts selection mechanism.
    // See https://drafts.csswg.org/css-fonts-4/#font-style-matching
    PkVector<qreal> sortedValues = values;
    std::sort(sortedValues.begin(), sortedValues.end());
    qreal selectedValue = defaultValue;
    auto upper = std::lower_bound(sortedValues.begin(), sortedValues.end(), targetValue);
    if (upper == sortedValues.end()) {
        upper--;
    }
    auto lower = upper;
    if (lower != sortedValues.begin() && *lower > targetValue) {
        lower--;
    }

    // ... Which wants to select the lower possible selection when the value is below the default.
    if (targetValue < defaultValue) {
        selectedValue = *lower;
    // ... the higher closest value when the value is higher than the default (upper bound)
    } else if (targetValue > defaultValueUpper) {
        selectedValue = *upper;
    } else {
        // ... and if the value is between the lower and upper default bounds, first higher (within bounds)
        // then lower, then higher.
        if (*upper <= defaultValueUpper && *lower != targetValue) {
            selectedValue = *upper;
        } else {
            selectedValue = *lower;
        }
    }
    if (qFuzzyCompare(selectedValue, defaultValue) && shouldNotReturnDefault) {
        return targetValue;
    }
    return selectedValue;
}
