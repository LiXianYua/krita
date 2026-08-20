#include "PkDataStream.h"

#include <cstdio>

namespace {

void printHex(const PkByteArray &bytes)
{
    for (int i = 0; i < bytes.size(); ++i) {
        std::printf("%02x", static_cast<unsigned int>(static_cast<unsigned char>(bytes.constData()[i])));
    }
}

} // namespace

int main()
{
    for (PkDataStream::ByteOrder order : {PkDataStream::BigEndian, PkDataStream::LittleEndian}) {
        PkByteArray bytes;
        PkDataStream writer(&bytes, PkStream::WriteOnly);
        writer.setVersion(PkDataStream::Qt_4_6);
        writer.setByteOrder(order);
        writer << false << true;

        bool first = true;
        bool second = false;
        PkDataStream reader(bytes);
        reader.setVersion(PkDataStream::Qt_4_6);
        reader.setByteOrder(order);
        reader >> first >> second;

        std::printf("order=%s bytes=", order == PkDataStream::BigEndian ? "big" : "little");
        printHex(bytes);
        std::printf(" values=%u%u write_status=%d read_status=%d\n",
                    first ? 1u : 0u, second ? 1u : 0u,
                    static_cast<int>(writer.status()), static_cast<int>(reader.status()));
    }
}
