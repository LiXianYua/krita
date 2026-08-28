#include "../qgiflibhandler.h"

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
    PkImage source(2, 1, PkImage::Format_ARGB32);
    source.setPixel(0, 0, pkRgba(255, 0, 0, 255));
    source.setPixel(1, 0, pkRgba(0, 0, 255, 0));

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
    require(decoded.width() == 2 && decoded.height() == 1, "GIF dimensions must round-trip");
    require(pkAlpha(decoded.pixel(0, 0)) == 255, "opaque pixel alpha must round-trip");
    require(pkAlpha(decoded.pixel(1, 0)) == 0, "transparent pixel alpha must round-trip");
    return 0;
}
