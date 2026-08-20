#include <QByteArray>
#include <QDataStream>

#include <cstdio>

namespace {

void printHex(const QByteArray &bytes)
{
    for (unsigned char byte : bytes) {
        std::printf("%02x", static_cast<unsigned int>(byte));
    }
}

} // namespace

int main()
{
    for (QDataStream::ByteOrder order : {QDataStream::BigEndian, QDataStream::LittleEndian}) {
        QByteArray bytes;
        QDataStream writer(&bytes, QIODevice::WriteOnly);
        writer.setVersion(QDataStream::Qt_4_6);
        writer.setByteOrder(order);
        writer << false << true;

        bool first = true;
        bool second = false;
        QDataStream reader(bytes);
        reader.setVersion(QDataStream::Qt_4_6);
        reader.setByteOrder(order);
        reader >> first >> second;

        std::printf("order=%s bytes=", order == QDataStream::BigEndian ? "big" : "little");
        printHex(bytes);
        std::printf(" values=%u%u write_status=%d read_status=%d\n",
                    first ? 1u : 0u, second ? 1u : 0u,
                    static_cast<int>(writer.status()), static_cast<int>(reader.status()));
    }
}
