#include "PkColor.h"
#include "PkDataStream.h"

#include <cstdio>
#include <initializer_list>
#include <type_traits>

class QColor;
class QDataStream;
static_assert(!std::is_same<PkColor, QColor>::value, "Qt and pk colors must be distinct");
static_assert(!std::is_same<PkDataStream, QDataStream>::value,
              "Qt and pk streams must be distinct");

namespace {

const char *orderName(PkDataStream::ByteOrder order)
{
    return order == PkDataStream::BigEndian ? "big" : "little";
}

void printHex(const PkByteArray &bytes)
{
    for (int i = 0; i < bytes.size(); ++i) {
        std::printf("%02x", static_cast<unsigned int>(
            static_cast<unsigned char>(bytes.constData()[i])));
    }
}

PkByteArray encode(const PkColor &color, PkDataStream::ByteOrder order)
{
    PkByteArray bytes;
    PkDataStream stream(&bytes, PkStream::WriteOnly);
    stream.setVersion(PkDataStream::Qt_4_6);
    stream.setByteOrder(order);
    stream << color;
    return bytes;
}

PkByteArray rawColor(std::int8_t spec, std::initializer_list<quint16> channels,
                     PkDataStream::ByteOrder order)
{
    PkByteArray bytes;
    PkDataStream stream(&bytes, PkStream::WriteOnly);
    stream.setVersion(PkDataStream::Qt_4_6);
    stream.setByteOrder(order);
    stream << spec;
    for (quint16 channel : channels) stream << static_cast<std::uint16_t>(channel);
    return bytes;
}

PkByteArray prefix(const PkByteArray &bytes, int length)
{
    return PkByteArray(bytes.constData(), length);
}

void emitBool(PkDataStream::ByteOrder order)
{
    PkByteArray bytes;
    PkDataStream stream(&bytes, PkStream::WriteOnly);
    stream.setVersion(PkDataStream::Qt_4_6);
    stream.setByteOrder(order);
    stream << false << true;
    std::printf("kind=write-bool order=%s name=false-true bytes=", orderName(order));
    printHex(bytes);
    std::printf(" status=%d\n", static_cast<int>(stream.status()));
}

void emitWrite(const char *name, const PkColor &color, PkDataStream::ByteOrder order)
{
    const PkByteArray bytes = encode(color, order);
    std::printf("kind=write-color order=%s name=%s bytes=", orderName(order), name);
    printHex(bytes);
    std::printf("\n");
}

void emitRead(const char *name, const PkByteArray &input, PkDataStream::ByteOrder order)
{
    PkColor color(9, 8, 7, 6);
    PkDataStream reader(input);
    reader.setVersion(PkDataStream::Qt_4_6);
    reader.setByteOrder(order);
    reader >> color;
    const PkByteArray reencoded = encode(color, order);

    std::printf("kind=read-color order=%s name=%s input=", orderName(order), name);
    printHex(input);
    std::printf(" status=%d spec=%d reencoded=",
                static_cast<int>(reader.status()), static_cast<int>(color.spec()));
    printHex(reencoded);
    std::printf("\n");
}

void emitMutations(const PkByteArray &extended, PkDataStream::ByteOrder order)
{
    const auto read = [&](PkColor &color) {
        PkDataStream stream(extended);
        stream.setVersion(PkDataStream::Qt_4_6);
        stream.setByteOrder(order);
        stream >> color;
    };
    const auto outputRow = [&](const char *name, const PkColor &color) {
        std::printf("kind=mutate-color order=%s name=%s bytes=", orderName(order), name);
        printHex(encode(color, order));
        std::printf(" spec=%d\n", static_cast<int>(color.spec()));
    };

    PkColor setRgbF;
    read(setRgbF);
    setRgbF.setRgbF(0.25, 0.5, 0.75, 1.0);
    outputRow("extended-set-rgbf", setRgbF);

    PkColor setRgb;
    read(setRgb);
    setRgb.setRgb(1, 2, 3, 4);
    outputRow("extended-set-rgb", setRgb);

    PkColor setAlphaF;
    read(setAlphaF);
    setAlphaF.setAlphaF(0.5);
    outputRow("extended-set-alphaf", setAlphaF);
}

} // namespace

int main()
{
    const PkColor cmyk = PkColor::fromWireState(
        {PkColor::Cmyk, {0x3232u, 0x0a0au, 0x1414u, 0x1e1eu, 0x2828u}});
    for (PkDataStream::ByteOrder order : {PkDataStream::BigEndian, PkDataStream::LittleEndian}) {
        emitBool(order);
        emitWrite("invalid", PkColor(), order);
        emitWrite("rgb", PkColor(1, 2, 3, 4), order);
        emitWrite("hsv", PkColor::fromHsv(120, 128, 64, 32), order);
        emitWrite("hsl", PkColor::fromHsl(240, 128, 64, 32), order);
        emitWrite("cmyk", cmyk, order);
        emitWrite("extended", PkColor::fromRgbF(1.25, -0.25, 0.5, 0.75), order);

        const PkByteArray rgb = rawColor(1, {0x0404u, 0x0101u, 0x0202u, 0x0303u, 0u}, order);
        for (int length = 0; length < rgb.size(); ++length) {
            char name[32];
            std::snprintf(name, sizeof(name), "rgb-prefix-%02d", length);
            emitRead(name, prefix(rgb, length), order);
        }
        emitRead("raw-spec-06", rawColor(6, {0x1111u, 0x2222u, 0x3333u, 0x4444u, 0x5555u}, order), order);
        emitRead("raw-spec-7f", rawColor(0x7f, {1u, 2u, 3u, 4u, 5u}, order), order);
        emitRead("raw-spec-ff", rawColor(-1, {0x1111u, 0x2222u, 0x3333u, 0x4444u, 0x5555u}, order), order);

        const PkByteArray extended = rawColor(5, {0x3a00u, 0x3d00u, 0xb400u, 0x3800u, 0xabcdu}, order);
        emitRead("extended-pad", extended, order);
        emitMutations(extended, order);
    }
}
