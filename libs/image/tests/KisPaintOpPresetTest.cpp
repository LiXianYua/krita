/*
 *  SPDX-FileCopyrightText: 2024 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisPaintOpPresetTest.h"

#include <testimage.h>

#include <testutil.h>
#include <KisResourceModel.h>

#include "kis_paintop_preset.h"
#include "kis_paintop_settings.h"
#include "KisLocalStrokeResources.h"
#include <KisGlobalResourcesInterface.h>
#include <KisResourceLoaderRegistry.h>
#include <KisResourceThumbnailCodec.h>
#include <KisMimeDatabase.h>
#include <PkMemoryStream.h>
#include <PkFileStream.h>

#include <QFileInfo>

#include <algorithm>
#include <map>
#include <cstring>
#include <string>
#include <type_traits>

namespace {
PkString toPkString(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return PkString::PkFromUtf8(utf8.constData(), utf8.size());
}

template <typename C, typename T = typename C::value_type>
QSet<T> toSet(const C &container) {
    return QSet<T>(container.begin(), container.end());
}

PkByteArray streamBytes(const PkMemoryStream &stream)
{
    return PkByteArray(stream.data(), static_cast<int>(stream.size()));
}

std::string signatureKey(const KoResourceSignature &signature)
{
    return signature.type.PkToUtf8() + "\n" +
        signature.md5sum.PkToUtf8() + "\n" +
        signature.filename.PkToUtf8() + "\n" +
        signature.name.PkToUtf8();
}

PkString withNonCanonicalBase64PadBits(const PkString &xml)
{
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string bytes = xml.PkToUtf8();
    std::size_t searchFrom = 0;
    while (true) {
        const std::size_t cdataStart = bytes.find("<![CDATA[", searchFrom);
        const std::size_t cdataEnd = cdataStart == std::string::npos
            ? std::string::npos : bytes.find("]]>", cdataStart);
        if (cdataEnd == std::string::npos || cdataEnd < cdataStart + 11) {
            return PkString();
        }

        std::size_t padding = 0;
        while (cdataEnd > cdataStart + 9 + padding &&
               bytes[cdataEnd - padding - 1] == '=') {
            ++padding;
        }
        if (padding > 0 && padding <= 2) {
            const std::size_t finalIndex = cdataEnd - padding - 1;
            const char *position = std::strchr(alphabet, bytes[finalIndex]);
            if (!position) {
                return PkString();
            }
            const int value = static_cast<int>(position - alphabet);
            const int nonCanonical = padding == 2 ? (value & 0x30) | 0x01
                                                  : (value & 0x3c) | 0x01;
            bytes[finalIndex] = alphabet[nonCanonical];
            return PkString::PkFromUtf8(bytes.data(),
                                        static_cast<int>(bytes.size()));
        }
        searchFrom = cdataEnd + 3;
    }
}

class FailingShortWriteStream : public PkStream
{
protected:
    pk_int64 readData(char *, pk_int64) override
    {
        return -1;
    }

    pk_int64 writeData(const char *, pk_int64 maxSize) override
    {
        if (m_wroteOnce) {
            return -1;
        }
        m_wroteOnce = true;
        return std::min<pk_int64>(17, maxSize);
    }

private:
    bool m_wroteOnce = false;
};

using HistoricalToXmlSignature =
    void (KisPaintOpPreset::*)(PkXmlDocument &, PkXmlElement &) const;
static_assert(std::is_same_v<decltype(&KisPaintOpPreset::toXML),
                             HistoricalToXmlSignature>);
}

void KisPaintOpPresetTest::testLoadingEmbeddedResources_data()
{
    QTest::addColumn<QString>("testFileName");
    QTest::addColumn<QStringList>("expectedSideLoadedResources");
    QTest::addColumn<QStringList>("expectedLinkedResources");
    QTest::addColumn<QStringList>("expectedEmbeddedResources");

    QTest::newRow("ver-2.2")
        << "test-embedded-resources-2.2.kpp"
        << QStringList{}
        << QStringList{"test_brush.png"}
        << QStringList{"6d8b596cfb5220b06174145fb3fbbaed"};

    QTest::newRow("ver-5.0")
        << "test-embedded-resources-5.0.kpp"
        << QStringList{"9a90b42a7bb2e7cef22689bf3abcdba6", "dc4e9099acb7c3cd33293a48f75c6ff7"}
        << QStringList{"9a90b42a7bb2e7cef22689bf3abcdba6", "dc4e9099acb7c3cd33293a48f75c6ff7"}
        << QStringList{};

        QTest::newRow("bad-md5")
            << "test-embedded-resources-bad-md5.kpp"
            << QStringList{"9a90b42a7bb2e7cef22689bf3abcdba6", "59bd2f6763fd9b1669c532a7c99ae781"}
            << QStringList{"9a90b42a7bb2e7cef22689bf3abcdba6", "59bd2f6763fd9b1669c532a7c99ae781"}
            << QStringList{};
}

void KisPaintOpPresetTest::testLoadingEmbeddedResources()
{
    QFETCH(QString, testFileName);
    QFETCH(QStringList, expectedSideLoadedResources);
    QFETCH(QStringList, expectedLinkedResources);
    QFETCH(QStringList, expectedEmbeddedResources);

    const QString fileName = QString(FILES_DATA_DIR) + QDir::separator() + testFileName;

    QVERIFY(QFileInfo(fileName).exists());

    KisResourcesInterfaceSP linkedResources(new KisLocalStrokeResources());

    const QByteArray fileNameUtf8 = fileName.toUtf8();
    KisPaintOpPresetSP preset(new KisPaintOpPreset(PkString::PkFromUtf8(fileNameUtf8.constData(), fileNameUtf8.size())));
    preset->load(linkedResources);

    QVERIFY(preset->valid());

    QSet<QString> realSideLoadedSignatures;
    QSet<QString> realLinkedSignatures;
    QSet<QString> realEmbeddedSignatures;

    Q_FOREACH (const KoResourceLoadResult &result, preset->sideLoadedResources(linkedResources)) {
        //qDebug() << "side-loaded" << result.type() << result.signature();
        QCOMPARE(result.type(), KoResourceLoadResult::EmbeddedResource);
        QVERIFY(result.embeddedResource().isValid());
        QVERIFY(result.embeddedResource().sanityCheckMd5());

            realSideLoadedSignatures << QString::fromUtf8(result.signature().md5sum.PkToUtf8().c_str());
    }

    // check if clearing the side-loaded resources actually clears them
    preset->clearSideLoadedResources();
    QCOMPARE(preset->sideLoadedResources(linkedResources).size(), 0);

    Q_FOREACH (const KoResourceLoadResult &result, preset->linkedResources(linkedResources)) {
        //qDebug() << "linked" << result.type() << result.signature();

        /**
         * The side-loaded resources are not yet loaded into linkedResources,
         * so the call should return fail-link resources.
         */
        QCOMPARE(result.type(), KoResourceLoadResult::FailedLink);

        /**
         * Linked resources in older versions may miss the MD5 sum, in which case
         * just fall back to a filename
         */
        realLinkedSignatures <<
            (!result.signature().md5sum.isEmpty() ?
                QString::fromUtf8(result.signature().md5sum.PkToUtf8().c_str()) :
                QString::fromUtf8(result.signature().filename.PkToUtf8().c_str()));
    }

    Q_FOREACH (const KoResourceLoadResult &result, preset->embeddedResources(linkedResources)) {
        //qDebug() << "embedded" << result.type() << result.signature();
        QCOMPARE(result.type(), KoResourceLoadResult::EmbeddedResource);
        realEmbeddedSignatures << QString::fromUtf8(result.signature().md5sum.PkToUtf8().c_str());
    }

    QCOMPARE(realSideLoadedSignatures, toSet(expectedSideLoadedResources));
    QCOMPARE(realLinkedSignatures, toSet(expectedLinkedResources));
    QCOMPARE(realEmbeddedSignatures, toSet(expectedEmbeddedResources));
}

void KisPaintOpPresetTest::testConflictingEmbeddedPatterns()
{
    KisResourcesInterfaceSP emptyLocalResources(new KisLocalStrokeResources());
    KisResourcesInterfaceSP resourcesInterface = KisGlobalResourcesInterface::instance();

    KisResourceModel model(ResourceType::PaintOpPresets);

    int verticalPatternResourceId = -1;

    {
        /// Firstly, load a preset that embeds a patterns with
        /// "stripes-pat.png" name

        QString presetFileName(TestUtil::fetchDataFileLazy("conflicting-patterns/preset-stripes-vert.kpp"));
        PkFileStream file(toPkString(presetFileName));
        KIS_ASSERT(file.open(PkStream::ReadOnly));

        const QString resourceName = QFileInfo(presetFileName).fileName();
        KoResourceSP resource = model.importResource(toPkString(resourceName), &file, false, "memory");
        QVERIFY(resource);

        KisPaintOpPresetSP preset = resource.dynamicCast<KisPaintOpPreset>();

        KoPatternSP loadedPattern = preset->linkedResources(resourcesInterface).first().resource<KoPattern>();
        QVERIFY(loadedPattern);
        QCOMPARE(loadedPattern->name(), "stripes-pat.png");
        QCOMPARE(loadedPattern->md5Sum(), "d3bc7abb7136295578c6b3af3da02fcc");

        QVERIFY(loadedPattern->resourceId() >= 0);
        verticalPatternResourceId = loadedPattern->resourceId();
    }

    {
        /// Secondly, load a preset that embeds the same pattern, it should reuse
        /// the same pattern object

        QString presetFileName(TestUtil::fetchDataFileLazy("conflicting-patterns/preset-stripes-vert-v2.kpp"));
        PkFileStream file(toPkString(presetFileName));
        KIS_ASSERT(file.open(PkStream::ReadOnly));

        const QString resourceName = QFileInfo(presetFileName).fileName();
        KoResourceSP resource = model.importResource(toPkString(resourceName), &file, false, "memory");
        QVERIFY(resource);

        KisPaintOpPresetSP preset = resource.dynamicCast<KisPaintOpPreset>();

        KoPatternSP loadedPattern = preset->linkedResources(resourcesInterface).first().resource<KoPattern>();
        QVERIFY(loadedPattern);
        QCOMPARE(loadedPattern->name(), "stripes-pat.png");
        QCOMPARE(loadedPattern->md5Sum(), "d3bc7abb7136295578c6b3af3da02fcc");

        // Krita should reuse the same pattern object
        QCOMPARE(loadedPattern->resourceId(), verticalPatternResourceId);
    }

    {
        /// Now load a preset that also embeds a "stripes-pat.png" pattern,
        /// but a different one (with a different md5sum)

        QString presetFileName(TestUtil::fetchDataFileLazy("conflicting-patterns/preset-stripes-cross.kpp"));
        PkFileStream file(toPkString(presetFileName));
        KIS_ASSERT(file.open(PkStream::ReadOnly));

        const QString resourceName = QFileInfo(presetFileName).fileName();
        KoResourceSP resource = model.importResource(toPkString(resourceName), &file, false, "memory");
        QVERIFY(resource);

        KisPaintOpPresetSP preset = resource.dynamicCast<KisPaintOpPreset>();

        /// try to fetch the signature of the linked resource by loading
        /// in an empty environment
        auto resourcesForEmptyEnvironment = preset->linkedResources(emptyLocalResources);
        QCOMPARE(resourcesForEmptyEnvironment.size(), 1);
        QCOMPARE(resourcesForEmptyEnvironment.first().type(), KoResourceLoadResult::FailedLink);
        const QString requiredMd5Sum = QString::fromUtf8(resourcesForEmptyEnvironment.first().signature().md5sum.PkToUtf8().c_str());
        QCOMPARE(requiredMd5Sum, "cca80dd27e4085ef089729462714e942");

        // now try to load in real environment, it should fetch the correct
        // deduplicated resource (with a different filename, but same MD5)
        KoPatternSP loadedPattern = preset->linkedResources(resourcesInterface).first().resource<KoPattern>();
        QVERIFY(loadedPattern);
        QCOMPARE(QString::fromUtf8(loadedPattern->md5Sum().PkToUtf8().c_str()), requiredMd5Sum);

        QVERIFY(loadedPattern->resourceId() != verticalPatternResourceId);
    }
}

void KisPaintOpPresetTest::testSaveLoadRoundTrip()
{
    const PkString fileName = PkString(FILES_DATA_DIR) +
        PkString("/test-embedded-resources-5.0.kpp");
    KisResourcesInterfaceSP sourceResources(new KisLocalStrokeResources());
    KisPaintOpPresetSP source(new KisPaintOpPreset(fileName));
    QVERIFY(source->load(sourceResources));
    QVERIFY(source->valid());

    const PkList<KoResourceLoadResult> sideLoaded =
        source->sideLoadedResources(sourceResources);
    QCOMPARE(sideLoaded.size(), 2);
    std::map<std::string, PkByteArray> expectedResources;
    PkList<KoResourceSP> materializedResources;
    for (const KoResourceLoadResult &result : sideLoaded) {
        const KoEmbeddedResource embedded = result.embeddedResource();
        QVERIFY(embedded.isValid());
        expectedResources.emplace(signatureKey(result.signature()), embedded.data());

        PkMemoryStream resourceStream;
        QVERIFY(resourceStream.open(PkStream::ReadWrite));
        const PkByteArray data = embedded.data();
        QCOMPARE(resourceStream.write(data.constData(), data.size()), data.size());
        QVERIFY(resourceStream.seek(0));

        const KoResourceSignature signature = embedded.signature();
        KisResourceLoaderBase *loader = KisResourceLoaderRegistry::instance()->loader(
            signature.type, KisMimeDatabase::mimeTypeForFile(signature.filename));
        QVERIFY(loader);
        KoResourceSP resource = loader->load(signature.filename,
                                             resourceStream,
                                             sourceResources);
        QVERIFY(resource);
        QVERIFY(resource->signature() == signature);
        KisResourceModel resourceModel(signature.type);
        QVERIFY(resourceModel.addResource(resource, "memory"));
        static_cast<KisLocalStrokeResources *>(sourceResources.data())->addResource(resource);
        materializedResources.append(resource);
    }
    source->setResourcesInterface(sourceResources);

    PkMemoryStream saved;
    QVERIFY(saved.open(PkStream::ReadWrite));
    const auto sourceSettingsSnapshot = source->settings()->getProperties();
    QVERIFY(source->saveToDevice(&saved));
    const PkByteArray savedBytes = streamBytes(saved);
    QVERIFY(!savedBytes.isEmpty());

    KisResourceThumbnailCodec::PngPayload payload;
    QVERIFY(KisResourceThumbnailCodec::decodePng(savedBytes, payload));
    QCOMPARE(payload.text.value(PkString("version")), PkString("5.0"));
    const PkString savedPresetXml = payload.text.value(PkString("preset"));
    QVERIFY(savedPresetXml.contains("<Preset"));
    QVERIFY(savedPresetXml.contains("<resources>"));
    QCOMPARE(payload.image.width(), source->image().width());
    QCOMPARE(payload.image.height(), source->image().height());
    for (int y = 0; y < source->image().height(); ++y) {
        for (int x = 0; x < source->image().width(); ++x) {
            QCOMPARE(payload.image.pixel(x, y), source->image().pixel(x, y));
        }
    }

    PkMemoryStream reloadStream;
    QVERIFY(reloadStream.open(PkStream::ReadWrite));
    QCOMPARE(reloadStream.write(savedBytes.constData(), savedBytes.size()), savedBytes.size());
    QVERIFY(reloadStream.seek(0));
    KisResourcesInterfaceSP emptyResources(new KisLocalStrokeResources());
    KisPaintOpPresetSP reloaded(new KisPaintOpPreset());
    QVERIFY(reloaded->loadFromDevice(&reloadStream, emptyResources));
    QVERIFY(reloaded->valid());
    QCOMPARE(reloaded->image().width(), source->image().width());
    QCOMPARE(reloaded->image().height(), source->image().height());
    QVERIFY(reloaded->settings()->getProperties() == sourceSettingsSnapshot);
    for (int y = 0; y < source->image().height(); ++y) {
        for (int x = 0; x < source->image().width(); ++x) {
            QCOMPARE(reloaded->image().pixel(x, y), source->image().pixel(x, y));
        }
    }

    const PkList<KoResourceLoadResult> reloadedResources =
        reloaded->sideLoadedResources(emptyResources);
    QCOMPARE(reloadedResources.size(), sideLoaded.size());
    std::map<std::string, PkByteArray> actualResources;
    for (const KoResourceLoadResult &result : reloadedResources) {
        QCOMPARE(result.type(), KoResourceLoadResult::EmbeddedResource);
        QVERIFY(result.embeddedResource().isValid());
        QVERIFY(result.embeddedResource().sanityCheckMd5());
        const KoEmbeddedResource embedded = result.embeddedResource();
        actualResources.emplace(signatureKey(result.signature()), embedded.data());

        PkMemoryStream resourceStream;
        QVERIFY(resourceStream.open(PkStream::ReadWrite));
        const PkByteArray data = embedded.data();
        QCOMPARE(resourceStream.write(data.constData(), data.size()), data.size());
        QVERIFY(resourceStream.seek(0));
        const KoResourceSignature signature = embedded.signature();
        KisResourceLoaderBase *loader = KisResourceLoaderRegistry::instance()->loader(
            signature.type, KisMimeDatabase::mimeTypeForFile(signature.filename));
        QVERIFY(loader);
        KoResourceSP resource = loader->load(signature.filename,
                                             resourceStream,
                                             emptyResources);
        QVERIFY(resource);
        QVERIFY(resource->signature() == signature);
    }
    QVERIFY(actualResources == expectedResources);

    source->setImage(PkImage());
    PkMemoryStream fallbackSaved;
    QVERIFY(fallbackSaved.open(PkStream::ReadWrite));
    QVERIFY(source->saveToDevice(&fallbackSaved));
    KisResourceThumbnailCodec::PngPayload fallbackPayload;
    QVERIFY(KisResourceThumbnailCodec::decodePng(streamBytes(fallbackSaved),
                                                  fallbackPayload));
    QCOMPARE(fallbackPayload.image.width(), 1);
    QCOMPARE(fallbackPayload.image.height(), 1);

    FailingShortWriteStream shortWrite;
    QVERIFY(shortWrite.open(PkStream::WriteOnly));
    QVERIFY(!source->saveToDevice(&shortWrite));

    QVERIFY(!materializedResources.isEmpty());
    materializedResources.first()->setStorageLocation(
        PkString("missing-storage-for-preset-export-test"));
    PkMemoryStream failedExport;
    QVERIFY(failedExport.open(PkStream::WriteOnly));
    QVERIFY(!source->saveToDevice(&failedExport));

    PkMap<PkString, PkString> nonCanonicalText = payload.text;
    const PkString nonCanonicalPreset =
        withNonCanonicalBase64PadBits(savedPresetXml);
    QVERIFY(!nonCanonicalPreset.isEmpty());
    nonCanonicalText.insert(PkString("preset"), nonCanonicalPreset);
    const PkByteArray nonCanonicalPng =
        KisResourceThumbnailCodec::encodePng(payload.image, nonCanonicalText);
    QVERIFY(!nonCanonicalPng.isEmpty());
    PkMemoryStream nonCanonicalStream;
    QVERIFY(nonCanonicalStream.open(PkStream::ReadWrite));
    QCOMPARE(nonCanonicalStream.write(nonCanonicalPng.constData(),
                                      nonCanonicalPng.size()),
             nonCanonicalPng.size());
    QVERIFY(nonCanonicalStream.seek(0));
    KisPaintOpPreset nonCanonicalReload;
    QVERIFY(!nonCanonicalReload.loadFromDevice(&nonCanonicalStream,
                                                emptyResources));
}

KISTEST_MAIN(KisPaintOpPresetTest)
