#include "../qgiflibhandler.h"
#include "gif_multiframe_fixture.h"

#include <PkRgb.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

namespace
{

class MemoryStream final : public PkStream
{
public:
    MemoryStream() = default;
    explicit MemoryStream(std::vector<char> bytes)
        : m_bytes(std::move(bytes))
    {
    }

    const std::vector<char> &bytes() const { return m_bytes; }
    pk_int64 size() const override { return static_cast<pk_int64>(m_bytes.size()); }

protected:
    pk_int64 readData(char *data, pk_int64 maximum) override
    {
        const auto available = std::max<pk_int64>(0, size() - pos());
        const auto count = std::min(maximum, available);
        if (count > 0) {
            std::memcpy(data, m_bytes.data() + pos(), static_cast<std::size_t>(count));
        }
        return count;
    }

    pk_int64 writeData(const char *data, pk_int64 count) override
    {
        if (count < 0) {
            return -1;
        }
        const auto end = pos() + count;
        if (end > size()) {
            m_bytes.resize(static_cast<std::size_t>(end));
        }
        std::memcpy(m_bytes.data() + pos(), data, static_cast<std::size_t>(count));
        return count;
    }

private:
    std::vector<char> m_bytes;
};

class ShortWriteStream final : public PkStream
{
public:
    explicit ShortWriteStream(pk_int64 limit)
        : m_limit(limit)
    {
    }

    pk_int64 size() const override { return pos(); }

protected:
    pk_int64 readData(char *, pk_int64) override { return -1; }
    pk_int64 writeData(const char *, pk_int64 count) override
    {
        const pk_int64 available = std::max<pk_int64>(0, m_limit - pos());
        return std::min(count, available);
    }

private:
    pk_int64 m_limit;
};

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main()
{
    PkImage source(3, 1, PkImage::Format_ARGB32);
    source.setPixel(0, 0, pkRgba(255, 0, 0, 255));
    source.setPixel(1, 0, pkRgba(0, 0, 1, 255));
    source.setPixel(2, 0, pkRgba(0, 0, 255, 0));

    MemoryStream encoded;
    require(encoded.open(PkStream::ReadWrite), "memory stream must open");
    require(GifLibCodec(&encoded).write(source), "giflib must encode a real image");
    require(encoded.bytes().size() > 6, "encoded GIF must contain payload");

    MemoryStream input(encoded.bytes());
    require(input.open(PkStream::ReadOnly), "input stream must open");
    GifLibCodec decoder(&input);
    require(decoder.canRead(), "GIF signature must be detected without consuming it");

    PkImage decoded;
    require(decoder.read(&decoded), "giflib must decode its output");
    require(decoded.width() == 3 && decoded.height() == 1, "GIF dimensions must round-trip");
    require(pkAlpha(decoded.pixel(0, 0)) == 255, "opaque pixel alpha must round-trip");
    require(pkBlue(decoded.pixel(1, 0)) > 0 && pkAlpha(decoded.pixel(1, 0)) == 255,
            "opaque dark blue must not collide with the reserved transparent index");
    require(pkAlpha(decoded.pixel(2, 0)) == 0, "transparent pixel alpha must round-trip");

    const std::vector<char> multiImageBytes = GifMultiframeFixture::create(DISPOSE_BACKGROUND);
    require(!multiImageBytes.empty(), "two-image GIF fixture must encode");
    require(GifMultiframeFixture::hasExpectedStructure(multiImageBytes),
            "fixture must contain two descriptors with offset, interlace, local palette, transparency, and disposal");
    GifTestMemoryStream multiImageInput(multiImageBytes);
    require(multiImageInput.open(PkStream::ReadOnly), "two-image input stream must open");
    PkImage multiImage;
    require(GifLibCodec(&multiImageInput).read(&multiImage),
            "giflib must accept a second image descriptor");
    require(multiImage.width() == 5 && multiImage.height() == 8,
            "multi-image GIF logical screen dimensions must be preserved");
    require(pkAlpha(multiImage.pixel(0, 0)) == 0 && pkAlpha(multiImage.pixel(4, 7)) == 0,
            "the selected frame background must use its transparent local palette entry");
    const uint8_t expectedRed[] = {0, 255, 255, 255, 255, 0, 0, 255};
    const uint8_t expectedGreen[] = {0, 255, 0, 0, 255, 0, 0, 255};
    const uint8_t expectedBlue[] = {255, 0, 255, 255, 0, 255, 255, 0};
    for (int y = 0; y < 8; ++y) {
        const auto pixel = multiImage.pixel(2, y);
        require(pkAlpha(pixel) == 255 && pkRed(pixel) == expectedRed[y] &&
                    pkGreen(pixel) == expectedGreen[y] && pkBlue(pixel) == expectedBlue[y],
                "interlaced rows must be restored in logical row order");
    }

    const std::vector<char> previousBytes = GifMultiframeFixture::create(DISPOSE_PREVIOUS);
    require(!previousBytes.empty() && GifMultiframeFixture::hasDisposal(previousBytes, DISPOSE_PREVIOUS),
            "paired fixture must differ only in disposal mode");
    GifTestMemoryStream previousInput(previousBytes);
    require(previousInput.open(PkStream::ReadOnly), "paired input stream must open");
    PkImage previousImage;
    require(GifLibCodec(&previousInput).read(&previousImage), "paired disposal GIF must decode");
    require(previousImage.width() == multiImage.width() && previousImage.height() == multiImage.height(),
            "paired disposal GIF dimensions must match");
    for (int y = 0; y < multiImage.height(); ++y) {
        for (int x = 0; x < multiImage.width(); ++x) {
            require(previousImage.pixel(x, y) == multiImage.pixel(x, y),
                    "disposal mode must not alter the selected-raster result");
        }
    }

    ShortWriteStream shortOutput(static_cast<PkStream::pk_int64>(encoded.bytes().size() - 1));
    require(shortOutput.open(PkStream::WriteOnly), "short output stream must open");
    require(!GifLibCodec(&shortOutput).write(source),
            "GIF trailer short-write must fail explicit finalization");
    return 0;
}
