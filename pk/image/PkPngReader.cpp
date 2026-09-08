#include "PkPngReader.h"

#include "PkImageFileDecoder.h"

#include <png.h>

#include <algorithm>
#include <csetjmp>
#include <cstring>
#include <new>
#include <stdexcept>

namespace
{

struct MemoryReader
{
    const uint8_t *data = nullptr;
    std::size_t size = 0;
    std::size_t offset = 0;
};

void readBytes(png_structp codec, png_bytep output, png_size_t count)
{
    auto *reader = static_cast<MemoryReader *>(png_get_io_ptr(codec));
    if (!reader || count > reader->size - std::min(reader->offset, reader->size)) {
        png_error(codec, "truncated PNG input");
        return;
    }
    std::memcpy(output, reader->data + reader->offset, count);
    reader->offset += count;
}

} // namespace

PkPngReadResult PkPngReader::read(const uint8_t *data, std::size_t size)
{
    PkPngReadResult result;
    if (!data || size < 8 || png_sig_cmp(data, 0, 8) != 0) {
        return result;
    }

    result.image = PkImageFileDecoder::decode(data, size, ".png");
    if (result.image.isNull()) {
        return result;
    }

    png_structp codec = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!codec) {
        result.image = PkImage();
        return result;
    }
    png_infop info = png_create_info_struct(codec);
    if (!info) {
        png_destroy_read_struct(&codec, nullptr, nullptr);
        result.image = PkImage();
        return result;
    }

    MemoryReader reader {data, size, 0};
    if (setjmp(png_jmpbuf(codec))) {
        png_destroy_read_struct(&codec, &info, nullptr);
        result.image = PkImage();
        result.text.clear();
        return result;
    }

    png_set_read_fn(codec, &reader, readBytes);
    png_read_info(codec, info);

    png_textp entries = nullptr;
    int count = 0;
    if (png_get_text(codec, info, &entries, &count) > 0) {
        try {
            for (int i = 0; i < count; ++i) {
                if (!entries[i].key || !entries[i].text) {
                    continue;
                }
                const std::size_t length = entries[i].text_length > 0
                    ? entries[i].text_length
                    : std::strlen(entries[i].text);
                result.text[entries[i].key] = std::string(entries[i].text, length);
            }
        } catch (const std::bad_alloc &) {
            result.image = PkImage();
            result.text.clear();
        } catch (const std::length_error &) {
            result.image = PkImage();
            result.text.clear();
        }
    }

    png_destroy_read_struct(&codec, &info, nullptr);
    return result;
}
