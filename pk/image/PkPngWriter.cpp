#include "PkPngWriter.h"

#include "PkImagePremultiply.h"

#include <png.h>

#include <csetjmp>
#include <cstring>
#include <new>
#include <stdexcept>
#include <vector>

namespace
{

// libpng 的向量输出回调：把编码后的字节追加进 std::vector。
// 用 png_get_io_ptr 取回目标向量（对应 PkPngReader.cpp 的 MemoryReader 手法）。
void appendBytes(png_structp codec, png_bytep data, png_size_t count)
{
    auto *out = static_cast<std::vector<uint8_t> *>(png_get_io_ptr(codec));
    out->insert(out->end(), data, data + count);
}

void flushNoop(png_structp)
{
}

// 源格式 → 是否本写侧支持，以及像素读回后要不要反预乘。
struct SourceTraits
{
    bool supported = false;
    bool premultiplied = false;
};

SourceTraits sourceTraits(PkImage::Format format)
{
    switch (format) {
    case PkImage::Format_ARGB32:
    case PkImage::Format_RGB32:
    case PkImage::Format_RGBA8888:
    case PkImage::Format_Grayscale8:
    case PkImage::Format_Indexed8:
    case PkImage::Format_Mono:
    case PkImage::Format_MonoLSB:
        return {true, false};
    case PkImage::Format_ARGB32_Premultiplied:
        return {true, true};
    default:
        // 其余格式 PkImage 目前没有像素级读（rawPixelArgb 的 default 分支），
        // 写侧不猜语义，直接判不支持。
        return {false, false};
    }
}

} // namespace

std::vector<uint8_t> PkPngWriter::write(const PkImage &image, int /*quality*/)
{
    std::vector<uint8_t> output;
    if (image.isNull()) {
        return output;
    }

    const SourceTraits traits = sourceTraits(image.format());
    if (!traits.supported) {
        return output;
    }

    const int width = image.width();
    const int height = image.height();

    // 先把源图折算成非预乘 ARGB32 的连续缓冲，并判断是否需要 alpha 通道。
    try {
        std::vector<uint32_t> argb(static_cast<size_t>(width) * static_cast<size_t>(height));
        bool anyNonOpaque = false;
        size_t index = 0;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x, ++index) {
                uint32_t value = image.pixel(x, y);
                if (traits.premultiplied) {
                    value = pkimage_detail::unpremultiply(value);
                }
                argb[index] = value;
                if ((value >> 24) != 0xffu) {
                    anyNonOpaque = true;
                }
            }
        }

        const int channels = anyNonOpaque ? 4 : 3;
        const int colorType = anyNonOpaque ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB;

        std::vector<uint8_t> rows(static_cast<size_t>(channels) * static_cast<size_t>(width) *
                                  static_cast<size_t>(height));
        std::vector<png_bytep> rowPointers(static_cast<size_t>(height));
        for (int y = 0; y < height; ++y) {
            uint8_t *row = rows.data() + static_cast<size_t>(y) * static_cast<size_t>(channels) *
                                              static_cast<size_t>(width);
            rowPointers[static_cast<size_t>(y)] = row;
            for (int x = 0; x < width; ++x) {
                const uint32_t value = argb[static_cast<size_t>(y) * static_cast<size_t>(width) +
                                            static_cast<size_t>(x)];
                row[static_cast<size_t>(x) * channels + 0] = pkimage_detail::pmRed(value);
                row[static_cast<size_t>(x) * channels + 1] = pkimage_detail::pmGreen(value);
                row[static_cast<size_t>(x) * channels + 2] = pkimage_detail::pmBlue(value);
                if (channels == 4) {
                    row[static_cast<size_t>(x) * channels + 3] = pkimage_detail::pmAlpha(value);
                }
            }
        }

        png_structp codec = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!codec) {
            return output;
        }
        png_infop info = png_create_info_struct(codec);
        if (!info) {
            png_destroy_write_struct(&codec, nullptr);
            return output;
        }

        if (setjmp(png_jmpbuf(codec))) {
            // libpng 用 longjmp 报告错误（与读侧同一套手法）：清干净再退回失败。
            png_destroy_write_struct(&codec, &info);
            output.clear();
            return output;
        }

        png_set_write_fn(codec, &output, appendBytes, flushNoop);
        png_set_IHDR(codec, info,
                     static_cast<png_uint_32>(width), static_cast<png_uint_32>(height),
                     8, colorType, PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
        png_write_info(codec, info);
        png_write_image(codec, rowPointers.data());
        png_write_end(codec, nullptr);
        png_destroy_write_struct(&codec, &info);
        return output;
    } catch (const std::bad_alloc &) {
        output.clear();
        return output;
    } catch (const std::length_error &) {
        output.clear();
        return output;
    }
}
