/*
 * SPDX-FileCopyrightText: 2026 S-09-g
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QTest>
#include <QLocale>
#include <QSet>
#include <KoFontRegistry.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <atomic>
#include <QTemporaryDir>
#include <QFile>
#include <KisResourceCacheDb.h>
#include <KisResourceLocator.h>
#include <PkThreadCallQueue.h>
#include <filesystem>
#include <chrono>

#include <KoSvgTextShapeMarkupConverter.h>
#include <KoSvgText.h>
#include <KoSvgTextProperties.h>
#include <KoWritingSystemUtils.h>
#include <SvgParser.h>

class KoSvgTextHtmlNativeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void localeRegionClosureMatchesQt515()
    {
        static const char *tags[] = {
#include "Qt515ScriptTags.inc"
        };
        QSet<QString> languages, regions;
        for (const auto &locale : QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyCountry)) {
            if (locale.language() == QLocale::C) continue;
            languages.insert(locale.name().section('_', 0, 0));
            regions.insert(locale.name().section('_', 1, 1));
        }
        int count = 0;
        for (const auto &language : languages) {
            for (const auto &region : regions) {
                const QString input = language + '-' + region;
                const QString expected = QString::fromLatin1(tags[int(QLocale(input).script())]);
                const QString actual = QString::fromStdString(KoWritingSystemUtils::scriptTagForLanguage(PkString(input.toUtf8().constData())).PkToUtf8());
                QVERIFY2(actual == expected, qPrintable(input + ": " + actual + " != " + expected));
                ++count;
            }
        }
        qInfo() << "Qt 5.15 language/region pairs checked:" << count;
        QCOMPARE(KoWritingSystemUtils::scriptTagForLanguage("zzzz"), PkString("Zyyy"));
        QCOMPARE(KoWritingSystemUtils::scriptTagForLanguage("sr-Latn-BA"), PkString("Latn"));
        QCOMPARE(KoWritingSystemUtils::scriptTagForLanguage("zh-Hans-TW"), PkString("Hans"));
    }

    void concurrentRefreshPublishesCompleteFamiliesAndQueuesStorageNotification()
    {
        QTemporaryDir database;
        QVERIFY(database.isValid());
        QVERIFY(KisResourceCacheDb::initialize(PkString(database.path().toUtf8().constData())));
        auto *locator = KisResourceLocator::instance();
        QVERIFY(locator);
        QVERIFY(locator->addStorage("fontregistry", KisResourceStorageSP(new KisResourceStorage("fontregistry"))));
        PkObject observer;
        std::atomic<int> notifications{0};
        std::atomic<int> wrongThread{0};
        const auto owner = PkThread::currentThreadId();
        auto connection = PkObject::connect(locator, &KisResourceLocator::storageResynchronized, &observer,
            [&](const PkString &, bool) {
                ++notifications;
                if (PkThread::currentThreadId() != owner) ++wrongThread;
            }, PkConnectionType::Direct);
        {
            KoFontRegistry registry;
            const auto font = QFINDTESTDATA("data/fonts/DejaVuSans.ttf");
            QVERIFY(registry.addFontFilePathToRegistry(PkString(font.toUtf8().constData())));
            std::mutex mutex;
            std::condition_variable condition;
            bool start = false;
            std::atomic<int> invalid{0};
            std::vector<std::thread> readers;
            for (int i = 0; i < 4; ++i) {
                readers.emplace_back([&] {
                    { std::unique_lock<std::mutex> lock(mutex); condition.wait(lock, [&] { return start; }); }
                    for (int j = 0; j < 12; ++j) {
                        if (!registry.representationByFamilyName("DejaVu Sans")) ++invalid;
                        KoCSSFontInfo info;
                        info.families = {"DejaVu Sans"};
                        info.size = 12;
                        PkVector<int> lengths;
                        const auto faces = registry.facesForCSSValues(lengths, info, "abc");
                        if (faces.empty() || lengths.isEmpty() || lengths.first() != 3) ++invalid;
                    }
                });
            }
            std::thread writer([&] {
                { std::unique_lock<std::mutex> lock(mutex); condition.wait(lock, [&] { return start; }); }
                for (int i = 0; i < 4; ++i) registry.updateConfig();
            });
            { std::lock_guard<std::mutex> lock(mutex); start = true; }
            condition.notify_all();
            for (auto &reader : readers) reader.join();
            writer.join();
            QCOMPARE(invalid.load(), 0);
            QCOMPARE(notifications.load(), 0);
        }
        // Delivery uses the locator lifetime, not a captured registry Private:
        // the registry is already destroyed when its notifications run.
        PkThreadCallQueue::processPendingCalls();
        QCOMPARE(notifications.load(), 4);
        QCOMPARE(wrongThread.load(), 0);
        PkObject::disconnect(connection);
    }

    void fontRefreshInvalidatesEveryQueryThread()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto font = QFINDTESTDATA("data/fonts/DejaVuSans.ttf");
        QVERIFY(QFile::copy(font, directory.filePath("font.ttf")));
        QFile config(directory.filePath("fonts.conf"));
        QVERIFY(config.open(QIODevice::WriteOnly));
        config.write(("<fontconfig><dir>" + directory.path() + "</dir><cachedir>"
                     + directory.filePath("cache") + "</cachedir></fontconfig>").toUtf8());
        config.close();
        const QByteArray oldConfig = qgetenv("FONTCONFIG_FILE");
        qputenv("FONTCONFIG_FILE", config.fileName().toUtf8());
        KoFontRegistry registry;
        if (oldConfig.isNull()) qunsetenv("FONTCONFIG_FILE");
        else qputenv("FONTCONFIG_FILE", oldConfig);
        std::mutex mutex;
        std::condition_variable condition;
        int ready = 0;
        bool refreshed = false;
        std::atomic<int> replaced{0};
        std::vector<std::thread> readers;
        for (int i = 0; i < 4; ++i) {
            readers.emplace_back([&] {
                KoCSSFontInfo info;
                info.families = {"DejaVu Sans"};
                info.size = 12;
                PkVector<int> lengths;
                const auto before = registry.facesForCSSValues(lengths, info, "abc");
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    ++ready;
                    condition.notify_all();
                    condition.wait(lock, [&] { return refreshed; });
                }
                const auto after = registry.facesForCSSValues(lengths, info, "abc");
                if (!before.empty() && !after.empty() && before.front().data() != after.front().data()
                    && lengths.size() > 0 && lengths.first() == 3) ++replaced;
            });
        }
        {
            std::unique_lock<std::mutex> lock(mutex);
            condition.wait(lock, [&] { return ready == 4; });
        }
        // A real watched-directory change, after all four threads cached a
        // face. No sleeps or filesystem timestamp-resolution assumptions.
        const auto path = directory.path().toStdString();
        std::filesystem::last_write_time(path, std::filesystem::last_write_time(path) + std::chrono::seconds(2));
        {
            std::lock_guard<std::mutex> lock(mutex);
            refreshed = true;
        }
        condition.notify_all();
        for (auto &reader : readers) reader.join();
        QCOMPARE(replaced.load(), 4);
    }

    void localeScriptsMatchQt515_data()
    {
        QTest::addColumn<QString>("locale");
        QTest::addColumn<QString>("script");
        static const char *tags[] = {
#include "Qt515ScriptTags.inc"
        };
        QSet<QString> inputs = {"bo", "dv", "syr", "sr-BA", "sr-Latn-BA",
            "az-IR", "ha-SD", "kk-CN", "ku-LB", "ky-CN", "mn-CN", "ms-CC",
            "pa-PK", "sd-IN", "sr-ME", "tg-PK", "ug-KZ", "uz-AF", "zh-HK", "zh-MO", "zh-TW"};
        for (const auto &locale : QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyCountry)) {
            if (locale.language() == QLocale::C) continue;
            inputs.insert(locale.name());
            inputs.insert(locale.bcp47Name());
            inputs.insert(locale.name().section('_', 0, 0));
        }
        for (const auto &input : inputs) {
            const QLocale oracle(input);
            QTest::newRow(input.toUtf8().constData()) << input << QString::fromLatin1(tags[int(oracle.script())]);
        }
    }

    void localeScriptsMatchQt515()
    {
        QFETCH(QString, locale);
        QFETCH(QString, script);
        QCOMPARE(QString::fromStdString(KoWritingSystemUtils::scriptTagForLanguage(PkString(locale.toUtf8().constData())).PkToUtf8()), script);
    }

    void preservesHtmlFragments_data()
    {
        QTest::addColumn<QString>("html");
        QTest::addColumn<QString>("expected");
        QTest::newRow("paragraph") << "<p>hello</p>" << "<text>hello</text>";
        QTest::newRow("span") << "<span>hello</span>" << "<text>hello</text>";
        QTest::newRow("document") << "<html><body><p>hello</p></body></html>"
                                  << "<text><tspan>hello</tspan></text>";
        QTest::newRow("unsupported-style")
            << "<html><body><p>one</p><div style=\"color:red\">two</div></body></html>"
            << "<text><tspan>one</tspan>two</text>";
    }

    void preservesHtmlFragments()
    {
        QFETCH(QString, html);
        QFETCH(QString, expected);
        KoSvgTextShapeMarkupConverter converter(nullptr);
        PkString svg, styles;
        QVERIFY(converter.convertFromHtml(PkString(html.toUtf8().constData()), &svg, &styles));
        QCOMPARE(QString::fromStdString(svg.PkToUtf8()), expected);
        const auto doc = SvgParser::createDocumentFromSvg(svg);
        QCOMPARE(doc.documentElement().tagName(), PkString("text"));
        QCOMPARE(doc.documentElement().text(), PkString(html.contains("one") ? "onetwo" : "hello"));
    }

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
