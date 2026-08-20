#include "color_stream_case.h"

#include "PkColor.h"
#include "PkDataStream.h"
#include "PkTest.h"

#include "pk_binder_color_stream_case.inc"

#include <vector>

namespace {

PkByteArray fromHex(const char *hex)
{
    std::vector<unsigned char> bytes;
    for (const char *p = hex; p[0] != '\0'; p += 2) {
        const auto nibble = [](char c) -> unsigned char {
            return c >= '0' && c <= '9' ? static_cast<unsigned char>(c - '0')
                 : c >= 'a' && c <= 'f' ? static_cast<unsigned char>(c - 'a' + 10)
                 : static_cast<unsigned char>(c - 'A' + 10);
        };
        bytes.push_back(static_cast<unsigned char>((nibble(p[0]) << 4) | nibble(p[1])));
    }
    return PkByteArray(bytes);
}

} // namespace

void ColorStreamCase::colorsMatchQt46WireBytes()
{
    struct Fixture {
        const char *hex;
        PkColor::WireState state;
    };
    const Fixture fixtures[]{
        {"00ffff0000000000000000", {PkColor::Invalid, {0xffffu, 0u, 0u, 0u, 0u}}},
        {"0104040101020203030000", {PkColor::Rgb, {0x0404u, 0x0101u, 0x0202u, 0x0303u, 0u}}},
        {"0220202ee0808040400000", {PkColor::Hsv, {0x2020u, 0x2ee0u, 0x8080u, 0x4040u, 0u}}},
        {"0420205dc0808040400000", {PkColor::Hsl, {0x2020u, 0x5dc0u, 0x8080u, 0x4040u, 0u}}},
        {"0332320a0a14141e1e2828", {PkColor::Cmyk, {0x3232u, 0x0a0au, 0x1414u, 0x1e1eu, 0x2828u}}},
        {"053a003d00b40038000000", {PkColor::ExtendedRgb, {0x3a00u, 0x3d00u, 0xb400u, 0x3800u, 0u}}},
    };

    for (const Fixture &fixture : fixtures) {
        PkColor decoded;
        PkDataStream reader(fromHex(fixture.hex));
        reader.setVersion(PkDataStream::Qt_4_6);
        reader >> decoded;
        PK_COMPARE(reader.status(), PkDataStream::Ok);
        PK_VERIFY(decoded.wireState() == fixture.state);

        PkByteArray encoded;
        PkDataStream writer(&encoded, PkStream::WriteOnly);
        writer.setVersion(PkDataStream::Qt_4_6);
        writer << PkColor::fromWireState(fixture.state);
        PK_COMPARE(writer.status(), PkDataStream::Ok);
        PK_VERIFY(encoded == fromHex(fixture.hex));
    }
}

void ColorStreamCase::shortReadsRetainQt46DecodedState()
{
    struct Fixture {
        const char *hex;
        PkDataStream::Status status;
        int spec;
        const char *reencodedHex;
    };
    const Fixture fixtures[]{
        {"", PkDataStream::ReadPastEnd, 0, "0000000000000000000000"},
        {"01", PkDataStream::ReadPastEnd, 1, "0100000000000000000000"},
        {"0104", PkDataStream::ReadPastEnd, 1, "0100000000000000000000"},
        {"010404", PkDataStream::ReadPastEnd, 1, "0104040000000000000000"},
    };

    for (const Fixture &fixture : fixtures) {
        PkColor decoded(9, 8, 7, 6);
        PkDataStream reader(fromHex(fixture.hex));
        reader.setVersion(PkDataStream::Qt_4_6);
        reader >> decoded;
        PK_COMPARE(reader.status(), fixture.status);
        PK_COMPARE(int(decoded.wireState().spec), fixture.spec);

        PkByteArray encoded;
        PkDataStream writer(&encoded, PkStream::WriteOnly);
        writer.setVersion(PkDataStream::Qt_4_6);
        writer << decoded;
        PK_COMPARE(writer.status(), PkDataStream::Ok);
        PK_VERIFY(encoded == fromHex(fixture.reencodedHex));
    }
}

void ColorStreamCase::rawSpecsArePreservedLikeQt46()
{
    struct Fixture {
        const char *hex;
        int spec;
    };
    const Fixture fixtures[]{
        {"0611112222333344445555", 6},
        {"ff11112222333344445555", -1},
    };

    for (const Fixture &fixture : fixtures) {
        PkColor decoded;
        PkDataStream reader(fromHex(fixture.hex));
        reader.setVersion(PkDataStream::Qt_4_6);
        reader >> decoded;
        PK_COMPARE(reader.status(), PkDataStream::Ok);
        PK_COMPARE(int(decoded.wireState().spec), fixture.spec);

        PkByteArray encoded;
        PkDataStream writer(&encoded, PkStream::WriteOnly);
        writer.setVersion(PkDataStream::Qt_4_6);
        writer << decoded;
        PK_COMPARE(writer.status(), PkDataStream::Ok);
        PK_VERIFY(encoded == fromHex(fixture.hex));
    }
}

PK_TEST_MAIN(ColorStreamCase)
