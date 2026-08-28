#include "../tiff_validation.h"
#include "../tiff_stream_adapter.h"

#include <PkStream.h>

#include <cstdlib>
#include <iostream>
#include <limits>
#include <cstring>
#include <vector>

namespace {
class ProbeStream final : public PkStream
{
public:
    explicit ProbeStream(bool shortWrite = false)
        : m_shortWrite(shortWrite)
    {
        open(PkStream::ReadWrite);
    }

    pk_int64 size() const override { return static_cast<pk_int64>(m_data.size()); }

protected:
    pk_int64 readData(char *data, pk_int64 maxSize) override
    {
        const pk_int64 available = size() - pos();
        const pk_int64 count = available < maxSize ? available : maxSize;
        if (count > 0) std::memcpy(data, m_data.data() + pos(), static_cast<std::size_t>(count));
        return count;
    }
    pk_int64 writeData(const char *data, pk_int64 maxSize) override
    {
        const pk_int64 count = m_shortWrite && maxSize > 0 ? maxSize - 1 : maxSize;
        const std::size_t end = static_cast<std::size_t>(pos() + count);
        if (m_data.size() < end) m_data.resize(end);
        if (count > 0) std::memcpy(m_data.data() + pos(), data, static_cast<std::size_t>(count));
        return count;
    }

private:
    std::vector<char> m_data;
    bool m_shortWrite;
};

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
}

int main()
{
    std::size_t bytes = 0;
    require(tiffCheckedRasterSize(19, 11, 4, 2, bytes) && bytes == 1672,
            "valid TIFF raster dimensions must produce the exact byte count");
    require(!tiffCheckedRasterSize(19, 11, 0, 2, bytes),
            "zero-channel TIFF rasters must be rejected");
    require(!tiffCheckedRasterSize(std::numeric_limits<std::size_t>::max(), 2, 4, 2, bytes),
            "TIFF raster allocation multiplication must reject overflow");
    require(tiffTagPayloadAvailable(12, 12) && !tiffTagPayloadAvailable(11, 12),
            "truncated TIFF/PSD tag payloads must be rejected before reading");
    require(!tiffValidDirectoryShape(static_cast<std::uint32_t>(std::numeric_limits<int>::max()) + 1u,
                                     1, 4, 1),
            "TIFF dimensions that narrow past signed image coordinates must be rejected");
    require(!tiffValidDirectoryShape(8, 8, 3, 4),
            "TIFF extra samples cannot exceed samples per pixel");
    require(!tiffValidChunkGeometry(16, 0, 128) &&
                !tiffValidChunkGeometry(16, 16, 0),
            "TIFF zero chunk dimensions and encoded sizes must be rejected");

    ProbeStream callbackStream;
    const char payload[] = "TIFF";
    require(kisTiffStreamWrite(&callbackStream, const_cast<char *>(payload), 4) == 4,
            "TIFF write callback must execute the PkStream writer");
    require(kisTiffStreamSeek(&callbackStream, 0, SEEK_SET) == 0,
            "TIFF seek callback must execute the PkStream seeker");
    char roundTrip[4] = {};
    require(kisTiffStreamRead(&callbackStream, roundTrip, 4) == 4 &&
                std::memcmp(roundTrip, payload, 4) == 0,
            "TIFF read callback must execute the PkStream reader");
    require(kisTiffStreamSeek(&callbackStream,
                              static_cast<toff_t>(static_cast<PkStream::pk_int64>(-2)),
                              SEEK_CUR) == 2,
            "TIFF seek callback must preserve libtiff's signed negative offset bits");

    ProbeStream shortWrite(true);
    require(!kisTiffWriteExact(shortWrite, payload, 4),
            "TIFF PSD writer helper must reject a short PkStream write");
    return 0;
}
