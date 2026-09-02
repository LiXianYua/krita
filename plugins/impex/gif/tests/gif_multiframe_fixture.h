#pragma once

#include <PkStream.h>

#include <gif_lib.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

class GifTestMemoryStream final : public PkStream
{
public:
    GifTestMemoryStream() = default;
    explicit GifTestMemoryStream(std::vector<char> bytes)
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

namespace GifMultiframeFixture
{

struct GifWriteCloser
{
    void operator()(GifFileType *gif) const
    {
        if (gif) {
            int error = 0;
            EGifCloseFile(gif, &error);
        }
    }
};

struct GifReadCloser
{
    void operator()(GifFileType *gif) const
    {
        if (gif) {
            int error = 0;
            DGifCloseFile(gif, &error);
        }
    }
};

inline int appendBytes(GifFileType *gif, const GifByteType *data, int count)
{
    auto *bytes = static_cast<std::vector<char> *>(gif->UserData);
    bytes->insert(bytes->end(), reinterpret_cast<const char *>(data),
                  reinterpret_cast<const char *>(data) + count);
    return count;
}

inline int readBytes(GifFileType *gif, GifByteType *data, int count)
{
    auto *stream = static_cast<GifTestMemoryStream *>(gif->UserData);
    const auto actual = stream->read(reinterpret_cast<char *>(data), count);
    return actual > 0 ? static_cast<int>(actual) : 0;
}

inline std::vector<char> create(int disposal = DISPOSE_BACKGROUND)
{
    std::vector<char> bytes;
    int error = 0;
    std::unique_ptr<GifFileType, GifWriteCloser> gif(EGifOpen(&bytes, appendBytes, &error));
    if (!gif) {
        return {};
    }

    std::unique_ptr<ColorMapObject, decltype(&GifFreeMapObject)> globalMap(
        GifMakeMapObject(2, nullptr), &GifFreeMapObject);
    std::unique_ptr<ColorMapObject, decltype(&GifFreeMapObject)> localMap(
        GifMakeMapObject(4, nullptr), &GifFreeMapObject);
    if (!globalMap || !localMap) {
        return {};
    }

    globalMap->Colors[0] = GifColorType{0, 0, 0};
    globalMap->Colors[1] = GifColorType{255, 0, 0};
    localMap->Colors[0] = GifColorType{0, 255, 0};
    localMap->Colors[1] = GifColorType{0, 0, 255};
    localMap->Colors[2] = GifColorType{255, 255, 0};
    localMap->Colors[3] = GifColorType{255, 0, 255};

    if (EGifPutScreenDesc(gif.get(), 5, 8, GifBitSize(2), 0, globalMap.get()) == GIF_ERROR ||
        EGifPutImageDesc(gif.get(), 0, 0, 5, 8, false, nullptr) == GIF_ERROR) {
        return {};
    }
    GifPixelType firstRow[] = {1, 1, 1, 1, 1};
    for (int row = 0; row < 8; ++row) {
        if (EGifPutLine(gif.get(), firstRow, 5) == GIF_ERROR) {
            return {};
        }
    }

    GraphicsControlBlock control = {disposal, false, 0, 0};
    GifByteType extension[4] = {};
    EGifGCBToExtension(&control, extension);
    if (EGifPutExtension(gif.get(), GRAPHICS_EXT_FUNC_CODE, sizeof(extension), extension) == GIF_ERROR ||
        EGifPutImageDesc(gif.get(), 2, 0, 1, 8, true, localMap.get()) == GIF_ERROR) {
        return {};
    }

    const GifPixelType rows[] = {1, 2, 3, 1, 2, 3, 1, 2};
    for (GifPixelType row : rows) {
        if (EGifPutLine(gif.get(), &row, 1) == GIF_ERROR) {
            return {};
        }
    }

    const int closeResult = EGifCloseFile(gif.release(), &error);
    return closeResult == GIF_ERROR ? std::vector<char>() : bytes;
}

inline bool hasExpectedStructure(const std::vector<char> &bytes)
{
    GifTestMemoryStream stream(bytes);
    if (!stream.open(PkStream::ReadOnly)) {
        return false;
    }
    int error = 0;
    std::unique_ptr<GifFileType, GifReadCloser> gif(DGifOpen(&stream, readBytes, &error));
    if (!gif || DGifSlurp(gif.get()) == GIF_ERROR || gif->ImageCount != 2 ||
        gif->SWidth != 5 || gif->SHeight != 8) {
        return false;
    }

    const GifImageDesc &second = gif->SavedImages[1].ImageDesc;
    GraphicsControlBlock control = {};
    return second.Left == 2 && second.Top == 0 && second.Width == 1 && second.Height == 8 &&
        second.Interlace && second.ColorMap && second.ColorMap->ColorCount == 4 &&
        DGifSavedExtensionToGCB(gif.get(), 1, &control) != GIF_ERROR &&
        control.DisposalMode == DISPOSE_BACKGROUND && control.TransparentColor == 0;
}

inline bool hasDisposal(const std::vector<char> &bytes, int expectedDisposal)
{
    GifTestMemoryStream stream(bytes);
    if (!stream.open(PkStream::ReadOnly)) {
        return false;
    }
    int error = 0;
    std::unique_ptr<GifFileType, GifReadCloser> gif(DGifOpen(&stream, readBytes, &error));
    if (!gif || DGifSlurp(gif.get()) == GIF_ERROR || gif->ImageCount != 2) {
        return false;
    }
    GraphicsControlBlock control = {};
    return DGifSavedExtensionToGCB(gif.get(), 1, &control) != GIF_ERROR &&
        control.DisposalMode == expectedDisposal;
}

} // namespace GifMultiframeFixture
