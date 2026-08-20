#include "PkDataStream.h"

#include <cstdio>
#include <type_traits>
#include <vector>

class QDataStream;
static_assert(!std::is_same<PkDataStream, QDataStream>::value,
              "Qt and pk streams must be distinct");

namespace {

void printHex(const PkByteArray &bytes)
{
    for (int i = 0; i < bytes.size(); ++i) {
        std::printf("%02x", static_cast<unsigned int>(
            static_cast<unsigned char>(bytes.constData()[i])));
    }
}

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

void emitWrite(PkDataStream::ByteOrder order)
{
    PkByteArray bytes;
    PkDataStream writer(&bytes, PkStream::WriteOnly);
    writer.setVersion(PkDataStream::Qt_4_6);
    writer.setByteOrder(order);
    writer << false << true;

    std::printf("kind=write order=%s name=false-true bytes=",
                order == PkDataStream::BigEndian ? "big" : "little");
    printHex(bytes);
    std::printf(" status=%d\n", static_cast<int>(writer.status()));
}

void emitReadOne(const char *name, const PkByteArray &bytes, PkDataStream::ByteOrder order)
{
    bool value = true;
    PkDataStream reader(bytes);
    reader.setVersion(PkDataStream::Qt_4_6);
    reader.setByteOrder(order);
    reader >> value;

    std::printf("kind=read-one order=%s name=%s input=",
                order == PkDataStream::BigEndian ? "big" : "little", name);
    printHex(bytes);
    std::printf(" value=%u status=%d\n", value ? 1u : 0u,
                static_cast<int>(reader.status()));
}

void emitReadTwo(const char *name, const PkByteArray &bytes, PkDataStream::ByteOrder order)
{
    bool first = false;
    bool second = true;
    PkDataStream reader(bytes);
    reader.setVersion(PkDataStream::Qt_4_6);
    reader.setByteOrder(order);
    reader >> first >> second;

    std::printf("kind=read-two order=%s name=%s input=",
                order == PkDataStream::BigEndian ? "big" : "little", name);
    printHex(bytes);
    std::printf(" first=%u second=%u status=%d\n",
                first ? 1u : 0u, second ? 1u : 0u,
                static_cast<int>(reader.status()));
}

} // namespace

int main()
{
    for (PkDataStream::ByteOrder order : {PkDataStream::BigEndian, PkDataStream::LittleEndian}) {
        emitWrite(order);
        emitReadOne("empty", PkByteArray(), order);
        emitReadOne("false", fromHex("00"), order);
        emitReadOne("true", fromHex("01"), order);
        emitReadOne("raw-02", fromHex("02"), order);
        emitReadOne("raw-ff", fromHex("ff"), order);
        emitReadTwo("complete", fromHex("0100"), order);
        emitReadTwo("short-second", fromHex("01"), order);
    }
}
