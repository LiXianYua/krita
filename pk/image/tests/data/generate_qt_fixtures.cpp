#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace
{

void u16(std::vector<uint8_t> &bytes, uint16_t value)
{
    bytes.push_back(static_cast<uint8_t>(value));
    bytes.push_back(static_cast<uint8_t>(value >> 8));
}

void u32(std::vector<uint8_t> &bytes, uint32_t value)
{
    u16(bytes, static_cast<uint16_t>(value));
    u16(bytes, static_cast<uint16_t>(value >> 16));
}

void write(const std::filesystem::path &path, const std::vector<uint8_t> &bytes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char *>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
}

void writeText(const std::filesystem::path &path, const std::string &text)
{
    write(path, std::vector<uint8_t>(text.begin(), text.end()));
}

std::vector<uint8_t> bmp24(int32_t width, int32_t height)
{
    const uint32_t rowBytes = (static_cast<uint32_t>(width) * 3u + 3u) & ~3u;
    std::vector<uint8_t> bytes;
    bytes.reserve(54u + rowBytes * static_cast<uint32_t>(height));
    bytes.push_back('B'); bytes.push_back('M');
    u32(bytes, 54u + rowBytes * static_cast<uint32_t>(height));
    u32(bytes, 0); u32(bytes, 54);
    u32(bytes, 40); u32(bytes, static_cast<uint32_t>(width));
    u32(bytes, static_cast<uint32_t>(height));
    u16(bytes, 1); u16(bytes, 24); u32(bytes, 0);
    u32(bytes, rowBytes * static_cast<uint32_t>(height));
    u32(bytes, 0); u32(bytes, 0); u32(bytes, 0); u32(bytes, 0);
    // BMP rows are bottom-up and pixels are BGR: opaque red, opaque green.
    bytes.insert(bytes.end(), {0, 0, 255, 0, 255, 0});
    while (bytes.size() < 54u + rowBytes) bytes.push_back(0);
    return bytes;
}

std::vector<uint8_t> bmpRle(uint16_t bits)
{
    const bool rle8 = bits == 8;
    const uint32_t paletteCount = rle8 ? 2u : 3u;
    const uint32_t pixelOffset = 14u + 40u + paletteCount * 4u;
    const std::vector<uint8_t> encoded = rle8
        ? std::vector<uint8_t>{2, 1, 0, 0, 0, 1}
        : std::vector<uint8_t>{2, 0x12, 0, 0, 0, 1};
    std::vector<uint8_t> bytes;
    bytes.push_back('B'); bytes.push_back('M');
    u32(bytes, pixelOffset + static_cast<uint32_t>(encoded.size()));
    u32(bytes, 0); u32(bytes, pixelOffset);
    u32(bytes, 40); u32(bytes, 2); u32(bytes, 1);
    u16(bytes, 1); u16(bytes, bits); u32(bytes, rle8 ? 1u : 2u);
    u32(bytes, static_cast<uint32_t>(encoded.size()));
    u32(bytes, 0); u32(bytes, 0); u32(bytes, paletteCount); u32(bytes, 0);
    bytes.insert(bytes.end(), {0, 0, 0, 0, 0, 0, 255, 0});
    if (!rle8) bytes.insert(bytes.end(), {0, 255, 0, 0});
    bytes.insert(bytes.end(), encoded.begin(), encoded.end());
    return bytes;
}

std::vector<uint8_t> dib32(uint32_t width, uint32_t height,
                           const std::vector<uint8_t> &bgra, bool includeMask)
{
    std::vector<uint8_t> dib;
    u32(dib, 40); u32(dib, width); u32(dib, height * 2u);
    u16(dib, 1); u16(dib, 32); u32(dib, 0); u32(dib, static_cast<uint32_t>(bgra.size()));
    u32(dib, 0); u32(dib, 0); u32(dib, 0); u32(dib, 0);
    dib.insert(dib.end(), bgra.begin(), bgra.end());
    if (includeMask) {
        const std::size_t maskRow = ((static_cast<std::size_t>(width) + 31u) / 32u) * 4u;
        dib.insert(dib.end(), maskRow * height, 0);
    }
    return dib;
}

std::vector<uint8_t> iconFromDibs(uint16_t type,
                                  const std::vector<std::vector<uint8_t>> &dibs,
                                  const std::vector<std::pair<uint8_t, uint8_t>> &sizes)
{
    std::vector<uint8_t> bytes;
    u16(bytes, 0); u16(bytes, type); u16(bytes, static_cast<uint16_t>(dibs.size()));
    uint32_t offset = 6u + static_cast<uint32_t>(dibs.size()) * 16u;
    for (std::size_t i = 0; i < dibs.size(); ++i) {
        bytes.push_back(sizes[i].first); bytes.push_back(sizes[i].second);
        bytes.push_back(0); bytes.push_back(0);
        if (type == 1) { u16(bytes, 1); u16(bytes, 32); }
        else { u16(bytes, static_cast<uint16_t>(i + 3)); u16(bytes, static_cast<uint16_t>(i + 5)); }
        u32(bytes, static_cast<uint32_t>(dibs[i].size())); u32(bytes, offset);
        offset += static_cast<uint32_t>(dibs[i].size());
    }
    for (const std::vector<uint8_t> &dib : dibs) bytes.insert(bytes.end(), dib.begin(), dib.end());
    return bytes;
}

std::vector<uint8_t> icon(uint16_t type)
{
    std::vector<uint8_t> dib;
    u32(dib, 40); u32(dib, 2); u32(dib, 2); // XOR height + AND height
    u16(dib, 1); u16(dib, 32); u32(dib, 0); u32(dib, 8);
    u32(dib, 0); u32(dib, 0); u32(dib, 0); u32(dib, 0);
    // BGRA: opaque red, alpha-64 green. AND mask row follows.
    dib.insert(dib.end(), {0, 0, 255, 255, 0, 255, 0, 64});
    dib.insert(dib.end(), {0, 0, 0, 0});

    std::vector<uint8_t> bytes;
    u16(bytes, 0); u16(bytes, type); u16(bytes, 1);
    bytes.push_back(2); bytes.push_back(1); bytes.push_back(0); bytes.push_back(0);
    if (type == 1) { u16(bytes, 1); u16(bytes, 32); }
    else { u16(bytes, 7); u16(bytes, 9); }
    u32(bytes, static_cast<uint32_t>(dib.size())); u32(bytes, 22);
    bytes.insert(bytes.end(), dib.begin(), dib.end());
    return bytes;
}

void writeTruncated(const std::filesystem::path &path, const std::vector<uint8_t> &bytes)
{
    write(path, std::vector<uint8_t>(bytes.begin(), bytes.begin() + bytes.size() / 2));
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    const std::filesystem::path out(argv[1]);

    const std::vector<uint8_t> bmp = bmp24(2, 1);
    const std::string pbm = "P1\n2 1\n0 1\n";
    const std::string pgm = "P2\n2 1\n255\n0 255\n";
    const std::string ppm = "P3\n2 1\n255\n255 0 0  0 255 0\n";
    const std::string xbm =
        "#define oracle_width 2\n#define oracle_height 1\n"
        "static unsigned char oracle_bits[] = { 0x02 };\n";
    const std::string xpm =
        "/* XPM */\nstatic const char *oracle[] = {\n"
        "\"2 1 2 1\",\n\"R c #ff0000\",\n\". c None\",\n\"R.\"\n};\n";
    const std::vector<uint8_t> ico = icon(1);
    const std::vector<uint8_t> cur = icon(2);
    const std::vector<uint8_t> noMaskRed = dib32(1, 1, {0, 0, 255, 255}, false);
    const std::vector<uint8_t> larger = dib32(2, 1, {0, 255, 0, 255, 255, 0, 0, 255}, false);
    std::vector<uint8_t> zeroAlphaIco = ico;
    zeroAlphaIco[22 + 40 + 3] = 0;
    zeroAlphaIco[22 + 40 + 7] = 0;
    std::vector<uint8_t> masked32Ico = ico;
    masked32Ico[22 + 40 + 8] = 0x80;

    write(out / "valid.bmp", bmp);
    write(out / "valid-rle4.bmp", bmpRle(4));
    write(out / "valid-rle8.bmp", bmpRle(8));
    writeText(out / "valid.pbm", pbm);
    writeText(out / "valid.pgm", pgm);
    writeText(out / "valid.ppm", ppm);
    write(out / "valid-raw.pbm", {'P', '4', '\n', '2', ' ', '1', '\n', 0x40});
    write(out / "valid-raw.pgm", {'P', '5', '\n', '2', ' ', '1', '\n', '2', '5', '5', '\n', 0, 255});
    write(out / "valid-raw.ppm", {'P', '6', '\n', '2', ' ', '1', '\n', '2', '5', '5', '\n',
                                  255, 0, 0, 0, 255, 0});
    writeText(out / "valid.xbm", xbm);
    writeText(out / "valid.xpm", xpm);
    writeText(out / "named-colors.xpm",
              "/* XPM */\nstatic const char *named[] = {\n"
              "\"2 1 2 1\",\n\"A c aliceblue\",\n\"G c green\",\n\"AG\"\n};\n");
    write(out / "valid.ico", ico);
    write(out / "valid.cur", cur);
    write(out / "no-mask-32.ico", iconFromDibs(1, {noMaskRed}, {{1, 1}}));
    write(out / "no-mask-32.cur", iconFromDibs(2, {noMaskRed}, {{1, 1}}));
    write(out / "multi.ico", iconFromDibs(1, {noMaskRed, larger}, {{1, 1}, {2, 1}}));
    write(out / "multi.cur", iconFromDibs(2, {noMaskRed, larger}, {{1, 1}, {2, 1}}));
    write(out / "zero-alpha.ico", zeroAlphaIco);
    write(out / "masked-32.ico", masked32Ico);

    writeTruncated(out / "truncated.bmp", bmp);
    writeText(out / "truncated.pbm", "P1\n2");
    writeText(out / "truncated.pgm", "P2\n2 1\n255\n0");
    writeText(out / "truncated.ppm", "P3\n2 1\n255\n255 0 0");
    writeTruncated(out / "truncated.xbm", {xbm.begin(), xbm.end()});
    writeTruncated(out / "truncated.xpm", {xpm.begin(), xpm.end()});
    writeTruncated(out / "truncated.ico", ico);
    writeTruncated(out / "truncated.cur", cur);

    writeText(out / "malformed.pnm", "P9\n2 1\n255\n0 0\n");
    writeText(out / "malformed.xbm", "#define broken_width 2\n#define broken_height 1\n{ 0xz };\n");
    writeText(out / "malformed.xpm", "/* XPM */\n{ \"2 1 1 1\", \"R c #red\", \"R\" };\n");
    write(out / "malformed.ico", {0, 0, 1, 0, 0, 0});

    std::vector<uint8_t> hugeBmp = bmp24(2, 1);
    hugeBmp[18] = 0xff; hugeBmp[19] = 0xff; hugeBmp[20] = 0xff; hugeBmp[21] = 0x7f;
    write(out / "oversize.bmp", hugeBmp);
    writeText(out / "oversize.ppm", "P3\n2147483647 2147483647\n255\n");
    writeText(out / "oversize.xbm", "#define huge_width 2147483647\n#define huge_height 2147483647\n{ 0x00 };\n");
    writeText(out / "oversize.xpm", "/* XPM */\n{ \"2147483647 2147483647 1 1\", \". c None\", \".\" };\n");
    std::vector<uint8_t> hugeIco = ico;
    hugeIco[22 + 4] = 0xff; hugeIco[22 + 5] = 0xff;
    hugeIco[22 + 6] = 0xff; hugeIco[22 + 7] = 0x7f;
    write(out / "oversize.ico", hugeIco);
    return 0;
}
