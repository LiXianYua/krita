#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
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
    std::vector<uint8_t> zeroAlphaIco = ico;
    zeroAlphaIco[22 + 40 + 3] = 0;
    zeroAlphaIco[22 + 40 + 7] = 0;
    std::vector<uint8_t> masked32Ico = ico;
    masked32Ico[22 + 40 + 8] = 0x80;

    write(out / "valid.bmp", bmp);
    writeText(out / "valid.pbm", pbm);
    writeText(out / "valid.pgm", pgm);
    writeText(out / "valid.ppm", ppm);
    write(out / "valid-raw.pbm", {'P', '4', '\n', '2', ' ', '1', '\n', 0x40});
    write(out / "valid-raw.pgm", {'P', '5', '\n', '2', ' ', '1', '\n', '2', '5', '5', '\n', 0, 255});
    write(out / "valid-raw.ppm", {'P', '6', '\n', '2', ' ', '1', '\n', '2', '5', '5', '\n',
                                  255, 0, 0, 0, 255, 0});
    writeText(out / "valid.xbm", xbm);
    writeText(out / "valid.xpm", xpm);
    write(out / "valid.ico", ico);
    write(out / "valid.cur", cur);
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
