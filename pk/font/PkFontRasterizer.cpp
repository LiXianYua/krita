#include "PkFontRasterizer.h"
#include "PkFontOutline_p.h"
#include <PkTransform.h>
#include <PkPoint.h>

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_FONT_FORMATS_H
#include <raqm.h>
#include <hb.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <memory>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace {

struct FtLibrary {
    FT_Library value = nullptr;
    FtLibrary()
    {
        if (FT_Init_FreeType(&value) == 0) {
            // QFontEngineFT 5.15 explicitly enables CFF stem darkening.
            FT_Bool noDarkening = false;
            FT_Property_Set(value, "cff", "no-stem-darkening", &noDarkening);
        }
    }
    ~FtLibrary() { if (value) FT_Done_FreeType(value); }
};

struct FtFaceDeleter {
    void operator()(FT_Face face) const { if (face) FT_Done_Face(face); }
};

struct FcPatternDeleter {
    void operator()(FcPattern *pattern) const { if (pattern) FcPatternDestroy(pattern); }
};

struct FontFile { std::string path; int index = 0; };

std::vector<FontFile> resolveFontFiles(const PkFont &font)
{
    std::vector<FontFile> result;
    std::unique_ptr<FcPattern, FcPatternDeleter> pattern(FcPatternCreate());
    if (pattern) {
        const std::string family = font.family().empty() ? "sans-serif" : font.family();
        FcPatternAddString(pattern.get(), FC_FAMILY,
                           reinterpret_cast<const FcChar8 *>(family.c_str()));
        FcPatternAddInteger(pattern.get(), FC_WEIGHT,
                            font.weight() >= 75 ? FC_WEIGHT_BOLD : FC_WEIGHT_REGULAR);
        FcPatternAddInteger(pattern.get(), FC_SLANT,
                            font.style() == PkFontStyleOblique ? FC_SLANT_OBLIQUE :
                            font.italic() ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
        FcConfigSubstitute(nullptr, pattern.get(), FcMatchPattern);
        FcDefaultSubstitute(pattern.get());
        // The existing KoFontProviderFontconfig::sortedMatches / KoFontRegistry
        // contract: ordered Fontconfig candidates, selected per missing glyph.
        FcResult matchResult = FcResultNoMatch;
        FcFontSet *matches = FcFontSort(nullptr, pattern.get(), FcTrue, nullptr, &matchResult);
        if (matches) {
            for (int i = 0; i < matches->nfont; ++i) {
                FcChar8 *path = nullptr;
                int index = 0;
                if (FcPatternGetString(matches->fonts[i], FC_FILE, 0, &path) != FcResultMatch || !path) continue;
                FcPatternGetInteger(matches->fonts[i], FC_INDEX, 0, &index);
                result.push_back({reinterpret_cast<const char *>(path), index});
            }
            FcFontSetDestroy(matches);
        }
    }
    return result;
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
    bool gammaCorrect = false;
    std::vector<unsigned char> pixels;
};

unsigned blendCoverage(unsigned previous, unsigned coverage, bool gammaCorrect)
{
    if (!gammaCorrect || coverage == 0 || coverage == 255)
        return (previous * (255u - coverage) + 127u) / 255u;
    // Qt 5.15's offscreen fontSmoothingGamma and QColorTrcLut quantization.
    struct Gamma {
        std::array<unsigned, 4081> toLinear, fromLinear;
        Gamma() {
            for (unsigned i = 0; i <= 4080; ++i) {
                toLinear[i] = std::lround(std::pow(i / 4080.0, 1.7) * 65280.0);
                fromLinear[i] = std::lround(std::pow(i / 4080.0, 1.0 / 1.7) * 65280.0);
            }
        }
    };
    static const Gamma gamma;
    unsigned linear = gamma.toLinear[previous << 4];
    linear += linear >> 8;
    const unsigned product = linear * (255u - coverage);
    linear = (product + (product >> 8) + 128u) >> 8;
    const unsigned index = (linear - (linear >> 8)) >> 4;
    return (gamma.fromLinear[index] + 128u) >> 8;
}

// The rasterized glyph coverage buffer. `stored` keeps the existing semantics:
// every cell starts at 255 (no ink) and `blendCoverage` accumulates into it.
// `(offsetX, offsetY)` places cell (0, 0) relative to the text baseline origin.
struct Raster {
    std::vector<unsigned char> stored;
    int width = 0;
    int height = 0;
    int offsetX = 0;
    int offsetY = 0;
    bool isEmpty() const { return width <= 0 || height <= 0; }
};

// A transform with a non-identity 2x2 part requires the per-glyph FT_Set_Transform
// path; an identity 2x2 (including pure translation) reproduces the original
// hint-rounded, integer-placed layout byte for byte.
bool hasLinearPart(const PkTransform &transform)
{
    return transform.m11() != 1.0 || transform.m12() != 0.0 ||
           transform.m21() != 0.0 || transform.m22() != 1.0;
}

Raster rasterize(const PkString &text, const PkFont &font,
                 const PkTransform &transform, const PkPointF &devicePoint)
{
    Raster result;
    FtLibrary library;
    const auto files = resolveFontFiles(font);
    if (!library.value || files.empty()) return result;
    using Face = std::unique_ptr<std::remove_pointer_t<FT_Face>, FtFaceDeleter>;
    std::vector<Face> faces(files.size());
    const auto loadFace = [&](std::size_t i) -> FT_Face {
        if (faces[i]) return faces[i].get();
        FT_Face face = nullptr;
        if (FT_New_Face(library.value, files[i].path.c_str(), files[i].index, &face)) return nullptr;
        faces[i].reset(face);
        const auto error = font.pixelSize() > 0
            ? FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(font.pixelSize()))
            : FT_Set_Char_Size(face, 0, (font.pointSize() > 0 ? font.pointSize() : 12) * 64, 96, 96);
        if (error) { faces[i].reset(); return nullptr; }
        return face;
    };
    FT_Face primary = loadFace(0);
    if (!primary) return result;
    const auto characters = codepoints(text);
    std::unique_ptr<raqm_t, decltype(&raqm_destroy)> layout(raqm_create(), raqm_destroy);
    if (!layout || !raqm_set_text(layout.get(), characters.data(), characters.size()) ||
        !raqm_set_freetype_face(layout.get(), primary) ||
        !raqm_set_freetype_load_flags(layout.get(), FT_LOAD_NO_BITMAP)) return result;
    int ascent = 0;
    int descent = 0;
    // Reuse the Krita text layout path: resolve face ranges before shaping the
    // entire string; Raqm retains cluster, combining, bidi and ligature context.
    for (std::size_t i = 0; i < characters.size(); ++i) {
        FT_Face selected = primary;
        for (std::size_t f = 0; f < files.size(); ++f) {
            FT_Face candidate = loadFace(f);
            if (candidate && FT_Get_Char_Index(candidate, characters[i])) { selected = candidate; break; }
        }
        if (!raqm_set_freetype_face_range(layout.get(), selected, i, 1)) return result;
        ascent = std::max(ascent, static_cast<int>((selected->size->metrics.ascender + 63) / 64));
        descent = std::max(descent, static_cast<int>((-selected->size->metrics.descender + 63) / 64));
    }
    if (!raqm_layout(layout.get())) return result;
    std::size_t glyphCount = 0;
    const raqm_glyph_t *shaped = raqm_get_glyphs(layout.get(), &glyphCount);
    if (!shaped) return result;

    if (!hasLinearPart(transform)) {
        // ---- Original render() core: Qt 5.15's hinted metrics box, integer pen
        //      placement, no per-glyph transform.  render() maps `stored` 1:1 onto
        //      the ARGB32 grayscale image, so this branch is byte-for-byte frozen.
        std::vector<Glyph> glyphs;
        FT_Pos pen = 0;
        int minimumX = std::numeric_limits<int>::max();
        int metricsWidth = 0;
        // Qt 5.15 QFontMetrics uses logical glyph order and the face's unkerned
        // advance for boundingBox; drawText uses the shaped visual positions.
        std::vector<std::size_t> logicalOrder(glyphCount);
        for (std::size_t i = 0; i < glyphCount; ++i) logicalOrder[i] = i;
        std::stable_sort(logicalOrder.begin(), logicalOrder.end(), [&](std::size_t a, std::size_t b) {
            return shaped[a].cluster < shaped[b].cluster;
        });
        std::vector<hb_script_t> scripts(characters.size());
        auto script = HB_SCRIPT_COMMON;
        for (std::size_t i = 0; i < characters.size(); ++i) {
            const auto current = hb_unicode_script(hb_unicode_funcs_get_default(), characters[i]);
            if (current != HB_SCRIPT_COMMON && current != HB_SCRIPT_INHERITED) script = current;
            scripts[i] = script;
        }
        FT_Pos metricsPen = 0;
        int runLeft = std::numeric_limits<int>::max();
        int runRight = 0;
        int runOrigin = 0;
        FT_Face runFace = nullptr;
        auto runScript = HB_SCRIPT_COMMON;
        const auto finishMetricsRun = [&] {
            if (runLeft == std::numeric_limits<int>::max()) return;
            minimumX = std::min(minimumX, runOrigin + runLeft);
            metricsWidth = std::max(metricsWidth, runOrigin + runRight - runLeft);
            runOrigin += static_cast<int>((metricsPen + 32) / 64);
            metricsPen = 0;
            runLeft = std::numeric_limits<int>::max();
            runRight = 0;
        };
        for (std::size_t i : logicalOrder) {
            const raqm_glyph_t &item = shaped[i];
            const auto itemScript = scripts[item.cluster];
            if (runFace && (runFace != item.ftface ||
                (itemScript != HB_SCRIPT_COMMON && runScript != HB_SCRIPT_COMMON && itemScript != runScript))) {
                finishMetricsRun();
            }
            runFace = item.ftface;
            runScript = itemScript;
            if (!item.x_advance || FT_Load_Glyph(item.ftface, item.index, FT_LOAD_NO_BITMAP)) continue;
            const FT_GlyphSlot slot = item.ftface->glyph;
            const double x = (metricsPen + item.x_offset + slot->metrics.horiBearingX) / 64.0;
            runLeft = std::min(runLeft, static_cast<int>(std::floor(x)));
            runRight = std::max(runRight, static_cast<int>(std::ceil(x + slot->metrics.width / 64.0)));
            metricsPen += slot->advance.x;
        }
        finishMetricsRun();
        if (minimumX == std::numeric_limits<int>::max()) minimumX = 0;
        for (std::size_t i = 0; i < glyphCount; ++i) {
            const raqm_glyph_t &item = shaped[i];
            FT_Face face = item.ftface;
            const double position = (pen + item.x_offset) / 64.0;
            pen += item.x_advance;
            if (FT_Load_Glyph(face, item.index, FT_LOAD_NO_BITMAP) != 0 ||
                FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
                continue;
            }

            Glyph glyph;
            const char *format = FT_Get_Font_Format(face);
            glyph.gammaCorrect = format && std::string(format) == "CFF";
            glyph.left = static_cast<int>(std::floor(position + 0.5)) + face->glyph->bitmap_left;
            glyph.top = face->glyph->bitmap_top + static_cast<int>(std::floor(item.y_offset / 64.0 + 0.5));
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
            glyphs.push_back(std::move(glyph));
        }

        const int width = std::max(1, metricsWidth);
        const int height = metricsWidth == 0 ? 1 : std::max(1, ascent + descent);
        result.width = width;
        result.height = height;
        // Offsets are relative to the snapped baseline origin (see `coverage`).
        // render()/_coverage_ at the origin snap to 0, so `render()`'s own
        // behavior is unaffected and stays byte-identical.
        result.offsetX = minimumX - static_cast<int>(std::floor(devicePoint.x()));
        result.offsetY = -ascent - static_cast<int>(std::lround(devicePoint.y()));
        result.stored.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 255);
        for (const Glyph &glyph : glyphs) {
            for (int row = 0; row < glyph.rows; ++row) {
                const int y = ascent - glyph.top + row;
                if (y < 0 || y >= height) continue;
                for (int column = 0; column < glyph.width; ++column) {
                    const int x = glyph.left - minimumX + column;
                    if (x < 0 || x >= width) continue;
                    const unsigned coverage = glyph.pixels[static_cast<std::size_t>(row) * glyph.pitch + column];
                    const unsigned previousValue = result.stored[static_cast<std::size_t>(y) * width + x];
                    result.stored[static_cast<std::size_t>(y) * width + x] =
                        blendCoverage(previousValue, coverage, glyph.gammaCorrect);
                }
            }
        }
        return result;
    }

    // ---- Transformed text: Qt's contract is that the transform is handed down to
    //      the font engine per glyph (qfontengine_p.h:206 / qrawfont.h:107), so the
    //      glyph is loaded at the brush size and FT_Set_Transform is applied to the
    //      slot.  matrix = the 2x2 part as FreeType 16.16 with the y axis flipped:
    //      fttypes.h:376 gives `x' = x*xx + y*xy, y' = x*yx + y*yy` in FreeType's
    //      y-up frame, so a PkTransform (row-vector, y down) maps to
    //      xx=m11, xy=-m21, yx=-m12, yy=m22.  delta.x = the 26.6 fractional part of
    //      the device-space pen x (Qt's subPixelPosition, which is x-only); the
    //      integer part places the bitmap.  The glyph outline is in 26.6 after the
    //      matrix, which is the unit FT_Outline_Translate works in, so delta is
    //      26.6 too.
    //      freetype.h:3674-3680: hinting is unaltered by the transform and
    //      `face.glyph.advance` is transformed but `face.glyph.metrics` is not --
    //      hence the pen accumulation below is in user space, not from metrics.
    const double m11 = transform.m11();
    const double m12 = transform.m12();
    const double m21 = transform.m21();
    const double m22 = transform.m22();
    FT_Matrix matrix;
    matrix.xx = static_cast<FT_Fixed>(std::lround(m11 * 65536.0));
    matrix.xy = static_cast<FT_Fixed>(std::lround(-m21 * 65536.0));
    matrix.yx = static_cast<FT_Fixed>(std::lround(-m12 * 65536.0));
    matrix.yy = static_cast<FT_Fixed>(std::lround(m22 * 65536.0));

    const double originX = devicePoint.x();
    const double originY = devicePoint.y();

    std::vector<Glyph> glyphs;
    FT_Pos pen = 0;
    int minLeft = std::numeric_limits<int>::max();
    int minTop = std::numeric_limits<int>::max();
    int maxRight = std::numeric_limits<int>::min();
    int maxBottom = std::numeric_limits<int>::min();
    for (std::size_t i = 0; i < glyphCount; ++i) {
        const raqm_glyph_t &item = shaped[i];
        FT_Face face = item.ftface;
        const double position = (pen + item.x_offset) / 64.0;
        pen += item.x_advance;
        // Device-space pen position (row-vector convention, qtransform.h).
        const double deviceX = originX + m11 * position + m21 * (item.y_offset / 64.0);
        const double deviceY = originY + m12 * position + m22 * (item.y_offset / 64.0);
        // Integer part of the pen -> placement, fraction -> delta.x.  This is
        // exactly Qt-with-FreeType: qfontengine.cpp:919 makes the sub-pixel
        // position `x - x.floor()` (in [0,1)) and qfontengine_ft.cpp:970 passes it
        // as the FT_Vector x, while qfontengine_ft.cpp:971 sets `v.y = 0` -- Qt
        // does no vertical sub-pixel positioning.  The matching blit,
        // qpaintengine_raster.cpp:2892-2893, is
        //   x = qFloor(pos.x) + bitmap_left,  y = qRound(pos.y) - bitmap_top
        // -- floor for x (the fraction went into the delta), round for y (there is
        // no fraction).  std::lround is round-half-away-from-zero, i.e. qRound.
        const int ix = static_cast<int>(std::floor(deviceX));
        const int iy = static_cast<int>(std::lround(deviceY));
        FT_Vector delta;
        delta.x = static_cast<FT_Pos>(std::lround((deviceX - ix) * 64.0));
        delta.y = 0;
        FT_Set_Transform(face, &matrix, &delta);
        if (FT_Load_Glyph(face, item.index, FT_LOAD_NO_BITMAP) != 0 ||
            FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
            continue;
        }
        Glyph glyph;
        const char *format = FT_Get_Font_Format(face);
        glyph.gammaCorrect = format && std::string(format) == "CFF";
        // FreeType reports bitmap_left/bitmap_top in the transformed space; device
        // y is down, so the bitmap's top edge sits bitmap_top above the pen line.
        glyph.left = ix + face->glyph->bitmap_left;
        glyph.top = iy - face->glyph->bitmap_top;
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
        minLeft = std::min(minLeft, glyph.left);
        minTop = std::min(minTop, glyph.top);
        maxRight = std::max(maxRight, glyph.left + glyph.width);
        maxBottom = std::max(maxBottom, glyph.top + glyph.rows);
        glyphs.push_back(std::move(glyph));
    }
    if (glyphs.empty()) return result;

    // The stored buffer is the axis-aligned ink box of the transformed glyphs,
    // anchored on the snapped baseline origin -- the same floor(x) / round(y) grid
    // the per-glyph pen above is snapped to, so the reported offset is exact.
    const int width = std::max(1, maxRight - minLeft);
    const int height = std::max(1, maxBottom - minTop);
    result.width = width;
    result.height = height;
    result.offsetX = minLeft - static_cast<int>(std::floor(originX));
    result.offsetY = minTop - static_cast<int>(std::lround(originY));
    result.stored.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 255);
    for (const Glyph &glyph : glyphs) {
        for (int row = 0; row < glyph.rows; ++row) {
            const int y = glyph.top - minTop + row;
            if (y < 0 || y >= height) continue;
            for (int column = 0; column < glyph.width; ++column) {
                const int x = glyph.left - minLeft + column;
                if (x < 0 || x >= width) continue;
                const unsigned coverage = glyph.pixels[static_cast<std::size_t>(row) * glyph.pitch + column];
                const unsigned previousValue = result.stored[static_cast<std::size_t>(y) * width + x];
                result.stored[static_cast<std::size_t>(y) * width + x] =
                    blendCoverage(previousValue, coverage, glyph.gammaCorrect);
            }
        }
    }
    return result;
}

}

PkPainterPath PkFontRasterizer::outline(const PkString &text, const PkFont &font, double *advance, Metrics *metrics)
{
    if (advance) *advance = 0;
    if (metrics) *metrics = {};
    PkPainterPath result;
    result.setFillRule(Pk::WindingFill);
    if (text.isEmpty()) return result;
    FtLibrary library;
    const auto files = resolveFontFiles(font);
    if (!library.value || files.empty()) return result;
    using Face = std::unique_ptr<std::remove_pointer_t<FT_Face>, FtFaceDeleter>;
    std::vector<Face> faces(files.size());
    const auto loadFace = [&](std::size_t i) -> FT_Face {
        if (faces[i]) return faces[i].get();
        FT_Face face = nullptr;
        if (FT_New_Face(library.value, files[i].path.c_str(), files[i].index, &face)) return nullptr;
        faces[i].reset(face);
        if (FT_Set_Pixel_Sizes(face, 0, font.pixelSize() > 0 ? font.pixelSize() : 12)) return nullptr;
        return face;
    };
    FT_Face primary = loadFace(0);
    if (!primary) return result;
    const auto characters = codepoints(text);
    std::unique_ptr<raqm_t, decltype(&raqm_destroy)> layout(raqm_create(), raqm_destroy);
    if (!layout || !raqm_set_text(layout.get(), characters.data(), characters.size()) ||
        !raqm_set_freetype_face(layout.get(), primary) ||
        !raqm_set_freetype_load_flags(layout.get(), FT_LOAD_NO_BITMAP)) return result;
    for (std::size_t i = 0; i < characters.size(); ++i) {
        FT_Face selected = primary;
        for (std::size_t f = 0; f < files.size(); ++f) {
            FT_Face candidate = loadFace(f);
            if (candidate && FT_Get_Char_Index(candidate, characters[i])) { selected = candidate; break; }
        }
        if (!raqm_set_freetype_face_range(layout.get(), selected, i, 1)) return {};
        if (metrics) {
            metrics->ascent = std::max(metrics->ascent, std::ceil(selected->size->metrics.ascender / 64.0));
            metrics->descent = std::max(metrics->descent, std::ceil(-selected->size->metrics.descender / 64.0));
        }
    }
    if (!raqm_layout(layout.get())) return result;
    std::size_t count = 0;
    const auto glyphs = raqm_get_glyphs(layout.get(), &count);
    FT_Pos penX = 0, penY = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const auto &glyph = glyphs[i];
        if (metrics && i + 1 == count) {
            FT_Set_Pixel_Sizes(glyph.ftface, 0, font.pixelSize() > 0 ? font.pixelSize() : 12);
            if (!FT_Load_Glyph(glyph.ftface, glyph.index, FT_LOAD_NO_BITMAP)) {
                const auto &m = glyph.ftface->glyph->metrics;
                metrics->rightOverhang = std::max(0.0, (m.horiBearingX + m.width - m.horiAdvance) / 64.0);
            }
        }
        // Qt's outline path hints at units-per-em, then scales with FT_MulFix.
        // Loading an unhinted outline directly at the requested size changes
        // the 26.6 rounding and loses the component placement of hinted fonts.
        const auto em = glyph.ftface->units_per_EM;
        if (!em || FT_Set_Char_Size(glyph.ftface, em << 6, em << 6, 0, 0)) continue;
        if (!FT_Load_Glyph(glyph.ftface, glyph.index, FT_LOAD_NO_BITMAP)) {
            const auto scale = FT_MulDiv((font.pixelSize() > 0 ? font.pixelSize() : 12) << 6, 1 << 10, em);
            auto &outline = glyph.ftface->glyph->outline;
            for (int point = 0; point < outline.n_points; ++point) {
                outline.points[point].x = FT_MulFix(outline.points[point].x, scale);
                outline.points[point].y = FT_MulFix(outline.points[point].y, scale);
            }
            const auto path = convertFromFreeTypeOutline(glyph.ftface->glyph);
            const PkTransform transform(1.0 / 64, 0, 0, -1.0 / 64,
                (penX + glyph.x_offset) / 64.0, -(penY + glyph.y_offset) / 64.0);
            result.addPath(transform.map(path));
        }
        // The Qt 5.15 default hinted text layout rounds shaped advances to
        // whole pixels before SVG applies its 100-pixel layout transform.
        penX += FT_Pos(std::round(glyph.x_advance / 64.0)) * 64;
        penY += FT_Pos(std::round(glyph.y_advance / 64.0)) * 64;
    }
    if (advance) *advance = penX / 64.0;
    return result;
}

PkImage PkFontRasterizer::render(const PkString &text, const PkFont &font)
{
    if (text.isEmpty()) {
        PkImage empty(1, 1, PkImage::Format_ARGB32);
        empty.fill(0xffffffffu);
        return empty;
    }
    const Raster raster = rasterize(text, font, PkTransform(), PkPointF(0, 0));
    if (raster.isEmpty()) return {};
    PkImage result(raster.width, raster.height, PkImage::Format_ARGB32);
    for (int y = 0; y < raster.height; ++y) {
        for (int x = 0; x < raster.width; ++x) {
            const unsigned value = raster.stored[static_cast<std::size_t>(y) * raster.width + x];
            result.setPixel(x, y, 0xff000000u | (value << 16) | (value << 8) | value);
        }
    }
    return result;
}

PkFontRasterizer::TextCoverage PkFontRasterizer::coverage(const PkString &text, const PkFont &font,
                                                          const PkPointF &devicePoint, const PkTransform &transform)
{
    TextCoverage coverage;
    if (text.isEmpty()) return coverage;
    const Raster raster = rasterize(text, font, transform, devicePoint);
    if (raster.isEmpty()) return coverage;
    coverage.width = raster.width;
    coverage.height = raster.height;
    coverage.offsetX = raster.offsetX;
    coverage.offsetY = raster.offsetY;
    coverage.mask.resize(raster.stored.size());
    for (std::size_t i = 0; i < raster.stored.size(); ++i) {
        coverage.mask[i] = static_cast<unsigned char>(255 - raster.stored[i]);
    }
    return coverage;
}
