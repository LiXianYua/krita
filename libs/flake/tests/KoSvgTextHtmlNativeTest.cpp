/*
 * SPDX-FileCopyrightText: 2026 S-09-g
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QTest>

#include <KoSvgTextShapeMarkupConverter.h>
#include <KoSvgText.h>
#include <KoSvgTextProperties.h>
#include <KoWritingSystemUtils.h>
#include <SvgParser.h>

class KoSvgTextHtmlNativeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void preservesInlineStyleAndParagraphOffsets()
    {
        KoSvgTextShapeMarkupConverter converter(nullptr);
        PkString svg;
        PkString styles;

        const bool ok = converter.convertFromHtml(
            "<html><body><p>one</p><p><b>two</b></p></body></html>",
            &svg,
            &styles);

        QVERIFY(ok);
        QVERIFY(styles.isEmpty());
        QVERIFY(svg == PkString("<text><tspan>one</tspan><tspan x=\"0\" dy=\"1em\"><tspan style=\"font-weight:700;\">two</tspan></tspan></text>"));
    }

    void pkFontStyleRoundTripsThroughCss()
    {
        const KoSvgText::CssFontStyleData oblique(PkFontStyleOblique);
        QVERIFY(KoSvgText::writeFontStyle(oblique) == PkString("oblique"));

        const KoSvgText::CssFontStyleData italic =
            KoSvgText::parseFontStyle("italic");
        QCOMPARE(int(italic.style), int(PkFontStyleItalic));
    }

    void generatesPkFontWithSvgValueSemantics()
    {
        KoSvgTextProperties properties;
        properties.setProperty(KoSvgTextProperties::FontFamiliesId,
                               PkStringList{PkString("Noto Sans")});
        properties.setProperty(KoSvgTextProperties::FontSizeId,
                               PkVariant::fromValue(KoSvgText::CssLengthPercentage(12.5)));
        properties.setProperty(KoSvgTextProperties::FontWeightId, 700);
        properties.setProperty(KoSvgTextProperties::FontStretchId, 125);
        properties.setProperty(KoSvgTextProperties::FontStyleId,
                               PkVariant::fromValue(KoSvgText::CssFontStyleData(PkFontStyleOblique)));
        KoSvgText::TextDecorations decorations = KoSvgText::DecorationUnderline |
                                                 KoSvgText::DecorationLineThrough |
                                                 KoSvgText::DecorationOverline;
        properties.setProperty(KoSvgTextProperties::TextDecorationLineId,
                               PkVariant::fromValue(decorations));

        const PkFont font = properties.generateFont();
        QCOMPARE(font.family(), std::string("Noto Sans"));
        QCOMPARE(font.pointSizeF(), 12.5);
        QCOMPARE(font.weight(), 700);
        QCOMPARE(font.stretch(), 125);
        QCOMPARE(int(font.style()), int(PkFontStyleOblique));
        QVERIFY(font.strikeOut());
        QVERIFY(font.underline());
        QVERIFY(font.overline());
        QCOMPARE(font.dpi(), 72);
    }

    void pkFontPointAndPixelSizesFollowQtSemantics()
    {
        PkFont font;
        font.setPointSizeF(12.7);
        QCOMPARE(font.pointSize(), 13);
        QCOMPARE(font.pointSizeF(), 12.7);
        QCOMPARE(font.pixelSize(), -1);

        font.setPixelSize(19);
        QCOMPARE(font.pointSize(), -1);
        QCOMPARE(font.pointSizeF(), -1.0);
        QCOMPARE(font.pixelSize(), 19);

        font.setPointSize(11);
        QCOMPARE(font.pointSize(), 11);
        QCOMPARE(font.pointSizeF(), 11.0);
        QCOMPARE(font.pixelSize(), -1);
    }

    void writingSystemSamplesPreserveQt515Coverage()
    {
        const PkMap<PkString, PkString> samples = KoWritingSystemUtils::samples();
        QCOMPARE(samples.size(), 38);
        QCOMPARE(samples.value(PkString::fromUtf8("ܕܥܖܦ")), PkString("s_Syrc"));
        QCOMPARE(samples.value(PkString::fromUtf8("અકથવ")), PkString("s_Gujr"));
        QCOMPARE(samples.value(PkString::fromUtf8("サンプルです")), PkString("s_Jpan"));
        QCOMPARE(samples.value(PkString::fromUtf8("ỗộốồ")), PkString("l_vi"));
    }

    void languageScriptLookupUsesLocaleDefaults()
    {
        QCOMPARE(KoWritingSystemUtils::scriptTagForLanguage("bn"), PkString("Beng"));
        QCOMPARE(KoWritingSystemUtils::scriptTagForLanguage("ta"), PkString("Taml"));
        QCOMPARE(KoWritingSystemUtils::scriptTagForLanguage("gu"), PkString("Gujr"));
        QCOMPARE(KoWritingSystemUtils::scriptTagForLanguage("ka"), PkString("Geor"));
        QCOMPARE(KoWritingSystemUtils::scriptTagForLanguage("my"), PkString("Mymr"));
        QCOMPARE(KoWritingSystemUtils::scriptTagForLanguage("zh-TW"), PkString("Hant"));
        QCOMPARE(KoWritingSystemUtils::scriptTagForLanguage("zh-Hant"), PkString("Hant"));
    }

    void svgDocumentCreationPreservesWhitespaceOnlyTextNodes()
    {
        const PkString xml("<svg><text><tspan>a</tspan> <tspan>b</tspan></text></svg>");

        const PkXmlDocument fromString = SvgParser::createDocumentFromSvg(xml);
        QCOMPARE(fromString.documentElement().firstChildElement().childNodes().count(), 3);

        const std::string utf8 = xml.PkToUtf8();
        const PkXmlDocument fromBytes = SvgParser::createDocumentFromSvg(
            PkByteArray(utf8.data(), static_cast<int>(utf8.size())));
        QCOMPARE(fromBytes.documentElement().firstChildElement().childNodes().count(), 3);
    }
};

QTEST_GUILESS_MAIN(KoSvgTextHtmlNativeTest)

#include "KoSvgTextHtmlNativeTest.moc"
