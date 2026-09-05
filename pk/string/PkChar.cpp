#include "PkChar.h"
#include <unicode/uchar.h>
#include <unicode/unorm2.h>
#include <map>
#include <string>

// ── 值已对齐 ICU 的枚举：直接透传 ──

namespace {
// ICU UCharDirection 值 → Qt/Pk Direction 枚举值（生成期核对，顺序差异仅在
// FSI/LRI/RLI/PDI 一段）
inline int pairs_dir(int icu) noexcept
{
    switch (icu) {
    case 0: return 0;  case 1: return 1;  case 2: return 2;  case 3: return 3;
    case 4: return 4;  case 5: return 5;  case 6: return 6;  case 7: return 7;
    case 8: return 8;  case 9: return 9;  case 10: return 10; case 11: return 11;
    case 12: return 12; case 13: return 13; case 14: return 14; case 15: return 15;
    case 16: return 16; case 17: return 17; case 18: return 18; case 19: return 21; /* FSI */
    case 20: return 19; /* LRI */ case 21: return 20; /* RLI */ case 22: return 22; /* PDI */
    default: return 0;
    }
}
}

PkChar::Category PkChar::category(uint ucs4) noexcept
{
    if (ucs4 > PkChar::LastValidCodePoint) return PkChar::Other_NotAssigned;
    return Category(u_getIntPropertyValue(UChar32(ucs4), UCHAR_GENERAL_CATEGORY));
}

PkChar::Direction PkChar::direction(uint ucs4) noexcept
{
    if (ucs4 > PkChar::LastValidCodePoint) return PkChar::DirL;
    return static_cast<Direction>(pairs_dir(u_getIntPropertyValue(UChar32(ucs4), UCHAR_BIDI_CLASS)));
}

PkChar::JoiningType PkChar::joiningType(uint ucs4) noexcept
{
    if (ucs4 > PkChar::LastValidCodePoint) return PkChar::Joining_None;
    return JoiningType(u_getIntPropertyValue(UChar32(ucs4), UCHAR_JOINING_TYPE));
}

PkChar::Decomposition PkChar::decompositionTag(uint ucs4) noexcept
{
    if (ucs4 > PkChar::LastValidCodePoint) return PkChar::NoDecomposition;
    return Decomposition(u_getIntPropertyValue(UChar32(ucs4), UCHAR_DECOMPOSITION_TYPE));
}

uint PkChar::mirroredChar(uint ucs4) noexcept
{
    return static_cast<uint>(u_charMirror(UChar32(ucs4)));
}

int PkChar::digitValue(uint ucs4) noexcept
{
    return u_digit(UChar32(ucs4), 10);
}

uint PkChar::toLower(uint ucs4) noexcept
{
    return static_cast<uint>(u_tolower(UChar32(ucs4)));
}

uint PkChar::toUpper(uint ucs4) noexcept
{
    return static_cast<uint>(u_toupper(UChar32(ucs4)));
}

uint PkChar::toTitleCase(uint ucs4) noexcept
{
    return static_cast<uint>(u_totitle(UChar32(ucs4)));
}

// ── Script：ICU UScriptCode 与 Qt 枚举顺序不同，按长名映射 ──
PkChar::Script PkChar::script(uint ucs4) noexcept
{
    if (ucs4 > PkChar::LastValidCodePoint) return PkChar::Script_Unknown;
    const int icu = u_getIntPropertyValue(UChar32(ucs4), UCHAR_SCRIPT);
    if (icu < 0) return PkChar::Script_Unknown;
    static const std::map<std::string, Script> m = {
        {"unknown", Script_Unknown},
        {"inherited", Script_Inherited},
        {"common", Script_Common},
        {"latin", Script_Latin},
        {"greek", Script_Greek},
        {"cyrillic", Script_Cyrillic},
        {"armenian", Script_Armenian},
        {"hebrew", Script_Hebrew},
        {"arabic", Script_Arabic},
        {"syriac", Script_Syriac},
        {"thaana", Script_Thaana},
        {"devanagari", Script_Devanagari},
        {"bengali", Script_Bengali},
        {"gurmukhi", Script_Gurmukhi},
        {"gujarati", Script_Gujarati},
        {"oriya", Script_Oriya},
        {"tamil", Script_Tamil},
        {"telugu", Script_Telugu},
        {"kannada", Script_Kannada},
        {"malayalam", Script_Malayalam},
        {"sinhala", Script_Sinhala},
        {"thai", Script_Thai},
        {"lao", Script_Lao},
        {"tibetan", Script_Tibetan},
        {"myanmar", Script_Myanmar},
        {"georgian", Script_Georgian},
        {"hangul", Script_Hangul},
        {"ethiopic", Script_Ethiopic},
        {"cherokee", Script_Cherokee},
        {"canadianaboriginal", Script_CanadianAboriginal},
        {"ogham", Script_Ogham},
        {"runic", Script_Runic},
        {"khmer", Script_Khmer},
        {"mongolian", Script_Mongolian},
        {"hiragana", Script_Hiragana},
        {"katakana", Script_Katakana},
        {"bopomofo", Script_Bopomofo},
        {"han", Script_Han},
        {"yi", Script_Yi},
        {"olditalic", Script_OldItalic},
        {"gothic", Script_Gothic},
        {"deseret", Script_Deseret},
        {"tagalog", Script_Tagalog},
        {"hanunoo", Script_Hanunoo},
        {"buhid", Script_Buhid},
        {"tagbanwa", Script_Tagbanwa},
        {"coptic", Script_Coptic},
        {"limbu", Script_Limbu},
        {"taile", Script_TaiLe},
        {"linearb", Script_LinearB},
        {"ugaritic", Script_Ugaritic},
        {"shavian", Script_Shavian},
        {"osmanya", Script_Osmanya},
        {"cypriot", Script_Cypriot},
        {"braille", Script_Braille},
        {"buginese", Script_Buginese},
        {"newtailue", Script_NewTaiLue},
        {"glagolitic", Script_Glagolitic},
        {"tifinagh", Script_Tifinagh},
        {"sylotinagri", Script_SylotiNagri},
        {"oldpersian", Script_OldPersian},
        {"kharoshthi", Script_Kharoshthi},
        {"balinese", Script_Balinese},
        {"cuneiform", Script_Cuneiform},
        {"phoenician", Script_Phoenician},
        {"phagspa", Script_PhagsPa},
        {"nko", Script_Nko},
        {"sundanese", Script_Sundanese},
        {"lepcha", Script_Lepcha},
        {"olchiki", Script_OlChiki},
        {"vai", Script_Vai},
        {"saurashtra", Script_Saurashtra},
        {"kayahli", Script_KayahLi},
        {"rejang", Script_Rejang},
        {"lycian", Script_Lycian},
        {"carian", Script_Carian},
        {"lydian", Script_Lydian},
        {"cham", Script_Cham},
        {"taitham", Script_TaiTham},
        {"taiviet", Script_TaiViet},
        {"avestan", Script_Avestan},
        {"egyptianhieroglyphs", Script_EgyptianHieroglyphs},
        {"samaritan", Script_Samaritan},
        {"lisu", Script_Lisu},
        {"bamum", Script_Bamum},
        {"javanese", Script_Javanese},
        {"meeteimayek", Script_MeeteiMayek},
        {"imperialaramaic", Script_ImperialAramaic},
        {"oldsoutharabian", Script_OldSouthArabian},
        {"inscriptionalparthian", Script_InscriptionalParthian},
        {"inscriptionalpahlavi", Script_InscriptionalPahlavi},
        {"oldturkic", Script_OldTurkic},
        {"kaithi", Script_Kaithi},
        {"batak", Script_Batak},
        {"brahmi", Script_Brahmi},
        {"mandaic", Script_Mandaic},
        {"chakma", Script_Chakma},
        {"meroiticcursive", Script_MeroiticCursive},
        {"meroitichieroglyphs", Script_MeroiticHieroglyphs},
        {"miao", Script_Miao},
        {"sharada", Script_Sharada},
        {"sorasompeng", Script_SoraSompeng},
        {"takri", Script_Takri},
        {"caucasianalbanian", Script_CaucasianAlbanian},
        {"bassavah", Script_BassaVah},
        {"duployan", Script_Duployan},
        {"elbasan", Script_Elbasan},
        {"grantha", Script_Grantha},
        {"pahawhhmong", Script_PahawhHmong},
        {"khojki", Script_Khojki},
        {"lineara", Script_LinearA},
        {"mahajani", Script_Mahajani},
        {"manichaean", Script_Manichaean},
        {"mendekikakui", Script_MendeKikakui},
        {"modi", Script_Modi},
        {"mro", Script_Mro},
        {"oldnortharabian", Script_OldNorthArabian},
        {"nabataean", Script_Nabataean},
        {"palmyrene", Script_Palmyrene},
        {"paucinhau", Script_PauCinHau},
        {"oldpermic", Script_OldPermic},
        {"psalterpahlavi", Script_PsalterPahlavi},
        {"siddham", Script_Siddham},
        {"khudawadi", Script_Khudawadi},
        {"tirhuta", Script_Tirhuta},
        {"warangciti", Script_WarangCiti},
        {"ahom", Script_Ahom},
        {"anatolianhieroglyphs", Script_AnatolianHieroglyphs},
        {"hatran", Script_Hatran},
        {"multani", Script_Multani},
        {"oldhungarian", Script_OldHungarian},
        {"signwriting", Script_SignWriting},
        {"adlam", Script_Adlam},
        {"bhaiksuki", Script_Bhaiksuki},
        {"marchen", Script_Marchen},
        {"newa", Script_Newa},
        {"osage", Script_Osage},
        {"tangut", Script_Tangut},
        {"masaramgondi", Script_MasaramGondi},
        {"nushu", Script_Nushu},
        {"soyombo", Script_Soyombo},
        {"zanabazarsquare", Script_ZanabazarSquare},
        {"dogra", Script_Dogra},
        {"gunjalagondi", Script_GunjalaGondi},
        {"hanifirohingya", Script_HanifiRohingya},
        {"makasar", Script_Makasar},
        {"medefaidrin", Script_Medefaidrin},
        {"oldsogdian", Script_OldSogdian},
        {"sogdian", Script_Sogdian},
        {"elymaic", Script_Elymaic},
        {"nandinagari", Script_Nandinagari},
        {"nyiakengpuachuehmong", Script_NyiakengPuachueHmong},
        {"wancho", Script_Wancho},
        {"chorasmian", Script_Chorasmian},
        {"divesakuru", Script_DivesAkuru},
        {"khitansmallscript", Script_KhitanSmallScript},
        {"yezidi", Script_Yezidi}
    };
    const char *nm = u_getPropertyValueName(UCHAR_SCRIPT, UProperty(icu), U_LONG_PROPERTY_NAME);
    std::string key(nm ? nm : "");
    std::string norm;
    for (char ch : key) if (ch != '_') norm += char(ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch);
    auto it = m.find(norm);
    return it != m.end() ? it->second : PkChar::Script_Unknown;
}

// ── decomposition()：raw（UnicodeData 分解字段，含兼容分解），对齐 QChar 语义 ──
PkString PkChar::decomposition(uint ucs4) noexcept
{
    UErrorCode ec = U_ZERO_ERROR;
    const UNormalizer2 *nfkd = unorm2_getNFKDInstance(&ec);
    if (U_FAILURE(ec) || !nfkd) return PkString();
    UChar buf[64];
    const int32_t len = unorm2_getRawDecomposition(nfkd, UChar32(ucs4), buf, 64, &ec);
    if (U_FAILURE(ec) || len <= 0) return PkString();
    return PkString::fromUtf16(reinterpret_cast<const char16_t*>(buf), len);
}

// ---- 内联分支的 _helper 出口（声明于 PkChar.h，实现在此；S-09-g 链接补齐）----
// isSpace 的 >127 段：对齐 QChar::isSpace_helper 的 Unicode White_Space 集合
// （0xa0/0x85 已被内联分支处理，这里只剩剩余项）。
bool PkChar::isSpace_helper(uint ucs4) noexcept
{
    return ucs4 == 0x1680
        || (ucs4 >= 0x2000 && ucs4 <= 0x200a)
        || ucs4 == 0x2028 || ucs4 == 0x2029 || ucs4 == 0x202f
        || ucs4 == 0x205f || ucs4 == 0x3000;
}

// 字母/数字类：走 category()（ICU 表驱动），不复制第二份 Unicode 数据。
bool PkChar::isLetter_helper(uint ucs4) noexcept
{
    switch (category(ucs4)) {
    case PkChar::Letter_Uppercase:
    case PkChar::Letter_Lowercase:
    case PkChar::Letter_Titlecase:
    case PkChar::Letter_Modifier:
    case PkChar::Letter_Other:
        return true;
    default:
        return false;
    }
}

bool PkChar::isNumber_helper(uint ucs4) noexcept
{
    switch (category(ucs4)) {
    case PkChar::Number_DecimalDigit:
    case PkChar::Number_Letter:
    case PkChar::Number_Other:
        return true;
    default:
        return false;
    }
}

bool PkChar::isLetterOrNumber_helper(uint ucs4) noexcept
{
    return isLetter_helper(ucs4) || isNumber_helper(ucs4);
}

bool operator==(PkChar c1, PkChar c2) noexcept { return c1.ucs == c2.ucs; }
bool operator<(PkChar c1, PkChar c2) noexcept { return c1.ucs < c2.ucs; }
