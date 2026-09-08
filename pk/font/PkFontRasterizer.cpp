#include "PkFontRasterizer.h"

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace {

struct FtLibrary {
    FT_Library value = nullptr;
    FtLibrary() { FT_Init_FreeType(&value); }
    ~FtLibrary() { if (value) FT_Done_FreeType(value); }
};

struct FtFaceDeleter {
    void operator()(FT_Face face) const { if (face) FT_Done_Face(face); }
};

struct FcPatternDeleter {
    void operator()(FcPattern *pattern) const { if (pattern) FcPatternDestroy(pattern); }
};

std::string resolveFontFile(const PkFont &font)
{
    std::unique_ptr<FcPattern, FcPatternDeleter> pattern(FcPatternCreate());
    if (pattern) {
        const std::string family = font.family().empty() ? "sans-serif" : font.family();
        FcPatternAddString(pattern.get(), FC_FAMILY,
                           reinterpret_cast<const FcChar8 *>(family.c_str()));
        FcPatternAddInteger(pattern.get(), FC_WEIGHT,
                            font.weight() >= 75 ? FC_WEIGHT_BOLD : FC_WEIGHT_REGULAR);
        FcPatternAddInteger(pattern.get(), FC_SLANT,
                            font.italic() ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
        FcConfigSubstitute(nullptr, pattern.get(), FcMatchPattern);
        FcDefaultSubstitute(pattern.get());
        FcResult result = FcResultNoMatch;
        std::unique_ptr<FcPattern, FcPatternDeleter> match(FcFontMatch(nullptr, pattern.get(), &result));
        FcChar8 *path = nullptr;
        if (match && result == FcResultMatch &&
            FcPatternGetString(match.get(), FC_FILE, 0, &path) == FcResultMatch && path) {
            return reinterpret_cast<const char *>(path);
        }
    }

    // Deterministic fallback for stripped/headless images with no fontconfig
    // configuration. Desktop/mobile packaging should supply its own match.
    if (font.family().empty() || font.family() == "DejaVu Sans" || font.family() == "sans-serif") {
        return "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    }
    return {};
}

std::vector<std::uint32_t> codepoints(const PkString &text)
{
    std::vector<std::uint32_t> result;
    const std::u16string utf16 = text.PkToU16();
    for (std::size_t i = 0; i < utf16.size(); ++i) {
        std::uint32_t codepoint = static_cast<std::uint16_t>(utf16[i]);
        if (codepoint >= 0xd800u && codepoint <= 0xdbffu && i + 1 < utf16.size()) {
            const std::uint32_t low = static_cast<std::uint16_t>(utf16[i + 1]);
            if (low >= 0xdc00u && low <= 0xdfffu) {
                codepoint = 0x10000u + ((codepoint - 0xd800u) << 10) + (low - 0xdc00u);
                ++i;
            }
        }
        result.push_back(codepoint);
    }
    return result;
}

struct Glyph {
    int left = 0;
    int top = 0;
    int width = 0;
    int rows = 0;
    int pitch = 0;
    std::vector<unsigned char> pixels;
};

}

PkImage PkFontRasterizer::render(const PkString &text, const PkFont &font)
{
    if (text.isEmpty()) {
        PkImage empty(1, 1, PkImage::Format_ARGB32);
        empty.fill(0xffffffffu);
        return empty;
    }

    FtLibrary library;
    const std::string path = resolveFontFile(font);
    if (!library.value || path.empty()) return {};

    FT_Face rawFace = nullptr;
    if (FT_New_Face(library.value, path.c_str(), 0, &rawFace) != 0) return {};
    std::unique_ptr<std::remove_pointer_t<FT_Face>, FtFaceDeleter> face(rawFace);

    if (font.pixelSize() > 0) {
        if (FT_Set_Pixel_Sizes(face.get(), 0, static_cast<FT_UInt>(font.pixelSize())) != 0) return {};
    } else {
        const int points = font.pointSize() > 0 ? font.pointSize() : 12;
        if (FT_Set_Char_Size(face.get(), 0, points * 64, 96, 96) != 0) return {};
    }

    const int ascent = static_cast<int>((face->size->metrics.ascender + 63) / 64);
    const int descent = static_cast<int>((-face->size->metrics.descender + 63) / 64);
    const int height = std::max(1, ascent + descent);

    std::vector<Glyph> glyphs;
    int pen = 0;
    int minimumX = 0;
    int maximumX = 0;
    FT_UInt previous = 0;
    for (const std::uint32_t codepoint : codepoints(text)) {
        const FT_UInt index = FT_Get_Char_Index(face.get(), codepoint);
        if (previous && index && FT_HAS_KERNING(face.get())) {
            FT_Vector kerning {};
            FT_Get_Kerning(face.get(), previous, index, FT_KERNING_DEFAULT, &kerning);
            pen += static_cast<int>(kerning.x >> 6);
        }
        if (FT_Load_Glyph(face.get(), index, FT_LOAD_DEFAULT) != 0 ||
            FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
            previous = index;
            continue;
        }

        Glyph glyph;
        glyph.left = pen + face->glyph->bitmap_left;
        glyph.top = face->glyph->bitmap_top;
        glyph.width = static_cast<int>(face->glyph->bitmap.width);
        glyph.rows = static_cast<int>(face->glyph->bitmap.rows);
        glyph.pitch = std::abs(face->glyph->bitmap.pitch);
        glyph.pixels.resize(static_cast<std::size_t>(glyph.pitch) * static_cast<std::size_t>(glyph.rows));
        for (int row = 0; row < glyph.rows; ++row) {
            const unsigned char *source = face->glyph->bitmap.pitch >= 0
                ? face->glyph->bitmap.buffer + row * face->glyph->bitmap.pitch
                : face->glyph->bitmap.buffer + (glyph.rows - 1 - row) * glyph.pitch;
            std::copy(source, source + glyph.pitch,
                      glyph.pixels.begin() + static_cast<std::size_t>(row) * glyph.pitch);
        }
        minimumX = std::min(minimumX, glyph.left);
        maximumX = std::max(maximumX, glyph.left + glyph.width);
        pen += static_cast<int>((face->glyph->advance.x + 32) >> 6);
        maximumX = std::max(maximumX, pen);
        glyphs.push_back(std::move(glyph));
        previous = index;
    }

    const int width = std::max(1, maximumX - minimumX);
    PkImage result(width, height, PkImage::Format_ARGB32);
    result.fill(0xffffffffu);
    for (const Glyph &glyph : glyphs) {
        for (int row = 0; row < glyph.rows; ++row) {
            const int y = ascent - glyph.top + row;
            if (y < 0 || y >= height) continue;
            for (int column = 0; column < glyph.width; ++column) {
                const int x = glyph.left - minimumX + column;
                if (x < 0 || x >= width) continue;
                const unsigned coverage = glyph.pixels[static_cast<std::size_t>(row) * glyph.pitch + column];
                const unsigned previousValue = result.pixel(x, y) & 0xffu;
                const unsigned value = (previousValue * (255u - coverage) + 127u) / 255u;
                result.setPixel(x, y, 0xff000000u | (value << 16) | (value << 8) | value);
            }
        }
    }
    return result;
}
