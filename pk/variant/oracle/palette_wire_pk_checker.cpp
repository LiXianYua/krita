#include "PkColor.h"
#include "PkDataStream.h"

#include <cstdio>
#include <vector>

namespace {

void printHex(const PkByteArray &bytes)
{
    for (int i = 0; i < bytes.size(); ++i) {
        std::printf("%02x", static_cast<unsigned int>(static_cast<unsigned char>(bytes.constData()[i])));
    }
}

void emitColor(const char *name, const PkColor &color, PkDataStream::ByteOrder order)
{
    PkByteArray bytes;
    PkDataStream stream(&bytes, PkStream::WriteOnly);
    stream.setVersion(PkDataStream::Qt_4_6);
    stream.setByteOrder(order);
    stream << color;
    std::printf("order=%s name=%s bytes=",
                order == PkDataStream::BigEndian ? "big" : "little", name);
    printHex(bytes);
    std::printf(" status=%d\n", static_cast<int>(stream.status()));
}

} // namespace

int main()
{
    const std::vector<std::pair<const char *, PkColor>> colors{
        {"invalid", PkColor()},
        {"rgb", PkColor(1, 2, 3, 4)},
        {"hsv", PkColor::fromHsv(120, 128, 64, 32)},
        {"hsl", PkColor::fromHsl(240, 128, 64, 32)},
        {"cmyk", PkColor::fromWireState(
            {PkColor::Cmyk, {0x3232u, 0x0a0au, 0x1414u, 0x1e1eu, 0x2828u}})},
        {"extended", PkColor::fromRgbF(1.25, -0.25, 0.5, 0.75)},
    };
    for (PkDataStream::ByteOrder order : {PkDataStream::BigEndian, PkDataStream::LittleEndian}) {
        PkByteArray boolBytes;
        PkDataStream boolStream(&boolBytes, PkStream::WriteOnly);
        boolStream.setVersion(PkDataStream::Qt_4_6);
        boolStream.setByteOrder(order);
        boolStream << false << true;
        std::printf("order=%s name=bool bytes=",
                    order == PkDataStream::BigEndian ? "big" : "little");
        printHex(boolBytes);
        std::printf(" status=%d\n", static_cast<int>(boolStream.status()));
        for (const auto &entry : colors) emitColor(entry.first, entry.second, order);
    }
}
