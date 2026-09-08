/*
 *  SPDX-FileCopyrightText: 2024 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KoWritingSystemUtils.h"
#include "KoLcLocale.h"
#include <PkChar.h>
#include <PkMap.h>
#include <PkString.h>

static bool isAsciiAlpha(const PkString &text)
{
    if (text.isEmpty()) return false;
    for (char16_t ch : text.PkToU16()) {
        if (!((ch >= u'A' && ch <= u'Z') || (ch >= u'a' && ch <= u'z'))) return false;
    }
    return true;
}

static bool isAsciiDigit(const PkString &text)
{
    if (text.isEmpty()) return false;
    for (char16_t ch : text.PkToU16()) {
        if (ch < u'0' || ch > u'9') return false;
    }
    return true;
}

static bool isAsciiAlphaNumeric(const PkString &text)
{
    if (text.isEmpty()) return false;
    for (char16_t ch : text.PkToU16()) {
        const bool alpha = (ch >= u'A' && ch <= u'Z') || (ch >= u'a' && ch <= u'z');
        if (!alpha && !(ch >= u'0' && ch <= u'9')) return false;
    }
    return true;
}

static PkMap<PkChar::Script, PkString> SCRIPT_TAG_MAP {
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

PkString KoWritingSystemUtils::scriptTagForQCharScript(PkChar::Script script)
{
    return SCRIPT_TAG_MAP.value(script);
}

PkChar::Script KoWritingSystemUtils::qCharScriptForScriptTag(const PkString &tag)
{
    return SCRIPT_TAG_MAP.key(tag, PkChar::Script_Unknown);
}

PkString KoWritingSystemUtils::scriptTagForLanguage(const PkString &locale)
{
    const Bcp47Locale bcp = parseBcp47Locale(locale);
    if (!bcp.scriptTag.isEmpty()) return bcp.scriptTag;
    if (bcp.languageTags.isEmpty()) return "Zyyy";
    const PkString script = KoLc::defaultScriptTag(bcp.languageTags.first(), bcp.regionTag);
    return script.isEmpty() ? PkString("Zyyy") : script;
}

PkMap<PkString, PkString> KoWritingSystemUtils::samples()
{
    PkMap <PkString, PkString> samples;
    samples.insert("AaBbGg", "s_Latn");
    samples.insert("\u263A\u2764\u2693\U0001F308", "s_Zsye"); // Emoji
    samples.insert("∆∅∞≠", "s_Zmth"); // Some math operators
    samples.insert("𝄞𝅘𝅥𝅮𝄿𝄻", "s_Zsym"); // Musical notes
    samples.insert("←↕↝↴", "s_Zsym"); // Arrows
    // Exact Qt 5.15 QFontDatabase::writingSystemSample() values. These are
    // data, not a platform-font lookup: retaining the full set preserves the
    // original font coverage classification without linking QtGui.
    samples.insert("AaÃáZz", "s_Latn");
    samples.insert("ΓαΩω", "s_Grek");
    samples.insert("Дджя", "s_Cyrl");
    samples.insert("ԿՏկտ", "s_Armn");
    samples.insert("אבגד", "s_Hebr");
    samples.insert("أبجدية عربية", "s_Arab");
    samples.insert("ܕܥܖܦ", "s_Syrc");
    samples.insert("ބޔތލ", "s_Thaa");
    samples.insert("अकथव", "s_Deva");
    samples.insert("আখদশ", "s_Beng");
    samples.insert("ਅਕਥਵ", "s_Guru");
    samples.insert("અકથવ", "s_Gujr");
    samples.insert("ଆଖଫଶ", "s_Orya");
    samples.insert("உஙனஹ", "s_Taml");
    samples.insert("అకథవ", "s_Telu");
    samples.insert("ಅಕಥವ", "s_Knda");
    samples.insert("അകഥവ", "s_Mylm");
    samples.insert("ඐචධව", "s_Sinh");
    samples.insert("ขฒยา", "s_Thai");
    samples.insert("ຍຝອຽ", "s_Laoo");
    samples.insert("ༀ༁༂༃", "s_Tibt");
    samples.insert("ကခဂဃ", "s_Mymr");
    samples.insert("ႠႰჀა", "s_Geor");
    samples.insert("កថឰៀ", "s_Khmr");
    samples.insert("中文范例", "s_Hans");
    samples.insert("中文範例", "s_Hant");
    samples.insert("サンプルです", "s_Jpan");
    samples.insert("가갑갚갯", "s_Kore");
    samples.insert("ỗộốồ", "l_vi");
    samples.insert("AaBbZz", "s_Zyyy");
    samples.insert("ᚁᚂᚃᚄ", "s_Ogam");
    samples.insert("ᚠᚡᚢᚣ", "s_Runr");
    samples.insert("ߊߋߌߍ", "s_Nkoo");
    return samples;
}

PkString KoWritingSystemUtils::sampleTagForLocale(const PkString &locale)
{
    const Bcp47Locale bcp = parseBcp47Locale(locale);
    if (!bcp.languageTags.isEmpty() && bcp.languageTags.first() == "vi") {
        return "l_vi";
    }
    return "s_" + scriptTagForLanguage(locale);
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

    // Language -- single primary language, followed by optional 3 letter extended tags.
    if (tags.first().size() == 2 || tags.first().size() == 3) {
        bcp.languageTags.append(tags.takeFirst().toLower());

        // extensions only happen when first tag is 2 or 3 long.
        while (!tags.isEmpty() && tags.first().size() == 3 && isAsciiAlpha(tags.first())) {
            bcp.languageTags.append(tags.takeFirst().toLower());
        }
    } else if (tags.first().size() >= 4 && tags.first().size() <= 8 && isAsciiAlpha(tags.first())) {
        // 4 alpha is reserved for future use and 5-8 is also legit, but practically doesn't exist...
        bcp.languageTags.append(tags.takeFirst().toLower());
    } else if (tags.first() == "i" && tags.size() > 0) {
        PkString total = tags.takeFirst();
        total += "-"+tags.takeFirst();
        bcp.languageTags.append(total.toLower());
    }

    if (tags.isEmpty()) return bcp;

    // Script -- This is an 4 letter alpha only.

    if (isAsciiAlpha(tags.first()) && tags.first().size() == 4) {
        bcp.scriptTag = tags.takeFirst().toLower();
        bcp.scriptTag = bcp.scriptTag.mid(0, 1).toUpper()+bcp.scriptTag.mid(1);
    }

    if (tags.isEmpty()) return bcp;

    // Region -- 2 letter alpha only.

    if ((isAsciiAlpha(tags.first()) && tags.first().size() == 2)
            || (isAsciiDigit(tags.first()) && tags.first().size() == 3)) {
        bcp.regionTag = tags.takeFirst().toUpper();
    }

    if (tags.isEmpty()) return bcp;

    // Variants -- [0+] alpha numerics, either between 5-8 char long, or 4 but starting with a digit.

    while (!tags.isEmpty()
           && ( (tags.first().size() >= 5 && tags.first().size() <= 8)
               || (tags.first().size() == 4
                   && tags.first().at(0) >= u'0' && tags.first().at(0) <= u'9'
                   && isAsciiAlphaNumeric(tags.first().mid(1))) )
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
