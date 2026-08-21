/*
 *  SPDX-FileCopyrightText: 2026 S-08 (R-31)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KoLcLocale.h"

#include <pk/string/PkStringCodec.h>

#include <map>
#include <cctype>
#include <string>
#include <vector>

namespace KoLc {

namespace {

// ── 语言子标签匹配 ────────────────────────────────────────────
// 取 langCode 里第一个 '-'/'_' 分隔符前的语言子标签（小写）。忽略脚本/地区/编码。
PkString languageSubtag(const PkString &langCode)
{
    for (int i = 0; i < langCode.size(); ++i) {
        const char16_t c = langCode.at(i);
        if (c == u'-' || c == u'_' || c == u'.') {
            return langCode.left(i).toLower();
        }
    }
    return langCode.toLower();
}

bool langIs(const PkString &langCode, const char *iso)
{
    return languageSubtag(langCode) == PkString(iso);
}

// ── UTF-16 <-> 码点 工具（capitalize/bcp47Name 用）─────────────
// 取 text 在 UTF-16 码元下标 i 处的码点（处理代理对），返回码点并写码元宽度。
unsigned codePointAt(const PkString &text, int i, int &width)
{
    const char16_t hi = text.at(i);
    if (hi >= 0xD800 && hi <= 0xDBFF && i + 1 < text.size()) {
        const char16_t lo = text.at(i + 1);
        if (lo >= 0xDC00 && lo <= 0xDFFF) {
            width = 2;
            return 0x10000 + ((static_cast<unsigned>(hi) - 0xD800) << 10)
                    + (static_cast<unsigned>(lo) - 0xDC00);
        }
    }
    width = 1;
    return hi;
}

PkString fromCodePoint(unsigned cp)
{
    std::string u8;
    if (cp <= 0x7F) {
        u8.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        u8.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        u8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        u8.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        u8.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        u8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        u8.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        u8.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        u8.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        u8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return PkString::PkFromUtf8(u8.data(), static_cast<int>(u8.size()));
}

// CSS word separator（KoCssTextUtils::IsCssWordSeparator 同一组）。
bool isCssWordSeparatorCp(unsigned cp)
{
    return cp == 0x0020 || cp == 0x00A0 || cp == 0x1361 || cp == 0x10100
            || cp == 0x10101 || cp == 0x1039F;
}

// 按码元表替换后走 Unicode 默认转换（Turkish/Azeri 用）。
PkString substituteThenCase(const PkString &text,
                            char16_t fromA, char16_t toA,
                            char16_t fromB, char16_t toB,
                            bool upper)
{
    std::vector<char16_t> units;
    units.reserve(static_cast<std::size_t>(text.size()));
    for (int i = 0; i < text.size(); ++i) {
        const char16_t c = text.at(i);
        if (c == fromA) {
            units.push_back(toA);
        } else if (c == fromB) {
            units.push_back(toB);
        } else {
            units.push_back(c);
        }
    }
    const std::string u8 = PkStringCodec::ToUtf8(units);
    const PkString prep = PkString::PkFromUtf8(u8.data(), static_cast<int>(u8.size()));
    return upper ? prep.toUpper() : prep.toLower();
}

} // namespace
// ── toUpper / toLower ─────────────────────────────────────────
PkString toUpper(const PkString &text, const PkString &langCode)
{
    if (langIs(langCode, "tr") || langIs(langCode, "az")) {
        // Turkish/Azeri：i(0x0069)→İ(0x0130)、ı(0x0131)→I(0x0049)，再走 Unicode 默认大写。
        // İ 的默认大写是自身，I 的默认大写是自身——替换后默认规则给出正确结果。
        return substituteThenCase(text, 0x0069, 0x0130, 0x0131, 0x0049, true);
    }
    // de：ß→SS 已含 PkString 默认大写。nl：ij→IJ 逐字符即达。
    return text.toUpper();
}

PkString toLower(const PkString &text, const PkString &langCode)
{
    if (langIs(langCode, "tr") || langIs(langCode, "az")) {
        // Turkish/Azeri：I(0x0049)→ı(0x0131)、İ(0x0130)→i(0x0069)，再走 Unicode 默认小写。
        return substituteThenCase(text, 0x0049, 0x0131, 0x0130, 0x0069, false);
    }
    return text.toLower();
}

// ── capitalize（CSS text-transform: capitalize 的 locale 层）───
PkString capitalize(const PkString &text, const PkString &langCode)
{
    if (text.isEmpty()) {
        return text;
    }
    const bool dutch = isDutch(langCode);
    bool capNext = true;
    PkString result;
    int i = 0;
    while (i < text.size()) {
        int width = 0;
        const unsigned cp = codePointAt(text, i, width);
        if (isCssWordSeparatorCp(cp)) {
            result += fromCodePoint(cp);
            capNext = true;
            i += width;
            continue;
        }
        PkString grapheme = text.mid(i, width);
        if (capNext) {
            // Dutch IJ：词首 grapheme 小写以 "i" 开头且下一 grapheme 小写以 "j" 开头
            // → 两个都大写（"ijsbeer" → "IJsbeer"）。判据照 KoCssTextUtils.cpp:74
            // 原语义：grapheme.toLower() 用 Unicode 默认小写（非 locale 感知）。
            bool dutchIj = dutch
                    && grapheme.toLower().startsWith(PkString("i"))
                    && i + width < text.size();
            if (dutchIj) {
                const PkString nextGrapheme = text.mid(i + width, 1);
                dutchIj = nextGrapheme.toLower().startsWith(PkString("j"));
            }
            grapheme = toUpper(grapheme, langCode);
            result += grapheme;
            capNext = dutchIj;
        } else {
            result += grapheme;
        }
        i += width;
    }
    return result;
}

// ── bcp47Name ─────────────────────────────────────────────────
// 语义照 Qt 5.15.7 QLocale::bcp47Name()（qlocale.cpp QLocaleId::
// withLikelySubtagsRemoved + withLikelySubtagsAdded）：CLDR likely-subtags
// 移除冗余子标签。数据取自 CLDR 40 likelySubtags，覆盖常用语言；表外语言
// 回退为"已解析的最短形式"（oracle 对拍登记差异）。

namespace {

// lang → (defaultScript, defaultCountry)。
struct LangDefault {
    const char *script;
    const char *country;
};

const std::map<std::string, LangDefault> &langDefaults()
{
    static const std::map<std::string, LangDefault> m = {
        {"af", {"Latn", "ZA"}}, {"am", {"Ethi", "ET"}}, {"ar", {"Arab", "EG"}},
        {"as", {"Beng", "IN"}}, {"az", {"Latn", "AZ"}}, {"be", {"Cyrl", "BY"}},
        {"bg", {"Cyrl", "BG"}}, {"bn", {"Beng", "BD"}}, {"bs", {"Latn", "BA"}},
        {"ca", {"Latn", "ES"}}, {"cs", {"Latn", "CZ"}}, {"cy", {"Latn", "GB"}},
        {"da", {"Latn", "DK"}}, {"de", {"Latn", "DE"}}, {"el", {"Grek", "GR"}},
        {"en", {"Latn", "US"}}, {"es", {"Latn", "ES"}}, {"et", {"Latn", "EE"}},
        {"eu", {"Latn", "ES"}}, {"fa", {"Arab", "IR"}}, {"fi", {"Latn", "FI"}},
        {"fil", {"Latn", "PH"}}, {"fr", {"Latn", "FR"}}, {"ga", {"Latn", "IE"}},
        {"gl", {"Latn", "ES"}}, {"gu", {"Gujr", "IN"}}, {"he", {"Hebr", "IL"}},
        {"hi", {"Deva", "IN"}}, {"hr", {"Latn", "HR"}}, {"hu", {"Latn", "HU"}},
        {"hy", {"Armn", "AM"}}, {"id", {"Latn", "ID"}}, {"is", {"Latn", "IS"}},
        {"it", {"Latn", "IT"}}, {"ja", {"Jpan", "JP"}}, {"ka", {"Geor", "GE"}},
        {"kk", {"Cyrl", "KZ"}}, {"km", {"Khmr", "KH"}}, {"kn", {"Knda", "IN"}},
        {"ko", {"Kore", "KR"}}, {"lo", {"Laoo", "LA"}}, {"lt", {"Latn", "LT"}},
        {"lv", {"Latn", "LV"}}, {"mk", {"Cyrl", "MK"}}, {"ml", {"Mlym", "IN"}},
        {"mn", {"Cyrl", "MN"}}, {"mr", {"Deva", "IN"}}, {"ms", {"Latn", "MY"}},
        {"my", {"Mymr", "MM"}}, {"nb", {"Latn", "NO"}}, {"ne", {"Deva", "NP"}},
        {"nl", {"Latn", "NL"}}, {"nn", {"Latn", "NO"}}, {"or", {"Orya", "IN"}},
        {"pa", {"Guru", "IN"}}, {"pl", {"Latn", "PL"}}, {"ps", {"Arab", "AF"}},
        {"pt", {"Latn", "BR"}}, {"ro", {"Latn", "RO"}}, {"ru", {"Cyrl", "RU"}},
        {"si", {"Sinh", "LK"}}, {"sk", {"Latn", "SK"}}, {"sl", {"Latn", "SI"}},
        {"sq", {"Latn", "AL"}}, {"sr", {"Cyrl", "RS"}}, {"sv", {"Latn", "SE"}},
        {"sw", {"Latn", "TZ"}}, {"ta", {"Taml", "IN"}}, {"te", {"Telu", "IN"}},
        {"th", {"Thai", "TH"}}, {"tr", {"Latn", "TR"}}, {"uk", {"Cyrl", "UA"}},
        {"ur", {"Arab", "PK"}}, {"uz", {"Latn", "UZ"}}, {"vi", {"Latn", "VN"}},
        {"zh", {"Hans", "CN"}}, {"zu", {"Latn", "ZA"}},
    };
    return m;
}

// (lang, country) → 非默认 script（country 隐含 script 的特例）。
const std::map<std::string, const char *> &langCountryScript()
{
    static const std::map<std::string, const char *> m = {
        {"zh-TW", "Hant"}, {"zh-HK", "Hant"}, {"zh-MO", "Hant"},
        {"zh-SG", "Hans"}, {"az-IR", "Arab"}, {"pa-PK", "Arab"},
        {"sr-ME", "Latn"}, {"sr-BA", "Latn"},
    };
    return m;
}

// (lang, script) → 非默认 country（script 隐含 country 的特例）。
const std::map<std::string, const char *> &langScriptCountry()
{
    static const std::map<std::string, const char *> m = {
        {"zh-Hant", "TW"}, {"zh-Hans", "CN"}, {"sr-Latn", "RS"},
        {"sr-Cyrl", "RS"}, {"az-Arab", "IR"}, {"pa-Arab", "PK"},
    };
    return m;
}

struct LikelyId {
    std::string lang;
    std::string script;
    std::string country;
};

// CLDR likely-subtags 近似：给定 (lang, script, country) 补全缺省子标签。
LikelyId likely(const std::string &lang, const std::string &script, const std::string &country)
{
    const auto &defs = langDefaults();
    const auto itDef = defs.find(lang);
    const std::string defScript = itDef == defs.end() ? "" : itDef->second.script;
    const std::string defCountry = itDef == defs.end() ? "" : itDef->second.country;

    if (!script.empty() && !country.empty()) {
        // 三元组齐备：按给定的脚本/地区返回（表内组合即最大 likely）。
        return {lang, script, country};
    }
    if (!script.empty()) {
        // (lang, script) → country
        const auto &sc = langScriptCountry();
        const auto itSc = sc.find(lang + "-" + script);
        if (itSc != sc.end()) {
            return {lang, script, itSc->second};
        }
        return {lang, script, defCountry};
    }
    if (!country.empty()) {
        // (lang, country) → script
        const auto &cs = langCountryScript();
        const auto itCs = cs.find(lang + "-" + country);
        if (itCs != cs.end()) {
            return {lang, itCs->second, country};
        }
        return {lang, defScript, country};
    }
    return {lang, defScript, defCountry};
}

bool sameId(const LikelyId &a, const LikelyId &b)
{
    return a.lang == b.lang && a.script == b.script && a.country == b.country;
}

} // namespace

PkString bcp47Name(const PkString &langCode)
{
    if (langCode.isEmpty()) {
        return langCode;
    }
    if (langIs(langCode, "C")) {
        return PkString("en");
    }

    // 解析子标签：language 可选 -script(4) 可选 -country(2) 可选 -rest
    std::string lang, script, country;
    int pos = 0;
    const int n = langCode.size();
    auto readSubtag = [&](int &p) -> std::string {
        std::string s;
        while (p < n && langCode.at(p) != u'-' && langCode.at(p) != u'_') {
            const char16_t c = langCode.at(p);
            if (c < 0x80) {
                s.push_back(static_cast<char>(c));
            }
            ++p;
        }
        if (p < n) {
            ++p;
        }
        return s;
    };
    lang = readSubtag(pos);
    if (pos < n) {
        const std::string s2 = readSubtag(pos);
        if (s2.size() == 4) {
            script = s2;
            if (pos < n) {
                country = readSubtag(pos);
            }
        } else if (s2.size() == 2 || s2.size() == 3) {
            country = s2;
        }
    }

    // BCP47 大小写规范：语言小写、地区大写、script Title-case。
    for (auto &c : lang) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    for (auto &c : country) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    if (!script.empty()) {
        script[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(script[0])));
        for (std::size_t k = 1; k < script.size(); ++k) {
            script[k] = static_cast<char>(std::tolower(static_cast<unsigned char>(script[k])));
        }
    }

    // withLikelySubtagsRemoved：
    //   max = likely(lang, script, country)
    //   1) likely(lang) == max            → lang
    //   2) likely(lang, country) == max   → lang-country
    //   3) likely(lang, script) == max    → lang-script
    //   4) else                            → max
    const auto &defs = langDefaults();
    const auto itDef = defs.find(lang);
    if (itDef == defs.end()) {
        // 表外语言：原样输出已解析的最短形式。
        PkString out(lang.c_str());
        if (!script.empty()) {
            out += PkString("-") + PkString(script.c_str());
        }
        if (!country.empty()) {
            out += PkString("-") + PkString(country.c_str());
        }
        return out;
    }

    const LikelyId max = likely(lang, script, country);

    // 1) 裸语言
    if (sameId(likely(lang, "", ""), max)) {
        return PkString(lang.c_str());
    }
    // 2) lang-country
    if (!country.empty() && sameId(likely(lang, "", country), max)) {
        return PkString(lang.c_str()) + PkString("-") + PkString(country.c_str());
    }
    // 3) lang-script
    if (!script.empty() && sameId(likely(lang, script, ""), max)) {
        return PkString(lang.c_str()) + PkString("-") + PkString(script.c_str());
    }
    // 4) max 本身（含 likely 补全的 script/country）
    PkString out(lang.c_str());
    if (!max.script.empty() && max.script != itDef->second.script) {
        out += PkString("-") + PkString(max.script.c_str());
    }
    if (!max.country.empty() && max.country != itDef->second.country) {
        out += PkString("-") + PkString(max.country.c_str());
    }
    if (out == PkString(lang.c_str())) {
        return PkString(lang.c_str());
    }
    return out;
}

// ── isDutch ───────────────────────────────────────────────────
bool isDutch(const PkString &langCode)
{
    return langIs(langCode, "nl");
}

} // namespace KoLc
