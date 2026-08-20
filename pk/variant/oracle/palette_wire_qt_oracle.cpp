#include <QByteArray>
#include <QColor>
#include <QDataStream>

#include <cstdio>
#include <initializer_list>
#include <type_traits>

class PkColor;
class PkDataStream;
static_assert(!std::is_same<QColor, PkColor>::value, "Qt and pk colors must be distinct");
static_assert(!std::is_same<QDataStream, PkDataStream>::value,
              "Qt and pk streams must be distinct");

namespace {

const char *orderName(QDataStream::ByteOrder order)
{
    return order == QDataStream::BigEndian ? "big" : "little";
}

void printHex(const QByteArray &bytes)
{
    for (unsigned char byte : bytes) std::printf("%02x", static_cast<unsigned int>(byte));
}

QByteArray encode(const QColor &color, QDataStream::ByteOrder order)
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_4_6);
    stream.setByteOrder(order);
    stream << color;
    return bytes;
}

QByteArray rawColor(qint8 spec, std::initializer_list<quint16> channels,
                    QDataStream::ByteOrder order)
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_4_6);
    stream.setByteOrder(order);
    stream << spec;
    for (quint16 channel : channels) stream << channel;
    return bytes;
}

void emitBool(QDataStream::ByteOrder order)
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_4_6);
    stream.setByteOrder(order);
    stream << false << true;
    std::printf("kind=write-bool order=%s name=false-true bytes=", orderName(order));
    printHex(bytes);
    std::printf(" status=%d\n", static_cast<int>(stream.status()));
}

void emitWrite(const char *name, const QColor &color, QDataStream::ByteOrder order)
{
    const QByteArray bytes = encode(color, order);
    std::printf("kind=write-color order=%s name=%s bytes=", orderName(order), name);
    printHex(bytes);
    std::printf("\n");
}

void emitRead(const char *name, const QByteArray &input, QDataStream::ByteOrder order)
{
    QColor color(9, 8, 7, 6);
    QDataStream reader(input);
    reader.setVersion(QDataStream::Qt_4_6);
    reader.setByteOrder(order);
    reader >> color;
    const QByteArray reencoded = encode(color, order);

    std::printf("kind=read-color order=%s name=%s input=", orderName(order), name);
    printHex(input);
    std::printf(" status=%d spec=%d reencoded=",
                static_cast<int>(reader.status()), static_cast<int>(color.spec()));
    printHex(reencoded);
    std::printf("\n");
}

void emitMutations(const QByteArray &extended, QDataStream::ByteOrder order)
{
    const auto read = [&](QColor &color) {
        QDataStream stream(extended);
        stream.setVersion(QDataStream::Qt_4_6);
        stream.setByteOrder(order);
        stream >> color;
    };
    const auto outputRow = [&](const char *name, const QColor &color) {
        std::printf("kind=mutate-color order=%s name=%s bytes=", orderName(order), name);
        printHex(encode(color, order));
        std::printf(" spec=%d\n", static_cast<int>(color.spec()));
    };

    QColor setRgbF;
    read(setRgbF);
    setRgbF.setRgbF(0.25, 0.5, 0.75, 1.0);
    outputRow("extended-set-rgbf", setRgbF);

    QColor setRgb;
    read(setRgb);
    setRgb.setRgb(1, 2, 3, 4);
    outputRow("extended-set-rgb", setRgb);

    QColor setAlphaF;
    read(setAlphaF);
    setAlphaF.setAlphaF(0.5);
    outputRow("extended-set-alphaf", setAlphaF);
}

} // namespace

int main()
{
    QColor cmyk;
    cmyk.setCmyk(10, 20, 30, 40, 50);
    for (QDataStream::ByteOrder order : {QDataStream::BigEndian, QDataStream::LittleEndian}) {
        emitBool(order);
        emitWrite("invalid", QColor(), order);
        emitWrite("rgb", QColor(1, 2, 3, 4), order);
        emitWrite("hsv", QColor::fromHsv(120, 128, 64, 32), order);
        emitWrite("hsl", QColor::fromHsl(240, 128, 64, 32), order);
        emitWrite("cmyk", cmyk, order);
        emitWrite("extended", QColor::fromRgbF(1.25, -0.25, 0.5, 0.75), order);

        const QByteArray rgb = rawColor(1, {0x0404u, 0x0101u, 0x0202u, 0x0303u, 0u}, order);
        for (int length = 0; length < rgb.size(); ++length) {
            char name[32];
            std::snprintf(name, sizeof(name), "rgb-prefix-%02d", length);
            emitRead(name, rgb.left(length), order);
        }
        emitRead("raw-spec-06", rawColor(6, {0x1111u, 0x2222u, 0x3333u, 0x4444u, 0x5555u}, order), order);
        emitRead("raw-spec-7f", rawColor(0x7f, {1u, 2u, 3u, 4u, 5u}, order), order);
        emitRead("raw-spec-ff", rawColor(-1, {0x1111u, 0x2222u, 0x3333u, 0x4444u, 0x5555u}, order), order);

        const QByteArray extended = rawColor(5, {0x3a00u, 0x3d00u, 0xb400u, 0x3800u, 0xabcdu}, order);
        emitRead("extended-pad", extended, order);
        emitMutations(extended, order);
    }
}
