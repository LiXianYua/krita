#include <QByteArray>
#include <QDataStream>

#include <cstdio>
#include <type_traits>

class PkDataStream;
static_assert(!std::is_same<QDataStream, PkDataStream>::value,
              "Qt and pk streams must be distinct");

namespace {

void printHex(const QByteArray &bytes)
{
    for (unsigned char byte : bytes) {
        std::printf("%02x", static_cast<unsigned int>(byte));
    }
}

void emitWrite(QDataStream::ByteOrder order)
{
    QByteArray bytes;
    QDataStream writer(&bytes, QIODevice::WriteOnly);
    writer.setVersion(QDataStream::Qt_4_6);
    writer.setByteOrder(order);
    writer << false << true;

    std::printf("kind=write order=%s name=false-true bytes=",
                order == QDataStream::BigEndian ? "big" : "little");
    printHex(bytes);
    std::printf(" status=%d\n", static_cast<int>(writer.status()));
}

void emitReadOne(const char *name, const QByteArray &bytes, QDataStream::ByteOrder order)
{
    bool value = true;
    QDataStream reader(bytes);
    reader.setVersion(QDataStream::Qt_4_6);
    reader.setByteOrder(order);
    reader >> value;

    std::printf("kind=read-one order=%s name=%s input=",
                order == QDataStream::BigEndian ? "big" : "little", name);
    printHex(bytes);
    std::printf(" value=%u status=%d\n", value ? 1u : 0u,
                static_cast<int>(reader.status()));
}

void emitReadTwo(const char *name, const QByteArray &bytes, QDataStream::ByteOrder order)
{
    bool first = false;
    bool second = true;
    QDataStream reader(bytes);
    reader.setVersion(QDataStream::Qt_4_6);
    reader.setByteOrder(order);
    reader >> first >> second;

    std::printf("kind=read-two order=%s name=%s input=",
                order == QDataStream::BigEndian ? "big" : "little", name);
    printHex(bytes);
    std::printf(" first=%u second=%u status=%d\n",
                first ? 1u : 0u, second ? 1u : 0u,
                static_cast<int>(reader.status()));
}

} // namespace

int main()
{
    for (QDataStream::ByteOrder order : {QDataStream::BigEndian, QDataStream::LittleEndian}) {
        emitWrite(order);
        emitReadOne("empty", QByteArray(), order);
        emitReadOne("false", QByteArray::fromHex("00"), order);
        emitReadOne("true", QByteArray::fromHex("01"), order);
        emitReadOne("raw-02", QByteArray::fromHex("02"), order);
        emitReadOne("raw-ff", QByteArray::fromHex("ff"), order);
        emitReadTwo("complete", QByteArray::fromHex("0100"), order);
        emitReadTwo("short-second", QByteArray::fromHex("01"), order);
    }
}
