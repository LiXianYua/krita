#pragma once

#include "PkImageData.h"
#include "../container/PkArrayData.h"
#include "../geometry/PkSize.h"
#include "../geometry/PkRect.h"

class PkImage
{
public:
    enum Format {
        Format_Invalid,
        Format_Mono,
        Format_MonoLSB,
        Format_Indexed8,
        Format_RGB32,
        Format_ARGB32,
        Format_ARGB32_Premultiplied,
        Format_RGB16,
        Format_ARGB8565_Premultiplied,
        Format_RGB666,
        Format_ARGB6666_Premultiplied,
        Format_RGB555,
        Format_ARGB8555_Premultiplied,
        Format_RGB888,
        Format_RGB444,
        Format_ARGB4444_Premultiplied,
        Format_RGBX8888,
        Format_RGBA8888,
        Format_RGBA8888_Premultiplied,
        Format_BGR30,
        Format_A2BGR30_Premultiplied,
        Format_RGB30,
        Format_A2RGB30_Premultiplied,
        Format_Alpha8,
        Format_Grayscale8,
        Format_RGBX64,
        Format_RGBA64,
        Format_RGBA64_Premultiplied,
        Format_Grayscale16,
        Format_BGR888
    };

    PkImage();
    PkImage(int width, int height, Format format);
    PkImage(const PkSize &size, Format format);

    PkImage(const PkImage &other) = default;
    PkImage(PkImage &&other) noexcept = default;
    PkImage &operator=(const PkImage &other) = default;
    PkImage &operator=(PkImage &&other) noexcept = default;
    ~PkImage() = default;

    Format format() const;

    int width() const;
    int height() const;
    PkSize size() const;
    PkRect rect() const;

    bool isNull() const;
    int depth() const;
    int bytesPerLine() const;
    long long sizeInBytes() const;
    int colorCount() const;

private:
    PkArrayData<PkImageData> m_d;
};
