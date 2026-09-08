#include "../kis_tiff_psd_layer_record.h"
#include "../kis_tiff_psd_resource_record.h"
#include "../../../color/lcms2engine/LcmsEnginePlugin.h"

#include <KoColor.h>
#include <KoColorModelStandardIds.h>
#include <KoColorSpaceRegistry.h>
#include <PkMemoryStream.h>
#include <kis_image.h>
#include <kis_paint_layer.h>
#include <psd_resource_block.h>

#include <tiff.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

std::uint16_t readU16(const std::vector<std::uint8_t> &bytes,
                      std::size_t offset,
                      bool bigEndian)
{
    require(offset + 2 <= bytes.size(), "u16 fixture read must stay in bounds");
    if (bigEndian) {
        return static_cast<std::uint16_t>((bytes[offset] << 8U) | bytes[offset + 1]);
    }
    return static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << 8U));
}

std::uint32_t readU32(const std::vector<std::uint8_t> &bytes,
                      std::size_t offset,
                      bool bigEndian)
{
    require(offset + 4 <= bytes.size(), "u32 fixture read must stay in bounds");
    std::uint32_t value = 0;
    if (bigEndian) {
        for (int i = 0; i < 4; ++i) {
            value = (value << 8U) | bytes[offset + static_cast<std::size_t>(i)];
        }
    } else {
        for (int i = 3; i >= 0; --i) {
            value = (value << 8U) | bytes[offset + static_cast<std::size_t>(i)];
        }
    }
    return value;
}

std::vector<std::uint8_t> snapshot(const PkMemoryStream &stream)
{
    const auto *first = reinterpret_cast<const std::uint8_t *>(stream.data());
    return std::vector<std::uint8_t>(first, first + stream.size());
}

void verifyResourceRecordFixture()
{
    static const std::array<std::uint8_t, 16> expected = {
        '8', 'B', 'I', 'M',
        0x03, 0xf0,
        0x00, 0x00,
        0x00, 0x00, 0x00, 0x04,
        0x10, 0x20, 0x30, 0x40,
    };

    PSDResourceBlock block;
    block.identifier = KisTiffPsdResourceRecord::CAPTION;
    block.name = PkString();
    block.dataSize = 4;
    block.data = PkByteArray("\x10\x20\x30\x40", 4);

    KisTiffPsdResourceRecord writer;
    writer.resources[KisTiffPsdResourceRecord::CAPTION] = &block;

    PkMemoryStream encoded;
    require(encoded.open(PkStream::ReadWrite), "resource output stream must open");
    require(writer.write(encoded), "resource record must serialize");
    const std::vector<std::uint8_t> bytes = snapshot(encoded);
    if (bytes.size() != expected.size() ||
        !std::equal(bytes.begin(), bytes.end(), expected.begin())) {
        std::cerr << "resource bytes (" << bytes.size() << "):";
        for (const std::uint8_t byte : bytes) {
            std::cerr << ' ' << std::hex << static_cast<unsigned int>(byte);
        }
        std::cerr << std::dec << '\n';
    }
    require(bytes.size() == expected.size() &&
                std::equal(bytes.begin(), bytes.end(), expected.begin()),
            "resource record bytes must match the independent big-endian fixture");

    require(encoded.seek(0), "resource fixture stream must rewind");
    KisTiffPsdResourceRecord reader;
    require(reader.read(encoded), "resource fixture must deserialize");
    PSDResourceBlock *decoded = reader.resources.value(KisTiffPsdResourceRecord::CAPTION, nullptr);
    require(decoded && decoded->identifier == KisTiffPsdResourceRecord::CAPTION &&
                decoded->name.isEmpty() && decoded->dataSize == 4 &&
                decoded->data == PkByteArray("\x10\x20\x30\x40", 4),
            "resource round trip must preserve id, name, size, and payload bytes");
    delete decoded;
    reader.resources.clear();
}

const KoColorSpace *rgbSpaceForDepth(std::uint16_t depth)
{
    KoColorSpaceRegistry *registry = KoColorSpaceRegistry::instance();
    const PkString profile = depth == 8
        ? PkString("sRGB-elle-V2-srgbtrc")
        : PkString("sRGB-elle-V2-g10");
    if (depth == 8) {
        return registry->colorSpace(RGBAColorModelID.id(),
                                    Integer8BitsColorDepthID.id(),
                                    profile);
    }
    if (depth == 16) {
        return registry->colorSpace(RGBAColorModelID.id(),
                                    Integer16BitsColorDepthID.id(),
                                    profile);
    }
    return registry->colorSpace(RGBAColorModelID.id(),
                                Float32BitsColorDepthID.id(),
                                profile);
}

KisImageSP makeOnePixelImage(std::uint16_t depth)
{
    const KoColorSpace *colorSpace = rgbSpaceForDepth(depth);
    require(colorSpace != nullptr, "RGB color space for probe depth must exist");

    KisImageSP image = new KisImage(nullptr, 2, 1, colorSpace, "TIFF PSD record probe");
    KisPaintLayerSP layer = new KisPaintLayer(image, "fixture", 255);

    KoColor color(colorSpace);
    PkVector<float> channels;
    channels << 0.125f << 0.375f << 0.625f << 1.0f;
    colorSpace->fromNormalisedChannelsValue(color.data(), channels);
    layer->paintDevice()->setPixel(0, 0, color);
    require(image->addNode(layer, image->root()), "probe layer must attach to image root");
    image->waitForDone();
    return image;
}

void verifyLayerRecordRoundTrip(bool bigEndian, std::uint16_t depth)
{
    constexpr std::size_t envelopeSize = 48;
    constexpr std::size_t layerCountOffset = envelopeSize;
    constexpr std::size_t boundsOffset = layerCountOffset + 2;
    constexpr std::size_t channelCountOffset = boundsOffset + 16;
    constexpr std::size_t channelInfoOffset = channelCountOffset + 2;
    constexpr std::size_t channelInfoSize = 6;
    static const char photoshopSignature[] = "Adobe Photoshop Document Data Block";

    KisImageSP image = makeOnePixelImage(depth);
    KisTiffPsdLayerRecord writer(bigEndian, 2, 1, depth, 4, PHOTOMETRIC_RGB, true);
    PkMemoryStream encoded;
    require(encoded.open(PkStream::ReadWrite), "layer output stream must open");
    require(writer.write(encoded, image->root(), psd_compression_type::RLE),
            "layer record must serialize");

    const std::vector<std::uint8_t> bytes = snapshot(encoded);
    const char *layerEnvelope = bigEndian ? "8BIMLayr" : "MIB8ryaL";
    require(bytes.size() > envelopeSize &&
                std::memcmp(bytes.data(), photoshopSignature, sizeof(photoshopSignature)) == 0,
            "layer record must start with the literal Photoshop TIFF signature and NUL");
    if (std::memcmp(bytes.data() + sizeof(photoshopSignature), layerEnvelope, 8) != 0) {
        std::cerr << "layer envelope bytes:";
        for (std::size_t i = sizeof(photoshopSignature);
             i < sizeof(photoshopSignature) + 8;
             ++i) {
            std::cerr << ' ' << std::hex << static_cast<unsigned int>(bytes[i]);
        }
        std::cerr << std::dec << '\n';
    }
    require(std::memcmp(bytes.data() + sizeof(photoshopSignature), layerEnvelope, 8) == 0,
            "layer record must contain the byte-ordered 8BIM/Layr envelope");

    const std::uint32_t payloadSize =
        readU32(bytes, sizeof(photoshopSignature) + 8, bigEndian);
    require(payloadSize > 0 && payloadSize <= bytes.size() - envelopeSize && payloadSize % 4 == 0,
            "layer payload size must use container byte order and four-byte padding");
    require(readU16(bytes, layerCountOffset, bigEndian) == 1,
            "layer payload must encode one layer in container byte order");
    require(readU32(bytes, boundsOffset, bigEndian) == 0 &&
                readU32(bytes, boundsOffset + 4, bigEndian) == 0 &&
                readU32(bytes, boundsOffset + 8, bigEndian) == 1 &&
                readU32(bytes, boundsOffset + 12, bigEndian) == 1,
            "layer bounds must preserve the one-pixel fixture geometry");
    require(readU16(bytes, channelCountOffset, bigEndian) == 4,
            "RGB layer record must encode alpha plus three color channels");
    std::array<std::uint32_t, 4> channelLengths{};
    for (std::size_t channel = 0; channel < 4; ++channel) {
        channelLengths[channel] =
            readU32(bytes, channelInfoOffset + channel * channelInfoSize + 2, bigEndian);
        require(channelLengths[channel] > 4,
                "RLE channel must contain a nonempty row payload");
    }

    constexpr std::size_t blendAndFlagsSize = 12;
    const std::size_t extraDataLengthOffset =
        channelInfoOffset + 4 * channelInfoSize + blendAndFlagsSize;
    const std::size_t pixelDataOffset =
        extraDataLengthOffset + 4 + readU32(bytes, extraDataLengthOffset, bigEndian);
    require(pixelDataOffset <= bytes.size(),
            "layer extra-data length must end within the serialized fixture");
    std::size_t channelOffset = pixelDataOffset;
    for (const std::uint32_t channelLength : channelLengths) {
        require(channelOffset + channelLength <= bytes.size(),
                "channel block length must stay within the serialized fixture");
        require(readU16(bytes, channelOffset, bigEndian) ==
                    static_cast<std::uint16_t>(psd_compression_type::RLE),
                "channel block must encode the requested RLE selector");
        require(readU16(bytes, channelOffset + 2, bigEndian) == channelLength - 4,
                "single-row RLE table must describe the complete compressed payload");
        channelOffset += channelLength;
    }

    require(encoded.seek(0), "layer fixture stream must rewind");
    KisTiffPsdLayerRecord reader(bigEndian, 2, 1, depth, 4, PHOTOMETRIC_RGB, true);
    require(reader.read(encoded) && reader.valid(), "layer record must deserialize");
    const std::shared_ptr<PSDLayerMaskSection> record = reader.record();
    require(record && record->nLayers == 1 && record->layers.size() == 1,
            "layer round trip must preserve the layer count");
    const PSDLayerRecord *decoded = record->layers.first();
    require(decoded->top == 0 && decoded->left == 0 && decoded->bottom == 1 &&
                decoded->right == 1 && decoded->nChannels == 4 &&
                decoded->channelInfoRecords.size() == 4 && reader.channelDepth() == depth,
            "layer round trip must preserve geometry, channels, and depth");
}

} // namespace

int main()
{
    registerLcmsEngine();
    verifyResourceRecordFixture();
    for (const std::uint16_t depth : {std::uint16_t(8), std::uint16_t(16), std::uint16_t(32)}) {
        verifyLayerRecordRoundTrip(true, depth);
        verifyLayerRecordRoundTrip(false, depth);
    }
    return 0;
}
