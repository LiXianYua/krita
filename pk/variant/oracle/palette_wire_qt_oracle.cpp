#include <QByteArray>
#include <QColor>
#include <QDataStream>

#include <cstdio>
#include <vector>

namespace {

void printHex(const QByteArray &bytes)
{
    for (unsigned char byte : bytes) {
        std::printf("%02x", static_cast<unsigned int>(byte));
    }
}

void emitColor(const char *name, const QColor &color, QDataStream::ByteOrder order)
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_4_6);
    stream.setByteOrder(order);
    stream << color;
    std::printf("order=%s name=%s bytes=",
                order == QDataStream::BigEndian ? "big" : "little", name);
    printHex(bytes);
    std::printf(" status=%d\n", static_cast<int>(stream.status()));
}

} // namespace

int main()
{
    QColor cmyk;
    cmyk.setCmyk(10, 20, 30, 40, 50);
    const std::vector<std::pair<const char *, QColor>> colors{
        {"invalid", QColor()},
        {"rgb", QColor(1, 2, 3, 4)},
        {"hsv", QColor::fromHsv(120, 128, 64, 32)},
        {"hsl", QColor::fromHsl(240, 128, 64, 32)},
        {"cmyk", cmyk},
        {"extended", QColor::fromRgbF(1.25, -0.25, 0.5, 0.75)},
    };
    for (QDataStream::ByteOrder order : {QDataStream::BigEndian, QDataStream::LittleEndian}) {
        QByteArray boolBytes;
        QDataStream boolStream(&boolBytes, QIODevice::WriteOnly);
        boolStream.setVersion(QDataStream::Qt_4_6);
        boolStream.setByteOrder(order);
        boolStream << false << true;
        std::printf("order=%s name=bool bytes=",
                    order == QDataStream::BigEndian ? "big" : "little");
        printHex(boolBytes);
        std::printf(" status=%d\n", static_cast<int>(boolStream.status()));
        for (const auto &entry : colors) emitColor(entry.first, entry.second, order);
    }
}
