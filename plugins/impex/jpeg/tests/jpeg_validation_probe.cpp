#include "../jpeg_validation.h"
#include "../jpeg_decompress_guard.h"
#include "../kis_jpeg_destination.h"
#include "../kis_jpeg_source.h"

#include <PkStream.h>
#include <jerror.h>

#include <cstdlib>
#include <iostream>
#include <limits>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {
class ProbeStream final : public PkStream
{
public:
    ProbeStream(const char *data, std::size_t size, bool shortWrite = false)
        : m_shortWrite(shortWrite)
    {
        if (data && size) m_data.assign(data, data + size);
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
        m_data.insert(m_data.end(), data, data + count);
        return count;
    }

private:
    std::vector<char> m_data;
    bool m_shortWrite;
};

void throwJpegError(j_common_ptr cinfo)
{
    char message[JMSG_LENGTH_MAX] = {};
    (*cinfo->err->format_message)(cinfo, message);
    throw std::runtime_error(message);
}

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
    require(jpegCheckedBufferSize(640, 480, 4, bytes) && bytes == 1228800,
            "valid JPEG output dimensions must produce the exact byte count");
    require(!jpegCheckedBufferSize(std::numeric_limits<std::size_t>::max(), 2, 4, bytes),
            "JPEG output allocation multiplication must reject overflow");
    require(jpegMarkerPayloadAvailable(29, 29),
            "JPEG marker parser must accept an exactly sized fixed header");
    require(!jpegMarkerPayloadAvailable(28, 29),
            "truncated JPEG marker headers must be rejected before subtraction");

    jpeg_decompress_struct decompressor{};
    jpeg_error_mgr errorManager{};
    decompressor.err = jpeg_std_error(&errorManager);
    errorManager.error_exit = throwJpegError;
    jpeg_create_decompress(&decompressor);
    try {
        KisJPEGDecompressGuard cleanup(&decompressor);
        ProbeStream empty(nullptr, 0);
        KisJPEGSource::setSource(&decompressor, &empty);
        jpeg_read_header(&decompressor, TRUE);
        require(false, "empty JPEG input must fault through the real source callback");
    } catch (const std::runtime_error &) {
    }
    require(decompressor.mem == nullptr,
            "JPEG fault unwinding must destroy the decompressor through RAII");

    jpeg_compress_struct compressor{};
    compressor.err = jpeg_std_error(&errorManager);
    errorManager.error_exit = throwJpegError;
    jpeg_create_compress(&compressor);
    ProbeStream shortOutput(nullptr, 0, true);
    KisJPEGDestination::setDestination(&compressor, &shortOutput);
    compressor.image_width = 1;
    compressor.image_height = 1;
    compressor.input_components = 3;
    compressor.in_color_space = JCS_RGB;
    jpeg_set_defaults(&compressor);
    bool destinationFailed = false;
    try {
        jpeg_start_compress(&compressor, TRUE);
        JSAMPLE pixel[3] = {0, 0, 0};
        JSAMPROW row = pixel;
        jpeg_write_scanlines(&compressor, &row, 1);
        jpeg_finish_compress(&compressor);
    } catch (const std::runtime_error &) {
        destinationFailed = true;
    }
    jpeg_destroy_compress(&compressor);
    require(destinationFailed,
            "JPEG destination callback must propagate a short PkStream write");
    return 0;
}
