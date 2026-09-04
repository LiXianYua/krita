#ifndef PK_CHAR_H
#define PK_CHAR_H

// PkChar —— QChar 的零 Qt 替代（S-09-g 实测：libs/flake/text 六个文件 ~200 处调用）。
//
// 枚举（SpecialCharacter/Category/Script/Direction/Decomposition/JoiningType）
// **逐字照抄** ci-env Qt 5.15 的 qchar.h —— 调用点机械替换的前提是值完全一致。
//
// Unicode 属性（category/direction/decompositionTag/joiningType/script/大小写/
// 镜像/数字值）走 ICU C API（Qt5 的 qchar.cpp 在 ICU 构建下同样如此）：
//   - Category/Direction/Decomposition/JoiningType 的枚举值在生成期与
//     ICU UCharCategory/UCharDirection/UCharDecompositionType/UCharJoiningType
//     配对对齐（按 Unicode 双字母码），运行期直接 static_cast。
//   - Script 与 ICU UScriptCode 顺序不同（Qt 按 Unicode 顺序，ICU 按字母序），
//     运行期用 ICU 属性值长名（U_LONG_PROPERTY_NAME）→ Qt 枚举名的静态映射。
//
// decomposition() = unorm2_getRawDecomposition（对齐 UnicodeData 的分解字段语义）。

#include <cstdint>
#include "PkString.h"

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;

class PkChar {
public:
    enum SpecialCharacter {
        Null = 0x0000,
        Tabulation = 0x0009,
        LineFeed = 0x000a,
        FormFeed = 0x000c,
        CarriageReturn = 0x000d,
        Space = 0x0020,
        Nbsp = 0x00a0,
        SoftHyphen = 0x00ad,
        ReplacementCharacter = 0xfffd,
        ObjectReplacementCharacter = 0xfffc,
        ByteOrderMark = 0xfeff,
        ByteOrderSwapped = 0xfffe,
        ParagraphSeparator = 0x2029,
        LineSeparator = 0x2028,
        LastValidCodePoint = 0x10ffff
    };

    PkChar() noexcept : ucs(0) {}
    PkChar(ushort rc) noexcept : ucs(rc) {} // implicit
    PkChar(uchar c, uchar r) noexcept : ucs(ushort((r << 8) | c)) {}
    PkChar(short rc) noexcept : ucs(ushort(rc)) {} // implicit
    PkChar(uint rc) noexcept : ucs(ushort(rc & 0xffff)) {}
    PkChar(int rc) noexcept : ucs(ushort(rc & 0xffff)) {}
    PkChar(SpecialCharacter s) noexcept : ucs(ushort(s)) {} // implicit
    PkChar(char16_t ch) noexcept : ucs(ushort(ch)) {} // implicit

    explicit PkChar(char c) noexcept : ucs(uchar(c)) { }
    explicit PkChar(uchar c) noexcept : ucs(c) { }
    // Unicode information

    enum Category
    {
        Mark_NonSpacing,          //   Mn
        Mark_SpacingCombining,    //   Mc
        Mark_Enclosing,           //   Me

        Number_DecimalDigit,      //   Nd
        Number_Letter,            //   Nl
        Number_Other,             //   No

        Separator_Space,          //   Zs
        Separator_Line,           //   Zl
        Separator_Paragraph,      //   Zp

        Other_Control,            //   Cc
        Other_Format,             //   Cf
        Other_Surrogate,          //   Cs
        Other_PrivateUse,         //   Co
        Other_NotAssigned,        //   Cn

        Letter_Uppercase,         //   Lu
        Letter_Lowercase,         //   Ll
        Letter_Titlecase,         //   Lt
        Letter_Modifier,          //   Lm
        Letter_Other,             //   Lo

        Punctuation_Connector,    //   Pc
        Punctuation_Dash,         //   Pd
        Punctuation_Open,         //   Ps
        Punctuation_Close,        //   Pe
        Punctuation_InitialQuote, //   Pi
        Punctuation_FinalQuote,   //   Pf
        Punctuation_Other,        //   Po

        Symbol_Math,              //   Sm
        Symbol_Currency,          //   Sc
        Symbol_Modifier,          //   Sk
        Symbol_Other              //   So
    };

    enum Script
    {
        Script_Unknown,
        Script_Inherited,
        Script_Common,

        Script_Latin,
        Script_Greek,
        Script_Cyrillic,
        Script_Armenian,
        Script_Hebrew,
        Script_Arabic,
        Script_Syriac,
        Script_Thaana,
        Script_Devanagari,
        Script_Bengali,
        Script_Gurmukhi,
        Script_Gujarati,
        Script_Oriya,
        Script_Tamil,
        Script_Telugu,
        Script_Kannada,
        Script_Malayalam,
        Script_Sinhala,
        Script_Thai,
        Script_Lao,
        Script_Tibetan,
        Script_Myanmar,
        Script_Georgian,
        Script_Hangul,
        Script_Ethiopic,
        Script_Cherokee,
        Script_CanadianAboriginal,
        Script_Ogham,
        Script_Runic,
        Script_Khmer,
        Script_Mongolian,
        Script_Hiragana,
        Script_Katakana,
        Script_Bopomofo,
        Script_Han,
        Script_Yi,
        Script_OldItalic,
        Script_Gothic,
        Script_Deseret,
        Script_Tagalog,
        Script_Hanunoo,
        Script_Buhid,
        Script_Tagbanwa,
        Script_Coptic,

        // Unicode 4.0 additions
        Script_Limbu,
        Script_TaiLe,
        Script_LinearB,
        Script_Ugaritic,
        Script_Shavian,
        Script_Osmanya,
        Script_Cypriot,
        Script_Braille,

        // Unicode 4.1 additions
        Script_Buginese,
        Script_NewTaiLue,
        Script_Glagolitic,
        Script_Tifinagh,
        Script_SylotiNagri,
        Script_OldPersian,
        Script_Kharoshthi,

        // Unicode 5.0 additions
        Script_Balinese,
        Script_Cuneiform,
        Script_Phoenician,
        Script_PhagsPa,
        Script_Nko,

        // Unicode 5.1 additions
        Script_Sundanese,
        Script_Lepcha,
        Script_OlChiki,
        Script_Vai,
        Script_Saurashtra,
        Script_KayahLi,
        Script_Rejang,
        Script_Lycian,
        Script_Carian,
        Script_Lydian,
        Script_Cham,

        // Unicode 5.2 additions
        Script_TaiTham,
        Script_TaiViet,
        Script_Avestan,
        Script_EgyptianHieroglyphs,
        Script_Samaritan,
        Script_Lisu,
        Script_Bamum,
        Script_Javanese,
        Script_MeeteiMayek,
        Script_ImperialAramaic,
        Script_OldSouthArabian,
        Script_InscriptionalParthian,
        Script_InscriptionalPahlavi,
        Script_OldTurkic,
        Script_Kaithi,

        // Unicode 6.0 additions
        Script_Batak,
        Script_Brahmi,
        Script_Mandaic,

        // Unicode 6.1 additions
        Script_Chakma,
        Script_MeroiticCursive,
        Script_MeroiticHieroglyphs,
        Script_Miao,
        Script_Sharada,
        Script_SoraSompeng,
        Script_Takri,

        // Unicode 7.0 additions
        Script_CaucasianAlbanian,
        Script_BassaVah,
        Script_Duployan,
        Script_Elbasan,
        Script_Grantha,
        Script_PahawhHmong,
        Script_Khojki,
        Script_LinearA,
        Script_Mahajani,
        Script_Manichaean,
        Script_MendeKikakui,
        Script_Modi,
        Script_Mro,
        Script_OldNorthArabian,
        Script_Nabataean,
        Script_Palmyrene,
        Script_PauCinHau,
        Script_OldPermic,
        Script_PsalterPahlavi,
        Script_Siddham,
        Script_Khudawadi,
        Script_Tirhuta,
        Script_WarangCiti,

        // Unicode 8.0 additions
        Script_Ahom,
        Script_AnatolianHieroglyphs,
        Script_Hatran,
        Script_Multani,
        Script_OldHungarian,
        Script_SignWriting,

        // Unicode 9.0 additions
        Script_Adlam,
        Script_Bhaiksuki,
        Script_Marchen,
        Script_Newa,
        Script_Osage,
        Script_Tangut,

        // Unicode 10.0 additions
        Script_MasaramGondi,
        Script_Nushu,
        Script_Soyombo,
        Script_ZanabazarSquare,

        // Unicode 12.1 additions
        Script_Dogra,
        Script_GunjalaGondi,
        Script_HanifiRohingya,
        Script_Makasar,
        Script_Medefaidrin,
        Script_OldSogdian,
        Script_Sogdian,
        Script_Elymaic,
        Script_Nandinagari,
        Script_NyiakengPuachueHmong,
        Script_Wancho,

        // Unicode 13.0 additions
        Script_Chorasmian,
        Script_DivesAkuru,
        Script_KhitanSmallScript,
        Script_Yezidi,

        ScriptCount
    };

    enum Direction
    {
        DirL, DirR, DirEN, DirES, DirET, DirAN, DirCS, DirB, DirS, DirWS, DirON,
        DirLRE, DirLRO, DirAL, DirRLE, DirRLO, DirPDF, DirNSM, DirBN,
        DirLRI, DirRLI, DirFSI, DirPDI
    };

    enum Decomposition
    {
        NoDecomposition,
        Canonical,
        Font,
        NoBreak,
        Initial,
        Medial,
        Final,
        Isolated,
        Circle,
        Super,
        Sub,
        Vertical,
        Wide,
        Narrow,
        Small,
        Square,
        Compat,
        Fraction
    };

    enum JoiningType {
        Joining_None,
        Joining_Causing,
        Joining_Dual,
        Joining_Right,
        Joining_Left,
        Joining_Transparent
    };

    enum Joining
    {
        OtherJoining, Dual, Right, Center
    };

    enum CombiningClass
    {
        Combining_BelowLeftAttached       = 200,
        Combining_BelowAttached           = 202,
        Combining_BelowRightAttached      = 204,
        Combining_LeftAttached            = 208,
        Combining_RightAttached           = 210,
        Combining_AboveLeftAttached       = 212,
        Combining_AboveAttached           = 214,
        Combining_AboveRightAttached      = 216,

        Combining_BelowLeft               = 218,
        Combining_Below                   = 220,
        Combining_BelowRight              = 222,
        Combining_Left                    = 224,
        Combining_Right                   = 226,
        Combining_AboveLeft               = 228,
        Combining_Above                   = 230,
        Combining_AboveRight              = 232,

        Combining_DoubleBelow             = 233,
        Combining_DoubleAbove             = 234,
        Combining_IotaSubscript           = 240
    };

    enum UnicodeVersion {
        Unicode_Unassigned,
        Unicode_1_1,
        Unicode_2_0,
        Unicode_2_1_2,
        Unicode_3_0,
        Unicode_3_1,
        Unicode_3_2,
        Unicode_4_0,
        Unicode_4_1,
        Unicode_5_0,
        Unicode_5_1,
        Unicode_5_2,
        Unicode_6_0,
        Unicode_6_1,
        Unicode_6_2,
        Unicode_6_3,
        Unicode_7_0,
        Unicode_8_0,
        Unicode_9_0,
        Unicode_10_0,
        Unicode_11_0,
        Unicode_12_0,
        Unicode_12_1,
        Unicode_13_0
    };

    inline Category category() const noexcept { return PkChar::category(ucs); }
    inline Direction direction() const noexcept { return PkChar::direction(ucs); }
    inline JoiningType joiningType() const noexcept { return PkChar::joiningType(ucs); }
    inline unsigned char combiningClass() const noexcept { return PkChar::combiningClass(ucs); }

    inline PkChar mirroredChar() const noexcept { return PkChar(PkChar::mirroredChar(ucs)); }
    inline bool hasMirrored() const noexcept { return PkChar::hasMirrored(ucs); }

    inline Decomposition decompositionTag() const noexcept { return PkChar::decompositionTag(ucs); }

    inline int digitValue() const noexcept { return PkChar::digitValue(ucs); }
    inline PkChar toLower() const noexcept { return PkChar(PkChar::toLower(ucs)); }
    inline PkChar toUpper() const noexcept { return PkChar(PkChar::toUpper(ucs)); }
    inline PkChar toTitleCase() const noexcept { return PkChar(PkChar::toTitleCase(ucs)); }
    inline PkChar toCaseFolded() const noexcept { return PkChar(PkChar::toCaseFolded(ucs)); }

    inline Script script() const noexcept { return PkChar::script(ucs); }

    inline UnicodeVersion unicodeVersion() const noexcept { return PkChar::unicodeVersion(ucs); }

    
    inline char toLatin1() const noexcept { return ucs > 0xff ? '\0' : char(ucs); }
    inline ushort unicode() const noexcept { return ucs; }
    inline ushort &unicode() noexcept { return ucs; }

    
    static inline PkChar fromLatin1(char c) noexcept { return PkChar(ushort(uchar(c))); }

    inline bool isNull() const noexcept { return ucs == 0; }

    inline bool isPrint() const noexcept { return PkChar::isPrint(ucs); }
    inline bool isSpace() const noexcept { return PkChar::isSpace(ucs); }
    inline bool isMark() const noexcept { return PkChar::isMark(ucs); }
    inline bool isPunct() const noexcept { return PkChar::isPunct(ucs); }
    inline bool isSymbol() const noexcept { return PkChar::isSymbol(ucs); }
    inline bool isLetter() const noexcept { return PkChar::isLetter(ucs); }
    inline bool isNumber() const noexcept { return PkChar::isNumber(ucs); }
    inline bool isLetterOrNumber() const noexcept { return PkChar::isLetterOrNumber(ucs); }
    inline bool isDigit() const noexcept { return PkChar::isDigit(ucs); }
    inline bool isLower() const noexcept { return PkChar::isLower(ucs); }
    inline bool isUpper() const noexcept { return PkChar::isUpper(ucs); }
    inline bool isTitleCase() const noexcept { return PkChar::isTitleCase(ucs); }

    inline bool isNonCharacter() const noexcept { return PkChar::isNonCharacter(ucs); }
    inline bool isHighSurrogate() const noexcept { return PkChar::isHighSurrogate(ucs); }
    inline bool isLowSurrogate() const noexcept { return PkChar::isLowSurrogate(ucs); }
    inline bool isSurrogate() const noexcept { return PkChar::isSurrogate(ucs); }

    inline uchar cell() const noexcept { return uchar(ucs & 0xff); }
    inline uchar row() const noexcept { return uchar((ucs>>8)&0xff); }
    inline void setCell(uchar acell) noexcept { ucs = ushort((ucs & 0xff00) + acell); }
    inline void setRow(uchar arow) noexcept { ucs = ushort((ushort(arow)<<8) + (ucs&0xff)); }

    static inline bool isNonCharacter(uint ucs4) noexcept
    {
        return ucs4 >= 0xfdd0 && (ucs4 <= 0xfdef || (ucs4 & 0xfffe) == 0xfffe);
    }
    static inline bool isHighSurrogate(uint ucs4) noexcept
    {
        return ((ucs4 & 0xfffffc00) == 0xd800);
    }
    static inline bool isLowSurrogate(uint ucs4) noexcept
    {
        return ((ucs4 & 0xfffffc00) == 0xdc00);
    }
    static inline bool isSurrogate(uint ucs4) noexcept
    {
        return (ucs4 - 0xd800u < 2048u);
    }
    static inline bool requiresSurrogates(uint ucs4) noexcept
    {
        return (ucs4 >= 0x10000);
    }
    static inline uint surrogateToUcs4(ushort high, ushort low) noexcept
    {
        return (uint(high)<<10) + low - 0x35fdc00;
    }
    static inline uint surrogateToUcs4(PkChar high, PkChar low) noexcept
    {
        return surrogateToUcs4(high.ucs, low.ucs);
    }
    static inline ushort highSurrogate(uint ucs4) noexcept
    {
        return ushort((ucs4>>10) + 0xd7c0);
    }
    static inline ushort lowSurrogate(uint ucs4) noexcept
    {
        return ushort(ucs4%0x400 + 0xdc00);
    }

    static Category category(uint ucs4) noexcept ;
    static Direction direction(uint ucs4) noexcept ;
    static JoiningType joiningType(uint ucs4) noexcept ;
    
    static unsigned char combiningClass(uint ucs4) noexcept ;

    static uint mirroredChar(uint ucs4) noexcept ;
    static bool hasMirrored(uint ucs4) noexcept ;

    static Decomposition decompositionTag(uint ucs4) noexcept ;

    static int digitValue(uint ucs4) noexcept ;
    static uint toLower(uint ucs4) noexcept ;
    static uint toUpper(uint ucs4) noexcept ;
    static uint toTitleCase(uint ucs4) noexcept ;
    static uint toCaseFolded(uint ucs4) noexcept ;

    static Script script(uint ucs4) noexcept ;

    static UnicodeVersion unicodeVersion(uint ucs4) noexcept ;

    static UnicodeVersion currentUnicodeVersion() noexcept ;

    static bool isPrint(uint ucs4) noexcept ;
    static inline bool isSpace(uint ucs4) noexcept 
    {
        // note that [0x09..0x0d] + 0x85 are exceptional Cc-s and must be handled explicitly
        return ucs4 == 0x20 || (ucs4 <= 0x0d && ucs4 >= 0x09)
                || (ucs4 > 127 && (ucs4 == 0x85 || ucs4 == 0xa0 || PkChar::isSpace_helper(ucs4)));
    }
    static bool isMark(uint ucs4) noexcept ;
    static bool isPunct(uint ucs4) noexcept ;
    static bool isSymbol(uint ucs4) noexcept ;
    static inline bool isLetter(uint ucs4) noexcept 
    {
        return (ucs4 >= 'A' && ucs4 <= 'z' && (ucs4 >= 'a' || ucs4 <= 'Z'))
                || (ucs4 > 127 && PkChar::isLetter_helper(ucs4));
    }
    static inline bool isNumber(uint ucs4) noexcept 
    { return (ucs4 <= '9' && ucs4 >= '0') || (ucs4 > 127 && PkChar::isNumber_helper(ucs4)); }
    static inline bool isLetterOrNumber(uint ucs4) noexcept 
    {
        return (ucs4 >= 'A' && ucs4 <= 'z' && (ucs4 >= 'a' || ucs4 <= 'Z'))
                || (ucs4 >= '0' && ucs4 <= '9')
                || (ucs4 > 127 && PkChar::isLetterOrNumber_helper(ucs4));
    }
    static inline bool isDigit(uint ucs4) noexcept 
    { return (ucs4 <= '9' && ucs4 >= '0') || (ucs4 > 127 && PkChar::category(ucs4) == Number_DecimalDigit); }
    static inline bool isLower(uint ucs4) noexcept 
    { return (ucs4 <= 'z' && ucs4 >= 'a') || (ucs4 > 127 && PkChar::category(ucs4) == Letter_Lowercase); }
    static inline bool isUpper(uint ucs4) noexcept 
    { return (ucs4 <= 'Z' && ucs4 >= 'A') || (ucs4 > 127 && PkChar::category(ucs4) == Letter_Uppercase); }
    static inline bool isTitleCase(uint ucs4) noexcept 
    { return ucs4 > 127 && PkChar::category(ucs4) == Letter_Titlecase; }

    // decomposition（返回分解串；QChar 版返回 QString，这里 PkString）
    static PkString decomposition(uint ucs4) noexcept;
    inline PkString decomposition() const noexcept { return PkChar::decomposition(ucs); }

private:
    static bool isSpace_helper(uint ucs4) noexcept ;
    static bool isLetter_helper(uint ucs4) noexcept ;
    static bool isNumber_helper(uint ucs4) noexcept ;
    static bool isLetterOrNumber_helper(uint ucs4) noexcept ;

    friend bool operator==(PkChar, PkChar) noexcept;
    friend bool operator< (PkChar, PkChar) noexcept;
    ushort ucs;
};

#endif // PK_CHAR_H
