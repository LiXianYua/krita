/*
 *  SPDX-FileCopyrightText: 2024 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KoWritingSystemUtils.h"
#include <QRegularExpression>

static PkMap<QFontDatabase::WritingSystem, PkString> WRITINGSYSTEM_SCRIPT_MAP {
    {{QFontDatabase::Any},{"Zyyy"}},
    {{QFontDatabase::Latin},{"Latn"}},
    {{QFontDatabase::Greek},{"Grek"}},
    {{QFontDatabase::Cyrillic},{"Cyrl"}},
    {{QFontDatabase::Armenian},{"Armn"}},
    {{QFontDatabase::Hebrew},{"Hebr"}},
    {{QFontDatabase::Arabic},{"Arab"}},
    {{QFontDatabase::Syriac},{"Syrc"}},
    {{QFontDatabase::Thaana},{"Thaa"}},
    {{QFontDatabase::Devanagari},{"Deva"}},
    {{QFontDatabase::Bengali},{"Beng"}},
    {{QFontDatabase::Gurmukhi},{"Guru"}},
    {{QFontDatabase::Gujarati},{"Gujr"}},
    {{QFontDatabase::Oriya},{"Orya"}},
    {{QFontDatabase::Tamil},{"Taml"}},
    {{QFontDatabase::Telugu},{"Telu"}},
    {{QFontDatabase::Kannada},{"Knda"}},
    {{QFontDatabase::Malayalam},{"Mylm"}},
    {{QFontDatabase::Sinhala},{"Sinh"}},
    {{QFontDatabase::Thai},{"Thai"}},
    {{QFontDatabase::Lao},{"Laoo"}},
    {{QFontDatabase::Tibetan},{"Tibt"}},
    {{QFontDatabase::Myanmar},{"Mymr"}},
    {{QFontDatabase::Georgian},{"Geor"}},
    {{QFontDatabase::Khmer},{"Khmr"}},
    {{QFontDatabase::SimplifiedChinese},{"Hans"}},
    {{QFontDatabase::TraditionalChinese},{"Hant"}},
    {{QFontDatabase::Japanese},{"Jpan"}},
    {{QFontDatabase::Korean},{"Kore"}},
    {{QFontDatabase::Ogham},{"Ogam"}},
    {{QFontDatabase::Runic},{"Runr"}},
    {{QFontDatabase::Nko},{"Nkoo"}},
    /*{{QFontDatabase::Symbol},{"Zsye"}}, Symbol refers to the wingdings fonts, not actually unicode scripts.*/
    {{QFontDatabase::Vietnamese},{"Latn"}},
};

static PkMap<QLocale::Script, PkString> QLOCALE_SCRIPT_MAP {
    {{QLocale::LatinScript},{"Latn"}},
    {{QLocale::GreekScript},{"Grek"}},
    {{QLocale::CyrillicScript},{"Cyrl"}},
    {{QLocale::ArmenianScript},{"Armn"}},
    {{QLocale::HebrewScript},{"Hebr"}},
    {{QLocale::ArabicScript},{"Arab"}},
    {{QLocale::SyriacScript},{"Syrc"}},
    {{QLocale::ThaanaScript},{"Thaa"}},
    {{QLocale::DevanagariScript},{"Deva"}},
    {{QLocale::BengaliScript},{"Beng"}},
    {{QLocale::GurmukhiScript},{"Guru"}},
    {{QLocale::GujaratiScript},{"Gujr"}},
    {{QLocale::OriyaScript},{"Orya"}},
    {{QLocale::TamilScript},{"Taml"}},
    {{QLocale::TeluguScript},{"Telu"}},
    {{QLocale::KannadaScript},{"Knda"}},
    {{QLocale::MalayalamScript},{"Mylm"}},
    {{QLocale::SinhalaScript},{"Sinh"}},
    {{QLocale::ThaiScript},{"Thai"}},
    {{QLocale::LaoScript},{"Laoo"}},
    {{QLocale::TibetanScript},{"Tibt"}},
    {{QLocale::MyanmarScript},{"Mymr"}},
    {{QLocale::GeorgianScript},{"Geor"}},
    {{QLocale::KhmerScript},{"Khmr"}},
    {{QLocale::SimplifiedChineseScript},{"Hans"}},
    {{QLocale::TraditionalChineseScript},{"Hant"}},
    {{QLocale::JapaneseScript},{"Jpan"}},
    {{QLocale::KoreanScript},{"Kore"}},
    {{QLocale::OghamScript},{"Ogam"}},
    {{QLocale::RunicScript},{"Runr"}},
    {{QLocale::NkoScript},{"Nkoo"}},

    {{QLocale::DeseretScript},{"Dsrt"}},
    {{QLocale::MongolianScript},{"Mong"}},
    {{QLocale::TifinaghScript},{"Tfng"}},
    {{QLocale::CherokeeScript},{"Cher"}},
    {{QLocale::EthiopicScript},{"Ethi"}},
    {{QLocale::YiScript},{"Yiii"}},
    {{QLocale::VaiScript},{"Vaii"}},
    {{QLocale::AvestanScript},{"Avst"}},
    {{QLocale::BalineseScript},{"Bali"}},
    {{QLocale::BamumScript},{"Bamu"}},
    {{QLocale::BopomofoScript},{"Bopo"}},
    {{QLocale::BrahmiScript},{"Brah"}},
    {{QLocale::BugineseScript},{"Bugi"}},
    {{QLocale::BuhidScript},{"Buhd"}},
    {{QLocale::CanadianAboriginalScript},{"Cans"}},
    {{QLocale::CarianScript},{"Cari"}},
    {{QLocale::ChakmaScript},{"Cakm"}},
    {{QLocale::ChamScript},{"Cham"}},
    {{QLocale::CopticScript},{"Copt"}},
    {{QLocale::CypriotScript},{"Cprt"}},
    {{QLocale::EgyptianHieroglyphsScript},{"Egyp"}},
    {{QLocale::FraserScript},{"Lisu"}},
    {{QLocale::GlagoliticScript},{"Glag"}},
    {{QLocale::GothicScript},{"Goth"}},
    {{QLocale::HanScript},{"Hani"}},
    {{QLocale::HangulScript},{"Hang"}},
    {{QLocale::HanunooScript},{"Hano"}},
    {{QLocale::ImperialAramaicScript},{"Armi"}},
    {{QLocale::InscriptionalPahlaviScript},{"Phli"}},
    {{QLocale::InscriptionalParthianScript},{"Prti"}},
    {{QLocale::JavaneseScript},{"Java"}},
    {{QLocale::KaithiScript},{"Kthi"}},
    {{QLocale::KatakanaScript},{"Kana"}},
    {{QLocale::KayahLiScript},{"Kali"}},
    {{QLocale::KharoshthiScript},{"Khar"}},
    {{QLocale::LannaScript},{"Lana"}},
    {{QLocale::LepchaScript},{"Lepc"}},
    {{QLocale::LimbuScript},{"Limb"}},
    {{QLocale::LinearBScript},{"Linb"}},
    {{QLocale::LycianScript},{"Lyci"}},
    {{QLocale::LydianScript},{"Lydi"}},
    {{QLocale::MandaeanScript},{"Mand"}},
    {{QLocale::MeiteiMayekScript},{"Mtei"}},
    {{QLocale::MeroiticScript},{"Mero"}},
    {{QLocale::MeroiticCursiveScript},{"Merc"}},
    {{QLocale::NewTaiLueScript},{"Talu"}},
    {{QLocale::OlChikiScript},{"Olck"}},
    {{QLocale::OldItalicScript},{"Ital"}},
    {{QLocale::OldPersianScript},{"Xpeo"}},
    {{QLocale::OldSouthArabianScript},{"Sarb"}},
    {{QLocale::OrkhonScript},{"Orkh"}},
    {{QLocale::OsmanyaScript},{"Osma"}},
    {{QLocale::PhagsPaScript},{"Phag"}},
    {{QLocale::PhoenicianScript},{"Phnx"}},
    {{QLocale::PollardPhoneticScript},{"Plrd"}},
    {{QLocale::RejangScript},{"Rjng"}},
    {{QLocale::SamaritanScript},{"Samr"}},
    {{QLocale::SaurashtraScript},{"Saur"}},
    {{QLocale::SharadaScript},{"Shrd"}},
    {{QLocale::ShavianScript},{"Shaw"}},
    {{QLocale::SoraSompengScript},{"Sora"}},
    {{QLocale::CuneiformScript},{"Xsux"}},
    {{QLocale::SundaneseScript},{"Sund"}},
    {{QLocale::SylotiNagriScript},{"Sylo"}},
    {{QLocale::TagalogScript},{"Tglg"}},
    {{QLocale::TagbanwaScript},{"Tagb"}},
    {{QLocale::TaiLeScript},{"Tale"}},
    {{QLocale::TaiVietScript},{"Tavt"}},
    {{QLocale::TakriScript},{"Takr"}},
    {{QLocale::UgariticScript},{"Ugar"}},
    {{QLocale::BrailleScript},{"Brai"}},
    {{QLocale::HiraganaScript},{"Hira"}},
    {{QLocale::CaucasianAlbanianScript},{"Aghb"}},
    {{QLocale::BassaVahScript},{"Bass"}},
    {{QLocale::DuployanScript},{"Dupl"}},
    {{QLocale::ElbasanScript},{"Elba"}},
    {{QLocale::GranthaScript},{"Gran"}},
    {{QLocale::PahawhHmongScript},{"Hmng"}},
    {{QLocale::KhojkiScript},{"Khoi"}},
    {{QLocale::LinearAScript},{"Lina"}},
    {{QLocale::MahajaniScript},{"Mahj"}},
    {{QLocale::ManichaeanScript},{"Mani"}},
    {{QLocale::MendeKikakuiScript},{"Mend"}},
    {{QLocale::ModiScript},{"Modi"}},
    {{QLocale::MroScript},{"Mroo"}},
    {{QLocale::OldNorthArabianScript},{"Narb"}},
    {{QLocale::NabataeanScript},{"Nbat"}},
    {{QLocale::PalmyreneScript},{"Palm"}},
    {{QLocale::PauCinHauScript},{"Pauc"}},
    {{QLocale::PsalterPahlaviScript},{"Phlp"}},
    {{QLocale::KhudawadiScript},{"Sind"}},
    {{QLocale::TirhutaScript},{"Tirh"}},
    {{QLocale::VarangKshitiScript},{"Wara"}},
    {{QLocale::AhomScript},{"Ahom"}},
    {{QLocale::AnatolianHieroglyphsScript},{"Hluw"}},
    {{QLocale::HatranScript},{"Hatr"}},
    {{QLocale::MultaniScript},{"Mult"}},
    {{QLocale::OldHungarianScript},{"Hung"}},
    {{QLocale::SignWritingScript},{"Sgnw"}},
    {{QLocale::AdlamScript},{"Adlm"}},
    {{QLocale::BhaiksukiScript},{"Bhks"}},
    {{QLocale::BatakScript},{"Batk"}},
    {{QLocale::MarchenScript},{"Marc"}},
    {{QLocale::NewaScript},{"Newa"}},
    {{QLocale::OsageScript},{"Osge"}},
    {{QLocale::TangutScript},{"Tang"}},
    {{QLocale::HanWithBopomofoScript},{"Hanb"}},
    {{QLocale::JamoScript},{"Jamo"}},
};

static PkMap<char16_t::Script, PkString> QCHAR_SCRIPT_MAP {
    {{char16_t::Script_Latin},{"Latn"}},
    {{char16_t::Script_Greek},{"Grek"}},
    {{char16_t::Script_Cyrillic},{"Cyrl"}},
    {{char16_t::Script_Armenian},{"Armn"}},
    {{char16_t::Script_Hebrew},{"Hebr"}},
    {{char16_t::Script_Arabic},{"Arab"}},
    {{char16_t::Script_Syriac},{"Syrc"}},
    {{char16_t::Script_Thaana},{"Thaa"}},
    {{char16_t::Script_Devanagari},{"Deva"}},
    {{char16_t::Script_Bengali},{"Beng"}},
    {{char16_t::Script_Gurmukhi},{"Guru"}},
    {{char16_t::Script_Gujarati},{"Gujr"}},
    {{char16_t::Script_Oriya},{"Orya"}},
    {{char16_t::Script_Tamil},{"Taml"}},
    {{char16_t::Script_Telugu},{"Telu"}},
    {{char16_t::Script_Kannada},{"Knda"}},
    {{char16_t::Script_Malayalam},{"Mylm"}},
    {{char16_t::Script_Sinhala},{"Sinh"}},
    {{char16_t::Script_Thai},{"Thai"}},
    {{char16_t::Script_Lao},{"Laoo"}},
    {{char16_t::Script_Tibetan},{"Tibt"}},
    {{char16_t::Script_Myanmar},{"Mymr"}},
    {{char16_t::Script_Georgian},{"Geor"}},
    {{char16_t::Script_Khmer},{"Khmr"}},
    {{char16_t::Script_Ogham},{"Ogam"}},
    {{char16_t::Script_Runic},{"Runr"}},
    {{char16_t::Script_Nko},{"Nkoo"}},

    {{char16_t::Script_Deseret},{"Dsrt"}},
    {{char16_t::Script_Mongolian},{"Mong"}},
    {{char16_t::Script_Tifinagh},{"Tfng"}},
    {{char16_t::Script_Cherokee},{"Cher"}},
    {{char16_t::Script_Ethiopic},{"Ethi"}},
    {{char16_t::Script_Yi},{"Yiii"}},
    {{char16_t::Script_Vai},{"Vaii"}},
    {{char16_t::Script_Avestan},{"Avst"}},
    {{char16_t::Script_Balinese},{"Bali"}},
    {{char16_t::Script_Bamum},{"Bamu"}},
    {{char16_t::Script_Bopomofo},{"Bopo"}},
    {{char16_t::Script_Brahmi},{"Brah"}},
    {{char16_t::Script_Buginese},{"Bugi"}},
    {{char16_t::Script_Buhid},{"Buhd"}},
    {{char16_t::Script_CanadianAboriginal},{"Cans"}},
    {{char16_t::Script_Carian},{"Cari"}},
    {{char16_t::Script_Chakma},{"Cakm"}},
    {{char16_t::Script_Cham},{"Cham"}},
    {{char16_t::Script_Coptic},{"Copt"}},
    {{char16_t::Script_Cypriot},{"Cprt"}},
    {{char16_t::Script_EgyptianHieroglyphs},{"Egyp"}},
    {{char16_t::Script_Lisu},{"Lisu"}},
    {{char16_t::Script_Glagolitic},{"Glag"}},
    {{char16_t::Script_Gothic},{"Goth"}},
    {{char16_t::Script_Han},{"Hani"}},
    {{char16_t::Script_Hangul},{"Hang"}},
    {{char16_t::Script_Hanunoo},{"Hano"}},
    {{char16_t::Script_ImperialAramaic},{"Armi"}},
    {{char16_t::Script_InscriptionalPahlavi},{"Phli"}},
    {{char16_t::Script_InscriptionalParthian},{"Prti"}},
    {{char16_t::Script_Javanese},{"Java"}},
    {{char16_t::Script_Kaithi},{"Kthi"}},
    {{char16_t::Script_Katakana},{"Kana"}},
    {{char16_t::Script_KayahLi},{"Kali"}},
    {{char16_t::Script_Kharoshthi},{"Khar"}},
    {{char16_t::Script_TaiTham}, {"Lana"}},
    {{char16_t::Script_Lepcha},{"Lepc"}},
    {{char16_t::Script_Limbu},{"Limb"}},
    {{char16_t::Script_LinearB},{"Linb"}},
    {{char16_t::Script_Lycian},{"Lyci"}},
    {{char16_t::Script_Lydian},{"Lydi"}},
    {{char16_t::Script_Mandaic},{"Mand"}},
    {{char16_t::Script_MeeteiMayek},{"Mtei"}},
    {{char16_t::Script_MeroiticHieroglyphs},{"Mero"}},
    {{char16_t::Script_MeroiticCursive},{"Merc"}},
    {{char16_t::Script_NewTaiLue},{"Talu"}},
    {{char16_t::Script_OlChiki},{"Olck"}},
    {{char16_t::Script_OldItalic},{"Ital"}},
    {{char16_t::Script_OldPersian},{"Xpeo"}},
    {{char16_t::Script_OldSouthArabian},{"Sarb"}},
    {{char16_t::Script_OldTurkic},{"Orkh"}},
    {{char16_t::Script_Osmanya},{"Osma"}},
    {{char16_t::Script_PhagsPa},{"Phag"}},
    {{char16_t::Script_Phoenician},{"Phnx"}},
    {{char16_t::Script_Miao},{"Plrd"}},
    {{char16_t::Script_Rejang},{"Rjng"}},
    {{char16_t::Script_Samaritan},{"Samr"}},
    {{char16_t::Script_Saurashtra},{"Saur"}},
    {{char16_t::Script_Sharada},{"Shrd"}},
    {{char16_t::Script_Shavian},{"Shaw"}},
    {{char16_t::Script_SoraSompeng},{"Sora"}},
    {{char16_t::Script_Cuneiform},{"Xsux"}},
    {{char16_t::Script_Sundanese},{"Sund"}},
    {{char16_t::Script_SylotiNagri},{"Sylo"}},
    {{char16_t::Script_Tagalog},{"Tglg"}},
    {{char16_t::Script_Tagbanwa},{"Tagb"}},
    {{char16_t::Script_TaiLe},{"Tale"}},
    {{char16_t::Script_TaiViet},{"Tavt"}},
    {{char16_t::Script_Takri},{"Takr"}},
    {{char16_t::Script_Ugaritic},{"Ugar"}},
    {{char16_t::Script_Braille},{"Brai"}},
    {{char16_t::Script_Hiragana},{"Hira"}},
    {{char16_t::Script_CaucasianAlbanian},{"Aghb"}},
    {{char16_t::Script_BassaVah},{"Bass"}},
    {{char16_t::Script_Duployan},{"Dupl"}},
    {{char16_t::Script_Elbasan},{"Elba"}},
    {{char16_t::Script_Grantha},{"Gran"}},
    {{char16_t::Script_PahawhHmong},{"Hmng"}},
    {{char16_t::Script_Khojki},{"Khoi"}},
    {{char16_t::Script_LinearA},{"Lina"}},
    {{char16_t::Script_Mahajani},{"Mahj"}},
    {{char16_t::Script_Manichaean},{"Mani"}},
    {{char16_t::Script_MendeKikakui},{"Mend"}},
    {{char16_t::Script_Modi},{"Modi"}},
    {{char16_t::Script_Mro},{"Mroo"}},
    {{char16_t::Script_OldNorthArabian},{"Narb"}},
    {{char16_t::Script_Nabataean},{"Nbat"}},
    {{char16_t::Script_Palmyrene},{"Palm"}},
    {{char16_t::Script_PauCinHau},{"Pauc"}},
    {{char16_t::Script_PsalterPahlavi},{"Phlp"}},
    {{char16_t::Script_Khudawadi},{"Sind"}},
    {{char16_t::Script_Tirhuta},{"Tirh"}},
    {{char16_t::Script_WarangCiti},{"Wara"}},
    {{char16_t::Script_Ahom},{"Ahom"}},
    {{char16_t::Script_AnatolianHieroglyphs},{"Hluw"}},
    {{char16_t::Script_Hatran},{"Hatr"}},
    {{char16_t::Script_Multani},{"Mult"}},
    {{char16_t::Script_OldHungarian},{"Hung"}},
    {{char16_t::Script_SignWriting},{"Sgnw"}},
    {{char16_t::Script_Adlam},{"Adlm"}},
    {{char16_t::Script_Bhaiksuki},{"Bhks"}},
    {{char16_t::Script_Batak},{"Batk"}},
    {{char16_t::Script_Marchen},{"Marc"}},
    {{char16_t::Script_Newa},{"Newa"}},
    {{char16_t::Script_Osage},{"Osge"}},
    {{char16_t::Script_Tangut},{"Tang"}},

    {{char16_t::Script_MasaramGondi},{"Gonm"}},
    {{char16_t::Script_Nushu},{"Nshu"}},
    {{char16_t::Script_Soyombo},{"Soyo"}},
    {{char16_t::Script_ZanabazarSquare},{"Zanb"}},

    {{char16_t::Script_Dogra},{"Dogr"}},
    {{char16_t::Script_GunjalaGondi},{"Gong"}},
    {{char16_t::Script_HanifiRohingya},{"Rogh"}},
    {{char16_t::Script_Makasar},{"Maka"}},
    {{char16_t::Script_Medefaidrin},{"Medf"}},
    {{char16_t::Script_OldSogdian},{"Sogo"}},
    {{char16_t::Script_Sogdian},{"Sogd"}},
    {{char16_t::Script_Elymaic},{"Elym"}},
    {{char16_t::Script_Nandinagari},{"Nand"}},
    {{char16_t::Script_NyiakengPuachueHmong},{"Hmnp"}},
    {{char16_t::Script_Wancho},{"Wcho"}},

    {{char16_t::Script_Chorasmian},{"Chrs"}},
    {{char16_t::Script_DivesAkuru},{"Diak"}},
    {{char16_t::Script_KhitanSmallScript},{"Kits"}},
    {{char16_t::Script_Yezidi},{"Yezi"}},
};

PkString KoWritingSystemUtils::scriptTagForWritingSystem(QFontDatabase::WritingSystem system) {
    return WRITINGSYSTEM_SCRIPT_MAP.value(system);
}

QFontDatabase::WritingSystem KoWritingSystemUtils::writingSystemForScriptTag(const PkString &tag)
{
    return WRITINGSYSTEM_SCRIPT_MAP.key(tag, QFontDatabase::Any);
}

PkString KoWritingSystemUtils::scriptTagForQLocaleScript(QLocale::Script script)
{
    return QLOCALE_SCRIPT_MAP.value(script);
}

QLocale::Script KoWritingSystemUtils::scriptForScriptTag(const PkString &tag)
{
    return QLOCALE_SCRIPT_MAP.key(tag, QLocale::AnyScript);
}

PkString KoWritingSystemUtils::scriptTagForQCharScript(char16_t::Script script)
{
    return QCHAR_SCRIPT_MAP.value(script);
}

char16_t::Script KoWritingSystemUtils::qCharScriptForScriptTag(const PkString &tag)
{
    return QCHAR_SCRIPT_MAP.key(tag, char16_t::Script_Unknown);
}

#include <QDebug>
// [migrate] missing include for Pk/Qt type
#include <PkMap.h>
// [migrate] missing include for Pk/Qt type
#include <PkString.h>
PkMap<PkString, PkString> KoWritingSystemUtils::samples()
{
    PkMap <PkString, PkString> samples;
    // Also add simplified latin sample. By doing this first, it'll fall back nicely.
    samples.insert("AaBbGg", "s_Latn");

    // Some symbol samples...
    samples.insert("\u263A\u2764\u2693\U0001F308", "s_Zsye"); // Emoji
    samples.insert("∆∅∞≠", "s_Zmth"); // Some math operators
    samples.insert("𝄞𝅘𝅥𝅮𝄿𝄻", "s_Zsym"); // Musical notes
    samples.insert("←↕↝↴", "s_Zsym"); // Arrows
    for (int i = 0; i < QFontDatabase::WritingSystemsCount; i++) {
        QFontDatabase::WritingSystem w = QFontDatabase::WritingSystem(i);
        if (w == QFontDatabase::WritingSystem::Any) continue;

        if (w == QFontDatabase::WritingSystem::Vietnamese) {
            samples.insert(QFontDatabase::writingSystemSample(QFontDatabase::Vietnamese),
                           "l_vi");
        } else {
            samples.insert(QFontDatabase::writingSystemSample(w),
                           "s_"+WRITINGSYSTEM_SCRIPT_MAP.value(w, "Zyyy"));
        }
    }

    return samples;
}

PkString KoWritingSystemUtils::sampleTagForQLocale(const QLocale &locale)
{
    const QLocale vietnamese(QLocale::Vietnamese, QLocale::LatinScript, QLocale::AnyCountry);

    if (locale == vietnamese) {
        return "l_vi";
    }

    return "s_"+QLOCALE_SCRIPT_MAP.value(locale.script(), "Zyyy");
}

// There's a number of tags that are kept around for compatibility.
const PkMap<PkString, PkString> grandFathered = {
    {"art-lojban", "jbo"},
    {"cel-gaulish", "xcg"}, // could also be xga or xtg
    {"en-GB-oed", "en-GB-oxendict"},
    {"i-ami", "ami"},
    {"i-default", ""},
    {"i-enochian", "i-enochian"},
    {"i-hak", "hak"},
    {"i-lux", "lb"},
    {"i-mingo", "i-mingo"},
    {"i-navajo", "nv"},
    {"i-pwn", "pwn"},
    {"i-tao", "tao"},
    {"i-tay", "tay"},
    {"i-tsu", "tsu"},
    {"no-bok", "nb"},
    {"no-nyn", "nn"},
    {"sgn-BE-FR", "sfb"},
    {"sgn-BE-NL", "vgt"},
    {"sgn-CH-DE", "sgg"},
    {"zh-guoyu", "cmn"},
    {"zh-hakka", "hak"},
    {"zh-min", "cdo"}, // This one could also be cpx, czo, mnp, or nan
    {"zh-min-nan", "nan"},
    {"zh-xiang", "hsn"},
};

KoWritingSystemUtils::Bcp47Locale KoWritingSystemUtils::parseBcp47Locale(const PkString &locale)
{
    Bcp47Locale bcp;

    PkStringList tags = grandFathered.value(locale, locale).split("-");

    if (tags.isEmpty()) return bcp;

    const QRegularExpression alphas("^[A-Za-z]+$");
    const QRegularExpression digits("^\\d+$");

    // Language -- single primary language, followed by optional 3 letter extended tags.
    if (tags.first().size() == 2 || tags.first().size() == 3) {
        bcp.languageTags.append(tags.takeFirst().toLower());

        // extensions only happen when first tag is 2 or 3 long.
        while (!tags.isEmpty() && tags.first().size() == 3 && tags.first().contains(alphas)) {
            bcp.languageTags.append(tags.takeFirst().toLower());
        }
    } else if (tags.first().size() >= 4 || tags.first().size() <= 8) {
        // 4 alpha is reserved for future use and 5-8 is also legit, but practically doesn't exist...
        bcp.languageTags.append(tags.takeFirst().toLower());
    } else if (tags.first() == "i" && tags.size() > 0) {
        PkString total = tags.takeFirst();
        total += "-"+tags.takeFirst();
        bcp.languageTags.append(total.toLower());
    }

    if (tags.isEmpty()) return bcp;

    // Script -- This is an 4 letter alpha only.

    if (tags.first().contains(alphas) && tags.first().size() == 4) {
        bcp.scriptTag = tags.takeFirst().toLower();
        bcp.scriptTag = bcp.scriptTag.at(0).toUpper()+bcp.scriptTag.mid(1);
    }

    if (tags.isEmpty()) return bcp;

    // Region -- 2 letter alpha only.

    if ((tags.first().contains(alphas) && tags.first().size() == 2)
            || (tags.first().contains(digits) && tags.first().size() == 3)) {
        bcp.regionTag = tags.takeFirst().toUpper();
    }

    if (tags.isEmpty()) return bcp;

    // Variants -- [0+] alpha numerics, either between 5-8 char long, or 4 but starting with a digit.

    const QRegularExpression variantAlphaNumeric("^\\d[A-Za-z0-9]{3}$");

    while (!tags.isEmpty()
           && ( (tags.first().size() >= 5 && tags.first().size() <= 8)
               || (tags.first().contains(variantAlphaNumeric) && tags.first().size() == 4) )
           ) {
        bcp.variantTags.append(tags.takeFirst().toLower());
    }

    if (tags.isEmpty()) return bcp;
    // extension and private use subtags. Each starts with a single letter to indicate the extension type.

    PkStringList currentExtension;
    while (!tags.isEmpty()) {
        if (!currentExtension.isEmpty() && tags.first().size() == 1) {
            if (currentExtension.first() == "x") {
                bcp.privateUseTags.append(currentExtension.join("-"));
            } else {
                bcp.extensionTags.append(currentExtension.join("-"));
            }
            currentExtension.clear();
        }
        currentExtension.append(tags.takeFirst().toLower());
    }

    if (!currentExtension.isEmpty()) {
        if (currentExtension.first() == "x") {
            bcp.privateUseTags.append(currentExtension.join("-"));
        } else {
            bcp.extensionTags.append(currentExtension.join("-"));
        }
    }

    return bcp;
}

QLocale KoWritingSystemUtils::localeFromBcp47Locale(const Bcp47Locale &locale)
{
    return QLocale(locale.toPosixLocaleFormat());
}

QLocale KoWritingSystemUtils::localeFromBcp47Locale(const PkString &locale)
{
    return localeFromBcp47Locale(parseBcp47Locale(locale));
}

bool KoWritingSystemUtils::Bcp47Locale::isValid() const
{
    return !languageTags.isEmpty() && !languageTags.first().isEmpty();
}

PkString KoWritingSystemUtils::Bcp47Locale::toPosixLocaleFormat() const
{
    // A Posix locale format is "language[_script][_territory][.codeset][@modifier]".

    if (!isValid()) return PkString();

    PkString posix;

    posix = languageTags.first();

    if (!scriptTag.isEmpty() && scriptTag.size() == 4) {
        posix.append("_");
        // Title case.
        posix.append(scriptTag);
    }

    if (!regionTag.isEmpty()) {
        posix.append("_");
        posix.append(regionTag.toUpper());
    }

    // Not writing @modifier for now...

    return posix;
}

PkString KoWritingSystemUtils::Bcp47Locale::toString() const
{
    PkStringList total;

    total.append(languageTags);
    if (!scriptTag.isEmpty()) {
        total.append(scriptTag);
    }
    if (!regionTag.isEmpty()) {
        total.append(regionTag);
    }
    total.append(variantTags);
    total.append(extensionTags);
    total.append(privateUseTags);

    return total.join("-");
}
