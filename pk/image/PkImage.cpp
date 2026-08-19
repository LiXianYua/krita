#include "PkImage.h"

namespace {

int depthTable(PkImage::Format format)
{
    switch (format) {
    case PkImage::Format_Invalid:                  return 0;
    case PkImage::Format_Mono:                      return 1;
    case PkImage::Format_MonoLSB:                   return 1;
    case PkImage::Format_Indexed8:                  return 8;
    case PkImage::Format_RGB32:                      return 32;
    case PkImage::Format_ARGB32:                     return 32;
    case PkImage::Format_ARGB32_Premultiplied:       return 32;
    case PkImage::Format_RGB16:                      return 16;
    case PkImage::Format_ARGB8565_Premultiplied:     return 24;
    case PkImage::Format_RGB666:                     return 24;
    case PkImage::Format_ARGB6666_Premultiplied:     return 24;
    case PkImage::Format_RGB555:                     return 16;
    case PkImage::Format_ARGB8555_Premultiplied:     return 24;
    case PkImage::Format_RGB888:                     return 24;
    case PkImage::Format_RGB444:                     return 16;
    case PkImage::Format_ARGB4444_Premultiplied:     return 16;
    case PkImage::Format_RGBX8888:                   return 32;
    case PkImage::Format_RGBA8888:                   return 32;
    case PkImage::Format_RGBA8888_Premultiplied:     return 32;
    case PkImage::Format_BGR30:                       return 32;
    case PkImage::Format_A2BGR30_Premultiplied:       return 32;
    case PkImage::Format_RGB30:                       return 32;
    case PkImage::Format_A2RGB30_Premultiplied:       return 32;
    case PkImage::Format_Alpha8:                       return 8;
    case PkImage::Format_Grayscale8:                    return 8;
    case PkImage::Format_RGBX64:                       return 64;
    case PkImage::Format_RGBA64:                       return 64;
    case PkImage::Format_RGBA64_Premultiplied:          return 64;
    case PkImage::Format_Grayscale16:                    return 16;
    case PkImage::Format_BGR888:                        return 24;
    }
    return 0;
}

} // namespace

PkImage::PkImage() = default;

PkImage::PkImage(int width, int height, Format format)
{
    if (width <= 0 || height <= 0 || format == Format_Invalid) {
        // 保持 m_d 默认哨兵，isNull()==true，不分配像素。
        return;
    }
    PkImageData data;
    data.width = width;
    data.height = height;
    data.format = static_cast<int>(format);
    int depthBits = depthTable(format);
    data.bytesPerLine = ((width * depthBits + 31) / 32) * 4;
    data.pixels.resize(static_cast<size_t>(data.bytesPerLine) * static_cast<size_t>(height));
    m_d = PkArrayData<PkImageData>(std::move(data));
}

PkImage::PkImage(const PkSize &size, Format format)
    : PkImage(size.width(), size.height(), format)
{
}

PkImage::Format PkImage::format() const
{
    return static_cast<Format>(m_d.PkConst().format);
}

int PkImage::width() const { return m_d.PkConst().width; }
int PkImage::height() const { return m_d.PkConst().height; }
PkSize PkImage::size() const { return PkSize(width(), height()); }
PkRect PkImage::rect() const { return PkRect(0, 0, width(), height()); }

bool PkImage::isNull() const
{
    return width() <= 0 || height() <= 0 || format() == Format_Invalid;
}

int PkImage::depth() const { return depthTable(format()); }

int PkImage::bytesPerLine() const { return m_d.PkConst().bytesPerLine; }

long long PkImage::sizeInBytes() const
{
    return static_cast<long long>(bytesPerLine()) * static_cast<long long>(height());
}

int PkImage::colorCount() const
{
    Format f = format();
    if (f == Format_Mono || f == Format_MonoLSB) {
        return 2;
    }
    if (f == Format_Indexed8) {
        return static_cast<int>(m_d.PkConst().colorTable.size());
    }
    return 0;
}
