/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <KisResourceThumbnailCodec.h>
#include <PkMap.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace
{

bool check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

PkByteArray readFixture(const char *name)
{
    const std::string path = std::string(PAINTOP_PRESET_DATA_DIR) + "/" + name;
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return PkByteArray();
    }
    const std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    return PkByteArray(bytes);
}

bool hasPngTextChunk(const PkByteArray &png,
                     const char chunkType[4],
                     const char *keyword,
                     int compressionFlag = -1)
{
    const auto *bytes = reinterpret_cast<const std::uint8_t *>(png.constData());
    const std::size_t size = static_cast<std::size_t>(png.size());
    std::size_t offset = 8;
    while (size >= 8 && offset + 12 <= size) {
        const std::size_t length =
            (static_cast<std::size_t>(bytes[offset]) << 24) |
            (static_cast<std::size_t>(bytes[offset + 1]) << 16) |
            (static_cast<std::size_t>(bytes[offset + 2]) << 8) |
            static_cast<std::size_t>(bytes[offset + 3]);
        if (length > size - offset - 12) {
            return false;
        }
        const char *type = reinterpret_cast<const char *>(bytes + offset + 4);
        const char *data = reinterpret_cast<const char *>(bytes + offset + 8);
        const std::size_t keywordLength = std::strlen(keyword);
        if (std::memcmp(type, chunkType, 4) == 0 &&
            length > keywordLength &&
            std::memcmp(data, keyword, keywordLength) == 0 &&
            data[keywordLength] == '\0') {
            return compressionFlag < 0 ||
                (length > keywordLength + 1 &&
                 static_cast<unsigned char>(data[keywordLength + 1]) ==
                     static_cast<unsigned char>(compressionFlag));
        }
        offset += length + 12;
    }
    return false;
}

bool checkFixture(const char *name, const char *version)
{
    const PkByteArray encoded = readFixture(name);
    KisResourceThumbnailCodec::PngPayload payload;
    return check(!encoded.isEmpty(), "fixture could not be read") &&
        check(KisResourceThumbnailCodec::decodePng(encoded, payload),
              "fixture PNG plus metadata did not decode") &&
        check(payload.image.width() == 200 && payload.image.height() == 200,
              "fixture preview dimensions changed") &&
        check(payload.text.value(PkString("version")) == PkString(version),
              "fixture version text was not preserved") &&
        check(payload.text.value(PkString("preset")).contains("<Preset"),
              "fixture preset XML was not preserved");
}

}

int main()
{
    bool ok = true;
    ok &= checkFixture("test-embedded-resources-2.2.kpp", "2.2");
    ok &= checkFixture("test-embedded-resources-5.0.kpp", "5.0");

    PkImage preview(2, 2, PkImage::Format_ARGB32);
    preview.setPixel(0, 0, 0xff102030U);
    preview.setPixel(1, 0, 0x80406080U);
    preview.setPixel(0, 1, 0xffabcdefU);
    preview.setPixel(1, 1, 0x00000000U);

    std::string xml = u8"<Preset name=\"水彩\">";
    for (int i = 0; i < 512; ++i) {
        xml += u8"画笔–данные";
    }
    xml += "</Preset>";

    PkMap<PkString, PkString> text;
    text.insert(PkString("version"), PkString("5.0"));
    text.insert(PkString("preset"),
                PkString::PkFromUtf8(xml.data(), static_cast<int>(xml.size())));
    const PkByteArray encoded = KisResourceThumbnailCodec::encodePng(preview, text);
    ok &= check(!encoded.isEmpty(), "metadata PNG encoding failed");
    ok &= check(hasPngTextChunk(encoded, "tEXt", "version"),
                "short ASCII text was not emitted as tEXt");
    ok &= check(hasPngTextChunk(encoded, "iTXt", "preset", 1),
                "large UTF-8 text was not emitted as compressed iTXt");

    KisResourceThumbnailCodec::PngPayload roundTrip;
    ok &= check(KisResourceThumbnailCodec::decodePng(encoded, roundTrip),
                "encoded metadata PNG did not decode");
    ok &= check(roundTrip.text == text, "PNG text round trip was not lossless");
    ok &= check(roundTrip.image.width() == preview.width() &&
                    roundTrip.image.height() == preview.height(),
                "preview dimensions did not round trip");
    for (int y = 0; y < preview.height(); ++y) {
        for (int x = 0; x < preview.width(); ++x) {
            ok &= check(roundTrip.image.pixel(x, y) == preview.pixel(x, y),
                        "preview pixel did not round trip");
        }
    }

    const PkImage oldApiImage = KisResourceThumbnailCodec::decodePng(encoded);
    ok &= check(oldApiImage.pixel(1, 0) == preview.pixel(1, 0),
                "old image-only decode API changed behavior");
    const PkByteArray oldApiEncoded = KisResourceThumbnailCodec::encodePng(preview);
    KisResourceThumbnailCodec::PngPayload oldApiPayload;
    ok &= check(!oldApiEncoded.isEmpty() &&
                    KisResourceThumbnailCodec::decodePng(oldApiEncoded, oldApiPayload),
                "old image-only encode API changed behavior");
    ok &= check(oldApiPayload.text.isEmpty() &&
                    oldApiPayload.image.pixel(0, 1) == preview.pixel(0, 1),
                "old image-only encode API added metadata or changed pixels");

    const PkByteArray malformed(std::vector<std::uint8_t>{
        0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n', 0, 0, 0, 13, 'I', 'H'});
    KisResourceThumbnailCodec::PngPayload malformedPayload;
    ok &= check(!KisResourceThumbnailCodec::decodePng(malformed, malformedPayload),
                "malformed metadata PNG was accepted");
    ok &= check(malformedPayload.image.isNull() && malformedPayload.text.isEmpty(),
                "failed decode returned partial payload");
    ok &= check(KisResourceThumbnailCodec::decodePng(malformed).isNull(),
                "old image-only API accepted malformed PNG");

    return ok ? 0 : 1;
}
