/*
 *  SPDX-FileCopyrightText: 2024 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KoWritingSystemUtils.h"
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
#include <PkChar.h>
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

static PkMap<PkChar::Script, PkString> QCHAR_SCRIPT_MAP {
    {{PkChar::Script_Latin},{"Latn"}},
    {{PkChar::Script_Greek},{"Grek"}},
    {{PkChar::Script_Cyrillic},{"Cyrl"}},
    {{PkChar::Script_Armenian},{"Armn"}},
    {{PkChar::Script_Hebrew},{"Hebr"}},
    {{PkChar::Script_Arabic},{"Arab"}},
    {{PkChar::Script_Syriac},{"Syrc"}},
    {{PkChar::Script_Thaana},{"Thaa"}},
    {{PkChar::Script_Devanagari},{"Deva"}},
    {{PkChar::Script_Bengali},{"Beng"}},
    {{PkChar::Script_Gurmukhi},{"Guru"}},
    {{PkChar::Script_Gujarati},{"Gujr"}},
    {{PkChar::Script_Oriya},{"Orya"}},
    {{PkChar::Script_Tamil},{"Taml"}},
    {{PkChar::Script_Telugu},{"Telu"}},
    {{PkChar::Script_Kannada},{"Knda"}},
    {{PkChar::Script_Malayalam},{"Mylm"}},
    {{PkChar::Script_Sinhala},{"Sinh"}},
    {{PkChar::Script_Thai},{"Thai"}},
    {{PkChar::Script_Lao},{"Laoo"}},
    {{PkChar::Script_Tibetan},{"Tibt"}},
    {{PkChar::Script_Myanmar},{"Mymr"}},
    {{PkChar::Script_Georgian},{"Geor"}},
    {{PkChar::Script_Khmer},{"Khmr"}},
    {{PkChar::Script_Ogham},{"Ogam"}},
    {{PkChar::Script_Runic},{"Runr"}},
    {{PkChar::Script_Nko},{"Nkoo"}},

    {{PkChar::Script_Deseret},{"Dsrt"}},
    {{PkChar::Script_Mongolian},{"Mong"}},
    {{PkChar::Script_Tifinagh},{"Tfng"}},
    {{PkChar::Script_Cherokee},{"Cher"}},
    {{PkChar::Script_Ethiopic},{"Ethi"}},
    {{PkChar::Script_Yi},{"Yiii"}},
    {{PkChar::Script_Vai},{"Vaii"}},
    {{PkChar::Script_Avestan},{"Avst"}},
    {{PkChar::Script_Balinese},{"Bali"}},
    {{PkChar::Script_Bamum},{"Bamu"}},
    {{PkChar::Script_Bopomofo},{"Bopo"}},
    {{PkChar::Script_Brahmi},{"Brah"}},
    {{PkChar::Script_Buginese},{"Bugi"}},
    {{PkChar::Script_Buhid},{"Buhd"}},
    {{PkChar::Script_CanadianAboriginal},{"Cans"}},
    {{PkChar::Script_Carian},{"Cari"}},
    {{PkChar::Script_Chakma},{"Cakm"}},
    {{PkChar::Script_Cham},{"Cham"}},
    {{PkChar::Script_Coptic},{"Copt"}},
    {{PkChar::Script_Cypriot},{"Cprt"}},
    {{PkChar::Script_EgyptianHieroglyphs},{"Egyp"}},
    {{PkChar::Script_Lisu},{"Lisu"}},
    {{PkChar::Script_Glagolitic},{"Glag"}},
    {{PkChar::Script_Gothic},{"Goth"}},
    {{PkChar::Script_Han},{"Hani"}},
    {{PkChar::Script_Hangul},{"Hang"}},
    {{PkChar::Script_Hanunoo},{"Hano"}},
    {{PkChar::Script_ImperialAramaic},{"Armi"}},
    {{PkChar::Script_InscriptionalPahlavi},{"Phli"}},
    {{PkChar::Script_InscriptionalParthian},{"Prti"}},
    {{PkChar::Script_Javanese},{"Java"}},
    {{PkChar::Script_Kaithi},{"Kthi"}},
    {{PkChar::Script_Katakana},{"Kana"}},
    {{PkChar::Script_KayahLi},{"Kali"}},
    {{PkChar::Script_Kharoshthi},{"Khar"}},
    {{PkChar::Script_TaiTham}, {"Lana"}},
    {{PkChar::Script_Lepcha},{"Lepc"}},
    {{PkChar::Script_Limbu},{"Limb"}},
    {{PkChar::Script_LinearB},{"Linb"}},
    {{PkChar::Script_Lycian},{"Lyci"}},
    {{PkChar::Script_Lydian},{"Lydi"}},
    {{PkChar::Script_Mandaic},{"Mand"}},
    {{PkChar::Script_MeeteiMayek},{"Mtei"}},
    {{PkChar::Script_MeroiticHieroglyphs},{"Mero"}},
    {{PkChar::Script_MeroiticCursive},{"Merc"}},
    {{PkChar::Script_NewTaiLue},{"Talu"}},
    {{PkChar::Script_OlChiki},{"Olck"}},
    {{PkChar::Script_OldItalic},{"Ital"}},
    {{PkChar::Script_OldPersian},{"Xpeo"}},
    {{PkChar::Script_OldSouthArabian},{"Sarb"}},
    {{PkChar::Script_OldTurkic},{"Orkh"}},
    {{PkChar::Script_Osmanya},{"Osma"}},
    {{PkChar::Script_PhagsPa},{"Phag"}},
    {{PkChar::Script_Phoenician},{"Phnx"}},
    {{PkChar::Script_Miao},{"Plrd"}},
    {{PkChar::Script_Rejang},{"Rjng"}},
    {{PkChar::Script_Samaritan},{"Samr"}},
    {{PkChar::Script_Saurashtra},{"Saur"}},
    {{PkChar::Script_Sharada},{"Shrd"}},
    {{PkChar::Script_Shavian},{"Shaw"}},
    {{PkChar::Script_SoraSompeng},{"Sora"}},
    {{PkChar::Script_Cuneiform},{"Xsux"}},
    {{PkChar::Script_Sundanese},{"Sund"}},
    {{PkChar::Script_SylotiNagri},{"Sylo"}},
    {{PkChar::Script_Tagalog},{"Tglg"}},
    {{PkChar::Script_Tagbanwa},{"Tagb"}},
    {{PkChar::Script_TaiLe},{"Tale"}},
    {{PkChar::Script_TaiViet},{"Tavt"}},
    {{PkChar::Script_Takri},{"Takr"}},
    {{PkChar::Script_Ugaritic},{"Ugar"}},
    {{PkChar::Script_Braille},{"Brai"}},
    {{PkChar::Script_Hiragana},{"Hira"}},
    {{PkChar::Script_CaucasianAlbanian},{"Aghb"}},
    {{PkChar::Script_BassaVah},{"Bass"}},
    {{PkChar::Script_Duployan},{"Dupl"}},
    {{PkChar::Script_Elbasan},{"Elba"}},
    {{PkChar::Script_Grantha},{"Gran"}},
    {{PkChar::Script_PahawhHmong},{"Hmng"}},
    {{PkChar::Script_Khojki},{"Khoi"}},
    {{PkChar::Script_LinearA},{"Lina"}},
    {{PkChar::Script_Mahajani},{"Mahj"}},
    {{PkChar::Script_Manichaean},{"Mani"}},
    {{PkChar::Script_MendeKikakui},{"Mend"}},
    {{PkChar::Script_Modi},{"Modi"}},
    {{PkChar::Script_Mro},{"Mroo"}},
    {{PkChar::Script_OldNorthArabian},{"Narb"}},
    {{PkChar::Script_Nabataean},{"Nbat"}},
    {{PkChar::Script_Palmyrene},{"Palm"}},
    {{PkChar::Script_PauCinHau},{"Pauc"}},
    {{PkChar::Script_PsalterPahlavi},{"Phlp"}},
    {{PkChar::Script_Khudawadi},{"Sind"}},
    {{PkChar::Script_Tirhuta},{"Tirh"}},
    {{PkChar::Script_WarangCiti},{"Wara"}},
    {{PkChar::Script_Ahom},{"Ahom"}},
    {{PkChar::Script_AnatolianHieroglyphs},{"Hluw"}},
    {{PkChar::Script_Hatran},{"Hatr"}},
    {{PkChar::Script_Multani},{"Mult"}},
    {{PkChar::Script_OldHungarian},{"Hung"}},
    {{PkChar::Script_SignWriting},{"Sgnw"}},
    {{PkChar::Script_Adlam},{"Adlm"}},
    {{PkChar::Script_Bhaiksuki},{"Bhks"}},
    {{PkChar::Script_Batak},{"Batk"}},
    {{PkChar::Script_Marchen},{"Marc"}},
    {{PkChar::Script_Newa},{"Newa"}},
    {{PkChar::Script_Osage},{"Osge"}},
    {{PkChar::Script_Tangut},{"Tang"}},

    {{PkChar::Script_MasaramGondi},{"Gonm"}},
    {{PkChar::Script_Nushu},{"Nshu"}},
    {{PkChar::Script_Soyombo},{"Soyo"}},
    {{PkChar::Script_ZanabazarSquare},{"Zanb"}},

    {{PkChar::Script_Dogra},{"Dogr"}},
    {{PkChar::Script_GunjalaGondi},{"Gong"}},
    {{PkChar::Script_HanifiRohingya},{"Rogh"}},
    {{PkChar::Script_Makasar},{"Maka"}},
    {{PkChar::Script_Medefaidrin},{"Medf"}},
    {{PkChar::Script_OldSogdian},{"Sogo"}},
    {{PkChar::Script_Sogdian},{"Sogd"}},
    {{PkChar::Script_Elymaic},{"Elym"}},
    {{PkChar::Script_Nandinagari},{"Nand"}},
    {{PkChar::Script_NyiakengPuachueHmong},{"Hmnp"}},
    {{PkChar::Script_Wancho},{"Wcho"}},

    {{PkChar::Script_Chorasmian},{"Chrs"}},
    {{PkChar::Script_DivesAkuru},{"Diak"}},
    {{PkChar::Script_KhitanSmallScript},{"Kits"}},
    {{PkChar::Script_Yezidi},{"Yezi"}},
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

PkString KoWritingSystemUtils::scriptTagForQCharScript(PkChar::Script script)
{
    return QCHAR_SCRIPT_MAP.value(script);
}

PkChar::Script KoWritingSystemUtils::qCharScriptForScriptTag(const PkString &tag)
{
    return QCHAR_SCRIPT_MAP.key(tag, PkChar::Script_Unknown);
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
